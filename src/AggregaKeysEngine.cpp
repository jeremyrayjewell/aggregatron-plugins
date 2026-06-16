#include "AggregaKeysEngine.h"

namespace
{
double noteNumberToFrequency(double noteNumber)
{
    return 440.0 * std::pow(2.0, (noteNumber - 69.0) / 12.0);
}

float wrapPhase01(float phase) noexcept
{
    phase -= std::floor(phase);
    return phase;
}

float wrapPhaseRadians(float phase) noexcept
{
    while (phase > juce::MathConstants<float>::pi)
        phase -= juce::MathConstants<float>::twoPi;

    while (phase < -juce::MathConstants<float>::pi)
        phase += juce::MathConstants<float>::twoPi;

    return phase;
}

float polyBlep(float phase, float phaseIncrement) noexcept
{
    if (phaseIncrement <= 0.0f)
        return 0.0f;

    if (phase < phaseIncrement)
    {
        const auto t = phase / phaseIncrement;
        return t + t - t * t - 1.0f;
    }

    if (phase > 1.0f - phaseIncrement)
    {
        const auto t = (phase - 1.0f) / phaseIncrement;
        return t * t + t + t + 1.0f;
    }

    return 0.0f;
}

float sawWave(float phase, float phaseIncrement) noexcept
{
    auto sample = 2.0f * phase - 1.0f;
    sample -= polyBlep(phase, phaseIncrement);
    return sample;
}

float squareWave(float phase, float phaseIncrement) noexcept
{
    auto sample = phase < 0.5f ? 1.0f : -1.0f;
    sample += polyBlep(phase, phaseIncrement);
    sample -= polyBlep(wrapPhase01(phase + 0.5f), phaseIncrement);
    return sample;
}

float triangleWave(float phase) noexcept
{
    return 1.0f - 4.0f * std::abs(phase - 0.5f);
}

float sineWave(float phase) noexcept
{
    return std::sin(phase * juce::MathConstants<float>::twoPi);
}
} // namespace

bool SynthSound::appliesToNote(int)
{
    return true;
}

bool SynthSound::appliesToChannel(int)
{
    return true;
}

SynthVoice::SynthVoice(juce::AudioProcessorValueTreeState& state)
    : parameters(state)
{
    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
}

bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*>(sound) != nullptr;
}

void SynthVoice::setCurrentPlaybackSampleRate(double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate(newRate);

    if (newRate <= 0.0)
        return;

    ampEnvelope.setSampleRate(newRate);
    filterEnvelope.setSampleRate(newRate);
    ampEnvelope.reset();
    filterEnvelope.reset();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = newRate;
    spec.maximumBlockSize = 2048;
    spec.numChannels = 1;
    filter.prepare(spec);
    filter.reset();
    lastPreparedSampleRate = 0.0;
}

void SynthVoice::setLegatoMode(bool shouldUseLegato) noexcept
{
    pendingLegato = shouldUseLegato;
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    prepareSmoothers();
    const auto shouldGlide = pendingLegato && isVoiceActive() && ampEnvelope.isActive();
    pendingLegato = false;

    currentMidiNote = midiNoteNumber;
    targetMidiNote = static_cast<float>(midiNoteNumber);
    noteVelocity = velocity;
    updatePitch();
    updateGlideTarget(shouldGlide);

    if (shouldGlide)
        return;

    osc1Phase = 0.0f;
    osc2Phase = 0.0f;
    lfoPhase = 0.0f;
    filter.reset();
    ampEnvelope.noteOn();
    filterEnvelope.noteOn();
}

void SynthVoice::stopNote(float, bool allowTailOff)
{
    ampEnvelope.noteOff();
    filterEnvelope.noteOff();

    if (! allowTailOff || ! ampEnvelope.isActive())
        clearCurrentNote();
}

void SynthVoice::pitchWheelMoved(int newPitchWheelValue)
{
    pitchWheelValue = newPitchWheelValue;
    updatePitch();
}

