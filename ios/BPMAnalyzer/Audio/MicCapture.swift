import AVFoundation

enum MicCaptureError: Error {
    case formatCreationFailed
    case converterCreationFailed
}

/// A fixed-capacity circular buffer of raw mono samples (currently write-only
/// — reserved for future waveform display / replay, per the Stage 2 plan).
/// Kept deliberately independent of bpmcore's own internal onset-envelope
/// buffer, which has no eviction and is fine for a single 15-20s session.
struct RingBuffer {
    let capacity: Int
    private var storage: [Float]
    private var writeIndex = 0

    init(seconds: Double, sampleRate: Double) {
        capacity = max(1, Int(seconds * sampleRate))
        storage = [Float](repeating: 0, count: capacity)
    }

    mutating func append(_ samples: [Float]) {
        for s in samples {
            storage[writeIndex] = s
            writeIndex = (writeIndex + 1) % capacity
        }
    }
}

/// Wraps AVAudioEngine microphone capture: requests permission, configures
/// the audio session for raw (unprocessed) input, resamples to 44.1kHz mono
/// Float32 regardless of the device's native format, and hands samples off
/// via `onSamples`. Knows nothing about bpmcore — the caller is responsible
/// for feeding samples into a BpmEngine on whatever thread/queue discipline
/// it needs (see the Stage 2 plan's thread-safety section).
final class MicCapture: NSObject, ObservableObject {
    @Published private(set) var isRunning = false
    @Published private(set) var inputLevel: Float = 0

    /// Called on the audio (tap) thread with 44.1kHz mono Float32 samples.
    /// Deliberately not hopped to main — feeding bpmcore has no business on
    /// the UI thread, and the caller owns its own serialization strategy.
    var onSamples: (([Float]) -> Void)?

    private let targetSampleRate: Double = 44100.0
    private let engine = AVAudioEngine()
    private var converter: AVAudioConverter?
    private var ringBuffer: RingBuffer
    private var shouldResumeAfterInterruption = false

    override init() {
        ringBuffer = RingBuffer(seconds: 20.0, sampleRate: targetSampleRate)
        super.init()
    }

