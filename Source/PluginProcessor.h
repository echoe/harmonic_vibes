#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
static constexpr int MAX_STEPS          = 16;
static constexpr int NUM_PLAYHEADS      = 4;
static constexpr int NUM_PITCHES        = 8;
static constexpr int NUM_RHYTHMS        = 4;
static constexpr int NUM_RHYTHM_CHOICES = 10;

static constexpr std::array<float, NUM_RHYTHM_CHOICES> kRhythmDivisors {
    0.25f,  // Whole note
    0.5f,   // Half note
    0.75f,  // Dotted quarter
    1.0f,   // Quarter
    1.5f,   // Dotted 8th
    2.0f,   // 8th
    3.0f,   // 8th triplet
    4.0f,   // 16th
    6.0f,   // 16th triplet
    8.0f,   // 32nd
};

static const juce::StringArray kRhythmNames {
    "Whole", "Half", "Dotted 1/4", "Quarter",
    "Dotted 8th", "8th", "8th Trip", "16th", "16th Trip", "32nd"
};

// Max subharmonic divisor (1 = unison, 16 = 1/16th frequency ≈ 4 octaves down)
static constexpr int MAX_SUBHARMONIC = 16;

//==============================================================================
struct Playhead
{
    // APVTS parameter pointers
    std::atomic<float>* numSteps      { nullptr };
    std::atomic<float>* volume        { nullptr };
    std::atomic<float>* active        { nullptr };
    std::atomic<float>* subharmonic   { nullptr }; // 1..16 (playhead 0 ignores this)

    // One bool subscription per rhythm slot
    std::array<std::atomic<float>*, NUM_RHYTHMS> slotActive { nullptr,nullptr,nullptr,nullptr };

    // One accumulator per rhythm slot — independent clocks
    std::array<double, NUM_RHYTHMS> accumulators    { 0.0, 0.0, 0.0, 0.0 };
    // Per-slot note tracking — each slot holds its own note independently
    std::array<int,    NUM_RHYTHMS> slotLastNote     { -1, -1, -1, -1 };
    std::array<double, NUM_RHYTHMS> slotNoteOffCountdown { -1.0, -1.0, -1.0, -1.0 };

    // Independent pitch index and step — not shared with other playheads
    int pitchIndex { 0 };
    int stepIndex  { 0 };

    void reset() noexcept
    {
        accumulators.fill (0.0);
        slotLastNote.fill (-1);
        slotNoteOffCountdown.fill (-1.0);
        pitchIndex = 0;
        stepIndex  = 0;
    }
};

//==============================================================================
struct PendingEvent
{
    int samplePos;
    int note;
    int velocity;   // 0 = note-off, >0 = note-on, -1 = suppressed
    int playhead;
};

//==============================================================================
class MultiheadSequencerAudioProcessor : public juce::AudioProcessor
{
public:
    MultiheadSequencerAudioProcessor();
    ~MultiheadSequencerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override {}
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return true;  }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::array<std::atomic<float>*, NUM_PITCHES> pitchParams  {};
    std::array<std::atomic<float>*, NUM_RHYTHMS> rhythmParams {};
    std::array<Playhead, NUM_PLAYHEADS>           playheads;

    // Per-playhead current step (for UI LEDs)
    std::array<std::atomic<int>, NUM_PLAYHEADS>  currentSteps;
    // Per-playhead current pitch index (for UI highlighting)
    std::array<std::atomic<int>, NUM_PLAYHEADS>  currentPitchIndices;

    std::atomic<float>* collisionMode { nullptr };

private:
    double currentSampleRate { 44100.0 };
    std::vector<PendingEvent> pendingEvents;

    double getHostBpm() const;
    double samplesPerSlot (std::size_t r) const;

    // Apply subharmonic transposition: returns MIDI note shifted by the
    // interval corresponding to frequency / divisor.
    // freq/N semitones = -12 * log2(N), rounded to nearest semitone.
    static int applySubharmonic (int midiNote, int divisor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiheadSequencerAudioProcessor)
};
