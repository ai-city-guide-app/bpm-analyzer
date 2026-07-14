include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.4
)
FetchContent_MakeAvailable(Catch2)

add_executable(bpmcore_tests
    test_onset_detector.cpp
    test_tempo_estimator.cpp
    test_beat_tracker.cpp
    test_bpm_analyzer_synthetic.cpp
)
target_link_libraries(bpmcore_tests PRIVATE bpmcore Catch2::Catch2WithMain)
target_compile_definitions(bpmcore_tests PRIVATE
    BPMCORE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

include(CTest)

# Regenerate the synthetic click-track fixtures before running the suite,
# rather than committing ~20MB of generated WAVs to the repo. Generation is
# deterministic (fixed PRNG seed), so this is reproducible and cheap in CI.
add_test(NAME generate_synthetic_fixtures
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/generate_click_tracks.py
            --out-dir ${CMAKE_CURRENT_SOURCE_DIR}/data/synthetic
)
set_tests_properties(generate_synthetic_fixtures PROPERTIES FIXTURES_SETUP synthetic_fixtures)

add_test(NAME bpmcore_tests COMMAND bpmcore_tests)
set_tests_properties(bpmcore_tests PROPERTIES FIXTURES_REQUIRED synthetic_fixtures)
