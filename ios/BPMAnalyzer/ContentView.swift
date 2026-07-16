import SwiftUI
import BpmCoreKit

/// Owns the BpmEngine instance and the serial queue that all access to it
/// must go through (see Stage 2 plan's thread-safety section): bpmcore's
/// BpmAnalyzer has no internal synchronization, so feed() (called from the
/// mic's audio thread) and current() (polled from a main-thread timer) must
/// never run concurrently.
final class BpmSession: ObservableObject {
    let engine = BpmEngine()
    let queue = DispatchQueue(label: "bpm.engine.serial")
}

struct ContentView: View {
    @State private var selfTestText: String = ""
    @State private var isSelfTestRunning = false

    @StateObject private var micCapture = MicCapture()
    @StateObject private var session = BpmSession()
    @State private var micResultText: String = ""
    @State private var pollTimer: Timer?

    var body: some View {
        VStack(spacing: 20) {
            Text("BPM Analyzer")
                .font(.largeTitle)
                .bold()

            Button(isSelfTestRunning ? "Running..." : "Run bpmcore self-test") {
                runSelfTest()
            }
            .disabled(isSelfTestRunning)
            .buttonStyle(.borderedProminent)

            if !selfTestText.isEmpty {
                Text(selfTestText)
                    .font(.title2)
                    .monospacedDigit()
            }

            Divider()

            Button(micCapture.isRunning ? "Stop mic capture" : "Start mic capture (Stage 2 test)") {
                if micCapture.isRunning {
                    stopMicTest()
                } else {
                    startMicTest()
                }
            }
            .buttonStyle(.bordered)

            if micCapture.isRunning {
                ProgressView(value: min(Double(micCapture.inputLevel) * 5.0, 1.0))
                    .frame(width: 200)
            }

            if !micResultText.isEmpty {
                Text(micResultText)
                    .font(.title2)
                    .monospacedDigit()
            }
        }
        .padding()
        .onDisappear {
            stopMicTest()
        }
    }

    private func runSelfTest() {
        isSelfTestRunning = true
        selfTestText = ""
        Task {
            let result = await Task.detached {
                BpmEngine.selfTest()
            }.value
            await MainActor.run {
                if result.ready {
                    selfTestText = String(format: "%.2f BPM, confidence %.2f", result.bpm, result.confidence)
                } else {
                    selfTestText = "not ready (unexpected)"
                }
                isSelfTestRunning = false
            }
        }
    }

    private func startMicTest() {
        micResultText = "requesting permission..."
        MicCapture.requestPermission { granted in
            guard granted else {
                self.micResultText = "Microphone permission denied"
                return
            }

            self.session.queue.async {
                self.session.engine.reset()
            }

            self.micCapture.onSamples = { samples in
                self.session.queue.async {
                    self.session.engine.feed(samples)
                }
            }

            do {
                try self.micCapture.start()
                self.micResultText = "listening..."
                self.startPolling()
            } catch {
                self.micResultText = "Mic start failed: \(error)"
            }
        }
    }

    private func stopMicTest() {
        micCapture.stop()
        pollTimer?.invalidate()
        pollTimer = nil
    }

    private func startPolling() {
        pollTimer?.invalidate()
        pollTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { _ in
            session.queue.async {
                let result = session.engine.current()
                DispatchQueue.main.async {
                    if result.ready {
                        micResultText = String(format: "%.2f BPM, confidence %.2f", result.bpm, result.confidence)
                    } else {
                        micResultText = "listening..."
                    }
                }
            }
        }
    }
}

#Preview {
    ContentView()
}
