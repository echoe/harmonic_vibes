# Multihead Sequencer — JUCE Plugin

A MIDI-output step sequencer inspired by the Moog Subharmonicon, with multiple independant playheads selecting any of four rhythms, running over a shared pitch sequence.
All text here is written by Claude. Claude is taking the wheel here and I'm basically just sharing it tbh (I did port to JUCE8 and change the title, but I mean, come on).

---

## Parameters

### Global
| Parameter | Range | Description |
|---|---|---|
| BPM | 20–300 | Fallback tempo when no DAW sync |

### Pitch Sequence (shared)
| Parameter | Range | Description |
|---|---|---|
| Pitch 1–8 | 0–127 | MIDI note numbers for each pitch slot |

### Per Playhead (×4)
| Parameter | Range | Description |
|---|---|---|
| Active | On/Off | Enable/disable this playhead |
| Subharmonic | 1/16-16 | The subharmonic of the main sequence that a secondary sequence plays |
| Steps | 1–16 | How many steps before looping |
| Rhythm | 8 choices | Step rate relative to quarter note: 1, 1/2, 1/3, 1/4, 1/6, 1/8, 1/12, 1/16 |
| Volume | 0–1 | Controls MIDI velocity (0–127) |

---

## How the pitch pool works

All four playheads share a **single global pitch index** that advances each time *any* playhead fires. This creates polyrhythmic pitch sequences that evolve as playheads with different step counts and rhythms combine. The result is emergent melodic patterns much like the Subharmonicon's interplay between its two sequencers.

---

## Architecture

```
PluginProcessor
├── apvts               AudioProcessorValueTreeState (all parameters + preset save/load)
├── pitchParams[8]      shared MIDI pitch pool (atomic float* → MIDI note 0-127)
├── playheads[4]        Playhead structs with accumulator, stepIndex, param pointers
├── currentSteps[4]     atomic<int> exposed to editor for LED display
└── pitchIndex          atomic<int> global pitch pool cursor

PluginEditor (30 Hz timer)
├── PitchStepComponent[8]    vertical sliders + highlight ring
└── PlayheadRowComponent[4]  LED strip + steps/rhythm/volume controls
```

---

## Extension Ideas

- **Independent pitch modes**: give each playhead its own pitch sequence instead of the shared pool
- **Gate length per step**: per-step note length control
- **Probability per step**: each step fires with a set probability
- **Transpose per playhead**: semitone offset per playhead
- **Step skip/mute**: toggle individual steps on/off per playhead
- **Clock input**: sync to incoming MIDI clock instead of host tempo