void SynthVoice::controllerMoved(int controllerNumber, int newControllerValue)
{
    if (controllerNumber == 1)
        modWheelValue = static_cast<float>(newControllerValue) / 127.0f;
}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    updateParameters();
    prepareSmoothers();

    if (! isVoiceActive())
        return;

    auto* left = outputBuffer.getWritePointer(0);
    auto* right = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer(1) : nullptr;

    while (--numSamples >= 0)
    {
        const auto lfoValue = std::sin(lfoPhase);
        const auto vibratoSemitones = lfoValue * (lfoDepthSemitones + (modWheelValue * 0.35f));
        const auto noteWithMod = static_cast<double>(smoothedMidiNote.getNextValue()) + static_cast<double>(pitchBendSemitones + vibratoSemitones);
        const auto osc1Frequency = noteNumberToFrequency(noteWithMod);
        const auto osc2Frequency = noteNumberToFrequency(noteWithMod + static_cast<double>(osc2DetuneSemitones) + static_cast<double>(osc2FineCents / 100.0f));

        osc1Delta = static_cast<float>(osc1Frequency / getSampleRate());
        osc2Delta = static_cast<float>(osc2Frequency / getSampleRate());

        const auto osc1Sample = renderOscillatorSample(osc1Waveform, osc1Phase, osc1Delta);
        const auto osc2Sample = renderOscillatorSample(osc2Waveform, osc2Phase, osc2Delta);
        auto sample = juce::jmap(oscMix, osc1Sample, osc2Sample);

        osc1Phase = wrapPhase(osc1Phase + osc1Delta);
        osc2Phase = wrapPhase(osc2Phase + osc2Delta);
        lfoPhase = wrapPhaseRadians(lfoPhase + static_cast<float>(juce::MathConstants<double>::twoPi * lfoRateHz / getSampleRate()));

        const auto filterEnvValue = filterEnvelope.getNextSample();
        const auto velocityCutoffBoost = noteVelocity * velocityFilterAmountHz;
        const auto cutoffMod = (filterEnvValue * filterEnvAmountHz) + velocityCutoffBoost + (lfoValue * lfoFilterAmountHz);
        const auto cutoff = juce::jlimit(20.0f, 18000.0f, smoothedFilterCutoffHz.getNextValue() + cutoffMod);
        filter.setCutoffFrequency(cutoff);
        filter.setResonance(filterResonance);

        sample = filter.processSample(0, sample);
        sample = std::tanh(sample * smoothedDriveAmount.getNextValue());
        const auto velocityGain = (1.0f - velocityAmpAmount) + (noteVelocity * velocityAmpAmount);
        sample *= ampEnvelope.getNextSample() * noteVelocity * velocityGain;

        left[startSample] += sample;
        if (right != nullptr)
            right[startSample] += sample;

        ++startSample;
    }

    if (! ampEnvelope.isActive())
        clearCurrentNote();
}

float SynthVoice::wrapPhase(float phase)
{
    return wrapPhase01(phase);
}

void SynthVoice::updatePitch()
{
    const auto normalizedWheel = (static_cast<float>(pitchWheelValue) - 8192.0f) / 8192.0f;
    pitchBendSemitones = normalizedWheel * pitchBendRangeSemitones;
}

void SynthVoice::prepareSmoothers()
{
    const auto sampleRate = getSampleRate();

    if (sampleRate <= 0.0 || sampleRate == lastPreparedSampleRate)
        return;

    smoothedFilterCutoffHz.reset(sampleRate, 0.02);
    smoothedDriveAmount.reset(sampleRate, 0.02);
    smoothedMidiNote.reset(sampleRate, juce::jmax(0.0, static_cast<double>(glideTimeSeconds)));
    smoothedFilterCutoffHz.setCurrentAndTargetValue(filterCutoffHz);
    smoothedDriveAmount.setCurrentAndTargetValue(driveAmount);
    smoothedMidiNote.setCurrentAndTargetValue(targetMidiNote);
    lastPreparedSampleRate = sampleRate;
    lastPreparedGlideTime = glideTimeSeconds;
}

