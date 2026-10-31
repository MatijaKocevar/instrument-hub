# Locked Decisions

Do not re-open these. If a task seems to contradict one, the task is wrong.

## Stack

- **Engine:** C++20, JUCE 8 (git submodule at `third_party/JUCE`).
- **UI:** React + TypeScript + Vite, embedded with `juce::WebBrowserComponent`.
- **Build:** CMake ≥ 3.22, Ninja or Unix Makefiles. `CMAKE_BUILD_TYPE=RelWithDebInfo`.
- **Deps:** header-only or vendored. No package manager for C++.

## Audio architecture

- **Never rewire the graph at runtime.** All slots are wired in parallel into
  a summing bus from startup. Switching only changes which slots get
  `processBlock()` called and applies a 2–5 ms gain ramp.
- `juce::AudioProcessorGraph` is **not** used for the live path. It takes a
  lock on edits. A custom fixed graph owns `AudioProcessor*` chains directly.
- Audio callback: no `new`, no `malloc`, no `std::mutex`, no `std::string`,
  no logging, no file I/O, no `SpinLock`.
- Cross-thread state: `juce::AbstractFifo` or a lock-free SPSC ring buffer.
- Buffers pre-allocated at max block size. Latency queried once per load.
- Plugin delay compensation is summed per chain and applied on the bus.

## Instruments

Single interface, three implementations:

```cpp
class Instrument {
public:
    virtual ~Instrument() = default;
    virtual void prepare(double sampleRate, int maxBlock) = 0;
    virtual void noteOn(int ch, int note, float vel) = 0;
    virtual void noteOff(int ch, int note) = 0;
    virtual void process(juce::AudioBuffer<float>&, juce::MidiBuffer&) = 0;
    virtual int  latencySamples() const = 0;
    virtual Kind kind() const = 0;   // Plugin | AudioInput | Sampler
};
```

## Controller

- **Target device:** Novation Launchpad Mini MK3 (64-pad 8×8 grid; no keys, no
  encoders). Drive it in **programmer mode**: `F0 00 20 29 02 0D 0E 01 F7`.
- Pads report notes 11..88 on the **MI** port and are lit by Note On on the same
  note with a palette velocity. The DA port carries no pad or LED data.
- Mapping lives in `set.yaml`, hot-reloadable. Never hardcode pad numbers.
- With no encoders, macros/pages live on the top row (CC 91..98); the right
  column (CC 89..19) selects scenes. Shift/modifier semantics belong to the
  mapping, not the driver.

## Control API

- One HTTP + WebSocket server, `cpp-httplib` (header-only, MIT) + a minimal
  WS implementation or `uWebSockets` if WS proves painful.
- Bind `127.0.0.1:8080` by default; LAN only when explicitly enabled.
- The embedded UI and the tablet remote are **the same React app**, different
  routes. No second protocol layer.
- Meters broadcast at ~30 Hz, coalesced. Never per audio buffer.
- Auth: 6-digit pairing code, token in a header. LAN-only, never public.

## Testing without hardware

- **Audio:** ALSA Loopback (`card 2`, already loaded) is the device for all
  round-trip and xrun tests.
- **MIDI:** ALSA sequencer virtual port impersonating the Launchpad.
- **Plugins:** a JUCE VST3 we compile ourselves as `TestPlugin`, plus an
  optional third-party VST3 when one is installed.
- **UI:** snapshot from inside JUCE (`createComponentSnapshot`), not a
  desktop screenshot tool.

## Persistence

- Set file: versioned JSON + binary blobs for plugin state, in a single
  `.ihub` container (ZIP-like). Never inline raw plugin chunks in JSON.

## Licensing

- JUCE is AGPLv3 for this project. Distribution of a closed-source binary
  requires a commercial licence — that is a human decision, tracked in
  `BLOCKERS.md`. Do not design around it.

## Naming

- Product name: **Instrument Hub**. Binary: `instrument-hub`.
- Repo layout: `engine/`, `ui/`, `shared/`, `third_party/`, `tests/`, `plan/`.
