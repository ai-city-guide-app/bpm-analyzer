#include <cmath>
#include <string>
#include <vector>

#include "bpmcore/BpmAnalyzer.hpp"
#include "bpmcore/Wav.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace {

std::string dataPath(const std::string& filename) {
    return std::string(BPMCORE_TEST_DATA_DIR) + "/synthetic/" + filename;
}

// Whether `bpm` is a plausible octave error of `expected` (half/double/2:3/3:2).
bool isOctaveError(double bpm, double expected) {
    const double ratios[] = {0.5, 2.0, 2.0 / 3.0, 1.5};
    for (double r : ratios) {
        if (std::abs(bpm - expected * r) < 0.5) {
            return true;
        }
    }
    return false;
}

bpmcore::Result analyzeFile(const std::string& path) {
    bpmcore::WavData wav = bpmcore::readWavMono(path);
    bpmcore::BpmAnalyzer analyzer(wav.sampleRate);
    analyzer.feed(wav.samples.data(), wav.samples.size());
    return analyzer.current();
}

} // namespace

TEST_CASE("BpmAnalyzer meets the Stage 1 acceptance criterion on clean click tracks", "[acceptance]") {
    struct Case {
        std::string tag;
        double bpm;
    };
    const std::vector<Case> cases = {{"120_00", 120.0}, {"128_00", 128.0}, {"95_00", 95.0}, {"174_00", 174.0}};

    for (const auto& c : cases) {
        auto result = analyzeFile(dataPath("click_" + c.tag + "_clean.wav"));
        INFO("bpm=" << c.bpm << " got=" << result.bpm << " confidence=" << result.confidence);

        REQUIRE(result.ready);
        REQUIRE_FALSE(isOctaveError(result.bpm, c.bpm));
        REQUIRE(std::abs(result.bpm - c.bpm) <= 0.05);
        REQUIRE(result.confidence > 0.8f);
    }
}

TEST_CASE("BpmAnalyzer stays in the right octave on noisy/reverb click tracks", "[robustness]") {
    struct Case {
        std::string tag;
        double bpm;
    };
    const std::vector<Case> cases = {{"120_00", 120.0}, {"128_00", 128.0}, {"95_00", 95.0}, {"174_00", 174.0}};

    for (const auto& c : cases) {
        for (const char* variant : {"noisy", "reverb"}) {
            auto result = analyzeFile(dataPath("click_" + c.tag + "_" + variant + ".wav"));
            INFO("bpm=" << c.bpm << " variant=" << variant << " got=" << result.bpm);

            REQUIRE(result.ready);
            REQUIRE_FALSE(isOctaveError(result.bpm, c.bpm));
            REQUIRE(std::abs(result.bpm - c.bpm) <= 1.0);
        }
    }
}
