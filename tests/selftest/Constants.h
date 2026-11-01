#pragma once

namespace SelfTest
{

constexpr unsigned int kSampleRate   = 48000;
constexpr unsigned int kChannels     = 1;
constexpr unsigned int kPeriodFrames = 128;
constexpr unsigned int kBufferFrames = 16384;
constexpr unsigned int kTotalFrames  = kSampleRate;
constexpr double       kFrequency    = 440.0;
constexpr double       kAmplitude    = 0.5;

constexpr const char* kDefaultPlayback = "hw:Loopback,0,0";
constexpr const char* kDefaultCapture  = "hw:Loopback,1,0";
constexpr const char* kDefaultOutput   = "out.wav";

}
