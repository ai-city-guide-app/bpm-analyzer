#include <cmath>

#include "bpmcore/TempoEstimator.hpp"
#include "catch_amalgamated.hpp"

namespace {

bpmcore::OnsetEnvelope makeImpulseTrain(double hopSeconds, double bpm, double durationSec) {
    bpmcore::OnsetEnvelope env;
    env.hopSeconds = hopSeconds;
    const int n = static_cast<int>(durationSec / hopSeconds);
    env.values.assign(n, 0.0f);

    const double periodFrames = (60.0 / bpm) / hopSeconds;
    for (double f = 0.0; f < n; f += periodFrames) {
        int idx = static_cast<int>(std::lround(f));
        if (idx >= 0 && idx < n) {
            env.values[idx] = 1.0f;
        }
    }
    return env;
}

} // namespace

TEST_CASE("TempoEstimator recovers the known tempo's octave without error", "[tempo]") {
    // TempoEstimator only needs to land within one STFT hop's worth of lag
    // resolution of the true tempo (it merely seeds BeatTracker's target
    // period; final BPM precision comes from beat-time refinement in
    // BpmAnalyzer). What matters here is that it never locks onto the wrong
    // octave/harmonic.
    const double hopSeconds = 512.0 / 44100.0;
    for (double bpm : {120.0, 128.0, 95.0, 174.0}) {
        auto env = makeImpulseTrain(hopSeconds, bpm, 20.0);
        auto estimate = bpmcore::TempoEstimator::estimate(env);
        INFO("bpm = " << bpm << " estimated = " << estimate.bpm);
        REQUIRE(estimate.bpm == Catch::Approx(bpm).margin(2.5));
    }
}
