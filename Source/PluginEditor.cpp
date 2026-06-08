#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
// Theme-aware colour helpers — read from ThemeColours singleton
static juce::Colour BG()      { return theme().background; }
static juce::Colour SURF()    { return theme().surface; }
static juce::Colour ACC()     { return theme().accent; }
static juce::Colour TXT()     { return theme().text(); }
static juce::Colour DIM()     { return theme().dim(); }
// kHighlight stays as the title accent — we keep it independent
static const juce::Colour kHighlight { 0xFFE94560u };

static void styleRotary (juce::Slider& s, juce::Colour fill)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setColour (juce::Slider::rotarySliderFillColourId,    fill);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, ACC());
    s.setColour (juce::Slider::thumbColourId,               TXT());
    s.setColour (juce::Slider::textBoxTextColourId,         TXT());
    s.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
}

static void styleHorizSlider (juce::Slider& s, juce::Colour col)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setColour (juce::Slider::trackColourId,               col.withAlpha (0.8f));
    s.setColour (juce::Slider::thumbColourId,               col);
    s.setColour (juce::Slider::backgroundColourId,          ACC());
    s.setColour (juce::Slider::textBoxTextColourId,         TXT());
    s.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
}

//==============================================================================
// PitchStepComponent
//==============================================================================
PitchStepComponent::PitchStepComponent (int index, juce::AudioProcessorValueTreeState& apvts)
{
    indexLabel.setText (juce::String (index + 1), juce::dontSendNotification);
    indexLabel.setFont (juce::Font (juce::FontOptions().withHeight (11.0f).withStyle ("Bold")));
    indexLabel.setColour (juce::Label::textColourId, DIM());
    indexLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (indexLabel);

    pitchSlider.setSliderStyle (juce::Slider::LinearVertical);
    pitchSlider.setRange (0, 127, 1);
    pitchSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 36, 14);
    pitchSlider.setColour (juce::Slider::trackColourId,             kHighlight);
    pitchSlider.setColour (juce::Slider::thumbColourId,             TXT());
    pitchSlider.setColour (juce::Slider::backgroundColourId,        ACC());
    pitchSlider.setColour (juce::Slider::textBoxTextColourId,       TXT());
    pitchSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    pitchSlider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible (pitchSlider);

    pitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "pitch_" + juce::String (index), pitchSlider);
}

PitchStepComponent::~PitchStepComponent() {}

void PitchStepComponent::setActivePlayheads (int bitmask)
{
    if (activePlayheadMask != bitmask) { activePlayheadMask = bitmask; repaint(); }
}

void PitchStepComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.f);

    g.setColour (activePlayheadMask != 0 ? SURF().brighter (0.08f) : SURF());
    g.fillRoundedRectangle (b, 6.f);

    juce::Colour borderCol = ACC();
    float borderW = 1.f;
    for (int ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        if (activePlayheadMask & (1 << ph))
        {
            borderCol = playheadColour (ph);
            borderW   = 2.f;
            break;
        }
    }
    g.setColour (borderCol);
    g.drawRoundedRectangle (b, 6.f, borderW);

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
                g.setColour (playheadColour (ph));
                g.fillEllipse (startX - static_cast<float>(col) * (dotR * 2.f + 2.f) - dotR,               
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
RhythmSlotComponent::RhythmSlotComponent (int idx, juce::AudioProcessorValueTreeState& apvts_)
    : slotIndex (idx), apvts (apvts_)
{
    slotLabel.setText ("R" + juce::String (idx + 1), juce::dontSendNotification);
    slotLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Bold")));
    slotLabel.setColour (juce::Label::textColourId, DIM());
    slotLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (slotLabel);

    styleHorizSlider (rhythmKnob, kHighlight);
    rhythmKnob.setRange (0, NUM_RHYTHM_CHOICES - 1, 1);
    rhythmKnob.setTextBoxStyle (juce::Slider::TextBoxRight, false, 68, 18);
    rhythmKnob.textFromValueFunction = [] (double v)
    {
        int i = juce::jlimit (0, NUM_RHYTHM_CHOICES - 1, (int) v);
        return kRhythmNames[i];
    };
    rhythmKnob.valueFromTextFunction = [] (const juce::String& t)
    {
        for (int i = 0; i < NUM_RHYTHM_CHOICES; ++i)
            if (kRhythmNames[i] == t) return (double) i;
        return 0.0;
    };
    addAndMakeVisible (rhythmKnob);

    rhythmAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, "rhythm_" + juce::String (idx), rhythmKnob);

    for (int ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        auto& btn = phButtons[(std::size_t) ph];
        btn.setButtonText ("P" + juce::String (ph + 1));
        btn.setClickingTogglesState (true);
        btn.setColour (juce::TextButton::textColourOffId, TXT());
        btn.setColour (juce::TextButton::textColourOnId,  BG());
        btn.onClick = [this, ph]
        {
            const juce::String id = "ph" + juce::String (ph)
                                  + "_slot" + juce::String (slotIndex);
            if (auto* param = apvts.getParameter (id))
                param->setValueNotifyingHost (
                    phButtons[(std::size_t) ph].getToggleState() ? 1.f : 0.f);
        };
        addAndMakeVisible (btn);
    }
    refreshButtons();
}

