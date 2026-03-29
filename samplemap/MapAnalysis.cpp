#include "MapAnalysis.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace aggregamap
{
std::vector<Region> stabiliseDetectedRegions(std::vector<Region> regions)
{
    if (! regions.empty())
    {
        constexpr int mappingLow = 48;
        constexpr int mappingHigh = 72;

        auto foldIntoMappingRange = [mappingLow, mappingHigh](int midi)
        {
            while (midi > mappingHigh)
                midi -= 12;

            while (midi < mappingLow)
                midi += 12;

            return std::clamp(midi, mappingLow, mappingHigh);
        };

        regions.front().rootMidiNote = foldIntoMappingRange(regions.front().rootMidiNote);
        auto previousMidi = regions.front().rootMidiNote;

        for (size_t i = 1; i < regions.size(); ++i)
        {
            auto bestMidi = foldIntoMappingRange(regions[i].rootMidiNote);
            auto bestCost = std::numeric_limits<double>::max();

            for (int octaveShift = -2; octaveShift <= 2; ++octaveShift)
            {
                const auto candidateMidi = foldIntoMappingRange(regions[i].rootMidiNote + (12 * octaveShift));

                double cost = static_cast<double>(std::abs(candidateMidi - previousMidi));
                if (candidateMidi < previousMidi - 2)
                    cost += 24.0;

                if (cost < bestCost)
                {
                    bestCost = cost;
                    bestMidi = candidateMidi;
                }
            }

            regions[i].rootMidiNote = bestMidi;
            previousMidi = bestMidi;
        }
    }

    std::sort(regions.begin(), regions.end(), [](const Region& a, const Region& b)
    {
        if (a.rootMidiNote != b.rootMidiNote)
            return a.rootMidiNote < b.rootMidiNote;

        if (a.rms != b.rms)
            return a.rms > b.rms;

        return a.startSample < b.startSample;
    });

    return regions;
}

std::vector<Region> inferRepresentativeRegions(const std::vector<Region>& regions)
{
    struct RootStats
    {
        int count = 0;
        float rmsSum = 0.0f;
        Region bestRegion {};
        bool hasBest = false;
    };

    std::array<RootStats, 128> stats {};
    int peakCount = 0;

    for (const auto& region : regions)
    {
        auto& stat = stats[static_cast<size_t>(std::clamp(region.rootMidiNote, 0, 127))];
        ++stat.count;
        stat.rmsSum += region.rms;

        if (! stat.hasBest || region.rms > stat.bestRegion.rms || (region.rms == stat.bestRegion.rms && region.startSample < stat.bestRegion.startSample))
        {
            stat.bestRegion = region;
            stat.hasBest = true;
        }

        peakCount = std::max(peakCount, stat.count);
    }

    if (peakCount == 0)
        return {};

    const auto minCountToKeep = std::max(3, peakCount / 12);
    std::vector<Region> inferred;
    inferred.reserve(regions.size());

    for (int midi = 0; midi < static_cast<int>(stats.size()); ++midi)
    {
        const auto& stat = stats[static_cast<size_t>(midi)];
        if (! stat.hasBest || stat.count < minCountToKeep)
            continue;

        auto representative = stat.bestRegion;
        representative.rootMidiNote = midi;
        representative.rms = stat.rmsSum / static_cast<float>(stat.count);
        inferred.push_back(representative);
    }

    if (inferred.empty())
    {
        for (int midi = 0; midi < static_cast<int>(stats.size()); ++midi)
        {
            const auto& stat = stats[static_cast<size_t>(midi)];
            if (! stat.hasBest)
                continue;

            auto representative = stat.bestRegion;
            representative.rootMidiNote = midi;
            representative.rms = stat.rmsSum / static_cast<float>(stat.count);
            inferred.push_back(representative);
        }
    }

    std::sort(inferred.begin(), inferred.end(), [](const Region& a, const Region& b)
    {
        return a.rootMidiNote < b.rootMidiNote;
    });

    return inferred;
}

void rebuildAssignmentsFromRegions(const std::vector<Region>& regions, std::array<NoteAssignment, 128>& assignments)
{
    for (auto& assignment : assignments)
        assignment = {};

    const auto inferredRegions = inferRepresentativeRegions(regions);
    if (inferredRegions.empty())
        return;

    std::vector<float> sortedRms;
    sortedRms.reserve(inferredRegions.size());
    for (const auto& region : inferredRegions)
        sortedRms.push_back(region.rms);

    std::sort(sortedRms.begin(), sortedRms.end());
    const auto referenceRms = sortedRms[sortedRms.size() / 2];

    for (int midi = 0; midi < static_cast<int>(assignments.size()); ++midi)
    {
        const Region* best = nullptr;
        int bestDistance = std::numeric_limits<int>::max();

        for (const auto& region : inferredRegions)
        {
            const auto distance = std::abs(region.rootMidiNote - midi);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = &region;
                continue;
            }

            if (distance == bestDistance && best != nullptr)
            {
                if (region.rms > best->rms || (region.rms == best->rms && region.startSample < best->startSample))
                    best = &region;
            }
        }

        if (best == nullptr)
            continue;

        const auto safeRegionRms = std::max(0.0001f, best->rms);
        const auto levelCompensation = std::clamp(referenceRms / safeRegionRms, 0.35f, 3.0f);
        assignments[static_cast<size_t>(midi)] = { best->rootMidiNote, best->startSample, best->endSample, levelCompensation, true };
    }
}
} // namespace aggregamap
