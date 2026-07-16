#include "bpmcore/OnsetDetector.hpp"

#include <algorithm>
#include <cmath>

extern "C" {
#include "kiss_fftr.h"
}

namespace bpmcore {

namespace {

// Frequency band boundaries (Hz) per the design plan: low end carries the
// most weight because it captures kick-drum onsets.
struct Band {
    double loHz;
    double hiHz;
    float weight;
};

constexpr Band kBands[3] = {
    {20.0, 150.0, 3.0f},    // low: kick drum
    {150.0, 2000.0, 1.2f},  // mid
    {2000.0, 20000.0, 0.6f} // high: hats/cymbals
};

} // namespace

OnsetDetector::OnsetDetector(double sampleRate, int fftSize, int hopSize)
    : sampleRate_(sampleRate), fftSize_(fftSize), hopSize_(hopSize) {
    window_.resize(fftSize_);
    for (int n = 0; n < fftSize_; ++n) {
        window_[n] = 0.5f - 0.5f * std::cos(2.0f * static_cast<float>(M_PI) * n / (fftSize_ - 1));
    }
    windowed_.resize(fftSize_);
    prevMag_.resize(fftSize_ / 2 + 1, 0.0f);
    envelope_.hopSeconds = static_cast<double>(hopSize_) / sampleRate_;

    fftCfg_ = kiss_fftr_alloc(fftSize_, 0, nullptr, nullptr);
}

OnsetDetector::~OnsetDetector() {
    if (fftCfg_) {
        kiss_fftr_free(fftCfg_);
    }
}

void OnsetDetector::reset() {
    buffer_.clear();
    havePrevMag_ = false;
    std::fill(prevMag_.begin(), prevMag_.end(), 0.0f);
    envelope_.values.clear();
}

void OnsetDetector::feed(const float* samples, size_t count) {
    buffer_.insert(buffer_.end(), samples, samples + count);
    while (buffer_.size() >= static_cast<size_t>(fftSize_)) {
        processFrame();
        buffer_.erase(buffer_.begin(), buffer_.begin() + hopSize_);
    }
}

void OnsetDetector::processFrame() {
    for (int n = 0; n < fftSize_; ++n) {
        windowed_[n] = buffer_[n] * window_[n];
    }

    const int numBins = fftSize_ / 2 + 1;
    std::vector<kiss_fft_cpx> freq(numBins);
    kiss_fftr(fftCfg_, windowed_.data(), freq.data());

    std::vector<float> mag(numBins);
    for (int k = 0; k < numBins; ++k) {
        mag[k] = std::sqrt(freq[k].r * freq[k].r + freq[k].i * freq[k].i);
    }

    float combined = 0.0f;
    if (havePrevMag_) {
        for (const Band& band : kBands) {
            int loBin = static_cast<int>(std::floor(band.loHz * fftSize_ / sampleRate_));
            int hiBin = static_cast<int>(std::ceil(band.hiHz * fftSize_ / sampleRate_));
            loBin = std::clamp(loBin, 0, numBins - 1);
            hiBin = std::clamp(hiBin, 0, numBins - 1);
            float flux = 0.0f;
            for (int k = loBin; k <= hiBin; ++k) {
                float diff = mag[k] - prevMag_[k];
                if (diff > 0.0f) {
                    flux += diff; // half-wave rectified spectral flux
                }
            }
            combined += band.weight * flux;
        }
    }

    envelope_.values.push_back(combined);
    prevMag_ = std::move(mag);
    havePrevMag_ = true;
}

} // namespace bpmcore
