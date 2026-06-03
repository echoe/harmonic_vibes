#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
MultiheadSequencerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        "bpm", "BPM",
        juce::NormalisableRange<float> (20.f, 300.f, 0.01f, 1.f), 120.f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        "collisionMode", "Collision Mode", false));

    // Shared pitch pool
    for (int i = 0; i < NUM_PITCHES; ++i)
        layout.add (std::make_unique<juce::AudioParameterInt> (
            "pitch_" + juce::String (i), "Pitch " + juce::String (i + 1),
            0, 127, 36 + i * 3));

    // Rhythm slot knobs
    for (int r = 0; r < NUM_RHYTHMS; ++r)
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            "rhythm_" + juce::String (r), "Rhythm " + juce::String (r + 1),
            kRhythmNames, r + 2));

    // Per-playhead parameters
    for (int ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        const juce::String p = "ph" + juce::String (ph) + "_";
        const juce::String n = "PH" + juce::String (ph + 1);

        layout.add (std::make_unique<juce::AudioParameterInt> (
            p + "steps",  n + " Steps",  1, MAX_STEPS, 8));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            p + "volume", n + " Volume",
            juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.8f));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            p + "active", n + " Active", ph == 0));

        // Subharmonic divisor: 1 = root pitch, 2 = 1 octave down, 3 = oct+5th, …
        // Playhead 0 is the "main" voice so its subharmonic is fixed at 1 (no param needed
        // but we add it anyway for consistency, just won't display it in the UI).
        layout.add (std::make_unique<juce::AudioParameterInt> (
            p + "subharmonic", n + " Subharmonic", 1, MAX_SUBHARMONIC, 1));

        // Rhythm slot subscriptions
        for (int r = 0; r < NUM_RHYTHMS; ++r)
            layout.add (std::make_unique<juce::AudioParameterBool> (
                p + "slot" + juce::String (r),
                n + " Slot " + juce::String (r + 1),
                ph == r));   // default: each playhead starts on its own slot
    }

    return layout;
}

//==============================================================================
MultiheadSequencerAudioProcessor::MultiheadSequencerAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    for (std::size_t i = 0; i < NUM_PITCHES; ++i)
    {
        pitchParams[i] = apvts.getRawParameterValue ("pitch_" + juce::String ((int) i));
        jassert (pitchParams[i] != nullptr);
    }
    for (std::size_t r = 0; r < NUM_RHYTHMS; ++r)
    {
        rhythmParams[r] = apvts.getRawParameterValue ("rhythm_" + juce::String ((int) r));
        jassert (rhythmParams[r] != nullptr);
    }
    for (std::size_t ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        const juce::String p = "ph" + juce::String ((int) ph) + "_";
        playheads[ph].numSteps    = apvts.getRawParameterValue (p + "steps");
        playheads[ph].volume      = apvts.getRawParameterValue (p + "volume");
        playheads[ph].active      = apvts.getRawParameterValue (p + "active");
        playheads[ph].subharmonic = apvts.getRawParameterValue (p + "subharmonic");
        jassert (playheads[ph].numSteps    != nullptr);
        jassert (playheads[ph].volume      != nullptr);
        jassert (playheads[ph].active      != nullptr);
        jassert (playheads[ph].subharmonic != nullptr);

        for (std::size_t r = 0; r < NUM_RHYTHMS; ++r)
        {
            playheads[ph].slotActive[r] = apvts.getRawParameterValue (
                p + "slot" + juce::String ((int) r));
            jassert (playheads[ph].slotActive[r] != nullptr);
        }

        currentSteps[ph].store (0);
        currentPitchIndices[ph].store (0);
    }
    collisionMode = apvts.getRawParameterValue ("collisionMode");
    jassert (collisionMode != nullptr);
}

MultiheadSequencerAudioProcessor::~MultiheadSequencerAudioProcessor() {}

//==============================================================================
void MultiheadSequencerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    for (std::size_t ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        playheads[ph].reset();
        currentSteps[ph].store (0);
        currentPitchIndices[ph].store (0);
    }
    pendingEvents.clear();
    pendingEvents.reserve (NUM_PLAYHEADS * NUM_RHYTHMS * 2);
}

void MultiheadSequencerAudioProcessor::releaseResources() {}

bool MultiheadSequencerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

//==============================================================================
double MultiheadSequencerAudioProcessor::getHostBpm() const
{
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 0.0)
                    return *bpm;
    if (auto* p = apvts.getRawParameterValue ("bpm"))
        return (double) p->load();
    return 120.0;
}

double MultiheadSequencerAudioProcessor::samplesPerSlot (std::size_t r) const
{
    const double qn = (60.0 / getHostBpm()) * currentSampleRate;
    if (rhythmParams[r] == nullptr) return qn;
    int idx = juce::jlimit (0, NUM_RHYTHM_CHOICES - 1, (int) rhythmParams[r]->load());
    return qn / (double) kRhythmDivisors[(std::size_t) idx];
}

int MultiheadSequencerAudioProcessor::applySubharmonic (int midiNote, int divisor)
{
    if (divisor <= 1) return midiNote;
    // Frequency / N  →  pitch shifted down by 12 * log2(N) semitones
    const int semitones = (int) std::round (12.0 * std::log2 ((double) divisor));
    return juce::jlimit (0, 127, midiNote - semitones);
}

