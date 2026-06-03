#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
static const juce::Colour kBackground { 0xFF1A1A2E };
static const juce::Colour kSurface    { 0xFF16213E };
static const juce::Colour kAccent     { 0xFF0F3460 };
static const juce::Colour kHighlight  { 0xFFE94560 };
static const juce::Colour kText       { 0xFFEEEEEE };
static const juce::Colour kDim        { 0xFF888899 };

static void styleRotary (juce::Slider& s, juce::Colour fill)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setColour (juce::Slider::rotarySliderFillColourId,    fill);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, kAccent);
    s.setColour (juce::Slider::thumbColourId,               kText);
    s.setColour (juce::Slider::textBoxTextColourId,         kText);
    s.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
}

static void styleHorizSlider (juce::Slider& s, juce::Colour col)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setColour (juce::Slider::trackColourId,               col.withAlpha (0.8f));
    s.setColour (juce::Slider::thumbColourId,               col);
    s.setColour (juce::Slider::backgroundColourId,          kAccent);
    s.setColour (juce::Slider::textBoxTextColourId,         kText);
    s.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
}

//==============================================================================
// PitchStepComponent
//==============================================================================
PitchStepComponent::PitchStepComponent (int index, juce::AudioProcessorValueTreeState& apvts)
    : stepIndex (index)
{
    indexLabel.setText (juce::String (index + 1), juce::dontSendNotification);
    indexLabel.setFont (juce::Font (11.f, juce::Font::bold));
    indexLabel.setColour (juce::Label::textColourId, kDim);
    indexLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (indexLabel);

    pitchSlider.setSliderStyle (juce::Slider::LinearVertical);
    pitchSlider.setRange (0, 127, 1);
    pitchSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 36, 14);
    pitchSlider.setColour (juce::Slider::trackColourId,               kHighlight);
    pitchSlider.setColour (juce::Slider::thumbColourId,               kText);
    pitchSlider.setColour (juce::Slider::backgroundColourId,          kAccent);
    pitchSlider.setColour (juce::Slider::textBoxTextColourId,         kText);
    pitchSlider.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    pitchSlider.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    addAndMakeVisible (pitchSlider);

    pitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "pitch_" + juce::String (index), pitchSlider);
}

PitchStepComponent::~PitchStepComponent() {}

void PitchStepComponent::setActivePlayheads (int bitmask)
{
    if (activePlayheadMask != bitmask)
    {
        activePlayheadMask = bitmask;
        repaint();
    }
}

void PitchStepComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.f);

    // Background — tint if any playhead is here
    g.setColour (activePlayheadMask != 0 ? kSurface.brighter (0.08f) : kSurface);
    g.fillRoundedRectangle (b, 6.f);

    // Border — use the first active playhead's colour, or dim
    juce::Colour borderCol = kAccent;
    float borderW = 1.f;
    for (int ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        if (activePlayheadMask & (1 << ph))
        {
            borderCol = playheadColour ((std::size_t) ph);
            borderW   = 2.f;
            break;
        }
    }
    g.setColour (borderCol);
    g.drawRoundedRectangle (b, 6.f, borderW);

    // Draw one coloured dot per active playhead in the top-right corner area
    if (activePlayheadMask != 0)
    {
        const float dotR   = 4.5f;
        const float startX = b.getRight() - 6.f;
        const float dotY   = b.getY() + 8.f;
        int col = 0;
        for (int ph = NUM_PLAYHEADS - 1; ph >= 0; --ph)
        {
            if (activePlayheadMask & (1 << ph))
            {
                g.setColour (playheadColour ((std::size_t) ph));
                g.fillEllipse (startX - col * (dotR * 2.f + 2.f) - dotR,
                               dotY - dotR, dotR * 2.f, dotR * 2.f);
                ++col;
            }
        }
    }
}