RhythmSlotComponent::~RhythmSlotComponent() {}

void RhythmSlotComponent::applyTheme()
{
    slotLabel.setColour (juce::Label::textColourId, DIM());
    styleHorizSlider (rhythmKnob, kHighlight);
    refreshButtons();
    repaint();
}

void RhythmSlotComponent::refreshButtons()
{
    for (int ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        const juce::String id = "ph" + juce::String (ph)
                              + "_slot" + juce::String (slotIndex);
        auto* raw = apvts.getRawParameterValue (id);
        const bool on = (raw != nullptr && raw->load() >= 0.5f);
        auto& btn = phButtons[(std::size_t) ph];
        btn.setToggleState (on, juce::dontSendNotification);
        const auto col = playheadColour (ph);
        btn.setColour (juce::TextButton::buttonColourId,
                       on ? col.withAlpha (0.85f) : ACC());
        btn.setColour (juce::TextButton::buttonOnColourId, col);
        btn.setColour (juce::TextButton::textColourOffId, TXT());
        btn.setColour (juce::TextButton::textColourOnId,  BG());
    }
}

void RhythmSlotComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.f);
    g.setColour (SURF());
    g.fillRoundedRectangle (b, 8.f);
    g.setColour (ACC());
    g.drawRoundedRectangle (b, 8.f, 1.f);
}

void RhythmSlotComponent::resized()
{
    auto b = getLocalBounds().reduced (6);
    slotLabel.setBounds (b.removeFromTop (16));
    b.removeFromTop (4);
    rhythmKnob.setBounds (b.removeFromTop (24));
    b.removeFromTop (4);
    const int btnW = b.getWidth() / NUM_PLAYHEADS;
    for (int ph = 0; ph < NUM_PLAYHEADS; ++ph)
        phButtons[(std::size_t) ph].setBounds (
            b.withWidth (btnW).withX (b.getX() + ph * btnW).reduced (2, 0));
}

//==============================================================================
// StepLedStrip — clickable per-step enable/disable
//==============================================================================
StepLedStrip::StepLedStrip (int phIdx, juce::AudioProcessorValueTreeState& apvts_)
    : phIndex (phIdx), apvts (apvts_)
{
    stepEnabled.fill (true);
    setRepaintsOnMouseActivity (false);
}

juce::Rectangle<float> StepLedStrip::stepRect (int s) const
{
    const float w = (float) getWidth();
    const float h = (float) getHeight();
    const float ledW = w / (float) MAX_STEPS;
    return juce::Rectangle<float> (static_cast<float>(s) * ledW, 0.f, ledW, h).reduced (2.f, 1.f);
}

