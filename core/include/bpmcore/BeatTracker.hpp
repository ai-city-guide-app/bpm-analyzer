#pragma once

#include <vector>

#include "bpmcore/OnsetDetector.hpp"

namespace bpmcore {

struct DetectedBeat {
    double timestampSec = 0.0;
    float confidence = 0.0f; // 0..1, local confidence of this individual beat
};

// Dynamic-programming beat tracker (Ellis 2007, "Beat Tracking by Dynamic
// Programming"): given an onset envelope and an estimated tempo, places beats
// through the whole envelope by maximizing onset strength at beat locations
// while penalizing inter-beat intervals that deviate from the target period.
class BeatTracker {
public:
    // bpm: tempo estimate driving the target beat period.
    // alpha: tightness of the period constraint (higher = stricter to bpm).
    static std::vector<DetectedBeat> track(const OnsetEnvelope& envelope, double bpm, double alpha = 200.0);
};

} // namespace bpmcore
