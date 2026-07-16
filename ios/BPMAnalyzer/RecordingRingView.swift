import SwiftUI

/// A circular recording-progress ring with arbitrary content (status text or
/// the live BPM reading) centered inside it. `progress` is expected in
/// 0...1; updates are animated linearly since callers only update it on a
/// discrete (roughly once-per-second) cadence.
struct RecordingRingView<Content: View>: View {
    let progress: Double
    @ViewBuilder let content: () -> Content

    var body: some View {
        ZStack {
            Circle()
                .stroke(Color.secondary.opacity(0.2), lineWidth: 12)

            Circle()
                .trim(from: 0, to: max(0, min(progress, 1)))
                .stroke(Color.accentColor, style: StrokeStyle(lineWidth: 12, lineCap: .round))
                .rotationEffect(.degrees(-90))
                .animation(.linear(duration: 1), value: progress)

            content()
                .multilineTextAlignment(.center)
                .padding()
        }
        .frame(width: 240, height: 240)
    }
}

#Preview {
    RecordingRingView(progress: 0.4) {
        Text("Слушаю…")
    }
}
