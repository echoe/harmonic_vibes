#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
static constexpr int MAX_STEPS          = 16;
static constexpr int NUM_PLAYHEADS      = 4;
static constexpr int NUM_PITCHES        = 16;
static constexpr int NUM_RHYTHMS        = 4;
static constexpr int NUM_RHYTHM_CHOICES = 14;

static constexpr std::array<float, NUM_RHYTHM_CHOICES> kRhythmDivisors {
    0.25f,  // Whole note
    0.5f,   // Half note
    0.75f,  // Dotted quarter
    1.0f,   // Quarter
    1.25,   // Fifth
    1.5f,   // Dotted 8th
    1.75,   // Seventh
    2.0f,   // 8th
    3.0f,   // 8th triplet
    4.0f,   // 16th
    6.0f,   // 16th triplet
    8.0f,   // 32nd
    12.0f,  // 32nd triplet
    16.0f,  // 64th
};

static const juce::StringArray kRhythmNames {
    "Whole", "Half", "Dotted 1/4", "Quarter", "Fifth", "Dotted 8th", "Seventh", 
    "8th", "8th Trip", "16th", "16th Trip", "32nd", "32nd triplet", "64th"
};

// Subharmonic choices: fractional (superharmonics) then integer divisors.
// Values < 1 transpose UP (e.g. 0.5 = one octave up = frequency * 2).
// Values > 1 transpose DOWN (e.g. 2 = one octave down = frequency / 2).
// Semitone shift = 12 * log2(1/value) = -12 * log2(value)
static const juce::StringArray kSubharmonicNames {
    "1/16", "1/8", "1/4", "1/2",
    "1 (Root)",
    "2", "3", "4", "5", "6", "7", "8",
    "9", "10", "11", "12", "13", "14", "15", "16"
};
// Corresponding float divisor values (frequency divided by this)
static constexpr std::array<float, 20> kSubharmonicValues {
    0.0625f, 0.125f, 0.25f, 0.5f,   // fractional: freq goes UP
    1.0f,                             // root
    2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f,
    9.f, 10.f, 11.f, 12.f, 13.f, 14.f, 15.f, 16.f
};
static constexpr int NUM_SUBHARMONIC_CHOICES = 20;
static constexpr int DEFAULT_SUBHARMONIC_IDX = 4; // "1 (Root)"

//==============================================================================
struct Playhead
{
    std::atomic<float>* numSteps      { nullptr };
    std::atomic<float>* volume        { nullptr };
    std::atomic<float>* active        { nullptr };
    std::atomic<float>* subharmonic   { nullptr }; // index into kSubharmonicValues
    std::atomic<float>* startNote     { nullptr }; // 0-based offset into pitch pool

    std::array<std::atomic<float>*, NUM_RHYTHMS> slotActive { nullptr,nullptr,nullptr,nullptr };

    // Per-step enabled flags (atomic pointers into APVTS)
    std::array<std::atomic<float>*, MAX_STEPS> stepEnabled {};

    std::array<double, NUM_RHYTHMS> accumulators         { 0.0, 0.0, 0.0, 0.0 };
    std::array<int,    NUM_RHYTHMS> slotLastNote         { -1,  -1,  -1,  -1  };
    std::array<double, NUM_RHYTHMS> slotNoteOffCountdown { -1.0,-1.0,-1.0,-1.0 };

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
    std::array<std::atomic<float>*, NUM_PITCHES> lengthParams {};
    std::array<std::atomic<float>*, NUM_RHYTHMS> rhythmParams {};
    std::array<Playhead, NUM_PLAYHEADS>           playheads;

    std::array<std::atomic<int>, NUM_PLAYHEADS>  currentSteps;
    std::array<std::atomic<int>, NUM_PLAYHEADS>  currentPitchIndices;

    std::atomic<float>* collisionMode { nullptr };

private:
    double currentSampleRate { 44100.0 };
    std::vector<PendingEvent> pendingEvents;

    double getHostBpm() const;
    double samplesPerSlot (std::size_t r) const;

    // Shift midiNote by the interval for frequency/divisorValue.
    // divisorValue < 1 → pitch goes UP, > 1 → pitch goes DOWN.
    static int applySubharmonic (int midiNote, float divisorValue);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiheadSequencerAudioProcessor)
};
