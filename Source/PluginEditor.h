#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
inline juce::Colour playheadColour (std::size_t i)
{
    constexpr std::array<juce::uint32, 4> vals {
        0xFFE94560u,  // coral red
        0xFF00B4D8u,  // cyan
        0xFF90E0EFu,  // light blue
        0xFFFFB703u,  // amber
    };
    return juce::Colour (vals[i % 4]);
}

//==============================================================================
// Pitch step: shows up to 4 coloured dots for each playhead currently on it
class PitchStepComponent : public juce::Component
{
public:
    PitchStepComponent (int index, juce::AudioProcessorValueTreeState& apvts);
    ~PitchStepComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // activePlayheads: bitmask of which playheads are currently on this step
    void setActivePlayheads (int bitmask);

private:
    int stepIndex;
    int activePlayheadMask { 0 };

    juce::Slider pitchSlider;
    juce::Label  indexLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchStepComponent)
};

//==============================================================================
// Rhythm slot: knob + 4 toggle buttons (any combination allowed)
class RhythmSlotComponent : public juce::Component
{
public:
    RhythmSlotComponent (int slotIndex,
                         juce::AudioProcessorValueTreeState& apvts);
    ~RhythmSlotComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Refresh toggle states from APVTS (call from timer)
    void refreshButtons();

private:
    int slotIndex;
    juce::AudioProcessorValueTreeState& apvts;

    juce::Label  slotLabel;
    juce::Slider rhythmKnob;
    std::array<juce::TextButton, NUM_PLAYHEADS> phButtons;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rhythmAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RhythmSlotComponent)
};

//==============================================================================
// Playhead row: LEDs + steps + volume + subharmonic knob (PH2–4 only)
class PlayheadRowComponent : public juce::Component
{
public:
    PlayheadRowComponent (int phIndex, juce::AudioProcessorValueTreeState& apvts);
    ~PlayheadRowComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setCurrentStep (int step, int numSteps);

private:
    int phIndex;
    int currentStep { 0 };
    int activeSteps { 8 };

    juce::Label        nameLabel;
    juce::ToggleButton activeButton;
    juce::Slider       stepsSlider;
    juce::Slider       volumeSlider;
    juce::Slider       subharmonicKnob;  // hidden for PH1 (phIndex == 0)
    juce::Label        stepsLabel;
    juce::Label        volLabel;
    juce::Label        subLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> activeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stepsAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subAttach;

    std::array<juce::Rectangle<float>, MAX_STEPS> ledRects;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayheadRowComponent)
};

//==============================================================================
class MultiheadSequencerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                private juce::Timer
{
public:
    explicit MultiheadSequencerAudioProcessorEditor (MultiheadSequencerAudioProcessor&);
    ~MultiheadSequencerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MultiheadSequencerAudioProcessor& processorRef;

    juce::Label   titleLabel;

    juce::Slider  bpmSlider;
    juce::Label   bpmLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttach;

    juce::TextButton collisionButton;
    juce::Label      collisionLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> collisionAttach;

    std::array<std::unique_ptr<PitchStepComponent>,   NUM_PITCHES>   pitchSteps;
    std::array<std::unique_ptr<RhythmSlotComponent>,  NUM_RHYTHMS>   rhythmSlots;
    std::array<std::unique_ptr<PlayheadRowComponent>, NUM_PLAYHEADS> playheadRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiheadSequencerAudioProcessorEditor)
};
