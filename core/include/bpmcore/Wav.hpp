#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bpmcore {

// Minimal mono WAV reader/writer (PCM16 or IEEE float), no platform dependencies.
struct WavData {
    double sampleRate = 0.0;
    std::vector<float> samples; // mono, downmixed if source was multi-channel, range [-1, 1]
};

// Throws std::runtime_error on malformed/unsupported files.
WavData readWavMono(const std::string& path);

// Writes mono PCM16 WAV. Throws std::runtime_error on I/O failure.
void writeWavMonoPCM16(const std::string& path, double sampleRate, const std::vector<float>& samples);

} // namespace bpmcore
