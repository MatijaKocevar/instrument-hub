#include "MeterSampler.h"

#include "Routes.h"

#include <chrono>
#include <utility>

namespace Api {

namespace {
constexpr int minimumMeterHz = 1;
constexpr int maximumMeterHz = 120;

int clampMeterHz(int hz) noexcept { return juce::jlimit(minimumMeterHz, maximumMeterHz, hz); }
}  // namespace

MeterSampler::MeterSampler(StateStore& stateToUse, int meterHzToUse)
    : state(stateToUse), meterHz(clampMeterHz(meterHzToUse)) {}

MeterSampler::~MeterSampler() { stop(); }

void MeterSampler::setProvider(MeterProvider newProvider) {
    const std::lock_guard<std::mutex> lock(providerMutex);
    provider = std::move(newProvider);
}

void MeterSampler::start() {
    if (running.exchange(true, std::memory_order_acq_rel)) return;

    thread = std::thread([this] { run(); });
}

void MeterSampler::stop() {
    running.store(false, std::memory_order_release);

    if (thread.joinable()) thread.join();
}

MeterSampler::LatestFrame MeterSampler::getLatestFrame() const {
    const std::lock_guard<std::mutex> lock(frameMutex);
    return latest;
}

MeterFrame MeterSampler::makeDefaultFrame() const {
    const auto snapshot = state.snapshot();

    MeterFrame frame;
    frame.timestampMs = snapshot.uptimeMs;
    frame.slots.reserve(snapshot.slots.size());

    for (const auto& slot : snapshot.slots) {
        SlotMeter meter;
        meter.slot = slot.index;
        meter.armed = slot.armed;
        meter.gain = slot.armed ? 1.0f : 0.0f;
        frame.slots.push_back(meter);
    }

    return frame;
}

void MeterSampler::run() {
    const auto interval = std::chrono::milliseconds(1000 / meterHz);
    auto next = std::chrono::steady_clock::now();
    std::uint64_t sequence = 0;

    while (running.load(std::memory_order_acquire)) {
        MeterProvider source;

        {
            const std::lock_guard<std::mutex> lock(providerMutex);
            source = provider;
        }

        MeterFrame frame;

        if (source != nullptr) {
            try {
                frame = source();
            } catch (...) {
                frame = makeDefaultFrame();
            }
        } else {
            frame = makeDefaultFrame();
        }

        frame.sequence = ++sequence;

        if (!(frame.timestampMs > 0.0)) frame.timestampMs = state.getUptimeMs();

        auto json = toJson(meterFrameToVar(frame));

        {
            const std::lock_guard<std::mutex> lock(frameMutex);
            latest.json = std::move(json);
            latest.version = sequence;
        }

        next += interval;
        const auto now = std::chrono::steady_clock::now();

        if (next > now)
            std::this_thread::sleep_until(next);
        else
            next = now;
    }
}

}  // namespace Api