void StepLedStrip::paint (juce::Graphics& g)
{
    const auto col = playheadColour (phIndex);
    for (int s = 0; s < MAX_STEPS; ++s)
    {
        auto cell = stepRect (s);
        const bool inRange  = (s < activeSteps);
        const bool active   = inRange && (s == currentStep);
        const bool enabled  = stepEnabled[(std::size_t) s];

        if (active)
        {
            g.setColour (col);
        }
        else if (inRange && enabled)
        {
            g.setColour (col.withAlpha (0.30f));
        }
        else if (inRange && !enabled)
        {
            // Muted step: dim X mark
            g.setColour (ACC().withAlpha (0.5f));
            g.fillRoundedRectangle (cell, 3.f);
            g.setColour (col.withAlpha (0.20f));
            g.drawLine (cell.getX() + 3, cell.getY() + 3,
                        cell.getRight() - 3, cell.getBottom() - 3, 1.f);
            g.drawLine (cell.getRight() - 3, cell.getY() + 3,
                        cell.getX() + 3, cell.getBottom() - 3, 1.f);
            continue;
        }
        else
        {
            g.setColour (ACC().withAlpha (0.35f));
        }
        g.fillRoundedRectangle (cell, 3.f);
    }
}

void StepLedStrip::mouseDown (const juce::MouseEvent& e)
{
    const float ledW = (float) getWidth() / (float) MAX_STEPS;
    const int s = juce::jlimit (0, MAX_STEPS - 1, (int) (e.position.x / ledW));

    const juce::String paramId = "ph" + juce::String (phIndex)
                                + "_step" + juce::String (s);
    if (auto* param = apvts.getParameter (paramId))
    {
        const bool current = (apvts.getRawParameterValue (paramId) != nullptr
                              && apvts.getRawParameterValue (paramId)->load() >= 0.5f);
        param->setValueNotifyingHost (current ? 0.f : 1.f);
        stepEnabled[(std::size_t) s] = !current;
    }
    repaint();
}

void StepLedStrip::setCurrentStep (int step, int numSteps)
{
    if (currentStep != step || activeSteps != numSteps)
    {
        currentStep = step;
        activeSteps = numSteps;
        repaint();
    }
}

void StepLedStrip::refreshFromApvts()
{
    for (int s = 0; s < MAX_STEPS; ++s)
    {
        const juce::String id = "ph" + juce::String (phIndex) + "_step" + juce::String (s);
        auto* raw = apvts.getRawParameterValue (id);
        stepEnabled[(std::size_t) s] = (raw == nullptr || raw->load() >= 0.5f);
    }
    repaint();
}

