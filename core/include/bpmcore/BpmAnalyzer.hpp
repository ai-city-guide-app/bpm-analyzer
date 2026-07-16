#pragma once

#include <cstddef>
#include <vector>

#include "bpmcore/OnsetDetector.hpp"

namespace bpmcore {

struct BeatEvent {
    double timestampSec = 0.0;
    float confidence = 0.0f; // 0..1, local confidence of this individual beat
};

struct Result {
    double bpm = 0.0;
    float confidence = 0.0f;      // 0..1, overall confidence
    bool ready = false;           // enough data accumulated for a first estimate
    std::vector<BeatEvent> beats; // for future visualization/sync
};

// Public API of the bpmcore engine: feed audio, read back the current
// tempo/beat estimate at any time, reset to analyze a new clip.
class BpmAnalyzer {
public:
    explicit BpmAnalyzer(double sampleRate = 44100.0);

    // samples: mono float32, range [-1, 1]. Can be called repeatedly with
    // successive chunks to simulate streaming input.
    void feed(const float* samples, size_t count);

    Result current() const;

    void reset();

private:
    double sampleRate_;
    OnsetDetector onsetDetector_;
    size_t totalSamplesFed_ = 0;
};

} // namespace bpmcore
