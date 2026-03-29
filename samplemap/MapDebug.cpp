#include "MapAnalysis.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>

namespace
{
float estimateRms(const float* data, int start, int length)
{
    if (length <= 0)
        return 0.0f;

    double sum = 0.0;
    for (int i = 0; i < length; ++i)
    {
        const auto sample = data[start + i];
        sum += static_cast<double>(sample) * static_cast<double>(sample);
    }

    return static_cast<float>(std::sqrt(sum / static_cast<double>(length)));
}

int normaliseMidiForMapping(int midi)
{
    constexpr int preferredLow = 48;
    constexpr int preferredHigh = 64;

    while (midi > preferredHigh)
        midi -= 12;

    while (midi < preferredLow)
        midi += 12;

    return juce::jlimit(0, 127, midi);
}

float estimatePitchHz(const float* data, int start, int length, double sampleRate)
{
    constexpr float minFrequency = 40.0f;
    constexpr float maxFrequency = 1760.0f;

    const auto minLag = std::max(2, static_cast<int>(sampleRate / maxFrequency));
    const auto maxLag = std::min(length - 2, static_cast<int>(sampleRate / minFrequency));
    if (maxLag <= minLag)
        return 0.0f;

    auto scoreForLag = [data, start, length](int lag)
    {
        double correlation = 0.0;
        double energyA = 0.0;
        double energyB = 0.0;

        for (int i = 0; i < length - lag; ++i)
        {
            const auto a = static_cast<double>(data[start + i]);
            const auto b = static_cast<double>(data[start + i + lag]);
            correlation += a * b;
            energyA += a * a;
            energyB += b * b;
        }

        const auto normaliser = std::sqrt(energyA * energyB);
        return normaliser > 0.0 ? correlation / normaliser : 0.0;
    };

    std::vector<std::pair<int, double>> lagScores;
    lagScores.reserve(static_cast<size_t>(maxLag - minLag + 1));

    double bestCorrelation = 0.0;
    int bestLag = 0;

    for (int lag = minLag; lag <= maxLag; lag += (lag < 128 ? 1 : 2))
    {
        const auto score = scoreForLag(lag);
        lagScores.emplace_back(lag, score);

        if (score > bestCorrelation)
        {
            bestCorrelation = score;
            bestLag = lag;
        }
    }

    if (bestLag == 0 || bestCorrelation < 0.72)
        return 0.0f;

    const auto preferredThreshold = std::max(0.52, bestCorrelation * 0.68);
    for (auto it = lagScores.rbegin(); it != lagScores.rend(); ++it)
    {
        if (it->second >= preferredThreshold)
        {
            bestLag = it->first;
            bestCorrelation = it->second;
            break;
        }
    }

    return static_cast<float>(sampleRate / static_cast<double>(bestLag));
}

std::vector<aggregamap::Region> analyseFile(const juce::File& file)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
        throw std::runtime_error("Could not read file");