//==============================================================================
// PlayheadRowComponent
//==============================================================================
PlayheadRowComponent::PlayheadRowComponent (int idx,
                                             juce::AudioProcessorValueTreeState& apvts,
                                             std::function<void(int)> colourButtonClicked)
    : phIndex (idx)
{
    const juce::String prefix = "ph" + juce::String (idx) + "_";
    const auto col = playheadColour (idx);

    // Colour swatch button — small square showing current colour
    colourButton.setButtonText ({});
    colourButton.setColour (juce::TextButton::buttonColourId, col);
    colourButton.setTooltip ("Click to change playhead colour");
    colourButton.onClick = [this, colourButtonClicked] { colourButtonClicked (phIndex); };
    addAndMakeVisible (colourButton);

    nameLabel.setText ("PH " + juce::String (idx + 1), juce::dontSendNotification);
    nameLabel.setFont (juce::Font (juce::FontOptions().withHeight (13.0f).withStyle ("Bold")));
    nameLabel.setColour (juce::Label::textColourId, col);
    nameLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (nameLabel);

    activeButton.setButtonText ("ON");
    activeButton.setColour (juce::ToggleButton::textColourId,         col);
    activeButton.setColour (juce::ToggleButton::tickColourId,         col);
    activeButton.setColour (juce::ToggleButton::tickDisabledColourId, DIM());
    addAndMakeVisible (activeButton);
    activeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, prefix + "active", activeButton);

    auto makeLabel = [&] (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::Font (juce::FontOptions().withHeight (9.0f).withStyle ("Bold")));
        l.setColour (juce::Label::textColourId, DIM());
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

    makeLabel (startNoteLabel, "START");
    styleHorizSlider (startNoteSlider, col);
    startNoteSlider.setRange (0, NUM_PITCHES - 1, 1);
    startNoteSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 22, 18);
    startNoteSlider.setTooltip ("Starting pitch slot (1 = first note in the pitch pool)");
    addAndMakeVisible (startNoteSlider);
    startNoteAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, prefix + "startNote", startNoteSlider);
    // Set AFTER attachment so the formatter isn't overwritten on initial value push
    startNoteSlider.textFromValueFunction = [] (double v) { return juce::String ((int) v + 1); };
    startNoteSlider.valueFromTextFunction = [] (const juce::String& t) { return (double) (t.getIntValue() - 1); };
    startNoteSlider.updateText();

    makeLabel (volLabel, "VOL");
    styleHorizSlider (volumeSlider, col);
    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (volumeSlider);
    volumeAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, prefix + "volume", volumeSlider);

    // Subharmonic choice slider
    makeLabel (subLabel, "SUB");
    styleHorizSlider (subharmonicKnob, col);
    subharmonicKnob.setRange (0, NUM_SUBHARMONIC_CHOICES - 1, 1);
    subharmonicKnob.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 18);
    subharmonicKnob.textFromValueFunction = [] (double v)
    {
        int choiceIdx = juce::jlimit (0, NUM_SUBHARMONIC_CHOICES - 1, (int) v);
        return kSubharmonicNames[choiceIdx];
    };
    subharmonicKnob.valueFromTextFunction = [] (const juce::String& t)
    {
        for (int i = 0; i < NUM_SUBHARMONIC_CHOICES; ++i)
            if (kSubharmonicNames[i] == t)
                return (double) i;
        return (double) DEFAULT_SUBHARMONIC_IDX;
    };
    addAndMakeVisible (subLabel);
    addAndMakeVisible (subharmonicKnob);
    subAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, prefix + "subharmonic", subharmonicKnob);

    // LED strip — clickable per-step buttons
    ledStrip = std::make_unique<StepLedStrip> (idx, apvts);
    addAndMakeVisible (*ledStrip);
}

PlayheadRowComponent::~PlayheadRowComponent() {}

void PlayheadRowComponent::setCurrentStep (int step, int numSteps)
{
    if (ledStrip)
        ledStrip->setCurrentStep (step, numSteps);
    if (currentStep != step || activeSteps != numSteps)
    {
        currentStep = step;
        activeSteps = numSteps;
        repaint();
    }
}

void PlayheadRowComponent::refreshColour()
{
    const auto col = playheadColour (phIndex);
    applyColourToControls (col);
    colourButton.setColour (juce::TextButton::buttonColourId, col);
    repaint();
}

void PlayheadRowComponent::applyTheme()
{
    activeButton.setColour (juce::ToggleButton::tickDisabledColourId, DIM());
    stepsLabel.setColour     (juce::Label::textColourId, DIM());
    startNoteLabel.setColour (juce::Label::textColourId, DIM());
    volLabel.setColour       (juce::Label::textColourId, DIM());
    subLabel.setColour       (juce::Label::textColourId, DIM());
    repaint();
}

void PlayheadRowComponent::applyColourToControls (juce::Colour col)
{
    nameLabel.setColour   (juce::Label::textColourId, col);
    activeButton.setColour (juce::ToggleButton::textColourId, col);
    activeButton.setColour (juce::ToggleButton::tickColourId, col);
    stepsSlider.setColour  (juce::Slider::trackColourId,  col.withAlpha (0.8f));
    stepsSlider.setColour  (juce::Slider::thumbColourId,  col);
    stepsSlider.setColour  (juce::Slider::backgroundColourId, ACC());
    startNoteSlider.setColour (juce::Slider::trackColourId,  col.withAlpha (0.8f));
    startNoteSlider.setColour (juce::Slider::thumbColourId,  col);
    startNoteSlider.setColour (juce::Slider::backgroundColourId, ACC());
    volumeSlider.setColour (juce::Slider::trackColourId,  col.withAlpha (0.7f));
    volumeSlider.setColour (juce::Slider::thumbColourId,  col);
    volumeSlider.setColour (juce::Slider::backgroundColourId, ACC());
    subharmonicKnob.setColour (juce::Slider::trackColourId,      col.withAlpha (0.8f));
    subharmonicKnob.setColour (juce::Slider::thumbColourId,      col);
    subharmonicKnob.setColour (juce::Slider::backgroundColourId, ACC());
    subharmonicKnob.setColour (juce::Slider::textBoxTextColourId, TXT());
    subharmonicKnob.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    subharmonicKnob.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    repaint();
}

