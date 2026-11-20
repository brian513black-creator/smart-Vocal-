cmake_minimum_required(VERSION 3.22)
project(BlackieVocalBox VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)
include(CTest)
option(BVB_BUILD_PLUGIN "Build the Blackie Vocal Box VST3" ${WIN32})

FetchContent_Declare(rnnoise
    GIT_REPOSITORY https://github.com/xiph/rnnoise.git
    GIT_TAG cdf196b1e9de2f8ff1003328ebf9a4316477429d)
FetchContent_MakeAvailable(rnnoise)
add_library(bvb_rnnoise STATIC)
foreach(source denoise rnn rnn_data pitch kiss_fft celt_lpc)
    target_sources(bvb_rnnoise PRIVATE "${rnnoise_SOURCE_DIR}/src/${source}.c")
endforeach()
target_include_directories(bvb_rnnoise PUBLIC "${rnnoise_SOURCE_DIR}/include")
target_compile_definitions(bvb_rnnoise PRIVATE _USE_MATH_DEFINES)
set_target_properties(bvb_rnnoise PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 11 C_STANDARD_REQUIRED ON)

add_library(bvb_dsp STATIC src/ReferenceEq.cpp src/VocalCleanup.cpp)
set_target_properties(bvb_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(bvb_dsp PUBLIC src)
target_compile_features(bvb_dsp PUBLIC cxx_std_17)
target_link_libraries(bvb_dsp PUBLIC bvb_rnnoise)

if(BUILD_TESTING)
    add_executable(bvb_dsp_test tests/VocalCleanupTest.cpp)
    target_link_libraries(bvb_dsp_test PRIVATE bvb_dsp)
    add_test(NAME vocal_cleanup COMMAND bvb_dsp_test)

    add_executable(bvb_reference_eq_test tests/ReferenceEqTest.cpp)
    target_link_libraries(bvb_reference_eq_test PRIVATE bvb_dsp)
    add_test(NAME reference_eq COMMAND bvb_reference_eq_test)
endif()

if(BVB_BUILD_PLUGIN)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG d6181bde38d858c283c3b7bf699ce6340c050b5d)
    FetchContent_MakeAvailable(juce)
    juce_add_plugin(BlackieVocalBox
        COMPANY_NAME "Blackie Vocal Box"
        BUNDLE_ID com.blackievocalbox.plugin
        PLUGIN_MANUFACTURER_CODE Blkv
        PLUGIN_CODE Bvbx
        FORMATS VST3
        PRODUCT_NAME "Blackie Vocal Box")
    target_sources(BlackieVocalBox PRIVATE src/PluginProcessor.cpp)
    target_compile_definitions(BlackieVocalBox PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(BlackieVocalBox PRIVATE bvb_dsp juce::juce_audio_processors)

    get_target_property(vst3_bundle BlackieVocalBox_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
    add_custom_command(TARGET BlackieVocalBox_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${vst3_bundle}/Contents/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${rnnoise_SOURCE_DIR}/COPYING"
            "${vst3_bundle}/Contents/Resources/RNNoise-LICENSE.txt"
        VERBATIM)
endif()
