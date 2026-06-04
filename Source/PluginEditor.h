#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
// Shared mutable colour table — all components read from here so a colour
// change is reflected everywhere immediately on the next repaint/refresh.
struct PlayheadColours
{
    static PlayheadColours& get()
    {
        static PlayheadColours instance;
        return instance;
    }

    juce::Colour colour (int ph) const
    {
	    return colours[static_cast<std::size_t>(juce::jlimit(0, NUM_PLAYHEADS - 1, ph))];
    }

    void set (int ph, juce::Colour c)
    {
	    colours[static_cast<std::size_t>(juce::jlimit (0, NUM_PLAYHEADS - 1, ph))] = c;
    }

private:
    // Default palette
    std::array<juce::Colour, NUM_PLAYHEADS> colours {
        juce::Colour (0xFFE94560u),  // coral red
        juce::Colour (0xFF00B4D8u),  // cyan
        juce::Colour (0xFF90E0EFu),  // light blue
        juce::Colour (0xFFFFB703u),  // amber
    };
};

// Convenience free function
inline juce::Colour playheadColour (int ph)
{
    return PlayheadColours::get().colour (ph);
}
inline juce::Colour playheadColour (std::size_t ph)
{
    return playheadColour ((int) ph);
}

//==============================================================================
class PitchStepComponent : public juce::Component
{
public:
    PitchStepComponent (int index, juce::AudioProcessorValueTreeState& apvts);
    ~PitchStepComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
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
class RhythmSlotComponent : public juce::Component
{
public:
    RhythmSlotComponent (int slotIndex, juce::AudioProcessorValueTreeState& apvts);
    ~RhythmSlotComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshButtons();   // sync button colours from APVTS + PlayheadColours

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
class PlayheadRowComponent : public juce::Component
{
public:
    PlayheadRowComponent (int phIndex,
                          juce::AudioProcessorValueTreeState& apvts,
                          std::function<void(int)> colourButtonClicked);
    ~PlayheadRowComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setCurrentStep (int step, int numSteps);
    // Call after a colour change to re-tint all child controls
    void refreshColour();

private:
    int phIndex;
    int currentStep { 0 };
    int activeSteps { 8 };

    juce::TextButton   colourButton;   // click to open colour picker
    juce::Label        nameLabel;
    juce::ToggleButton activeButton;
    juce::Slider       stepsSlider;
    juce::Slider       volumeSlider;
    juce::Slider       subharmonicKnob;
    juce::Label        stepsLabel;
    juce::Label        volLabel;
    juce::Label        subLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> activeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stepsAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> subAttach;

    std::array<juce::Rectangle<float>, MAX_STEPS> ledRects;

    void applyColourToControls (juce::Colour col);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayheadRowComponent)
};

//==============================================================================
class MultiheadSequencerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                private juce::Timer
{
public:
    explicit MultiheadSequencerAudioProcessorEditor (MultiheadSequencerAudioProcessor&);
    ~MultiheadSequencerAudioProcessorEditor() override;

    void onColourChanged (int playheadIndex, juce::Colour newColour);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void openColourPicker (int playheadIndex);

    MultiheadSequencerAudioProcessor& processorRef;

    juce::Label      titleLabel;
    juce::Slider     bpmSlider;
    juce::Label      bpmLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttach;

    juce::TextButton collisionButton;
    juce::Label      collisionLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> collisionAttach;

    std::array<std::unique_ptr<PitchStepComponent>,   NUM_PITCHES>   pitchSteps;
    std::array<std::unique_ptr<RhythmSlotComponent>,  NUM_RHYTHMS>   rhythmSlots;
    std::array<std::unique_ptr<PlayheadRowComponent>, NUM_PLAYHEADS> playheadRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiheadSequencerAudioProcessorEditor)
};
