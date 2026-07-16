import CBpmCore

public struct BeatEvent: Sendable {
    public let timestampSec: Double
    public let confidence: Float
}

public struct BpmResult: Sendable {
    public let bpm: Double
    public let confidence: Float
    public let ready: Bool
    public let beats: [BeatEvent]
}

/// Swift-facing wrapper over the bpmcore C++ engine (see `BpmBridge.hpp`).
/// Feed mono float32 samples, read back the current tempo/beat estimate at
/// any time, reset to analyze a new clip.
public final class BpmEngine {
    private var bridge: BpmBridge

    public init(sampleRate: Double = 44100.0) {
        bridge = BpmBridge(sampleRate)
    }

    public func feed(_ samples: [Float]) {
        samples.withUnsafeBufferPointer { buffer in
            bridge.feed(buffer.baseAddress, buffer.count)
        }
    }

    public func current() -> BpmResult {
        let r = bridge.current()
        var beats: [BeatEvent] = []
        let count = bridge.beatCount()
        beats.reserveCapacity(count)
        for i in 0..<count {
            let b = bridge.beatAt(i)
            beats.append(BeatEvent(timestampSec: b.timestampSec, confidence: b.confidence))
        }
        return BpmResult(bpm: r.bpm, confidence: r.confidence, ready: r.ready, beats: beats)
    }

    public func reset() {
        bridge.reset()
    }

    /// Runs a short in-process synthetic click-track analysis to verify the
    /// full Swift -> C++ -> DSP pipeline works, without needing microphone
    /// capture or a bundled WAV file.
    public static func selfTest() -> BpmResult {
        let r = BpmBridge.runSelfTest()
        return BpmResult(bpm: r.bpm, confidence: r.confidence, ready: r.ready, beats: [])
    }
}