void PlayheadRowComponent::paint (juce::Graphics& g)
{
    const auto col = playheadColour (phIndex);
    auto bounds    = getLocalBounds().toFloat().reduced (2.f);

    g.setColour (SURF());
    g.fillRoundedRectangle (bounds, 8.f);
    g.setColour (col.withAlpha (0.3f));
    g.drawRoundedRectangle (bounds, 8.f, 1.5f);
}

void PlayheadRowComponent::resized()
{
    auto b = getLocalBounds().reduced (6);

    // LED strip at bottom
    auto ledArea = b.removeFromBottom (28);
    if (ledStrip)
        ledStrip->setBounds (ledArea.reduced (2, 2));

    b.removeFromBottom (4);

    // Far left: colour swatch button
    auto swatchArea = b.removeFromLeft (20);
    colourButton.setBounds (swatchArea.withSizeKeepingCentre (18, 18));
    b.removeFromLeft (4);

    // Next: name + active toggle stacked
    auto leftCol = b.removeFromLeft (54);
    nameLabel.setBounds    (leftCol.removeFromTop (22));
    activeButton.setBounds (leftCol);

    b.removeFromLeft (6);

    // Far right: subharmonic slider (~2x the old knob width)
    auto subArea = b.removeFromRight (160);
    subLabel.setBounds        (subArea.removeFromTop (14));
    subharmonicKnob.setBounds (subArea.removeFromTop (22));
    b.removeFromRight (6);

    // Centre: steps full-width, then start + vol side by side at half width each
    auto stepsRow = b.removeFromTop (20);
    stepsLabel.setBounds   (stepsRow.removeFromLeft (42));
    stepsSlider.setBounds  (stepsRow);

    b.removeFromTop (3);

    auto smallRow = b.removeFromTop (20);
    const int halfW = smallRow.getWidth() / 2;

    auto startHalf = smallRow.removeFromLeft (halfW);
    startNoteLabel.setBounds  (startHalf.removeFromLeft (42));
    startNoteSlider.setBounds (startHalf);

    auto volHalf = smallRow;
    volLabel.setBounds     (volHalf.removeFromLeft (42));
    volumeSlider.setBounds (volHalf);
}

