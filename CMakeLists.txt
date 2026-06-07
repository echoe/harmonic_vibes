cmake_minimum_required(VERSION 3.22)
project(HarmonicVibes VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-flto=auto)
add_link_options(-flto=auto)

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
    GIT_TAG        8.0.13          # pin to a stable release
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# -----------------------------------------------------------------------
# 2. Plugin target
# -----------------------------------------------------------------------
juce_add_plugin(HarmonicVibes
    COMPANY_NAME            "echoe"
    PLUGIN_MANUFACTURER_CODE Ynme
    PLUGIN_CODE             Mhsq
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "HarmonicVibes"
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
target_sources(HarmonicVibes
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

# -----------------------------------------------------------------------
# 4. JUCE modules
# -----------------------------------------------------------------------
target_compile_definitions(HarmonicVibes
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
	JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
)

target_link_libraries(HarmonicVibes
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
