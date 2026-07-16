#include "BpmBridge.hpp"

#include <cmath>
#include <vector>

#include "bpmcore/BpmAnalyzer.hpp"

class BpmBridge::Impl {
public:
    explicit Impl(double sampleRate) : analyzer(sampleRate) {}

    bpmcore::BpmAnalyzer analyzer;
    bpmcore::Result lastResult;
};

BpmBridge::BpmBridge(double sampleRate) : impl_(new Impl(sampleRate)) {}

BpmBridge::~BpmBridge() {
    delete impl_;
}

BpmBridge::BpmBridge(BpmBridge&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
}

BpmBridge& BpmBridge::operator=(BpmBridge&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

void BpmBridge::feed(const float* samples, size_t count) {
    impl_->analyzer.feed(samples, count);
}

BpmBridgeResult BpmBridge::current() const {
    impl_->lastResult = impl_->analyzer.current();
    const auto& r = impl_->lastResult;
    return BpmBridgeResult{r.bpm, r.confidence, r.ready};
}

size_t BpmBridge::beatCount() const {
    return impl_->lastResult.beats.size();
}

BpmBridgeBeat BpmBridge::beatAt(size_t index) const {
    const auto& b = impl_->lastResult.beats.at(index);
    return BpmBridgeBeat{b.timestampSec, b.confidence};
}

void BpmBridge::reset() {
    impl_->analyzer.reset();
    impl_->lastResult = bpmcore::Result{};
}

namespace {

// Mirrors scripts/generate_click_tracks.py's clean click track, generated
// in-memory instead of as a WAV file.
std::vector<float> makeClickTrain(double sampleRate, double bpm, double durationSec) {
    const size_t n = static_cast<size_t>(durationSec * sampleRate);
    std::vector<float> samples(n, 0.0f);

    const double periodSec = 60.0 / bpm;
    const int clickLen = static_cast<int>(0.015 * sampleRate);
    const double clickFreq = 1000.0;

    for (double t = 0.0; t < durationSec; t += periodSec) {
        int start = static_cast<int>(t * sampleRate);
        for (int i = 0; i < clickLen && start + i < static_cast<int>(n); ++i) {
            double envelope = std::pow(1.0 - static_cast<double>(i) / clickLen, 2.0);
            double phase = 2.0 * M_PI * clickFreq * (static_cast<double>(i) / sampleRate);
            samples[start + i] = static_cast<float>(envelope * std::sin(phase));
        }
    }
    return samples;
}

} // namespace

BpmBridgeResult BpmBridge::runSelfTest() {
    const double sampleRate = 44100.0;
    auto samples = makeClickTrain(sampleRate, 120.0, 8.0);

    bpmcore::BpmAnalyzer analyzer(sampleRate);
    analyzer.feed(samples.data(), samples.size());

    bpmcore::Result r = analyzer.current();
    return BpmBridgeResult{r.bpm, r.confidence, r.ready};
}
