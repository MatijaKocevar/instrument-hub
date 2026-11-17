#pragma once

#include "dsp/drive/DriveVoicing.h"

#include <array>

namespace ihub::dsp {

struct Biquad {
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;

    float z1 = 0.0f;
    float z2 = 0.0f;

    void reset() noexcept {
        z1 = 0.0f;
        z2 = 0.0f;
    }

    void setCoefficients(const Biquad& other) noexcept {
        b0 = other.b0;
        b1 = other.b1;
        b2 = other.b2;
        a1 = other.a1;
        a2 = other.a2;
    }

    float process(float input) noexcept {
        const float output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        return output;
    }
};

class DriveFilters {
public:
    void prepare(double sampleRate, int numChannels);

    void reset() noexcept;

    void setVoicing(const DriveVoicing& voicing) noexcept;

    void setTone(float tone) noexcept;

    float processPre(int channel, float sample) noexcept;

    float processDc(int channel, float sample) noexcept;

    float processTone(int channel, float sample) noexcept;

    float processCab(int channel, float sample) noexcept;

private:
    struct Channel {
        Biquad highPass;
        Biquad mid;
        Biquad presence;

        Biquad dcBlock;
        Biquad tone;

        Biquad cabHighPass;
        Biquad cabLowPass;
        Biquad cabLowMid;
        Biquad cabPresence;
    };

    static Biquad makeHighPass(double sampleRate, float frequencyHz, float q) noexcept;
    static Biquad makeLowPass(double sampleRate, float frequencyHz, float q) noexcept;
    static Biquad makePeak(double sampleRate, float frequencyHz, float gainDb, float q) noexcept;

    Channel& channelFor(int channel) noexcept;

    std::array<Channel, 2> channels;
    DriveVoicing voicing;
    int preparedChannels = 1;
    double sampleRate = 48000.0;
    float toneValue = -1.0f;
};

}  // namespace ihub::dsp