void SynthVoice::updateParameters()
{
    juce::ADSR::Parameters ampParams;
    ampParams.attack = parameters.getRawParameterValue("attack")->load();
    ampParams.decay = parameters.getRawParameterValue("decay")->load();
    ampParams.sustain = parameters.getRawParameterValue("sustain")->load();
    ampParams.release = parameters.getRawParameterValue("release")->load();
    ampEnvelope.setParameters(ampParams);

    juce::ADSR::Parameters filterParams;
    filterParams.attack = parameters.getRawParameterValue("filterAttack")->load();
    filterParams.decay = parameters.getRawParameterValue("filterDecay")->load();
    filterParams.sustain = parameters.getRawParameterValue("filterSustain")->load();
    filterParams.release = parameters.getRawParameterValue("filterRelease")->load();
    filterEnvelope.setParameters(filterParams);

    osc1Waveform = static_cast<Waveform>(juce::jlimit(0, 3, juce::roundToInt(parameters.getRawParameterValue("osc1Wave")->load())));
    osc2Waveform = static_cast<Waveform>(juce::jlimit(0, 3, juce::roundToInt(parameters.getRawParameterValue("osc2Wave")->load())));
    oscMix = parameters.getRawParameterValue("oscMix")->load();
    osc2DetuneSemitones = parameters.getRawParameterValue("osc2Detune")->load();
    osc2FineCents = parameters.getRawParameterValue("osc2Fine")->load();
    filterCutoffHz = parameters.getRawParameterValue("filterCutoff")->load();
    filterResonance = parameters.getRawParameterValue("filterResonance")->load();
    filterEnvAmountHz = parameters.getRawParameterValue("filterEnvAmount")->load();
    velocityFilterAmountHz = parameters.getRawParameterValue("velocityFilter")->load();
    lfoRateHz = parameters.getRawParameterValue("lfoRate")->load();
    lfoDepthSemitones = parameters.getRawParameterValue("lfoPitchDepth")->load();
    lfoFilterAmountHz = parameters.getRawParameterValue("lfoFilterDepth")->load();
    glideTimeSeconds = parameters.getRawParameterValue("glide")->load();
    velocityAmpAmount = parameters.getRawParameterValue("velocityAmp")->load();
    driveAmount = juce::jmap(parameters.getRawParameterValue("drive")->load(), 1.0f, 8.0f);

    if (lastPreparedSampleRate > 0.0)
    {
        if (! juce::approximatelyEqual(glideTimeSeconds, lastPreparedGlideTime))
        {
            const auto currentNote = smoothedMidiNote.getCurrentValue();
            smoothedMidiNote.reset(lastPreparedSampleRate, juce::jmax(0.0, static_cast<double>(glideTimeSeconds)));
            smoothedMidiNote.setCurrentAndTargetValue(currentNote);
            smoothedMidiNote.setTargetValue(targetMidiNote);
            lastPreparedGlideTime = glideTimeSeconds;
        }

        smoothedFilterCutoffHz.setTargetValue(filterCutoffHz);
        smoothedDriveAmount.setTargetValue(driveAmount);
        smoothedMidiNote.setTargetValue(targetMidiNote);
    }
}

void SynthVoice::updateGlideTarget(bool shouldGlide)
{
    if (lastPreparedSampleRate <= 0.0)
        return;

    if (! shouldGlide || glideTimeSeconds <= 0.0f)
    {
        smoothedMidiNote.setCurrentAndTargetValue(targetMidiNote);
        return;
    }

    smoothedMidiNote.setTargetValue(targetMidiNote);
}

float SynthVoice::renderOscillatorSample(Waveform waveform, float phase, float phaseIncrement) noexcept
{
    switch (waveform)
    {
        case Waveform::square:
            return squareWave(phase, phaseIncrement);
        case Waveform::triangle:
            return triangleWave(phase);
        case Waveform::sine:
            return sineWave(phase);
        case Waveform::saw:
        default:
            return sawWave(phase, phaseIncrement);
    }
}

void AggregaKeysSynth::setMaximumPlayableVoices(int newMaxVoices) noexcept
{
    maximumPlayableVoices = juce::jlimit(1, 16, newMaxVoices);
}

void AggregaKeysSynth::setMonoMode(bool shouldBeMono)
{
    if (monoMode == shouldBeMono)
        return;

    monoMode = shouldBeMono;
    heldNotes.clear();

    if (monoMode)
        stopExtraVoices();
}

void AggregaKeysSynth::noteOn(int midiChannel, int midiNoteNumber, float velocity)
{
    if (! monoMode)
    {
        juce::Synthesiser::noteOn(midiChannel, midiNoteNumber, velocity);
        return;
    }

    const auto hadHeldNotes = getLastHeldNote() != nullptr;
    updateHeldNote(midiChannel, midiNoteNumber, velocity);

    if (auto* sound = findSoundFor(midiChannel, midiNoteNumber))
    {
        if (auto* voice = getPrimaryVoice())
        {
            voice->setLegatoMode(hadHeldNotes);
            startVoice(voice, sound, midiChannel, midiNoteNumber, velocity);
            stopExtraVoices();
        }
    }
}

