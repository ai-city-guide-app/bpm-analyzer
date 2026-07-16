add_library(kissfft STATIC
    third_party/kissfft/kiss_fft.c
    third_party/kissfft/kiss_fftr.c
)
target_include_directories(kissfft PUBLIC third_party/kissfft)
target_compile_definitions(kissfft PUBLIC kiss_fft_scalar=float)
set_target_properties(kissfft PROPERTIES C_STANDARD 99)

add_library(bpmcore STATIC
    src/Wav.cpp
    src/OnsetDetector.cpp
    src/TempoEstimator.cpp
    src/BeatTracker.cpp
    src/BpmAnalyzer.cpp
)
target_include_directories(bpmcore PUBLIC include)
target_link_libraries(bpmcore PUBLIC kissfft)
target_compile_features(bpmcore PUBLIC cxx_std_17)

add_executable(bpmcli tools/bpmcli.cpp)
target_link_libraries(bpmcli PRIVATE bpmcore)

if(BPMCORE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
