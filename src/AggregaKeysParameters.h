#pragma once

#include <type_traits>

struct AggregaKeysParameterValues
{
    float gain = 0.7f;
    float velocityAmp = 0.0f;
    float attack = 0.02f;
    float decay = 0.15f;
    float sustain = 0.8f;
    float release = 0.35f;
    int osc1Wave = 0;
    int osc2Wave = 1;
    float oscMix = 0.35f;
    float osc2Detune = 0.0f;
    float osc2Fine = 0.0f;
    float filterCutoff = 4000.0f;
    float filterResonance = 0.3f;
    float filterEnvAmount = 2500.0f;
    float velocityFilter = 0.0f;
    float filterAttack = 0.01f;
    float filterDecay = 0.2f;
    float filterSustain = 0.1f;
    float filterRelease = 0.3f;
    float lfoRate = 5.0f;
    float lfoPitchDepth = 0.0f;
    float lfoFilterDepth = 0.0f;
    float glide = 0.0f;
    int polyphony = 8;
    bool monoMode = false;
    float drive = 0.15f;
    float reverbMix = 0.18f;
    float reverbSize = 0.45f;
    float reverbDamping = 0.3f;
};

static_assert(std::is_trivially_copyable_v<AggregaKeysParameterValues>,
              "AggregaKeysParameterValues must stay trivially copyable.");
