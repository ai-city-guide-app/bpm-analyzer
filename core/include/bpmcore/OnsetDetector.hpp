#pragma once

#include <cstddef>
#include <vector>

// Forward-declare KissFFT real-FFT config to avoid leaking the C header into
// every translation unit that includes this file.
struct kiss_fftr_state;
typedef struct kiss_fftr_state* kiss_fftr_cfg;

namespace bpmcore {

// Onset envelope: one weighted multi-band spectral-flux value per STFT hop.
struct OnsetEnvelope {
    double hopSeconds = 0.0;
    std::vector<float> values;
};

// STFT (window 1024, hop 512 by default) + multi-band spectral flux onset
// detector, per section 2 of the design plan. Streams samples via feed();
// the resulting envelope grows incrementally and is available via envelope().
class OnsetDetector {
public:
    explicit OnsetDetector(double sampleRate, int fftSize = 1024, int hopSize = 512);
    ~OnsetDetector();

    OnsetDetector(const OnsetDetector&) = delete;
    OnsetDetector& operator=(const OnsetDetector&) = delete;

    void feed(const float* samples, size_t count);
    const OnsetEnvelope& envelope() const { return envelope_; }
    void reset();

    double sampleRate() const { return sampleRate_; }
    int fftSize() const { return fftSize_; }
    int hopSize() const { return hopSize_; }

private:
    void processFrame();

    double sampleRate_;
    int fftSize_;
    int hopSize_;

    std::vector<float> window_;      // Hann window, size fftSize_
    std::vector<float> buffer_;      // unprocessed input samples (ring-less, simple deque via vector)
    std::vector<float> windowed_;    // scratch: windowed frame, size fftSize_
    std::vector<float> prevMag_;     // magnitude spectrum of previous frame, size fftSize_/2+1
    bool havePrevMag_ = false;

    kiss_fftr_cfg fftCfg_ = nullptr;

    OnsetEnvelope envelope_;
};

} // namespace bpmcore
