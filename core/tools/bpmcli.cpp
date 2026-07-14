#include <cstdio>
#include <exception>

#include "bpmcore/BpmAnalyzer.hpp"
#include "bpmcore/Wav.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <track.wav>\n", argv[0]);
        return 1;
    }

    try {
        bpmcore::WavData wav = bpmcore::readWavMono(argv[1]);
        bpmcore::BpmAnalyzer analyzer(wav.sampleRate);
        analyzer.feed(wav.samples.data(), wav.samples.size());

        bpmcore::Result result = analyzer.current();
        if (!result.ready) {
            std::fprintf(stderr, "not enough audio to estimate BPM (need at least a few seconds)\n");
            return 1;
        }

        std::printf("%.2f BPM, confidence %.2f\n", result.bpm, result.confidence);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