void AggregaKeysSynth::noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff)
{
    if (! monoMode)
        return juce::Synthesiser::noteOff(midiChannel, midiNoteNumber, velocity, allowTailOff);

    removeHeldNote(midiChannel, midiNoteNumber);

    auto* voice = getPrimaryVoice();
    if (voice == nullptr || ! voice->isVoiceActive())
        return;

    if (const auto* held = getLastHeldNote())
    {
        if (held->midiNoteNumber != voice->getCurrentlyPlayingNote())
        {
            if (auto* sound = findSoundFor(held->midiChannel, held->midiNoteNumber))
            {
                voice->setLegatoMode(true);
                startVoice(voice, sound, held->midiChannel, held->midiNoteNumber, held->velocity);
            }
        }

        return;
    }

    stopVoice(voice, velocity, allowTailOff);
}

juce::SynthesiserVoice* AggregaKeysSynth::findFreeVoice(juce::SynthesiserSound* soundToPlay,
                                                        int midiChannel,
                                                        int midiNoteNumber,
                                                        bool stealIfNoneAvailable) const
{
    juce::ignoreUnused(midiChannel, midiNoteNumber);

    const auto eligibleVoices = juce::jlimit(1, voices.size(), monoMode ? 1 : maximumPlayableVoices);

    for (int i = 0; i < eligibleVoices; ++i)
    {
        auto* voice = voices.getUnchecked(i);

        if ((! voice->isVoiceActive()) && voice->canPlaySound(soundToPlay))
            return voice;
    }

    for (int i = 0; i < eligibleVoices; ++i)
    {
        auto* voice = voices.getUnchecked(i);

        if (voice->isVoiceActive()
            && ! voice->isKeyDown()
            && ! voice->isSustainPedalDown()
            && ! voice->isSostenutoPedalDown()
            && voice->canPlaySound(soundToPlay))
            return voice;
    }

    if (! stealIfNoneAvailable)
        return nullptr;

    SynthesiserVoice* oldestVoice = nullptr;

    for (int i = 0; i < eligibleVoices; ++i)
    {
        auto* voice = voices.getUnchecked(i);

        if (! voice->canPlaySound(soundToPlay))
            continue;

        if (oldestVoice == nullptr || voice->wasStartedBefore(*oldestVoice))
            oldestVoice = voice;
    }

    return oldestVoice;
}

void AggregaKeysSynth::stopExtraVoices()
{
    for (int i = 1; i < voices.size(); ++i)
    {
        if (auto* voice = voices.getUnchecked(i); voice->isVoiceActive())
            stopVoice(voice, 0.0f, false);
    }
}

void AggregaKeysSynth::updateHeldNote(int midiChannel, int midiNoteNumber, float velocity)
{
    removeHeldNote(midiChannel, midiNoteNumber);
    heldNotes.push_back({ midiChannel, midiNoteNumber, velocity });
}

void AggregaKeysSynth::removeHeldNote(int midiChannel, int midiNoteNumber)
{
    heldNotes.erase(std::remove_if(heldNotes.begin(), heldNotes.end(),
                                   [=](const HeldNote& heldNote)
                                   {
                                       return heldNote.midiChannel == midiChannel
                                           && heldNote.midiNoteNumber == midiNoteNumber;
                                   }),
                    heldNotes.end());
}

const AggregaKeysSynth::HeldNote* AggregaKeysSynth::getLastHeldNote() const noexcept
{
    if (heldNotes.empty())
        return nullptr;

    return &heldNotes.back();
}

juce::SynthesiserSound* AggregaKeysSynth::findSoundFor(int midiChannel, int midiNoteNumber) const
{
    for (auto* sound : sounds)
    {
        if (sound->appliesToNote(midiNoteNumber) && sound->appliesToChannel(midiChannel))
            return sound;
    }

    return nullptr;
}

SynthVoice* AggregaKeysSynth::getPrimaryVoice() const
{
    if (voices.isEmpty())
        return nullptr;

    return dynamic_cast<SynthVoice*>(voices.getUnchecked(0));
}
