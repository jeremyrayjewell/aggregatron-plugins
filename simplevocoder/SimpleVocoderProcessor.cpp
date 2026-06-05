#include "SimpleVocoderProcessor.h"
#include "SimpleVocoderEditor.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr std::array<float, 8> baseFrequencies { 120.0f, 220.0f, 360.0f, 520.0f, 820.0f, 1300.0f, 2200.0f, 3800.0f };
constexpr std::array<int, 7> dMajorPitchClasses { 2, 4, 6, 7, 9, 11, 1 };
}

SimpleVocoderAudioProcessor::SimpleVocoderAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout()),
      analysisBuffer(1, analysisWindowSize)
{
    updateBandFrequencies();
}

void SimpleVocoderAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
    updateBandFrequencies();
    std::fill(envelopes.begin(), envelopes.end(), 0.0f);
    leftCarrier = {};
    rightCarrier = {};
    analysisBuffer.clear();
    analysisWritePosition = 0;
    smoothedCarrierFrequency = 146.83f;
    envelopeMeter.store(0.0f);
    detectedPitchHz.store(0.0f);
    targetMidiNote.store(62);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(juce::jmax(32, samplesPerBlock));
    spec.numChannels = 1;

    for (auto& filter : modulatorFilters)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    }

    for (auto& filter : carrierFiltersLeft)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    }

    for (auto& filter : carrierFiltersRight)
    {
        filter.reset();
        filter.prepare(spec);
        filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
    }
}

void SimpleVocoderAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleVocoderAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (input != output)
        return false;

    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}
#endif

void SimpleVocoderAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    updateBandFrequencies();

    const auto tone = parameters.getRawParameterValue("tone")->load();
    const auto robot = parameters.getRawParameterValue("robot")->load();
    const auto mix = parameters.getRawParameterValue("mix")->load();
    const auto outputGain = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("outputGain")->load());
    const auto resonance = juce::jmap(robot, 0.8f, 2.2f);
    const auto attackCoeff = std::exp(-1.0f / static_cast<float>(juce::jmax(1.0, currentSampleRate * 0.0015)));
    const auto releaseCoeff = std::exp(-1.0f / static_cast<float>(juce::jmax(1.0, currentSampleRate * 0.03)));
    auto* analysisData = analysisBuffer.getWritePointer(0);
    juce::HeapBlock<float> contiguous(static_cast<size_t>(analysisWindowSize));

    for (size_t band = 0; band < numBands; ++band)
    {
        modulatorFilters[band].setCutoffFrequency(bandFrequencies[band]);
        modulatorFilters[band].setResonance(resonance);
        carrierFiltersLeft[band].setCutoffFrequency(bandFrequencies[band]);
        carrierFiltersLeft[band].setResonance(resonance);
        carrierFiltersRight[band].setCutoffFrequency(bandFrequencies[band]);
        carrierFiltersRight[band].setResonance(resonance);
    }

    float blockEnvelopePeak = 0.0f;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto inLeft = buffer.getSample(0, sample);
        const auto inRight = totalNumInputChannels > 1 ? buffer.getSample(1, sample) : inLeft;
        const auto modulator = std::tanh((inLeft + inRight) * (1.7f + robot * 1.6f));
        analysisData[analysisWritePosition] = modulator;
        analysisWritePosition = (analysisWritePosition + 1) % analysisWindowSize;

        if ((sample % 64) == 0)
        {
            for (int i = 0; i < analysisWindowSize; ++i)
                contiguous[i] = analysisData[(analysisWritePosition + i) % analysisWindowSize];

            const auto estimatedHz = estimatePitchHz(contiguous.getData(), analysisWindowSize, currentSampleRate);
            if (estimatedHz > 0.0f)
            {
                detectedPitchHz.store(estimatedHz);
                const auto detectedMidi = 69.0f + 12.0f * std::log2(estimatedHz / 440.0f);
                const auto snappedMidi = static_cast<float>(findNearestAllowedMidiNote(detectedMidi));
                targetMidiNote.store(juce::roundToInt(snappedMidi));
                const auto snappedHz = 440.0f * std::pow(2.0f, (snappedMidi - 69.0f) / 12.0f);
                const auto glide = juce::jmap(robot, 0.12f, 0.45f);
                smoothedCarrierFrequency += (snappedHz - smoothedCarrierFrequency) * glide;
            }
        }

        const auto leftCarrierSample = renderCarrierSample(leftCarrier, smoothedCarrierFrequency, tone);
        const auto rightCarrierSample = renderCarrierSample(rightCarrier, smoothedCarrierFrequency, tone);

        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        float envelopeSum = 0.0f;

        for (size_t band = 0; band < numBands; ++band)
        {
            const auto modBand = modulatorFilters[band].processSample(0, modulator);
            const auto envTarget = juce::jlimit(0.0f, 1.0f, std::abs(modBand) * 8.0f);
            const auto coeff = envTarget > envelopes[band] ? attackCoeff : releaseCoeff;
            envelopes[band] = (coeff * envelopes[band]) + ((1.0f - coeff) * envTarget);
            envelopeSum += envelopes[band];

            wetLeft += carrierFiltersLeft[band].processSample(0, leftCarrierSample) * envelopes[band];
            wetRight += carrierFiltersRight[band].processSample(0, rightCarrierSample) * envelopes[band];
        }

        const auto norm = envelopeSum > 0.001f ? (1.8f / envelopeSum) : 0.0f;
        wetLeft = std::tanh(wetLeft * norm * juce::jmap(robot, 2.8f, 4.8f));
        wetRight = std::tanh(wetRight * norm * juce::jmap(robot, 2.8f, 4.8f));

        const auto roboticBlend = juce::jmap(robot, 0.18f, 0.55f);
        wetLeft = (wetLeft * (1.0f - roboticBlend)) + (modulator * roboticBlend * 0.35f);
        wetRight = (wetRight * (1.0f - roboticBlend)) + (modulator * roboticBlend * 0.35f);

        blockEnvelopePeak = juce::jmax(blockEnvelopePeak, juce::jlimit(0.0f, 1.0f, envelopeSum / static_cast<float>(numBands)));

        buffer.setSample(0, sample, juce::jlimit(-1.0f, 1.0f, ((inLeft * (1.0f - mix)) + (wetLeft * mix)) * outputGain));
        if (totalNumOutputChannels > 1)
            buffer.setSample(1, sample, juce::jlimit(-1.0f, 1.0f, ((inRight * (1.0f - mix)) + (wetRight * mix)) * outputGain));
    }

    envelopeMeter.store(blockEnvelopePeak);
}

juce::AudioProcessorEditor* SimpleVocoderAudioProcessor::createEditor()
{
    return new SimpleVocoderAudioProcessorEditor(*this);
}

bool SimpleVocoderAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String SimpleVocoderAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleVocoderAudioProcessor::acceptsMidi() const
{
    return false;
}

bool SimpleVocoderAudioProcessor::producesMidi() const
{
    return false;
}

bool SimpleVocoderAudioProcessor::isMidiEffect() const
{
    return false;
}

double SimpleVocoderAudioProcessor::getTailLengthSeconds() const
{
    return 0.1;
}

int SimpleVocoderAudioProcessor::getNumPrograms()
{
    return 1;
}

int SimpleVocoderAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleVocoderAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String SimpleVocoderAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void SimpleVocoderAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void SimpleVocoderAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState(); auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void SimpleVocoderAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));

    updateBandFrequencies();
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleVocoderAudioProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;
    layout.push_back(std::make_unique<FloatParam>("tone", "Tone", juce::NormalisableRange<float>(0.0f, 1.0f), 0.55f));
    layout.push_back(std::make_unique<FloatParam>("robot", "Robot", juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    layout.push_back(std::make_unique<FloatParam>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.85f));
    layout.push_back(std::make_unique<FloatParam>("outputGain", "Output", juce::NormalisableRange<float>(-18.0f, 12.0f, 0.01f), 0.0f));
    return { layout.begin(), layout.end() };
}

