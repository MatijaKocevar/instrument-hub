#pragma once

#include "TapTempo.h"

#include <atomic>
#include <cstdint>

namespace AudioEngine {

class Transport final {
public:
    enum class SyncSource { internal, external };

    struct Snapshot {
        bool playing = false;
        double bpm = 120.0;
        double ppq = 0.0;
        std::int64_t positionSamples = 0;

        double sampleRate = 0.0;
        int maximumBlockSize = 0;

        SyncSource syncSource = SyncSource::internal;
        bool externalClockLocked = false;

        double tapTempoBpm = 0.0;
    };

    struct ExternalSync {
        bool locked = false;

        bool playing = false;
        double bpm = 120.0;
        double ppq = 0.0;
        std::int64_t positionSamples = 0;
    };

    static constexpr double minBpm = 20.0;
    static constexpr double maxBpm = 300.0;
    static constexpr double defaultBpm = 120.0;

    static constexpr int ppqTicksPerQuarterNote = 960;

    static constexpr int tapHistorySize = TapTempo::defaultHistorySize;

    Transport() = default;
    ~Transport() = default;

    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

    void prepare(double sampleRate, int maximumBlockSize) noexcept;

    void reset() noexcept;

    void rewind() noexcept;

    void start() noexcept;
    void stop() noexcept;
    void setPlaying(bool shouldPlay) noexcept;
    bool isPlaying() const noexcept;

    void setBpm(double newBpm) noexcept;
    double getBpm() const noexcept;

    void setPpq(double newPpq) noexcept;

    void setPositionSamples(std::int64_t newPositionSamples) noexcept;

    double getPpq() const noexcept;
    std::int64_t getPositionSamples() const noexcept;

    double getSampleRate() const noexcept;
    int getMaximumBlockSize() const noexcept;

    void process(int numSamples) noexcept;

    double getSamplesPerQuarterNote() const noexcept;

    double getPpqAtSampleOffset(int offsetSamples) const noexcept;

    double tapAt(double timestampSeconds) noexcept;

    double tap() noexcept;

    void resetTapTempo() noexcept;

    double getTapTempoBpm() const noexcept;

    void setSyncSource(SyncSource newSource) noexcept;
    SyncSource getSyncSource() const noexcept;

    bool applyExternalSync(const ExternalSync& sync) noexcept;

    bool isExternalClockLocked() const noexcept;

    Snapshot snapshot() const noexcept;

private:
    static double clampBpm(double bpm) noexcept;

    std::atomic<double> sampleRate{0.0};
    std::atomic<int> maximumBlockSize{0};

    std::atomic<bool> playing{false};
    std::atomic<double> bpm{defaultBpm};
    std::atomic<double> ppq{0.0};
    std::atomic<std::int64_t> positionSamples{0};

    std::atomic<SyncSource> syncSource{SyncSource::internal};
    std::atomic<bool> externalClockLocked{false};

    TapTempo tapTempo;
};

}  // namespace AudioEngine