void PitchStepComponent::resized()
{
    auto b = getLocalBounds().reduced (4);
    indexLabel.setBounds  (b.removeFromTop (16));
    pitchSlider.setBounds (b);
}

//==============================================================================
// RhythmSlotComponent
//==============================================================================
RhythmSlotComponent::RhythmSlotComponent (int idx,
                                           juce::AudioProcessorValueTreeState& apvts_)
    : slotIndex (idx), apvts (apvts_)
{
    slotLabel.setText ("R" + juce::String (idx + 1), juce::dontSendNotification);
    slotLabel.setFont (juce::Font (12.f, juce::Font::bold));
    slotLabel.setColour (juce::Label::textColourId, kDim);
    slotLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (slotLabel);

    styleRotary (rhythmKnob, kHighlight);
    rhythmKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 14);
    addAndMakeVisible (rhythmKnob);

    rhythmAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "rhythm_" + juce::String (idx), rhythmKnob);

    // Toggle buttons — directly wired to the bool param "ph{ph}_slot{slot}"
    for (int ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        auto& btn = phButtons[(std::size_t) ph];
        btn.setButtonText ("P" + juce::String (ph + 1));
        btn.setClickingTogglesState (true);
        btn.setColour (juce::TextButton::buttonColourId,   kAccent);
        btn.setColour (juce::TextButton::buttonOnColourId, playheadColour ((std::size_t) ph));
        btn.setColour (juce::TextButton::textColourOffId,  kText);
        btn.setColour (juce::TextButton::textColourOnId,   kBackground);
        btn.onClick = [this, ph]
        {
            const juce::String id = "ph" + juce::String (ph) + "_slot" + juce::String (slotIndex);
            if (auto* param = apvts.getParameter (id))
            {
                bool newState = phButtons[(std::size_t) ph].getToggleState();
                param->setValueNotifyingHost (newState ? 1.f : 0.f);
            }
        };
        addAndMakeVisible (btn);
    }
    refreshButtons();
}

RhythmSlotComponent::~RhythmSlotComponent() {}

void RhythmSlotComponent::refreshButtons()
{
    for (int ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        const juce::String id = "ph" + juce::String (ph) + "_slot" + juce::String (slotIndex);
        if (auto* raw = apvts.getRawParameterValue (id))
        {
            const bool on = raw->load() >= 0.5f;
            phButtons[(std::size_t) ph].setToggleState (on, juce::dontSendNotification);
            phButtons[(std::size_t) ph].setColour (juce::TextButton::buttonColourId,
                on ? playheadColour ((std::size_t) ph).withAlpha (0.85f) : kAccent);
        }
    }
}

void RhythmSlotComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.f);
    g.setColour (kSurface);
    g.fillRoundedRectangle (b, 8.f);
    g.setColour (kAccent);
    g.drawRoundedRectangle (b, 8.f, 1.f);
}

void RhythmSlotComponent::resized()
{
    auto b = getLocalBounds().reduced (6);
    slotLabel.setBounds (b.removeFromTop (16));
    b.removeFromTop (2);

    auto knobArea = b.removeFromTop (b.getHeight() - 30);
    rhythmKnob.setBounds (knobArea);

    b.removeFromTop (4);
    const int btnW = b.getWidth() / NUM_PLAYHEADS;
    for (int ph = 0; ph < NUM_PLAYHEADS; ++ph)
        phButtons[(std::size_t) ph].setBounds (
            b.withWidth (btnW).withX (b.getX() + ph * btnW).reduced (2, 0));
}

