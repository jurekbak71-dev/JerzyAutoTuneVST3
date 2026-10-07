cmake_minimum_required(VERSION 3.24)
project(JerzyAutoTune VERSION 1.5.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

FetchContent_Declare(signalsmith-stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG a670068d9aeb64913331d5cc29337b19a457a7df)
FetchContent_MakeAvailable(signalsmith-stretch)

juce_add_plugin(JerzyAutoTune
    COMPANY_NAME "Jerzy Audio"
    BUNDLE_ID "audio.jerzy.autotune.v14"
    PLUGIN_MANUFACTURER_CODE JzAu
    PLUGIN_CODE JzB4
    FORMATS VST3
    PRODUCT_NAME "JERZY AUTO TUNE")

juce_generate_juce_header(JerzyAutoTune)
target_sources(JerzyAutoTune PRIVATE
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h)
target_compile_features(JerzyAutoTune PRIVATE cxx_std_17)
target_compile_options(JerzyAutoTune PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)
target_compile_definitions(JerzyAutoTune PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(JerzyAutoTune PRIVATE
    juce::juce_audio_utils juce::juce_dsp
    signalsmith-stretch
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)

include(CTest)
if(BUILD_TESTING)
    juce_add_console_app(JerzyAutoTuneTests PRODUCT_NAME "Auto Tune regression tests")
    juce_generate_juce_header(JerzyAutoTuneTests)
    target_sources(JerzyAutoTuneTests PRIVATE tests/autotune_test.cpp)
    target_compile_features(JerzyAutoTuneTests PRIVATE cxx_std_17)
    target_include_directories(JerzyAutoTuneTests PRIVATE Source)
    target_link_libraries(JerzyAutoTuneTests PRIVATE JerzyAutoTune juce::juce_audio_utils juce::juce_dsp signalsmith-stretch)
    add_test(NAME autotune_gui_and_parameters COMMAND JerzyAutoTuneTests)
endif()
