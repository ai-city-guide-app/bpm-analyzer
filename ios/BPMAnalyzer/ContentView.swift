import SwiftUI
import SwiftData
import UIKit
import BpmCoreKit

/// Owns the BpmEngine instance and the serial queue that all access to it
/// must go through: bpmcore's BpmAnalyzer has no internal synchronization,
/// so feed() (called from the mic's audio thread) and current() (polled
/// from a main-thread timer) must never run concurrently.
final class BpmSession: ObservableObject {
    let engine = BpmEngine()
    let queue = DispatchQueue(label: "bpm.engine.serial")
}

enum RecordingState {
    case idle
    case listening
    case finished
    case permissionDenied
}

struct ContentView: View {
    @Environment(\.modelContext) private var modelContext
    @Query(sort: \BpmMeasurement.date, order: .reverse) private var history: [BpmMeasurement]

    @StateObject private var micCapture = MicCapture()
    @StateObject private var session = BpmSession()

    @State private var state: RecordingState = .idle
    @State private var lastResult: BpmResult?
    @State private var startTime: Date?
    @State private var elapsed: TimeInterval = 0
    @State private var pollTimer: Timer?

    private let targetDuration: TimeInterval = 20.0

    var body: some View {
        VStack(spacing: 24) {
            Text("BPM Analyzer")
                .font(.largeTitle)
                .bold()

            RecordingRingView(progress: elapsed / targetDuration) {
                ringContent
            }
            .onTapGesture {
                handleTap()
            }

            statusView

            if !history.isEmpty {
                VStack(alignment: .leading, spacing: 8) {
                    Text("История")
                        .font(.headline)
                    ForEach(history.prefix(5)) { measurement in
                        HStack {
                            Text(measurement.date, style: .time)
                            Spacer()
                            Text(String(format: "%.2f BPM", measurement.bpm))
                            Text(String(format: "%.0f%%", measurement.confidence * 100))
                                .foregroundStyle(.secondary)
                        }
                        .font(.subheadline)
                    }
                }
                .padding(.horizontal)
            }
        }
        .padding()
        .onDisappear {
            micCapture.stop()
            pollTimer?.invalidate()
        }
    }

    @ViewBuilder
    private var ringContent: some View {
        switch state {
        case .idle:
            Text("Нажмите, чтобы начать")
        case .listening:
            if let lastResult, lastResult.ready {
                bpmText(lastResult)
            } else {
                Text("Слушаю…")
            }
        case .finished:
            if let lastResult, lastResult.ready {
                bpmText(lastResult)
            } else {
                Text("Не удалось определить темп")
                    .font(.callout)
            }
        case .permissionDenied:
            Text("Нет доступа к микрофону")
                .font(.callout)
        }
    }

    private func bpmText(_ result: BpmResult) -> some View {
        VStack(spacing: 4) {
            Text(String(format: "%.2f", result.bpm))
                .font(.system(size: 48, weight: .bold))
                .monospacedDigit()
            Text("confidence \(Int(result.confidence * 100))%")
                .font(.subheadline)
                .foregroundStyle(.secondary)
        }
    }

    @ViewBuilder
    private var statusView: some View {
        switch state {
        case .idle:
            Text("Нажмите на круг, чтобы начать запись (20 сек)")
                .foregroundStyle(.secondary)
        case .listening:
            Text("Идёт запись… нажмите, чтобы остановить")
                .foregroundStyle(.secondary)
        case .finished:
            Button("Начать заново") {
                startRecording()
            }
            .buttonStyle(.borderedProminent)
        case .permissionDenied:
            Button("Открыть настройки") {
                if let url = URL(string: UIApplication.openSettingsURLString) {
                    UIApplication.shared.open(url)
                }
            }
            .buttonStyle(.borderedProminent)
        }
    }

    private func handleTap() {
        switch state {
        case .idle, .finished:
            startRecording()
        case .listening:
            finishRecording()
        case .permissionDenied:
            break
        }
    }

    private func startRecording() {
        MicCapture.requestPermission { granted in
            guard granted else {
                state = .permissionDenied
                return
            }

            lastResult = nil
            elapsed = 0
            startTime = Date()

            session.queue.async {
                session.engine.reset()
            }

            micCapture.onSamples = { samples in
                session.queue.async {
                    session.engine.feed(samples)
                }
            }

            do {
                try micCapture.start()
                state = .listening
                startPolling()
            } catch {
                lastResult = nil
                state = .finished
            }
        }
    }

    /// Stops the mic (explicitly — otherwise it, and the status-bar
    /// recording indicator, would stay active while in `.finished`), saves
    /// a history entry only if a result was actually reached, and shows the
    /// appropriate `.finished` sub-state (with vs. without a result).
    private func finishRecording() {
        micCapture.stop()
        pollTimer?.invalidate()
        pollTimer = nil

        if let lastResult, lastResult.ready {
            modelContext.insert(BpmMeasurement(bpm: lastResult.bpm, confidence: lastResult.confidence))
        }

        state = .finished
    }

    private func startPolling() {
        pollTimer?.invalidate()
        pollTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { _ in
            if let startTime {
                elapsed = Date().timeIntervalSince(startTime)
            }

            session.queue.async {
                let result = session.engine.current()
                DispatchQueue.main.async {
                    lastResult = result
                    if elapsed >= targetDuration {
                        finishRecording()
                    }
                }
            }
        }
    }
}

#Preview {
    ContentView()
        .modelContainer(for: BpmMeasurement.self, inMemory: true)
}
