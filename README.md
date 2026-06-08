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
| Start | 1-16 | Controls which step you start at |
| Rhythm | 14 choices | Step rate relative to quarter note: 1, 1/2, 1/3, 1/4, 1/5, 1/6, 1/7, 1/8, 1/12, 1/16, 1/24, 1/32, 1/48, 1/64 |
| Volume | 0–1 | Controls MIDI velocity (0–127) |

---

## How the pitch pool works

All four playheads share a **single global pitch index** that advances each time *any* playhead fires. This creates polyrhythmic pitch sequences that evolve as playheads with different step counts and rhythms combine. The result is emergent melodic patterns much like the Subharmonicon's interplay between its two sequencers.
