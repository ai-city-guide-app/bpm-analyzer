// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "BpmCoreKit",
    platforms: [.iOS(.v16)],
    products: [
        .library(name: "BpmCoreKit", targets: ["BpmCoreKit"])
    ],
    targets: [
        .target(
            name: "CBpmCore",
            path: "Sources/CBpmCore",
            cSettings: [
                .headerSearchPath("include")
            ],
            cxxSettings: [
                .headerSearchPath("include")
            ]
        ),
        .target(
            name: "BpmCoreKit",
            dependencies: ["CBpmCore"],
            path: "Sources/BpmCoreKit",
            swiftSettings: [
                .interoperabilityMode(.Cxx)
            ]
        )
    ],
    cLanguageStandard: .c99,
    cxxLanguageStandard: .cxx17
)
