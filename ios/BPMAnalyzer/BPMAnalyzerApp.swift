import SwiftUI
import SwiftData

@main
struct BPMAnalyzerApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
        .modelContainer(for: BpmMeasurement.self)
    }
}
