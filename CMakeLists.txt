cmake_minimum_required(VERSION 3.22)

project(CUBES VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.13
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(CUBES
    VERSION 1.0.0
    COMPANY_NAME "yourcompany"
    COMPANY_WEBSITE "www.yourcompany.com"
    BUNDLE_ID "com.yourcompany.Cubes"
    PLUGIN_NAME "Cubes"
    DESCRIPTION "Cubes"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Raej
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS AU VST3
    PRODUCT_NAME "Cubes"
)

juce_generate_juce_header(CUBES)

target_sources(CUBES PRIVATE
    Source/Branding.cpp
    Source/Branding.h
    Source/PitchResampler.cpp
    Source/PitchResampler.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/RecordingPanel.cpp
    Source/RecordingPanel.h
    Source/SketchyControls.cpp
    Source/SketchyControls.h
    Source/SpectralResampler.cpp
    Source/SpectralResampler.h
)

target_compile_features(CUBES PRIVATE cxx_std_17)

target_compile_definitions(CUBES PUBLIC
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(CUBES PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
