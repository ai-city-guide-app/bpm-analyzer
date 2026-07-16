import Foundation
import SwiftData

/// A completed BPM measurement, persisted locally for the history list.
///
/// Named `BpmMeasurement` rather than `Measurement` deliberately:
/// `Foundation.Measurement<UnitType>` already exists and is imported
/// automatically alongside SwiftUI, so `Measurement` here would risk
/// "ambiguous type" resolution errors with `@Query`/`.modelContainer`.
@Model
final class BpmMeasurement {
    var date: Date
    var bpm: Double
    var confidence: Float

    init(date: Date = .now, bpm: Double, confidence: Float) {
        self.date = date
        self.bpm = bpm
        self.confidence = confidence
    }
}
