#include "DistrhoPlugin.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

START_NAMESPACE_DISTRHO

namespace {
constexpr uint32_t kVoiceCount = 16;
constexpr float kDefaultGain = 0.7f;
constexpr float kVoiceScale = 0.12f;
constexpr float kTwoPi = 6.28318530717958647692f;

struct Voice {
    bool active = false;
    uint8_t note = 0;
    uint8_t channel = 0;
    float velocity = 0.0f;
    float phase = 0.0f;
    float phaseIncrement = 0.0f;
    uint64_t age = 0;
};

static float noteToFrequency(const uint8_t note) noexcept
{
    return 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
}
} // namespace

class AggregaKeysVST2ProbePlugin : public Plugin
{
public:
    AggregaKeysVST2ProbePlugin()
        : Plugin(kParameterCount, 0, 0),
          fSampleRate(static_cast<float>(getSampleRate())),
          fGain(kDefaultGain),
          fVoiceCounter(0)
    {
        sampleRateChanged(getSampleRate());
        resetVoices();
    }

protected:
    const char* getLabel() const override
    {
        return "AggregaKeysVST2Probe";
    }

    const char* getDescription() const override
    {
        return "Temporary DPF VST2 validation synth for Windows x64 toolchain testing.";
    }

    const char* getMaker() const override
    {
        return "Aggregatron";
    }

    const char* getHomePage() const override
    {
        return "";
    }

    const char* getLicense() const override
    {
        return "See repository and DPF notices";
    }

    uint32_t getVersion() const override
    {
        return d_version(0, 0, 1);
    }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if (index != kParameterGain)
            return;

        parameter.hints = kParameterIsAutomatable;
        parameter.name = "Gain";
        parameter.symbol = "gain";
        parameter.ranges.min = 0.0f;
        parameter.ranges.max = 1.0f;
        parameter.ranges.def = kDefaultGain;
    }

    float getParameterValue(uint32_t index) const override
    {
        return index == kParameterGain ? fGain : 0.0f;
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index == kParameterGain)
            fGain = value;
    }

    void activate() override
    {
        resetVoices();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fSampleRate = static_cast<float>(newSampleRate > 1.0 ? newSampleRate : 44100.0);

        for (uint32_t i = 0; i < kVoiceCount; ++i)
        {
            if (fVoices[i].active)
                fVoices[i].phaseIncrement = noteToFrequency(fVoices[i].note) / fSampleRate;
        }
    }

    void run(const float**, float** outputs, uint32_t frames,
             const MidiEvent* midiEvents, uint32_t midiEventCount) override
    {
        float* const outLeft = outputs[0];
        float* const outRight = outputs[1];

        std::memset(outLeft, 0, sizeof(float) * frames);
        std::memset(outRight, 0, sizeof(float) * frames);

        uint32_t frameStart = 0;

        for (uint32_t eventIndex = 0; eventIndex < midiEventCount; ++eventIndex)
        {
            const MidiEvent& event = midiEvents[eventIndex];
            const uint32_t eventFrame = event.frame < frames ? event.frame : frames;

            if (eventFrame > frameStart)
                renderRange(outLeft, outRight, frameStart, eventFrame);

            handleMidiEvent(event);
            frameStart = eventFrame;
        }

        if (frameStart < frames)
            renderRange(outLeft, outRight, frameStart, frames);
    }

private:
    void resetVoices() noexcept
    {
        for (uint32_t i = 0; i < kVoiceCount; ++i)
            fVoices[i] = Voice{};

        fVoiceCounter = 0;
    }

    void renderRange(float* outLeft, float* outRight, uint32_t startFrame, uint32_t endFrame) noexcept
    {
        for (uint32_t frame = startFrame; frame < endFrame; ++frame)
        {
            float sample = 0.0f;

            for (uint32_t voiceIndex = 0; voiceIndex < kVoiceCount; ++voiceIndex)
            {
                Voice& voice = fVoices[voiceIndex];
                if (!voice.active)
                    continue;

                sample += std::sin(voice.phase * kTwoPi) * (voice.velocity * fGain * kVoiceScale);
                voice.phase += voice.phaseIncrement;

                if (voice.phase >= 1.0f)
                    voice.phase -= std::floor(voice.phase);
            }

            if (std::fabs(sample) < 1.0e-20f)
                sample = 0.0f;

            outLeft[frame] = sample;
            outRight[frame] = sample;
        }
    }

    void handleMidiEvent(const MidiEvent& event) noexcept
    {
        const uint8_t* const data = event.size > MidiEvent::kDataSize && event.dataExt != nullptr
            ? event.dataExt
            : event.data;

        if (event.size < 1 || data == nullptr)
            return;

        const uint8_t status = data[0];
        const uint8_t message = status & 0xF0u;
        const uint8_t channel = status & 0x0Fu;

        switch (message)
        {
        case 0x80u:
            if (event.size >= 2)
                noteOff(channel, data[1]);
            break;
        case 0x90u:
            if (event.size >= 3)
            {
                if (data[2] == 0)
                    noteOff(channel, data[1]);
                else
                    noteOn(channel, data[1], data[2]);
            }
            break;
        case 0xB0u:
            if (event.size >= 3)
                controller(channel, data[1], data[2]);
            break;
        default:
            break;
        }
    }

    void noteOn(uint8_t channel, uint8_t note, uint8_t velocity) noexcept
    {
        Voice* voice = findVoice(channel, note);

        if (voice == nullptr)
            voice = findFreeVoice();

        voice->active = true;
        voice->note = note;
        voice->channel = channel;
        voice->velocity = static_cast<float>(velocity) / 127.0f;
        voice->phase = 0.0f;
        voice->phaseIncrement = noteToFrequency(note) / fSampleRate;
        voice->age = ++fVoiceCounter;
    }

    void noteOff(uint8_t channel, uint8_t note) noexcept
    {
        for (uint32_t i = 0; i < kVoiceCount; ++i)
        {
            Voice& voice = fVoices[i];
            if (voice.active && voice.channel == channel && voice.note == note)
                voice.active = false;
        }
    }

    void controller(uint8_t, uint8_t controllerNumber, uint8_t) noexcept
    {
        if (controllerNumber == 120u || controllerNumber == 123u)
        {
            for (uint32_t i = 0; i < kVoiceCount; ++i)
                fVoices[i].active = false;
        }
    }

    Voice* findVoice(uint8_t channel, uint8_t note) noexcept
    {
        for (uint32_t i = 0; i < kVoiceCount; ++i)
        {
            Voice& voice = fVoices[i];
            if (voice.active && voice.channel == channel && voice.note == note)
                return &voice;
        }

        return nullptr;
    }

    Voice* findFreeVoice() noexcept
    {
        for (uint32_t i = 0; i < kVoiceCount; ++i)
        {
            if (!fVoices[i].active)
                return &fVoices[i];
        }

        Voice* oldestVoice = &fVoices[0];

        for (uint32_t i = 1; i < kVoiceCount; ++i)
        {
            if (fVoices[i].age < oldestVoice->age)
                oldestVoice = &fVoices[i];
        }

        return oldestVoice;
    }

    float fSampleRate;
    float fGain;
    uint64_t fVoiceCounter;
    Voice fVoices[kVoiceCount];

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AggregaKeysVST2ProbePlugin)
};

Plugin* createPlugin()
{
    return new AggregaKeysVST2ProbePlugin();
}

END_NAMESPACE_DISTRHO
