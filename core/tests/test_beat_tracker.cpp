#include <cmath>

#include "bpmcore/BeatTracker.hpp"
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

TEST_CASE("BeatTracker places beats at the known period", "[beattracker]") {
    const double hopSeconds = 512.0 / 44100.0;
    const double bpm = 128.0;
    auto env = makeImpulseTrain(hopSeconds, bpm, 15.0);

    auto beats = bpmcore::BeatTracker::track(env, bpm);

    REQUIRE(beats.size() >= 10);

    const double expectedPeriod = 60.0 / bpm;
    for (size_t i = 1; i < beats.size(); ++i) {
        double interval = beats[i].timestampSec - beats[i - 1].timestampSec;
        REQUIRE(interval == Catch::Approx(expectedPeriod).margin(hopSeconds * 1.5));
    }
}
