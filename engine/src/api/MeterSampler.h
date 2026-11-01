#pragma once

#include "StateStore.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace Api
{

class MeterSampler
{
public:
    struct LatestFrame
    {
        std::uint64_t version = 0;
        std::string json;
    };

    MeterSampler (StateStore& state, int meterHz);
    ~MeterSampler();

    MeterSampler (const MeterSampler&) = delete;
    MeterSampler& operator= (const MeterSampler&) = delete;

    void setProvider (MeterProvider provider);

    void start();

    void stop();

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

}