float SimpleVocoderAudioProcessor::estimatePitchHz(const float* samples, int length, double sampleRate)
{
    constexpr double minFrequency = 70.0;
    constexpr double maxFrequency = 700.0;

    const auto minLag = juce::jmax(2, juce::roundToInt(sampleRate / maxFrequency));
    const auto maxLag = juce::jmin(length - 2, juce::roundToInt(sampleRate / minFrequency));
    if (maxLag <= minLag)
        return 0.0f;

    auto scoreForLag = [&](int lag)
    {
        double correlation = 0.0;
        double energyA = 0.0;
        double energyB = 0.0;

        for (int i = 0; i < length - lag; ++i)
        {
            const auto a = static_cast<double>(samples[i]);
            const auto b = static_cast<double>(samples[i + lag]);
            correlation += a * b;
            energyA += a * a;
            energyB += b * b;
        }

        const auto normaliser = std::sqrt(energyA * energyB);
        return normaliser > 0.0 ? correlation / normaliser : 0.0;
    };

    double bestScore = 0.0;
    int bestLag = 0;
    for (int lag = minLag; lag <= maxLag; lag += (lag < 128 ? 1 : 2))
    {
        const auto score = scoreForLag(lag);
        if (score > bestScore)
        {
            bestScore = score;
            bestLag = lag;
        }
    }

    if (bestLag == 0 || bestScore < 0.55)
        return 0.0f;

    return static_cast<float>(sampleRate / static_cast<double>(bestLag));
}

float SimpleVocoderAudioProcessor::wrapPhase(float phase) noexcept
{
    while (phase >= 1.0f)
        phase -= 1.0f;
    while (phase < 0.0f)
        phase += 1.0f;
    return phase;
}

int SimpleVocoderAudioProcessor::findNearestAllowedMidiNote(float midiNote)
{
    const auto rounded = juce::roundToInt(midiNote);
    int bestNote = 62;
    float bestDistance = std::numeric_limits<float>::max();

    for (int note = 36; note <= 96; ++note)
    {
        const auto pitchClass = ((note % 12) + 12) % 12;
        const auto allowed = std::find(dMajorPitchClasses.begin(), dMajorPitchClasses.end(), pitchClass) != dMajorPitchClasses.end();
        if (! allowed)
            continue;

        const auto distance = std::abs(static_cast<float>(note) - midiNote);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestNote = note;
        }
    }

    return bestNote;
}

void SimpleVocoderAudioProcessor::updateBandFrequencies()
{
    const auto robot = parameters.getRawParameterValue("robot")->load();
    const auto spreadScale = juce::jmap(robot, 0.9f, 1.2f);

    for (size_t i = 0; i < numBands; ++i)
        bandFrequencies[i] = baseFrequencies[i] * spreadScale;
}

float SimpleVocoderAudioProcessor::renderCarrierSample(CarrierState& state, float frequency, float tone) noexcept
{
    const auto sampleRate = static_cast<float>(currentSampleRate);
    state.phase = wrapPhase(state.phase + (frequency / sampleRate));
    state.subPhase = wrapPhase(state.subPhase + ((frequency * 0.5f) / sampleRate));

    const auto saw = (state.phase * 2.0f) - 1.0f;
    const auto pulse = state.phase < 0.48f ? 1.0f : -1.0f;
    const auto sub = std::sin(state.subPhase * juce::MathConstants<float>::twoPi);
    const auto bright = juce::jmap(tone, saw, pulse);
    return ((bright * 0.82f) + (sub * 0.18f)) * 0.35f;
}

juce::String SimpleVocoderAudioProcessor::pitchClassName(int pitchClass)
{
    static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[juce::jlimit(0, 11, pitchClass)];
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleVocoderAudioProcessor();
}