//==============================================================================
// MultiheadSequencerAudioProcessorEditor
//==============================================================================
MultiheadSequencerAudioProcessorEditor::MultiheadSequencerAudioProcessorEditor (
        MultiheadSequencerAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    titleLabel.setText ("Harmonic Vibes", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions().withName("Courier New").withHeight (18.0f).withStyle ("Bold")));
    titleLabel.setColour (juce::Label::textColourId, kHighlight);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    bpmLabel.setText ("BPM", juce::dontSendNotification);
    bpmLabel.setFont (juce::Font (juce::FontOptions().withHeight (10.0f).withStyle ("Bold")));
    bpmLabel.setColour (juce::Label::textColourId, DIM());
    bpmLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (bpmLabel);

    styleRotary (bpmSlider, kHighlight);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 16);
    addAndMakeVisible (bpmSlider);
    bpmAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        p.apvts, "bpm", bpmSlider);

    collisionLabel.setText ("COLLISION", juce::dontSendNotification);
    collisionLabel.setFont (juce::Font (juce::FontOptions().withHeight (9.0f).withStyle ("Bold")));
    collisionLabel.setColour (juce::Label::textColourId, DIM());
    collisionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (collisionLabel);

    collisionButton.setClickingTogglesState (true);
    collisionButton.setColour (juce::TextButton::buttonColourId,   ACC());
    collisionButton.setColour (juce::TextButton::buttonOnColourId, kHighlight);
    collisionButton.setColour (juce::TextButton::textColourOffId,  TXT());
    collisionButton.setColour (juce::TextButton::textColourOnId,   TXT());
    addAndMakeVisible (collisionButton);
    collisionAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, "collisionMode", collisionButton);
    collisionButton.setButtonText (collisionButton.getToggleState() ? "ALL" : "ONE");
    collisionButton.onClick = [this]
    {
        collisionButton.setButtonText (collisionButton.getToggleState() ? "ALL" : "ONE");
    };

    // Theme colour picker buttons
    themeLabel.setText ("THEME", juce::dontSendNotification);
    themeLabel.setFont (juce::Font (juce::FontOptions().withHeight (9.0f).withStyle ("Bold")));
    themeLabel.setColour (juce::Label::textColourId, DIM());
    themeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (themeLabel);

    const juce::StringArray themeNames { "BG", "SURF", "ACCENT" };
    for (int i = 0; i < 3; ++i)
    {
        auto& btn = themeButtons[(std::size_t) i];
        btn.setButtonText (themeNames[i]);
        btn.setColour (juce::TextButton::textColourOffId, TXT());
        const int slot = i;
        btn.onClick = [this, slot] { openThemePicker (slot); };
        addAndMakeVisible (btn);
    }
    applyThemeToAll();

    for (std::size_t i = 0; i < NUM_PITCHES; ++i)
    {
        pitchSteps[i] = std::make_unique<PitchStepComponent> ((int) i, p.apvts);
        addAndMakeVisible (*pitchSteps[i]);
    }

    for (std::size_t r = 0; r < NUM_RHYTHMS; ++r)
    {
        rhythmSlots[r] = std::make_unique<RhythmSlotComponent> ((int) r, p.apvts);
        addAndMakeVisible (*rhythmSlots[r]);
    }

    for (std::size_t ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        playheadRows[ph] = std::make_unique<PlayheadRowComponent> (
            (int) ph, p.apvts,
            [this] (int idx) { openColourPicker (idx); });
        addAndMakeVisible (*playheadRows[ph]);
    }

    startTimerHz (30);
    setResizable (true, false);
    setSize (1000, 700);
}

MultiheadSequencerAudioProcessorEditor::~MultiheadSequencerAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void MultiheadSequencerAudioProcessorEditor::applyThemeToAll()
{
    // Update theme button swatch colours
    juce::Colour swatches[3] = { BG(), SURF(), ACC() };
    for (int i = 0; i < 3; ++i)
    {
        themeButtons[(std::size_t) i].setColour (juce::TextButton::buttonColourId,
                                                  swatches[i]);
        themeButtons[(std::size_t) i].setColour (juce::TextButton::textColourOffId,
                                                  swatches[i].contrasting (0.9f));
    }

    bpmLabel.setColour       (juce::Label::textColourId, DIM());
    collisionLabel.setColour (juce::Label::textColourId, DIM());
    themeLabel.setColour     (juce::Label::textColourId, DIM());

    styleRotary (bpmSlider, kHighlight);
    collisionButton.setColour (juce::TextButton::buttonColourId, ACC());

    for (auto& rs : rhythmSlots)
        if (rs) rs->applyTheme();

    for (auto& pr : playheadRows)
        if (pr) { pr->refreshColour(); pr->applyTheme(); }

    for (auto& ps : pitchSteps)
        if (ps) ps->repaint();

    repaint();
}

//==============================================================================
// Colour picker bridge — inner class shared by playhead and theme pickers
struct ColourPickerBridge : public juce::ChangeListener
{
    ColourPickerBridge (std::function<void(juce::Colour)> cb, juce::ColourSelector& sel)
        : callback (cb), selector (sel) {}

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        callback (selector.getCurrentColour());
    }

    std::function<void(juce::Colour)> callback;
    juce::ColourSelector& selector;
};