//==============================================================================
// PlayheadRowComponent
//==============================================================================
PlayheadRowComponent::PlayheadRowComponent (int idx, juce::AudioProcessorValueTreeState& apvts)
    : phIndex (idx)
{
    const juce::String prefix = "ph" + juce::String (idx) + "_";
    const auto col = playheadColour ((std::size_t) idx);

    nameLabel.setText ("PH " + juce::String (idx + 1), juce::dontSendNotification);
    nameLabel.setFont (juce::Font (13.f, juce::Font::bold));
    nameLabel.setColour (juce::Label::textColourId, col);
    nameLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (nameLabel);

    activeButton.setButtonText ("ON");
    activeButton.setColour (juce::ToggleButton::textColourId,         col);
    activeButton.setColour (juce::ToggleButton::tickColourId,         col);
    activeButton.setColour (juce::ToggleButton::tickDisabledColourId, kDim);
    addAndMakeVisible (activeButton);
    activeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, prefix + "active", activeButton);

    auto makeLabel = [&] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::Font (9.f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, kDim);
        l.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (l);
    };

    makeLabel (stepsLabel, "STEPS");
    styleHorizSlider (stepsSlider, col);
    stepsSlider.setRange (1, MAX_STEPS, 1);
    stepsSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 22, 18);
    addAndMakeVisible (stepsSlider);
    stepsAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, prefix + "steps", stepsSlider);

    makeLabel (volLabel, "VOL");
    styleHorizSlider (volumeSlider, col);
    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (volumeSlider);
    volumeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, prefix + "volume", volumeSlider);

    // Subharmonic: only visible for PH2+ (phIndex > 0)
    if (phIndex > 0)
    {
        makeLabel (subLabel, "SUB ÷");
        styleRotary (subharmonicKnob, col);
        subharmonicKnob.setRange (1, MAX_SUBHARMONIC, 1);
        subharmonicKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 14);
        // Show integer divisor value
        subharmonicKnob.textFromValueFunction = [] (double v)
            { return juce::String ((int) v); };
        addAndMakeVisible (subLabel);
        addAndMakeVisible (subharmonicKnob);
        subAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, prefix + "subharmonic", subharmonicKnob);
    }
}

PlayheadRowComponent::~PlayheadRowComponent() {}

void PlayheadRowComponent::setCurrentStep (int step, int numSteps)
{
    if (currentStep != step || activeSteps != numSteps)
    {
        currentStep = step;
        activeSteps = numSteps;
        repaint();
    }
}

void PlayheadRowComponent::paint (juce::Graphics& g)
{
    const auto ph  = (std::size_t) phIndex;
    const auto col = playheadColour (ph);
    auto bounds    = getLocalBounds().toFloat().reduced (2.f);

    g.setColour (kSurface);
    g.fillRoundedRectangle (bounds, 8.f);
    g.setColour (col.withAlpha (0.3f));
    g.drawRoundedRectangle (bounds, 8.f, 1.5f);

    // LED strip
    auto ledArea = bounds.removeFromBottom (20.f).reduced (4.f, 2.f);
    const float ledW = ledArea.getWidth() / (float) MAX_STEPS;

    for (int s = 0; s < MAX_STEPS; ++s)
    {
        auto cell = ledArea.withWidth (ledW)
                           .withX (ledArea.getX() + s * ledW)
                           .reduced (2.f, 1.f);
        ledRects[(std::size_t) s] = cell;

        const bool inRange = (s < activeSteps);
        const bool active  = inRange && (s == currentStep);

        if (active)        g.setColour (col);
        else if (inRange)  g.setColour (col.withAlpha (0.22f));
        else               g.setColour (kAccent.withAlpha (0.35f));

        g.fillRoundedRectangle (cell, 3.f);
    }
}

void PlayheadRowComponent::resized()
{
    auto b = getLocalBounds().reduced (6);
    b.removeFromBottom (24);   // LEDs

    // Left: name + active
    auto leftCol = b.removeFromLeft (58);
    nameLabel.setBounds    (leftCol.removeFromTop (22));
    activeButton.setBounds (leftCol);

    b.removeFromLeft (6);

    // Right of left: if subharmonic visible, carve it off the right
    if (phIndex > 0)
    {
        auto subArea = b.removeFromRight (70);
        subLabel.setBounds           (subArea.removeFromTop (14));
        subharmonicKnob.setBounds    (subArea);
        b.removeFromRight (4);
    }

    // Steps and volume stacked
    auto stepsRow = b.removeFromTop (20);
    stepsLabel.setBounds  (stepsRow.removeFromLeft (42));
    stepsSlider.setBounds (stepsRow);

    b.removeFromTop (4);

    auto volRow = b.removeFromTop (20);
    volLabel.setBounds     (volRow.removeFromLeft (42));
    volumeSlider.setBounds (volRow);
}