    static func requestPermission(completion: @escaping (Bool) -> Void) {
        if #available(iOS 17.0, *) {
            AVAudioApplication.requestRecordPermission { granted in
                DispatchQueue.main.async { completion(granted) }
            }
        } else {
            AVAudioSession.sharedInstance().requestRecordPermission { granted in
                DispatchQueue.main.async { completion(granted) }
            }
        }
    }

    func start() throws {
        let session = AVAudioSession.sharedInstance()
        // .measurement disables the system's AGC/noise-suppression/echo-
        // cancellation processing on the mic input. That processing smears
        // transients, which is exactly what the spectral-flux onset detector
        // (Stage 1) depends on being sharp. This is the single most
        // important line in this file.
        try session.setCategory(.record, mode: .measurement)
        // Best-effort: most iPhones honor this and the converter below
        // becomes a near no-op. Not guaranteed, so the converter stays
        // mandatory regardless.
        try? session.setPreferredSampleRate(targetSampleRate)
        try session.setActive(true)

        registerNotifications()

        let inputNode = engine.inputNode
        let inputFormat = inputNode.outputFormat(forBus: 0)

        guard let targetFormat = AVAudioFormat(
            commonFormat: .pcmFormatFloat32,
            sampleRate: targetSampleRate,
            channels: 1,
            interleaved: false
        ) else {
            throw MicCaptureError.formatCreationFailed
        }

        guard let converter = AVAudioConverter(from: inputFormat, to: targetFormat) else {
            throw MicCaptureError.converterCreationFailed
        }
        self.converter = converter

        inputNode.installTap(onBus: 0, bufferSize: 4096, format: inputFormat) { [weak self] buffer, _ in
            self?.process(buffer: buffer, inputFormat: inputFormat, targetFormat: targetFormat)
        }

        engine.prepare()
        try engine.start()

        DispatchQueue.main.async { self.isRunning = true }
    }

    func stop() {
        engine.inputNode.removeTap(onBus: 0)
        engine.stop()
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
        unregisterNotifications()
        DispatchQueue.main.async { self.isRunning = false }
    }

    private func process(buffer: AVAudioPCMBuffer, inputFormat: AVAudioFormat, targetFormat: AVAudioFormat) {
        guard let converter = converter else { return }

        // convert(to:from:) does not resample between differing sample
        // rates; the input-block variant is required for that.
        let ratio = targetFormat.sampleRate / inputFormat.sampleRate
        let outputCapacity = AVAudioFrameCount((Double(buffer.frameLength) * ratio).rounded(.up))
        guard outputCapacity > 0,
              let outputBuffer = AVAudioPCMBuffer(pcmFormat: targetFormat, frameCapacity: outputCapacity)
        else {
            return
        }

        var conversionError: NSError?
        var inputConsumed = false
        let status = converter.convert(to: outputBuffer, error: &conversionError) { _, outStatus in
            if inputConsumed {
                outStatus.pointee = .noDataNow
                return nil
            }
            inputConsumed = true
            outStatus.pointee = .haveData
            return buffer
        }

        guard status != .error, conversionError == nil, let channelData = outputBuffer.floatChannelData else {
            return
        }

        let frameCount = Int(outputBuffer.frameLength)
        let samples = Array(UnsafeBufferPointer(start: channelData[0], count: frameCount))

        ringBuffer.append(samples)

        let rms = Self.computeRMS(samples)
        DispatchQueue.main.async { [weak self] in
            self?.inputLevel = rms
        }

        onSamples?(samples)
    }

    private static func computeRMS(_ samples: [Float]) -> Float {
        guard !samples.isEmpty else { return 0 }
        let sumSquares = samples.reduce(Float(0)) { $0 + $1 * $1 }
        return sqrt(sumSquares / Float(samples.count))
    }

    // MARK: - Interruptions

    private func registerNotifications() {
        NotificationCenter.default.addObserver(
            self, selector: #selector(handleInterruption),
            name: AVAudioSession.interruptionNotification, object: nil
        )
        NotificationCenter.default.addObserver(
            self, selector: #selector(handleRouteChange),
            name: AVAudioSession.routeChangeNotification, object: nil
        )
    }

    private func unregisterNotifications() {
        NotificationCenter.default.removeObserver(self, name: AVAudioSession.interruptionNotification, object: nil)
        NotificationCenter.default.removeObserver(self, name: AVAudioSession.routeChangeNotification, object: nil)
    }

    @objc private func handleInterruption(_ notification: Notification) {
        guard let info = notification.userInfo,
              let typeValue = info[AVAudioSessionInterruptionTypeKey] as? UInt,
              let type = AVAudioSession.InterruptionType(rawValue: typeValue)
        else { return }

        switch type {
        case .began:
            engine.stop()
            DispatchQueue.main.async { self.isRunning = false }
        case .ended:
            shouldResumeAfterInterruption = false
            if let optionsValue = info[AVAudioSessionInterruptionOptionKey] as? UInt {
                shouldResumeAfterInterruption = AVAudioSession.InterruptionOptions(rawValue: optionsValue)
                    .contains(.shouldResume)
            }
            guard shouldResumeAfterInterruption else { return }
            do {
                try AVAudioSession.sharedInstance().setActive(true)
                try engine.start()
                DispatchQueue.main.async { self.isRunning = true }
            } catch {
                DispatchQueue.main.async { self.isRunning = false }
            }
        @unknown default:
            break
        }
    }

    @objc private func handleRouteChange(_ notification: Notification) {
        guard let info = notification.userInfo,
              let reasonValue = info[AVAudioSessionRouteChangeReasonKey] as? UInt,
              AVAudioSession.RouteChangeReason(rawValue: reasonValue) == .oldDeviceUnavailable
        else { return }

        // The previous input route (e.g. an external mic) disappeared. The
        // built-in mic is normally still available and AVAudioEngine
        // recovers on its own; this is a hook for future refinement once
        // validated on real devices with external inputs.
    }
}