static void launchColourPicker (juce::Component* parent,
                                  juce::Rectangle<int> anchor,
                                  juce::Colour initial,
                                  std::function<void(juce::Colour)> onChange)
{
    auto selectorOwned = std::make_unique<juce::ColourSelector> (
        juce::ColourSelector::showColourAtTop
        | juce::ColourSelector::showSliders
        | juce::ColourSelector::showColourspace);

    auto* selector = selectorOwned.get();
    selector->setCurrentColour (initial);
    selector->setSize (300, 400);

    struct BridgeOwner : public juce::Component, public juce::ChangeListener
    {
        BridgeOwner (std::function<void(juce::Colour)> cb, juce::ColourSelector& sel)
            : callback (cb), selector (sel)
        { setVisible (false); setSize (1, 1); }

        void changeListenerCallback (juce::ChangeBroadcaster*) override
        { callback (selector.getCurrentColour()); }

        std::function<void(juce::Colour)> callback;
        juce::ColourSelector& selector;
    };

    auto* bridge = new BridgeOwner (onChange, *selector);
    selector->addChildComponent (bridge);
    selector->addChangeListener (bridge);

    juce::CallOutBox::launchAsynchronously (std::move (selectorOwned), anchor, parent);
}

void MultiheadSequencerAudioProcessorEditor::openColourPicker (int playheadIndex)
{
    launchColourPicker (
        this,
        playheadRows[(std::size_t) playheadIndex]->getBoundsInParent(),
        playheadColour (playheadIndex),
        [this, playheadIndex] (juce::Colour c) { onColourChanged (playheadIndex, c); });
}

void MultiheadSequencerAudioProcessorEditor::openThemePicker (int slot)
{
    juce::Colour initial;
    if      (slot == 0) initial = BG();
    else if (slot == 1) initial = SURF();
    else                initial = ACC();

    launchColourPicker (
        this,
        themeButtons[(std::size_t) slot].getBoundsInParent(),
        initial,
        [this, slot] (juce::Colour c)
        {
            if      (slot == 0) theme().background = c;
            else if (slot == 1) theme().surface    = c;
            else                theme().accent     = c;
            applyThemeToAll();
        });
}

void MultiheadSequencerAudioProcessorEditor::onColourChanged (int playheadIndex,
                                                               juce::Colour newColour)
{
    PlayheadColours::get().set (playheadIndex, newColour);

    if (playheadRows[(std::size_t) playheadIndex])
        playheadRows[(std::size_t) playheadIndex]->refreshColour();

    for (std::size_t r = 0; r < NUM_RHYTHMS; ++r)
        if (rhythmSlots[r])
            rhythmSlots[r]->refreshButtons();

    for (std::size_t i = 0; i < NUM_PITCHES; ++i)
        if (pitchSteps[i])
            pitchSteps[i]->repaint();
}

//==============================================================================
void MultiheadSequencerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (BG());
    const float w = (float) getWidth();

    g.setColour (DIM());
    g.setFont (juce::Font (juce::FontOptions().withHeight (9.0f).withStyle ("Bold")));
    g.drawText ("PITCH SEQUENCE", juce::Rectangle<int> (10, 62,  140, 12),
                juce::Justification::centredLeft);
    g.drawText ("RHYTHM SLOTS",   juce::Rectangle<int> (10, 232, 140, 12),
                juce::Justification::centredLeft);
    g.drawText ("PLAYHEADS",      juce::Rectangle<int> (10, 392, 140, 12),
                juce::Justification::centredLeft);

    g.setColour (ACC());
    g.drawHorizontalLine (60,  10.f, w - 10.f);
    g.drawHorizontalLine (230, 10.f, w - 10.f);
    g.drawHorizontalLine (390, 10.f, w - 10.f);
}