//==============================================================================
// MultiheadSequencerAudioProcessorEditor
//==============================================================================
MultiheadSequencerAudioProcessorEditor::MultiheadSequencerAudioProcessorEditor (
        MultiheadSequencerAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Title
    titleLabel.setText ("MULTIHEAD SEQUENCER", juce::dontSendNotification);
    titleLabel.setFont (juce::Font ("Courier New", 18.f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, kHighlight);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    // BPM
    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setFont (juce::Font (10.f, juce::Font::bold));
    bpmLabel.setColour (juce::Label::textColourId, kDim);
    bpmLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (bpmLabel);

    styleRotary (bpmSlider, kHighlight);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 16);
    addAndMakeVisible (bpmSlider);
    bpmAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        p.apvts, "bpm", bpmSlider);

    // Collision
    collisionLabel.setText ("COLLISION", juce::dontSendNotification);
    collisionLabel.setFont (juce::Font (9.f, juce::Font::bold));
    collisionLabel.setColour (juce::Label::textColourId, kDim);
    collisionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (collisionLabel);

    collisionButton.setButtonText ("ONE");
    collisionButton.setClickingTogglesState (true);
    collisionButton.setColour (juce::TextButton::buttonColourId,   kAccent);
    collisionButton.setColour (juce::TextButton::buttonOnColourId, kHighlight);
    collisionButton.setColour (juce::TextButton::textColourOffId,  kText);
    collisionButton.setColour (juce::TextButton::textColourOnId,   kText);
    collisionButton.onClick = [this] {
        collisionButton.setButtonText (collisionButton.getToggleState() ? "ALL" : "ONE");
    };
    addAndMakeVisible (collisionButton);
    collisionAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, "collisionMode", collisionButton);
    collisionButton.setButtonText (collisionButton.getToggleState() ? "ALL" : "ONE");

    // Pitch steps
    for (std::size_t i = 0; i < NUM_PITCHES; ++i)
    {
        pitchSteps[i] = std::make_unique<PitchStepComponent> ((int) i, p.apvts);
        addAndMakeVisible (*pitchSteps[i]);
    }

    // Rhythm slots
    for (std::size_t r = 0; r < NUM_RHYTHMS; ++r)
    {
        rhythmSlots[r] = std::make_unique<RhythmSlotComponent> ((int) r, p.apvts);
        addAndMakeVisible (*rhythmSlots[r]);
    }

    // Playhead rows
    for (std::size_t ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        playheadRows[ph] = std::make_unique<PlayheadRowComponent> ((int) ph, p.apvts);
        addAndMakeVisible (*playheadRows[ph]);
    }

    startTimerHz (30);
    setResizable (true, false);
    setSize (1000, 720);
}

MultiheadSequencerAudioProcessorEditor::~MultiheadSequencerAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void MultiheadSequencerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);
    const float w = (float) getWidth();

    g.setColour (kDim);
    g.setFont (juce::Font (9.f, juce::Font::bold));
    g.drawText ("PITCH SEQUENCE", juce::Rectangle<int> (10, 62,  140, 12), juce::Justification::centredLeft);
    g.drawText ("RHYTHM SLOTS",   juce::Rectangle<int> (10, 232, 140, 12), juce::Justification::centredLeft);
    g.drawText ("PLAYHEADS",      juce::Rectangle<int> (10, 392, 140, 12), juce::Justification::centredLeft);

    g.setColour (kAccent);
    g.drawHorizontalLine (60,  10.f, w - 10.f);
    g.drawHorizontalLine (230, 10.f, w - 10.f);
    g.drawHorizontalLine (390, 10.f, w - 10.f);
}

