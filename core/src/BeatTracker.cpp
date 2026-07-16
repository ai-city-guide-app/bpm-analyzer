#include "bpmcore/BeatTracker.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace bpmcore {

std::vector<DetectedBeat> BeatTracker::track(const OnsetEnvelope& envelope, double bpm, double alpha) {
    std::vector<DetectedBeat> beats;
    const int n = static_cast<int>(envelope.values.size());
    if (n == 0 || bpm <= 0.0 || envelope.hopSeconds <= 0.0) {
        return beats;
    }

    const double tau = 60.0 / bpm / envelope.hopSeconds; // target period, in frames
    if (tau < 1.0) {
        return beats;
    }

    const int loDelta = std::max(1, static_cast<int>(std::floor(tau / 2.0)));
    const int hiDelta = std::max(loDelta, static_cast<int>(std::ceil(tau * 2.0)));

    std::vector<double> cumScore(n, 0.0);
    std::vector<int> backlink(n, -1);

    float maxOnset = 0.0f;
    for (float v : envelope.values) {
        maxOnset = std::max(maxOnset, v);
    }
    if (maxOnset <= 0.0f) {
        maxOnset = 1.0f;
    }

    for (int t = 0; t < n; ++t) {
        double best = 0.0; // corresponds to "no predecessor" (transition score 0)
        int bestDelta = -1;
        int deltaMax = std::min(hiDelta, t);
        for (int delta = loDelta; delta <= deltaMax; ++delta) {
            double logRatio = std::log(static_cast<double>(delta) / tau);
            double transitionScore = -alpha * logRatio * logRatio;
            double candidate = transitionScore + cumScore[t - delta];
            if (candidate > best) {
                best = candidate;
                bestDelta = delta;
            }
        }
        cumScore[t] = static_cast<double>(envelope.values[t]) + best;
        backlink[t] = bestDelta >= 0 ? t - bestDelta : -1;
    }

    int bestT = 0;
    double bestFinal = -std::numeric_limits<double>::infinity();
    for (int t = 0; t < n; ++t) {
        if (cumScore[t] > bestFinal) {
            bestFinal = cumScore[t];
            bestT = t;
        }
    }

    std::vector<int> indices;
    for (int t = bestT; t >= 0;) {
        indices.push_back(t);
        int prev = backlink[t];
        if (prev < 0) {
            break;
        }
        t = prev;
    }
    std::reverse(indices.begin(), indices.end());

    beats.reserve(indices.size());
    for (int idx : indices) {
        DetectedBeat b;
        b.timestampSec = idx * envelope.hopSeconds;
        b.confidence = std::clamp(envelope.values[idx] / maxOnset, 0.0f, 1.0f);
        beats.push_back(b);
    }

    return beats;
}

} // namespace bpmcore
