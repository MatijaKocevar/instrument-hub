#pragma once

#include "StateStore.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace Api
{

//==============================================================================
/**
    Samples the engine meters on its own thread and serialises the newest frame.

    A frame is produced `meterHz` times a second (clamped to 1..120). The
    provider is copied under a mutex, then called on this sampler's thread, so
    reading the engine must stay lock-free (see `MeterProvider`). When no
    provider is installed, or the provider throws, the sampler publishes the
    synthetic frame derived from `StateStore` so `/meters` always has data.

    Socket transport lives in `WebSocketRoutes`; this class only owns sampling.
*/
class MeterSampler
{
public:
    /** One serialised frame plus the sequence clients poll. */
    struct LatestFrame
    {
        std::uint64_t version = 0;
        std::string json;
    };

    MeterSampler (StateStore& state, int meterHz);
    ~MeterSampler();

    MeterSampler (const MeterSampler&) = delete;
    MeterSampler& operator= (const MeterSampler&) = delete;

    /** Replaces the meter source. Safe before or after `start()`. */
    void setProvider (MeterProvider provider);

    /** Starts the sampling thread. Idempotent. */
    void start();

    /** Stops and joins the sampling thread. Idempotent. */
    void stop();

    /** The newest frame; `version` is 0 before the first sample. */
    LatestFrame getLatestFrame() const;

private:
    MeterFrame makeDefaultFrame() const;
    void run();

    StateStore& state;
    const int meterHz;

    std::atomic<bool> running { false };
    std::thread thread;

    mutable std::mutex providerMutex;
    MeterProvider provider;

    mutable std::mutex frameMutex;
    LatestFrame latest;
};

} // namespace Api