void MultiheadSequencerAudioProcessorEditor::resized()
{
    if (pitchSteps[0] == nullptr || rhythmSlots[0] == nullptr || playheadRows[0] == nullptr)
        return;

    auto area = getLocalBounds().reduced (10);

    // Top bar
    auto topBar = area.removeFromTop (50);
    titleLabel.setBounds (topBar.removeFromLeft (320));

    auto collArea = topBar.removeFromRight (90);
    collisionLabel.setBounds  (collArea.removeFromTop (14));
    collisionButton.setBounds (collArea.reduced (4, 2));

    auto bpmArea = topBar.removeFromRight (100);
    bpmLabel.setBounds  (bpmArea.removeFromTop (14));
    bpmSlider.setBounds (bpmArea);

    area.removeFromTop (14);

    // Pitch grid
    auto pitchRow = area.removeFromTop (160);
    const int stepW = pitchRow.getWidth() / (int) NUM_PITCHES;
    for (std::size_t i = 0; i < NUM_PITCHES; ++i)
        pitchSteps[i]->setBounds (
            pitchRow.withWidth (stepW).withX (pitchRow.getX() + (int) i * stepW));

    area.removeFromTop (18);

    // Rhythm slots
    auto rhythmRow = area.removeFromTop (150);
    const int slotW = rhythmRow.getWidth() / (int) NUM_RHYTHMS;
    for (std::size_t r = 0; r < NUM_RHYTHMS; ++r)
        rhythmSlots[r]->setBounds (
            rhythmRow.withWidth (slotW)
                     .withX (rhythmRow.getX() + (int) r * slotW)
                     .reduced (3, 0));

    area.removeFromTop (18);

    // Playhead rows — give equal space to remaining area
    const int rowH = area.getHeight() / (int) NUM_PLAYHEADS;
    for (std::size_t ph = 0; ph < NUM_PLAYHEADS; ++ph)
        playheadRows[ph]->setBounds (area.removeFromTop (rowH).reduced (0, 2));
}

void MultiheadSequencerAudioProcessorEditor::timerCallback()
{
    if (pitchSteps[0] == nullptr || playheadRows[0] == nullptr) return;

    // Update playhead LED strips
    for (std::size_t ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        const int step = processorRef.currentSteps[ph].load();
        auto* raw = processorRef.apvts.getRawParameterValue (
            "ph" + juce::String ((int) ph) + "_steps");
        const int numSteps = raw ? juce::jlimit (1, MAX_STEPS, (int) raw->load()) : 8;
        playheadRows[ph]->setCurrentStep (step, numSteps);
    }

    // Build per-pitch-step bitmask of which playheads are currently on that step
    std::array<int, NUM_PITCHES> masks {};
    masks.fill (0);
    for (std::size_t ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        // Only highlight if playhead is active
        auto* activeRaw = processorRef.apvts.getRawParameterValue (
            "ph" + juce::String ((int) ph) + "_active");
        if (activeRaw == nullptr || activeRaw->load() < 0.5f) continue;

        const int pIdx = processorRef.currentPitchIndices[ph].load() % (int) NUM_PITCHES;
        masks[(std::size_t) pIdx] |= (1 << (int) ph);
    }

    for (std::size_t i = 0; i < NUM_PITCHES; ++i)
        pitchSteps[i]->setActivePlayheads (masks[i]);

    // Refresh rhythm slot buttons (handles undo / preset load)
    for (std::size_t r = 0; r < NUM_RHYTHMS; ++r)
        if (rhythmSlots[r])
            rhythmSlots[r]->refreshButtons();

    collisionButton.setButtonText (collisionButton.getToggleState() ? "ALL" : "ONE");
}
