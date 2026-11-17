#include "audio/FxRack.h"

#include "dsp/tuner/Tuner.h"

#include <cstddef>

namespace AudioEngine {

bool FxRack::getTunerReading(int index, FxTunerReading& reading) const {
    if (index < 0 || index >= size()) return false;

    const auto& insert = inserts[static_cast<std::size_t>(index)];

    if (insert.type != FxInsertType::tuner || insert.effect == nullptr) return false;

    const auto* tuner = static_cast<const ihub::dsp::Tuner*>(insert.effect->getWrappedProcessor());

    if (tuner == nullptr) return false;

    const auto current = tuner->getReading();

    reading.valid = current.valid;
    reading.frequencyHz = current.frequencyHz;
    reading.midiNote = current.midiNote;
    reading.cents = current.cents;
    reading.confidence = current.confidence;
    reading.levelDb = current.levelDb;
    return true;
}

}  // namespace AudioEngine
