#include "Instrument.h"

namespace ihub {

const char* toString(Instrument::Kind kind) noexcept {
  switch (kind) {
    case Instrument::Kind::Plugin:
      return "plugin";
    case Instrument::Kind::AudioInput:
      return "audio-input";
    case Instrument::Kind::Sampler:
      return "sampler";
  }

  return "plugin";
}

}  // namespace ihub
