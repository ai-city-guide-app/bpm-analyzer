#include "bpmcore/BpmAnalyzer.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "bpmcore/BeatTracker.hpp"
#include "bpmcore/TempoEstimator.hpp"

namespace bpmcore {

namespace {

constexpr double kMinAnalysisSeconds = 4.0;
constexpr int kMinBeatsForEstimate = 4;

// Sub-frame refinement of a beat's timestamp via quadratic (parabolic)
// interpolation of the onset envelope around the detected peak frame. This
// counteracts the hop-size quantization (11.6ms at 44.1kHz/hop512), which
// would otherwise dominate the error budget for BPM precision.
double refineBeatTimeSec(const OnsetEnvelope& envelope, int frameIdx) {
    const int n = static_cast<int>(envelope.values.size());
    double offsetFrames = 0.0;
    if (frameIdx > 0 && frameIdx < n - 1) {
        double alpha = envelope.values[frameIdx - 1];
        double beta = envelope.values[frameIdx];
        double gamma = envelope.values[frameIdx + 1];
        double denom = (alpha - 2.0 * beta + gamma);
        if (std::abs(denom) > 1e-9) {
            offsetFrames = std::clamp(0.5 * (alpha - gamma) / denom, -0.5, 0.5);
        }
    }
    return (frameIdx + offsetFrames) * envelope.hopSeconds;
}

double median(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    size_t mid = values.size() / 2;
    if (values.size() % 2 == 0) {
        return 0.5 * (values[mid - 1] + values[mid]);
    }
    return values[mid];
}

} // namespace

BpmAnalyzer::BpmAnalyzer(double sampleRate) : sampleRate_(sampleRate), onsetDetector_(sampleRate) {}

void BpmAnalyzer::feed(const float* samples, size_t count) {
    onsetDetector_.feed(samples, count);
    totalSamplesFed_ += count;
}

void BpmAnalyzer::reset() {
    onsetDetector_.reset();
    totalSamplesFed_ = 0;
}

Result BpmAnalyzer::current() const {
    Result result;

    const double secondsFed = static_cast<double>(totalSamplesFed_) / sampleRate_;
    const OnsetEnvelope& envelope = onsetDetector_.envelope();

    TempoEstimate tempo = TempoEstimator::estimate(envelope);
    if (tempo.bpm <= 0.0) {
        return result;
    }

    std::vector<DetectedBeat> rawBeats = BeatTracker::track(envelope, tempo.bpm);

    std::vector<double> refinedTimes;
    refinedTimes.reserve(rawBeats.size());
    for (const auto& b : rawBeats) {
        int frameIdx = static_cast<int>(std::lround(b.timestampSec / envelope.hopSeconds));
        refinedTimes.push_back(refineBeatTimeSec(envelope, frameIdx));
    }

    result.beats.reserve(rawBeats.size());
    for (size_t i = 0; i < rawBeats.size(); ++i) {
        result.beats.push_back(BeatEvent{refinedTimes[i], rawBeats[i].confidence});
    }

    result.ready = secondsFed >= kMinAnalysisSeconds && static_cast<int>(rawBeats.size()) >= kMinBeatsForEstimate;
    if (!result.ready) {
        return result;
    }

    std::vector<double> ibi;
    ibi.reserve(refinedTimes.size() - 1);
    for (size_t i = 1; i < refinedTimes.size(); ++i) {
        double d = refinedTimes[i] - refinedTimes[i - 1];
        if (d > 0.0) {
            ibi.push_back(d);
        }
    }
    if (ibi.empty()) {
        return result;
    }

    // Use the median IBI as a robust reference to reject outlier intervals
    // (missed/doubled beat detections), then average the surviving "inlier"
    // intervals. Averaging (rather than taking the median value itself)
    // cancels the hop-quantization jitter of individual beat timestamps,
    // which is what the 0.05 BPM accuracy target requires.
    double medianIbi = median(ibi);
    std::vector<double> inliers;
    inliers.reserve(ibi.size());
    for (double v : ibi) {
        if (v >= 0.8 * medianIbi && v <= 1.2 * medianIbi) {
            inliers.push_back(v);
        }
    }
    if (inliers.empty()) {
        inliers = ibi;
    }

    double meanIbi = std::accumulate(inliers.begin(), inliers.end(), 0.0) / static_cast<double>(inliers.size());
    result.bpm = 60.0 / meanIbi;

    double variance = 0.0;
    for (double v : inliers) {
        double d = v - meanIbi;
        variance += d * d;
    }
    variance /= static_cast<double>(inliers.size());
    double stddev = std::sqrt(variance);
    double cv = meanIbi > 0.0 ? stddev / meanIbi : 1.0;

    // Base confidence from IBI stability: CV=0 -> 1.0, CV>=0.15 -> 0.0.
    double baseConfidence = std::clamp(1.0 - cv / 0.15, 0.0, 1.0);
    // Modulators: fewer than ~8 beats reduces confidence in the estimate;
    // a diffuse (non-dominant) autocorrelation peak reduces it further.
    double countFactor = std::clamp(static_cast<double>(ibi.size()) / 8.0, 0.0, 1.0);
    double strengthFactor = 0.5 + 0.5 * tempo.strength;

    result.confidence = static_cast<float>(std::clamp(baseConfidence * countFactor * strengthFactor, 0.0, 1.0));

    return result;
}

} // namespace bpmcore
