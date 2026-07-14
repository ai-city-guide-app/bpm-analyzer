#include "bpmcore/TempoEstimator.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace bpmcore {

namespace {

int bpmToLagFrames(double bpm, double hopSeconds) {
    double lagSeconds = 60.0 / bpm;
    return std::max(1, static_cast<int>(std::lround(lagSeconds / hopSeconds)));
}

double lagFramesToBpm(int lag, double hopSeconds) {
    return 60.0 / (lag * hopSeconds);
}

// Comb-filtered autocorrelation score at a given lag: reinforces periodicity
// by summing energy at the fundamental lag plus its first two harmonics.
double combScore(const std::vector<float>& env, int lag) {
    const int n = static_cast<int>(env.size());
    if (lag <= 0 || lag >= n) {
        return 0.0;
    }

    auto autocorrAtLag = [&](int l) -> double {
        if (l <= 0 || l >= n) {
            return 0.0;
        }
        double sum = 0.0;
        for (int i = 0; i + l < n; ++i) {
            sum += static_cast<double>(env[i]) * static_cast<double>(env[i + l]);
        }
        return sum / (n - l);
    };

    double score = autocorrAtLag(lag);
    score += 0.5 * autocorrAtLag(2 * lag);
    score += 0.33 * autocorrAtLag(3 * lag);
    return score;
}

} // namespace

TempoEstimate TempoEstimator::estimate(const OnsetEnvelope& envelope, double minBpm, double maxBpm,
                                        double preferredLoBpm, double preferredHiBpm) {
    TempoEstimate result;
    if (envelope.values.empty() || envelope.hopSeconds <= 0.0) {
        return result;
    }

    // Search directly within [minBpm, maxBpm]: octave-related candidates
    // (half/double/2:3/3:2 of the true tempo) are handled below by explicitly
    // evaluating that multiplier family, not by widening the raw search.
    // Widening the initial search invites a harmonic-bias artifact where a
    // higher multiple of the true period (which also autocorrelates strongly,
    // since it's still a multiple of the fundamental) outscores the
    // fundamental itself.
    const int lagMin = bpmToLagFrames(maxBpm, envelope.hopSeconds);
    const int lagMax = bpmToLagFrames(minBpm, envelope.hopSeconds);
    if (lagMin >= lagMax || lagMax >= static_cast<int>(envelope.values.size())) {
        return result;
    }

    std::map<int, double> scoreByLag;
    int bestLag = lagMin;
    double bestScore = -1.0;
    for (int lag = lagMin; lag <= lagMax; ++lag) {
        double s = combScore(envelope.values, lag);
        scoreByLag[lag] = s;
        if (s > bestScore) {
            bestScore = s;
            bestLag = lag;
        }
    }

    if (bestScore <= 0.0) {
        return result;
    }

    double bestBpm = lagFramesToBpm(bestLag, envelope.hopSeconds);

    // Octave-error correction: consider the harmonic/subharmonic family of the
    // raw best candidate and prefer whichever member falls in the preferred
    // range, as long as its score is competitive with the global best.
    const double candidateMultipliers[] = {1.0, 0.5, 2.0, 2.0 / 3.0, 1.5};
    double chosenBpm = bestBpm;
    double chosenScore = bestScore;

    for (double mult : candidateMultipliers) {
        double candidateBpm = bestBpm * mult;
        if (candidateBpm < minBpm || candidateBpm > maxBpm) {
            continue;
        }
        // Search a small neighborhood around the nominal candidate lag: a
        // pure round-trip through BPM can land exactly on a half-integer lag
        // (e.g. doubling an odd bestLag), where rounding direction alone
        // would otherwise pick a lag with no real periodicity alignment.
        int nominalLag = bpmToLagFrames(candidateBpm, envelope.hopSeconds);
        int candidateLag = nominalLag;
        double candidateScore = -1.0;
        for (int lag = std::max(1, nominalLag - 1); lag <= nominalLag + 1; ++lag) {
            if (lag >= static_cast<int>(envelope.values.size())) {
                continue;
            }
            double s = combScore(envelope.values, lag);
            if (s > candidateScore) {
                candidateScore = s;
                candidateLag = lag;
            }
        }
        if (candidateScore < 0.0) {
            continue;
        }
        candidateBpm = lagFramesToBpm(candidateLag, envelope.hopSeconds);
        bool inPreferred = candidateBpm >= preferredLoBpm && candidateBpm <= preferredHiBpm;
        bool chosenInPreferred = chosenBpm >= preferredLoBpm && chosenBpm <= preferredHiBpm;
        bool competitive = candidateScore >= 0.6 * bestScore;

        if (competitive && inPreferred && !chosenInPreferred) {
            chosenBpm = candidateBpm;
            chosenScore = candidateScore;
        } else if (competitive && inPreferred && chosenInPreferred && candidateScore > chosenScore) {
            chosenBpm = candidateBpm;
            chosenScore = candidateScore;
        }
    }

    if (chosenBpm < minBpm || chosenBpm > maxBpm) {
        // Fold back into the requested range as a last resort.
        while (chosenBpm < minBpm) chosenBpm *= 2.0;
        while (chosenBpm > maxBpm) chosenBpm /= 2.0;
    }

    result.bpm = chosenBpm;

    // Peak "strength": how dominant the chosen peak is relative to the mean
    // score across the searched lag range (sharp single peak -> near 1).
    double sum = 0.0;
    for (auto& kv : scoreByLag) {
        sum += kv.second;
    }
    double mean = sum / static_cast<double>(scoreByLag.size());
    double sharpness = mean > 0.0 ? (chosenScore - mean) / chosenScore : 0.0;
    result.strength = static_cast<float>(std::clamp(sharpness, 0.0, 1.0));

    return result;
}

} // namespace bpmcore
