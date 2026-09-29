find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(edgar_tests edgar_tests.cpp)
target_link_libraries(edgar_tests PRIVATE edgar GTest::gtest_main)
gtest_discover_tests(edgar_tests)

add_executable(edgar_parity_tests edgar_parity_tests.cpp)
target_link_libraries(edgar_parity_tests PRIVATE edgar GTest::gtest_main)
gtest_discover_tests(edgar_parity_tests)

find_package(yaml-cpp CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
add_executable(preset_loader_tests preset_loader_tests.cpp ../apps/main/preset_loader.cpp)
target_include_directories(preset_loader_tests PRIVATE "${CMAKE_SOURCE_DIR}/src/apps/main")
target_link_libraries(preset_loader_tests PRIVATE edgar yaml-cpp::yaml-cpp GTest::gtest_main)
gtest_discover_tests(preset_loader_tests)

add_executable(generation_diagnostic_test generation_diagnostic_test.cpp ../apps/main/preset_loader.cpp)
target_include_directories(generation_diagnostic_test PRIVATE "${CMAKE_SOURCE_DIR}/src/apps/main")
target_link_libraries(generation_diagnostic_test PRIVATE edgar yaml-cpp::yaml-cpp spdlog::spdlog GTest::gtest_main)
gtest_discover_tests(generation_diagnostic_test)

# Golden parity with the reference C# (logical comparison; see test_data/parity/README.md)
find_package(nlohmann_json CONFIG REQUIRED)
add_executable(parity_golden_test parity_golden_test.cpp)
target_link_libraries(parity_golden_test PRIVATE edgar nlohmann_json::nlohmann_json GTest::gtest_main)
target_compile_definitions(parity_golden_test PRIVATE PARITY_DATA_DIR="${CMAKE_SOURCE_DIR}/test_data/parity")
gtest_discover_tests(parity_golden_test)

# Layout generation benchmark (cross-platform replacement for tools/benchmark_layout_generation.ps1)
add_executable(benchmark_layout benchmark_layout.cpp ../apps/main/preset_loader.cpp)
target_include_directories(benchmark_layout PRIVATE "${CMAKE_SOURCE_DIR}/src/apps/main")
target_link_libraries(benchmark_layout PRIVATE edgar yaml-cpp::yaml-cpp nlohmann_json::nlohmann_json)
# Smoke gate with a generous threshold; finer thresholds via tools/benchmark_layout_generation.py
# NOTE: dense maps (9/17/41vertices, corridors) do not converge in reasonable time yet —
# tracked as a separate performance investigation; see docs/parity_next_steps_plan.md (этап H).
add_test(NAME BenchmarkLayout.Smoke COMMAND benchmark_layout --map tutorial_basic.yml --iterations 5 --threshold-ms 30000)
