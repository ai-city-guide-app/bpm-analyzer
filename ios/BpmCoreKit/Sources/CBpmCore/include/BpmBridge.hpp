#pragma once

#include <cstddef>

// Swift/C++-interop-friendly wrapper over bpmcore::BpmAnalyzer. Only
// primitives and POD structs cross the Swift boundary (no std::vector, no
// std::string) since that is the most robust interop surface documented by
// Apple for Swift 5.9+ direct C++ interop.
struct BpmBridgeBeat {
    double timestampSec;
    float confidence;
};

struct BpmBridgeResult {
    double bpm;
    float confidence;
    bool ready;
};

class BpmBridge {
public:
    explicit BpmBridge(double sampleRate);
    ~BpmBridge();

    BpmBridge(const BpmBridge&) = delete;
    BpmBridge& operator=(const BpmBridge&) = delete;

    BpmBridge(BpmBridge&& other) noexcept;
    BpmBridge& operator=(BpmBridge&& other) noexcept;

    void feed(const float* samples, size_t count);
    BpmBridgeResult current() const;
    size_t beatCount() const;
    BpmBridgeBeat beatAt(size_t index) const;
    void reset();

    // Builds a short synthetic click track (~8s at 120 BPM) in-process and
    // analyzes it, exercising the full Swift -> C++ -> DSP pipeline without
    // needing microphone capture or a bundled WAV file.
    static BpmBridgeResult runSelfTest();

private:
    class Impl;
    Impl* impl_;
};
