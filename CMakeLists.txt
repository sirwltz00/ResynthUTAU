cmake_minimum_required(VERSION 3.20)
project(ResynthUTAU VERSION 0.2.0 LANGUAGES CXX)

option(RESYNTHUTAU_BUILD_TESTS "Build the automated test suite" ON)

add_library(resynth_core
    src/AudioBuffer.cpp
    src/FFT.cpp
    src/PitchAnalyzer.cpp
    src/HarmonicAnalyzer.cpp
    src/SpectralEnvelope.cpp
    src/NoiseAnalyzer.cpp
    src/AnalysisEngine.cpp
    src/WavReader.cpp
    src/WavWriter.cpp
    src/ParameterParser.cpp
    src/UTAUInterface.cpp
    src/ResynthesisEngine.cpp
)
target_include_directories(resynth_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(resynth_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(resynth_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(resynth_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(ResynthUTAU src/main.cpp)
target_link_libraries(ResynthUTAU PRIVATE resynth_core)
set_target_properties(ResynthUTAU PROPERTIES OUTPUT_NAME "ResynthUTAU")
if(WIN32)
    target_link_libraries(ResynthUTAU PRIVATE shell32)
endif()

if(RESYNTHUTAU_BUILD_TESTS)
    enable_testing()
    add_executable(resynth_tests tests/test_main.cpp)
    target_link_libraries(resynth_tests PRIVATE resynth_core)
    add_test(NAME resynth_tests COMMAND resynth_tests)
endif()
