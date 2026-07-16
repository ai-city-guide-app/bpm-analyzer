import SwiftUI
import BpmCoreKit

struct ContentView: View {
    @State private var resultText: String = ""
    @State private var isRunning = false

    var body: some View {
        VStack(spacing: 20) {
            Text("BPM Analyzer")
                .font(.largeTitle)
                .bold()

            Button(isRunning ? "Running..." : "Run bpmcore self-test") {
                runSelfTest()
            }
            .disabled(isRunning)
            .buttonStyle(.borderedProminent)

            if !resultText.isEmpty {
                Text(resultText)
                    .font(.title2)
                    .monospacedDigit()
            }
        }
        .padding()
    }

    private func runSelfTest() {
        isRunning = true
        resultText = ""
        Task {
            let result = await Task.detached {
                BpmEngine.selfTest()
            }.value
            await MainActor.run {
                if result.ready {
                    resultText = String(format: "%.2f BPM, confidence %.2f", result.bpm, result.confidence)
                } else {
                    resultText = "not ready (unexpected)"
                }
                isRunning = false
            }
        }
    }
}

#Preview {
    ContentView()
}