//==============================================================================
void MultiheadSequencerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                      juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midiMessages.clear();

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;

    const bool allNotes = (collisionMode != nullptr && collisionMode->load() >= 0.5f);

    // Pre-compute samples-per-slot once per block
    std::array<double, NUM_RHYTHMS> sps;
    for (std::size_t r = 0; r < NUM_RHYTHMS; ++r)
        sps[r] = samplesPerSlot (r);

    pendingEvents.clear();

    for (std::size_t ph = 0; ph < NUM_PLAYHEADS; ++ph)
    {
        Playhead& head = playheads[ph];

        if (head.active == nullptr || head.numSteps == nullptr || head.volume == nullptr)
            continue;

        if (head.active->load() < 0.5f)
        {
            // Release any notes held by each slot
            for (std::size_t r = 0; r < NUM_RHYTHMS; ++r)
            {
                if (head.slotLastNote[r] >= 0)
                {
                    pendingEvents.push_back ({ 0, head.slotLastNote[r], 0, (int) ph });
                    head.slotLastNote[r]         = -1;
                    head.slotNoteOffCountdown[r] = -1.0;
                }
            }
            continue;
        }

        const int numSteps = juce::jlimit (1, MAX_STEPS, (int) head.numSteps->load());
        const int vel      = juce::jlimit (1, 127, (int) (head.volume->load() * 100.f) + 27);
        const int subDiv   = head.subharmonic != nullptr
                             ? juce::jlimit (1, MAX_SUBHARMONIC, (int) head.subharmonic->load())
                             : 1;

        for (int s = 0; s < numSamples; ++s)
        {
            // Per-slot note-off countdowns — each slot tracks its own held note
            for (std::size_t r = 0; r < NUM_RHYTHMS; ++r)
            {
                if (head.slotNoteOffCountdown[r] >= 0.0)
                {
                    head.slotNoteOffCountdown[r] -= 1.0;
                    if (head.slotNoteOffCountdown[r] < 0.0 && head.slotLastNote[r] >= 0)
                    {
                        pendingEvents.push_back ({ s, head.slotLastNote[r], 0, (int) ph });
                        head.slotLastNote[r] = -1;
                    }
                }
            }

            // Per-slot accumulators — each slot fires completely independently
            for (std::size_t r = 0; r < NUM_RHYTHMS; ++r)
            {
                // Always advance accumulator for phase continuity,
                // even when unsubscribed, so enabling a slot later stays in sync
                head.accumulators[r] += 1.0;

                const bool subscribed = (head.slotActive[r] != nullptr
                                         && head.slotActive[r]->load() >= 0.5f);

                if (sps[r] < 2.0)
                {
                    head.accumulators[r] = 0.0;
                    continue;
                }

                if (head.accumulators[r] >= sps[r])
                {
                    head.accumulators[r] -= sps[r];

                    if (!subscribed) continue;

                    // Each slot advances the playhead's shared step + pitch index
                    head.stepIndex  = (head.stepIndex  + 1) % numSteps;
                    head.pitchIndex = (head.pitchIndex + 1) % (int) NUM_PITCHES;
                    currentSteps[ph].store (head.stepIndex);
                    currentPitchIndices[ph].store (head.pitchIndex);

                    if (pitchParams[(std::size_t) head.pitchIndex] == nullptr) continue;
                    const int rawNote  = juce::jlimit (0, 127,
                                             (int) pitchParams[(std::size_t) head.pitchIndex]->load());
                    const int midiNote = applySubharmonic (rawNote, subDiv);

                    // Kill this slot's previous note before firing a new one
                    if (head.slotLastNote[r] >= 0)
                    {
                        pendingEvents.push_back ({ s, head.slotLastNote[r], 0, (int) ph });
                        head.slotLastNote[r] = -1;
                    }

                    pendingEvents.push_back ({ s, midiNote, vel, (int) ph });
                    head.slotLastNote[r]         = midiNote;
                    head.slotNoteOffCountdown[r] = sps[r] * 0.45;
                }
            }
        }
    }

    // Sort by timestamp (stable → note-offs before note-ons at same position)
    std::stable_sort (pendingEvents.begin(), pendingEvents.end(),
                      [] (const PendingEvent& a, const PendingEvent& b) noexcept
                      { return a.samplePos < b.samplePos; });

    // ONE mode: suppress all but the first note-on per sample position
    if (!allNotes)
    {
        int  lastOnSample     = -1;
        bool seenOnThisSample = false;
        for (auto& ev : pendingEvents)
        {
            if (ev.velocity <= 0) { seenOnThisSample = false; continue; }
            if (ev.samplePos != lastOnSample) { lastOnSample = ev.samplePos; seenOnThisSample = false; }
            if (seenOnThisSample) ev.velocity = -1;
            else seenOnThisSample = true;
        }
    }

    for (const auto& ev : pendingEvents)
    {
        if (ev.velocity == -1) continue;
        const int pos = juce::jlimit (0, numSamples - 1, ev.samplePos);
        if (ev.velocity == 0)
            midiMessages.addEvent (juce::MidiMessage::noteOff (1, ev.note), pos);
        else
            midiMessages.addEvent (juce::MidiMessage::noteOn (1, ev.note,
                                       (juce::uint8) ev.velocity), pos);
    }
}

//==============================================================================
juce::AudioProcessorEditor* MultiheadSequencerAudioProcessor::createEditor()
{
    return new MultiheadSequencerAudioProcessorEditor (*this);
}

void MultiheadSequencerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MultiheadSequencerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultiheadSequencerAudioProcessor();
}
