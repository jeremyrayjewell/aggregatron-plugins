#pragma once

#include <array>
#include <vector>

namespace aggregamap
{
struct Region
{
    int rootMidiNote = 60;
    int startSample = 0;
    int endSample = 0;
    float rms = 0.0f;
};

struct NoteAssignment
{
    int rootMidiNote = 60;
    int startSample = 0;
    int endSample = 0;
    float levelCompensation = 1.0f;
    bool valid = false;
};

std::vector<Region> stabiliseDetectedRegions(std::vector<Region> regions);
std::vector<Region> inferRepresentativeRegions(const std::vector<Region>& regions);
void rebuildAssignmentsFromRegions(const std::vector<Region>& regions, std::array<NoteAssignment, 128>& assignments);
} // namespace aggregamap
