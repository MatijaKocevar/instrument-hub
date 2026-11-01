#pragma once

#include <alsa/asoundlib.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace SelfTest
{

std::string alsaError (const char* what, int err);
void checkAlsa (int err, const char* what);
snd_pcm_t* openPcm (const std::string& device, snd_pcm_stream_t stream);
void configurePcm (snd_pcm_t* pcm, const std::string& device);
std::vector<std::int16_t> makeSine();

void playbackLoop (snd_pcm_t* pcm,
                   const std::vector<std::int16_t>& source,
                   std::atomic<unsigned int>& xruns,
                   std::atomic<bool>& failed,
                   std::atomic<std::size_t>& writtenFrames);

void captureLoop (snd_pcm_t* pcm,
                  std::vector<std::int16_t>& destination,
                  std::atomic<unsigned int>& xruns,
                  std::atomic<bool>& failed);

} // namespace SelfTest
