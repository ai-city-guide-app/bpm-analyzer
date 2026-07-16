#pragma once

#include "bpmcore/OnsetDetector.hpp"

namespace bpmcore {

struct TempoEstimate {
    double bpm = 0.0;
    float strength = 0.0f; // 0..1, sharpness/dominance of the autocorrelation peak
};

// Autocorrelation + comb-filter tempo induction with octave-error correction,
// per section 2 (steps 3-4) of the design plan.
class TempoEstimator {
public:
    static TempoEstimate estimate(const OnsetEnvelope& envelope, double minBpm = 60.0, double maxBpm = 200.0,
                                   double preferredLoBpm = 70.0, double preferredHiBpm = 180.0);
};

} // namespace bpmcore
