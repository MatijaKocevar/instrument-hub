# Instrument Hub

A live-performance workspace for VST3 instruments, effects and the Novation
Launchpad Mini MK3.

Instrument Hub combines a JUCE 8 / C++20 audio engine with an embedded React
interface. Eight prepared tracks stay warm, the Launchpad switches between them
instantly, and the same control API drives the desktop UI and a tablet remote.

## Highlights

- Eight-track performance grid with instruments, FX chains, two hot FX and a
  five-step mixer fader.
- Fixed low-latency audio graph; no allocation, locks or logging in the audio
  callback.
- Launchpad Mini MK3 programmer-mode LEDs; generic MIDI keyboards and
  controllers are supported too.
- VST3 hosting with out-of-process scanning, per-patch state capture and
  automatic recovery when a plugin host dies.
- Sampler, live audio input, guitar drive, tuner, EQ, compressor, delay and reverb inserts.
- Versioned `.ihub` projects with autosave and per-patch plugin state.
- Local HTTP + WebSocket API shared by the desktop UI and tablet remote.

## Build from source (Linux)

Prerequisites:

- CMake ≥ 3.22, Ninja, a C++20 compiler
- Node.js ≥ 22.13 and pnpm 11 (the repository pins pnpm via `packageManager`;
  `corepack pnpm` works)
- ALSA, JACK, freetype, X11, GL and WebKitGTK 4.1 development headers
- OpenSSL development headers (HTTPS for the phone MIDI bridge)

On Ubuntu 24.04:

```bash
sudo apt install build-essential pkg-config cmake ninja-build \
  libasound2-dev libjack-jackd2-dev libfreetype-dev libx11-dev \
  libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
  libxrandr-dev libxrender-dev libwebkit2gtk-4.1-dev \
  libglu1-mesa-dev libcurl4-openssl-dev libssl-dev
```

Clone and build:

```bash
git clone --recursive https://github.com/MatijaKocevar/instrument-hub.git
cd instrument-hub
corepack pnpm --dir ui install --frozen-lockfile
corepack pnpm --dir ui run build
cmake --preset release
cmake --build --preset release --parallel \
  --target instrument-hub ihub-scanner ihub-sandbox
```

Run:

```bash
./build-release/engine/instrument-hub
```

For interactive UI work, `scripts/dev.sh` starts Vite on port 5173 and the
debug build.

## First launch

1. Start the app. On a fresh machine it creates an eight-track blank project
   in `~/Music/Instrument Hub/Untitled.ihub`.
2. Open **Audio & MIDI**, select the interface and buffer size.
3. Open a track from the Grid, choose **Change source**, and load a VST3,
   sampler kit or live audio input.
4. Connect the Launchpad Mini MK3. Its `MI` port is detected automatically.
5. Save the project from the footer; autosave keeps the current `.ihub` up to
   date after each edit.

The default mapping is the track layout in `shared/tracks-set.yaml`.

## Phone MIDI bridge

The engine can take MIDI from a phone on the same network. The phone reads a
USB-MIDI keyboard with Chrome's Web MIDI support and forwards the messages over
the control WebSocket; the desktop sees the phone as a normal MIDI input.

1. Open **Settings & Account**, turn on LAN mode and keep the app running.
2. Connect the keyboard to the Android phone with a USB OTG adapter.
3. Scan the QR code, or open the `https://<lan-ip>:8443/remote?code=...` link
   from the pairing panel in Chrome on the phone.
4. Accept the self-signed certificate warning, pair, then tap **MIDI** in the
   top right.
5. Allow MIDI devices, choose the keyboard, and tap **Start sending**.

The phone input appears in **Audio & MIDI → MIDI** as a `WiFi` input and can
be enabled, muted or switched to the pad grid like any other input. Held notes
are released automatically if the WiFi link drops.

Notes:

- Web MIDI needs a secure context. The engine serves the phone UI over HTTPS on
  port 8443 with a certificate generated under `~/.config/Instrument Hub/tls`.
  Browsers warn until the certificate is trusted; in Chrome tap
  **Advanced → Proceed**.
- Android Chrome is required. Safari on iPhone has no Web MIDI.
- The keyboard must be USB-MIDI class compliant, or use a USB-MIDI interface for
  5-pin DIN keyboards.
- Keep the bridge tab visible; Chrome throttles background tabs.
