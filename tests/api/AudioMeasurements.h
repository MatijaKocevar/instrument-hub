#pragma once

#include "Session.h"

#include <vector>

namespace ApiTest
{

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 128;

void renderBlocks (ihub::Session& session, int blocks, int blockSize = kBlockSize);

std::vector<float> capture (ihub::Session& session,
                            int settleBlocks,
                            int measureBlocks,
                            int blockSize = kBlockSize);

double rmsOf (const std::vector<float>& samples);

double dbfs (double linear);

double maxDifference (const std::vector<float>& a, const std::vector<float>& b);

double estimateFrequency (const std::vector<float>& samples, double sampleRate = kSampleRate);

double captureFrequency (ihub::Session& session,
                         double seconds,
                         double sampleRate = kSampleRate,
                         int blockSize = kBlockSize);

}
