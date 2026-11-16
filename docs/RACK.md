# Rack editor

SIDSynth 0.4.0 implements the selected Figcraft Rack as native JUCE controls.
Three voice strips share the one filter at the right. All 67 existing parameter
IDs and schema 3 remain unchanged.

## Playing and editing

- Select **3 parts** or **Layered**, the chip model, and PAL/NTSC clock at the
  top. In 3 parts, each strip has an editable MIDI channel. In Layered, the
  top **Layered input** selector is active and the part selectors are disabled.
- Waveform switches combine independently. Sync/ring labels identify the
  preceding oscillator; ring modulation acts on Triangle.
- **MIDI** source uses MIDI notes, tuning and bend. Tuning is in semitones with
  0.01-semitone steps (one cent). **Manual** replaces tuning with the exact
  0–65535 frequency register and enables **Hold**. Frequency in Hz is the
  register multiplied by the selected chip clock, divided by 16777216.
- The **GATE** lamp reports the actual SID gate bit, sampled at audio-block
  boundaries. It does not indicate the release tail or output loudness.
  **Hold** is a saved patch setting. After Panic, stop or recall it can remain
  selected while the actual gate is off; toggle it off then on to rearm.
- Pulse width, ADSR, cutoff, resonance and SID volume display their raw hardware
  ranges. Hover for details, including nominal ADSR times. Cutoff is not a
  calibrated Hz value; the two chip models respond differently.
- Filter routing and LP/BP/HP switches combine independently. A routed voice
  needs a selected filter mode to be heard. Voice 3 direct output affects only
  its unfiltered path; it remains available to the filter and sync/ring paths.
- Click a numeric value to type. Values clamp to their defined ranges.
  Tab/Shift-Tab move through native controls; arrows adjust focused sliders or
  choices, Space activates focused buttons. Double-clicking a slider track
  restores that parameter's default. Mouse-wheel slider edits are disabled.
- **Advanced** opens the three TEST/reset bits and the external-input routing
  switch. Close or Escape dismisses it. TEST is the actual hardware bit and
  suppresses that oscillator's waveform while enabled. EXT IN still requires a
  host audio-input route; the tested native Renoise workflow has none.

## Audition

The mouse keyboard spans MIDI 48–83 (C3–B5 with middle C labelled C4).
In **3 parts**, choose an explicit **Audition target**; only that voice plays,
even if other voices share its MIDI channel. In **Layered**, all MIDI-source
voices play. Manual voices retain their exact frequency/Hold ownership.

Audition is monophonic with last-key priority and held-key fallback. Its intent
crosses to the audio thread through lock-free atomics and applies at the next
callback boundary. Host MIDI retains its original within-block offsets.
Audition temporarily overrides the selected MIDI voice; release returns to any
still-held host note, including its bend, without changing saved parameters.
The keyboard does not capture QWERTY typing as notes.

**Panic / reset** acts on press and briefly displays **Reset**.
Panic, transport stop, routing/source/model/clock edits, valid recall and host
reset cancel audition. Epoch-tagged requests reject stale held keys after a
reset. Changing audition target, closing the editor, opening Advanced or
pressing Escape releases the editor's keys. Audition intent, key ownership,
selection, editor size and live gate bits are not saved in the patch.

## Size and recall

Default size is **1120×780**. Drag the corner with a fixed aspect ratio or use
**80 / 100 / 125 / 150%** (896×624 through 1680×1170). Text and controls scale
as native components. Reopening starts at 100%; window size is not a sound
parameter. These sizes do not establish every desktop DPI or compositor setup.

Save/reload in the host to recall the complete patch. This restores all 67
parameters and disarms live notes/manual holds. A preset browser or separate
patch-file format is not introduced in this milestone.

The editable source is `design/sid-interface-proposals.figcraft`, with the
selected Stage 4 Rack and an advanced-hardware detail. Console is retained as
history. The PNG/SVG previews are references; the plugin draws its own native
controls. Validation evidence and user confirmations are in `STATUS.md` and
`RENOISE_MCP.md`.