void MultiheadSequencerAudioProcessorEditor::resized()
{
    if (pitchSteps[0] == nullptr || rhythmSlots[0] == nullptr || playheadRows[0] == nullptr)
        return;

    auto area = getLocalBounds().reduced (10);

    // Top bar: title | ... | theme | bpm | collision
    auto topBar = area.removeFromTop (50);
    titleLabel.setBounds (topBar.removeFromLeft (240));

    auto collArea = topBar.removeFromRight (90);
    collisionLabel.setBounds  (collArea.removeFromTop (14));
    collisionButton.setBounds (collArea.reduced (4, 2));

    auto bpmArea = topBar.removeFromRight (100);
    bpmLabel.setBounds  (bpmArea.removeFromTop (14));
    bpmSlider.setBounds (bpmArea);

    // Theme picker section: label + 3 swatch buttons
    topBar.removeFromRight (8);
    auto themeArea = topBar.removeFromRight (200);
    themeLabel.setBounds (themeArea.removeFromTop (14));
    const int swW = themeArea.getWidth() / 3;
    for (int i = 0; i < 3; ++i)
        themeButtons[(std::size_t) i].setBounds (
            themeArea.withWidth (swW).withX (themeArea.getX() + i * swW).reduced (3, 2));

    area.removeFromTop (14);

    // 16 pitch sliders
    auto pitchRow = area.removeFromTop (160);
    const int stepW = pitchRow.getWidth() / (int) NUM_PITCHES;
    for (std::size_t i = 0; i < NUM_PITCHES; ++i)
        pitchSteps[i]->setBounds (
            pitchRow.withWidth (stepW).withX (pitchRow.getX() + (int) i * stepW));

    area.removeFromTop (18);

    auto rhythmRow = area.removeFromTop (80);
    const int slotW = rhythmRow.getWidth() / (int) NUM_RHYTHMS;
    for (std::size_t r = 0; r < NUM_RHYTHMS; ++r)
        rhythmSlots[r]->setBounds (
            rhythmRow.withWidth (slotW)
                     .withX (rhythmRow.getX() + (int) r * slotW)
                     .reduced (3, 0));

    area.removeFromTop (18);

    const int rowH = area.getHeight() / (int) NUM_PLAYHEADS;
    for (std::size_t ph = 0; ph < NUM_PLAYHEADS; ++ph)
        playheadRows[ph]->setBounds (area.removeFromTop (rowH).reduced (0, 2));
}

void MultiheadSequencerAudioProcessorEditor::timerCallback()
{
    if (pitchSteps[0] == nullptr || playheadRows[0] == nullptr) return;

    for (std::size_t ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        const int step = processorRef.currentSteps[ph].load();
        auto* raw = processorRef.apvts.getRawParameterValue (
            "ph" + juce::String ((int) ph) + "_steps");
        const int numSteps = raw ? juce::jlimit (1, MAX_STEPS, (int) raw->load()) : 8;
        playheadRows[ph]->setCurrentStep (step, numSteps);
    }

    std::array<int, NUM_PITCHES> masks {};
    masks.fill (0);
    for (std::size_t ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        auto* activeRaw = processorRef.apvts.getRawParameterValue (
            "ph" + juce::String ((int) ph) + "_active");
        if (activeRaw == nullptr || activeRaw->load() < 0.5f) continue;
        const int pIdx = processorRef.currentPitchIndices[ph].load() % (int) NUM_PITCHES;
        masks[(std::size_t) pIdx] |= (1 << (int) ph);
    }
    for (std::size_t i = 0; i < NUM_PITCHES; ++i)
        pitchSteps[i]->setActivePlayheads (masks[i]);

    for (std::size_t r = 0; r < NUM_RHYTHMS; ++r)
        if (rhythmSlots[r])
            rhythmSlots[r]->refreshButtons();

    // Sync step LEDs from APVTS (catches external automation changes)
    // (done via ledStrip::refreshFromApvts — but skip every frame to save CPU;
    //  mouseDown already updates locally so this is just a safety net)

    collisionButton.setButtonText (collisionButton.getToggleState() ? "ALL" : "ONE");
}
