cmake_minimum_required(VERSION 3.22)
project(MultiheadSequencer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------
# 1. Find or fetch JUCE
# -----------------------------------------------------------------------
# Option A (recommended): clone JUCE alongside this folder and point here:
#   git clone https://github.com/juce-framework/JUCE.git ../JUCE
# Option B: use FetchContent (downloads at configure time ~200 MB)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9          # pin to a stable release
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# -----------------------------------------------------------------------
# 2. Plugin target
# -----------------------------------------------------------------------
juce_add_plugin(MultiheadSequencer
    COMPANY_NAME            "YourName"
    PLUGIN_MANUFACTURER_CODE Ynme
    PLUGIN_CODE             Mhsq
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "Multihead Sequencer"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       TRUE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES         "Instrument" "Sequencer"
    AU_MAIN_TYPE            kAudioUnitType_MusicDevice
)

# -----------------------------------------------------------------------
# 3. Source files
# -----------------------------------------------------------------------
target_sources(MultiheadSequencer
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

# -----------------------------------------------------------------------
# 4. JUCE modules
# -----------------------------------------------------------------------
target_compile_definitions(MultiheadSequencer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
	JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
)

target_link_libraries(MultiheadSequencer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