    juce::AudioBuffer<float> source(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
    reader->read(&source, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

    if (source.getNumSamples() < 4096)
        return {};

    juce::AudioBuffer<float> mono(1, source.getNumSamples());
    mono.copyFrom(0, 0, source, 0, 0, source.getNumSamples());
    if (source.getNumChannels() > 1)
        mono.addFrom(0, 0, source, 1, 0, source.getNumSamples(), 1.0f);
    mono.applyGain(0.5f);

    auto* monoData = mono.getWritePointer(0);
    const auto cutoffHz = 1200.0f;
    const auto rc = 1.0f / (juce::MathConstants<float>::twoPi * cutoffHz);
    const auto dt = 1.0f / static_cast<float>(reader->sampleRate);
    const auto alpha = dt / (rc + dt);
    float filtered = monoData[0];
    for (int i = 1; i < mono.getNumSamples(); ++i)
    {
        filtered += alpha * (monoData[i] - filtered);
        monoData[i] = filtered;
    }

    constexpr int frameSize = 4096;
    constexpr int hopSize = 2048;
    const auto* data = mono.getReadPointer(0);

    struct FrameInfo { int midi = -1; int start = 0; int length = 0; float rms = 0.0f; };
    std::vector<FrameInfo> frames;
    frames.reserve(static_cast<size_t>(mono.getNumSamples() / hopSize) + 1);

    for (int start = 0; start + frameSize < mono.getNumSamples(); start += hopSize)
    {
        const auto rms = estimateRms(data, start, frameSize);
        if (rms < 0.015f)
        {
            frames.push_back({ -1, start, frameSize, rms });
            continue;
        }

        const auto pitchHz = estimatePitchHz(data, start, frameSize, reader->sampleRate);
        if (pitchHz <= 0.0f)
        {
            frames.push_back({ -1, start, frameSize, rms });
            continue;
        }

        const auto detectedMidi = juce::roundToInt(69.0 + 12.0 * std::log2(static_cast<double>(pitchHz) / 440.0));
        const auto midi = normaliseMidiForMapping(detectedMidi);
        frames.push_back({ midi, start, frameSize, rms });
    }

    std::vector<aggregamap::Region> regions;
    if (frames.empty())
        return regions;

    const auto minSegmentSamples = juce::jmax(1, static_cast<int>(reader->sampleRate * 0.08));
    const auto maxSegmentSamples = juce::jmax(minSegmentSamples, static_cast<int>(reader->sampleRate * 0.7));

    int currentMidi = frames.front().midi;
    int regionStart = frames.front().start;
    float rmsAccumulator = currentMidi >= 0 ? frames.front().rms : 0.0f;
    int rmsCount = currentMidi >= 0 ? 1 : 0;

    auto flushRegion = [&](int midi, int startSample, int endSample, float rmsValue)
    {
        const auto clampedStart = juce::jlimit(0, mono.getNumSamples() - 1, startSample);
        const auto clampedEnd = juce::jlimit(clampedStart + 1, mono.getNumSamples(), endSample);
        const auto length = clampedEnd - clampedStart;
        if (midi < 0 || length < minSegmentSamples)
            return;

        for (int chunkStart = clampedStart; chunkStart < clampedEnd; chunkStart += maxSegmentSamples)
        {
            const auto chunkEnd = juce::jmin(clampedEnd, chunkStart + maxSegmentSamples);
            const auto chunkLength = chunkEnd - chunkStart;
            if (chunkLength < minSegmentSamples)
                break;

            regions.push_back({ midi, chunkStart, chunkEnd, rmsValue });
        }
    };

    for (size_t i = 1; i < frames.size(); ++i)
    {
        const auto& frame = frames[i];
        if (frame.midi == currentMidi)
        {
            if (frame.midi >= 0)
            {
                rmsAccumulator += frame.rms;
                ++rmsCount;
            }
            continue;
        }

        flushRegion(currentMidi, regionStart, frame.start, rmsCount > 0 ? rmsAccumulator / static_cast<float>(rmsCount) : 0.0f);
        currentMidi = frame.midi;
        regionStart = frame.start;
        rmsAccumulator = frame.midi >= 0 ? frame.rms : 0.0f;
        rmsCount = frame.midi >= 0 ? 1 : 0;
    }

    const auto& lastFrame = frames.back();
    flushRegion(currentMidi, regionStart, lastFrame.start + lastFrame.length, rmsCount > 0 ? rmsAccumulator / static_cast<float>(rmsCount) : 0.0f);
    return aggregamap::stabiliseDetectedRegions(std::move(regions));
}
}

int main(int argc, char* argv[])
{
    using aggregamap::NoteAssignment;

    if (argc < 2)
    {
        std::cout << "Usage: AggregaMapDebug <audio-file>\n";
        return 1;
    }

    const juce::File file(juce::String::fromUTF8(argv[1]));
    if (! file.existsAsFile())
    {
        std::cout << "File not found: " << file.getFullPathName() << "\n";
        return 1;
    }

    try
    {
        auto stabilised = analyseFile(file);
        const auto inferredRoots = aggregamap::inferRepresentativeRegions(stabilised);
        std::array<NoteAssignment, 128> assignments {};
        aggregamap::rebuildAssignmentsFromRegions(stabilised, assignments);

        std::map<int, int> rootCounts;
        for (const auto& region : stabilised)
            ++rootCounts[region.rootMidiNote];

        std::vector<aggregamap::Region> strongest = stabilised;
        std::sort(strongest.begin(), strongest.end(), [](const auto& a, const auto& b)
        {
            if (a.rms != b.rms)
                return a.rms > b.rms;
            return a.startSample < b.startSample;
        });

        if (strongest.size() > 20)
            strongest.resize(20);

        std::cout << "Analysed file: " << file.getFullPathName() << "\n";
        std::cout << "Detected regions: " << stabilised.size() << "\n";
        std::cout << "Inferred note roots: ";
        for (size_t i = 0; i < inferredRoots.size(); ++i)
        {
            std::cout << inferredRoots[i].rootMidiNote;
            if (i + 1 < inferredRoots.size())
                std::cout << ", ";
        }
        std::cout << "\n\n";

        std::cout << "Root histogram:\n";
        for (const auto& [root, count] : rootCounts)
            std::cout << "  root=" << std::setw(3) << root << " count=" << count << "\n";

        std::cout << "\nStrongest regions:\n";
        for (const auto& region : strongest)
        {
            std::cout << "  root=" << std::setw(3) << region.rootMidiNote
                      << " start=" << region.startSample
                      << " end=" << region.endSample
                      << " rms=" << region.rms << "\n";
        }

        std::cout << "\nAssignments around D3..A4:\n";
        for (int midi = 50; midi <= 69; ++midi)
        {
            const auto& assignment = assignments[static_cast<size_t>(midi)];
            std::cout << "  midi=" << midi << " -> root=" << assignment.rootMidiNote
                      << " valid=" << assignment.valid
                      << " gain=" << assignment.levelCompensation << "\n";
        }
    }
    catch (const std::exception& ex)
    {
        std::cout << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
