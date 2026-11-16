# Parameter, saved-state and MIDI contract

SIDSynth 0.5.0 is an early development instrument. Preserve these identifiers
when adding controls. Do not recycle removed IDs. Add an explicit migration
before changing a saved value's meaning.

| Item | Stable value |
| --- | --- |
| JUCE manufacturer code | `Sids` |
| JUCE plugin code | `Sydi` |
| Bundle ID | `org.sidsynth.instrument` |
| State root/current version | `SIDSynthPatch`, `schemaVersion="3"` |

| Parameter ID | Range / choices | Default | Version hint |
| --- | --- | --- | --- |
| `output.gainDb` | −60.0 to 0.0 dB, 0.1 dB steps | −12.0 dB | 1 |
| `midi.mode` | 0 = 3 parts, 1 = Layered | 0 | 2 |
| `midi.layeredChannel` | MIDI channel 1–16 | 1 | 2 |
| `voice1.midiChannel` | MIDI channel 1–16 | 1 | 2 |
| `voice2.midiChannel` | MIDI channel 1–16 | 2 | 2 |
| `voice3.midiChannel` | MIDI channel 1–16 | 3 | 2 |

VST3 parameter IDs use JUCE's stable string-ID hashing. Do not enable JUCE's
legacy parameter-ID mode or change the plugin/manufacturer codes.

## Saved patch

State uses JUCE's binary XML envelope (`copyXmlToBinary`). Every parameter is
written as an attribute; this abbreviated example omits the new SID fields:

```xml
<SIDSynthPatch schemaVersion="3" output.gainDb="-12.0" midi.mode="0"
  midi.layeredChannel="1" voice1.midiChannel="1" voice2.midiChannel="2"
  voice3.midiChannel="3"/>
```

Saving reads atomic patch values. It does not serialize held notes, repeat
counts, sustain, bend, live MIDI gate, oscillator phase, filter memory or editor actions. Explicit
manual hold is a saved patch parameter, but its runtime armed state is not saved.
Restoring a valid patch requests a chip/performance reset at the next callback
boundary. No nonportable upstream SID state blob is used.

Schema **1** is explicitly migrated: preserve `output.gainDb`, use 3 parts on
channels 1/2/3 and layered input 1. The old channel-1 saw remains compatible. Schema **2** preserves gain and
routing while defaulting the new sound controls.
Schema 1 did not save settings for voices 2/3. Future schemas are rejected.

Missing known parameters use defaults; unknown attributes are ignored. Finite
out-of-range numbers are clamped; mode/channel numbers are rounded to integers.
Unrecognized roots/versions, malformed/truncated XML, malformed/nonfinite numeric
fields, and data larger than 1 MiB leave the entire patch untouched. Validate
all fields before applying any. Number parsing is independent of host locale.
Only fields defined by the loaded schema are interpreted during restoration.

## Stage 3 SID controls

Current schema: **3**, plugin version **0.5.0**. The existing six IDs and their
version hints remain unchanged. All new IDs use version hint **3**. Existing
schemas **1 and 2** migrate explicitly: preserve their known fields and default
all subsequent controls. Future versions are rejected. Each known finite field
is clamped to its hardware/musical range; integer/boolean/choice fields round.
Unknown attributes are ignored. Invalid known numeric fields reject the whole
patch. Legacy schemas ignore attributes introduced by a later schema.

For each prefix `voice1`, `voice2`, `voice3`:

| ID suffix | Range / choices | Default |
| --- | --- | --- |
| `.source` | 0 MIDI, 1 Manual | MIDI |
| `.frequency` | raw 16-bit frequency register 0–65535 | 7493 |
| `.tuning` | −96 to +96 semitones, 0.01 steps | 0 |
| `.pulseWidth` | raw 12-bit register 0–4095 | 2048 |
| `.attack`, `.decay`, `.sustain`, `.release` | raw nibble 0–15 each | 0 / 4 / 12 / 5 |
| `.triangle`, `.saw`, `.pulse`, `.noise` | independent waveform bits | off / on / off / off |
| `.manualGate` | explicit manual hold off/on | off |
| `.sync`, `.ring`, `.test` | independent hardware bits | off |

| Shared ID | Range / choices | Default |
| --- | --- | --- |
| `filter.cutoff` | raw 11-bit register 0–2047 | 2047 |
| `filter.resonance` | raw nibble 0–15 | 0 |
| `filter.voice1`, `.voice2`, `.voice3`, `.external` | independent routing bits | off |
| `filter.lowPass`, `.bandPass`, `.highPass` | independent mode bits | off |
| `chip.volume` | raw nibble 0–15 | 15 |
| `chip.model` | 0 6581, 1 8580 | 8580 |
| `chip.clock` | 0 PAL 985248 Hz, 1 NTSC 1022730 Hz | PAL |
| `chip.voice3Direct` | voice 3 unfiltered direct output off/on | on |

These are actual hardware controls: waveform and filter-mode switches combine
bitwise. Filtered voices with no mode selected do not reach the output. Voice 3
direct-off sets MODE/VOL bit 7: it removes only voice 3's unfiltered signal; its
filtered path and ability to modulate voice 1 remain. Sync/ring source cycle is
voice 1 ← voice 3 ← voice 2 ← voice 1, provided by the single shared chip.
TEST holds/resets the selected oscillator's accumulator/noise hardware; it is
not a global chip reset or an extra digital mute. SID envelope rates/cutoff are
register values, not promises of linear milliseconds or Hz across both models.

### Manual versus MIDI ownership

In MIDI source, note + tuning + bend determines the rounded/saturated 16-bit
frequency register using the selected clock, and MIDI owns the gate. The saved
manual frequency and hold are ignored. In Manual source, the exact raw frequency
register and explicit hold own those controls; notes, sustain, bend and tuning do
not compete with them. Manual oscillators with hold off can still serve as
sync/ring sources while another voice or EXT IN clocks the chip.

Changing any source clears all held MIDI notes/bend/pedal and chip performance.
Selecting Manual with hold already on intentionally arms that voice. A global
Panic, host reset, transport stop, routing/model/clock change or valid patch
restore disarms all manual holds while leaving their saved parameter values
intact. Toggle hold off and on to rearm. An unrelated parameter edit cannot
restart a disarmed hold. CC120 hard-mutes the addressed manual or MIDI voice;
MIDI note-on restores a MIDI voice, and a manual hold edge restores a manual
voice. CC123 affects MIDI notes; it does not change an explicit manual hold.

All sound/routing parameter targets are read at audio callback boundaries.
JUCE's AudioProcessor/APVTS interface does not deliver within-block VST3 parameter
point offsets. Do not claim sample-accurate automation: MIDI events retain their
sample offsets, and hardware controls intentionally step without smoothing.
Only the post-chip Output gain uses a 10 ms ramp. Model/clock edits reset all
performance instead of retuning a live envelope underneath the patch.

### Core and real-time configuration

Exactly one libresidfp SID supplies the three oscillators and shared filter.
The pinned upstream model setter allocates DAC tables and locks a global cache;
its sampling setter allocates a resampler. `SidBackend` captures both models'
lookup tables at construction and prepares PAL/NTSC resamplers outside playback.
Audio-thread configuration copies fixed tables and swaps retained resamplers,
without calling those upstream setters. These caches contain no extra SID core.
CMake makes a build-tree source copy with one explicit `SidBackend` friend
addition to `SID.h`; downloaded pinned sources stay unchanged. See third-party
notices for the adapter contract.

The wrapper retains the 5 Hz DC blocker, one-second analog settling during
preparation, and upstream minimal deterministic dither level 1 (level 0 has an
undefined negative shift). Normal release retains a small noise floor. Panic
mutes exactly with no EXT IN signal. Upstream reset preserves oscillator
accumulators, so reset does not promise phase-aligned unison.

### External input bus

An optional **EXT IN** main audio-input bus defaults disabled and accepts mono
or stereo, with mono or stereo output. Stereo is averaged to mono, finite input
is clamped to ±1 and converted to signed 16-bit samples; nonfinite input becomes
zero. Each host input frame is held through the SID cycles needed to produce
one output sample (causal sample-and-hold upsampling). The signal goes through
the one chip's EXT IN path, routing bit and master volume. No MIDI gate is needed.
Panic does not disconnect a continuing external signal. No extra digital effect
or separate filter is substituted. The tested Renoise native/API workflow provides no instrument input
destination; the optional bus remains disabled there. See `RENOISE_MCP.md`.

## Playing modes and ownership

- **3 parts:** each oscillator is monophonic on its selected channel. Assigning
  the same channel to several voices intentionally plays them together.
- **Layered:** the selected input channel drives all three oscillators. Other
  channels are ignored. Each oscillator retains its own sound/tuning settings;
  routing does not replace those with voice 1's patch.
- **Last-note priority:** each channel keeps up to all 128 MIDI pitches in
  recency order. A positive note-on moves that pitch to the front of priority
  and requests a SID gate off/on retrigger. Releasing the current pitch falls
  back to the most recently held or sustained pitch, with the gate kept high
  (legato). Releasing a noncurrent or unmatched pitch does not interrupt it.
- **Repeated pitches:** positive note-ons are counted per pitch. Each matching
  channel/pitch note-off consumes one count. The pitch remains held until its
  count reaches zero. Counters saturate at 2^32−1 rather than wrapping; further
  note-ons at that limit are ignored. Storage and work remain bounded.
- **Velocity:** zero is a note-off; positive 1–127 use the same level. No extra
  per-voice digital gain or velocity-to-envelope mapping is introduced.
- MIDI note/controller events take effect at their sample offsets. All patch
  parameter targets are sampled at callback boundaries, following the
  current JUCE processor interface. Changing any mode/channel selector clears
  all notes, sustain and bend and resets the chip at the next boundary, even
  if the changed selector belongs to the inactive mode.

## Controllers and reset

| Message | Policy on the assigned channel |
| --- | --- |
| CC64 sustain | Values ≥64 hold released pitches; pedal-up discards released pitches and falls back to held keys, or releases the gate |
| Pitch wheel | Fixed −2 to +2 semitones, center 8192; both endpoints reach the full range; applies to current and subsequent notes |
| CC120 All Sound Off | Clear ownership and sustain; hard-mute the channel's assigned oscillators; retain bend |
| CC123 All Notes Off | Clear key counts, respecting sustain; normal envelope release when pedal is up |
| CC121 Reset All Controllers | Center bend, release sustain; physically held keys remain |
| Program change, pressure, RPN, other CC, SysEx | Ignored; bend range is not changed by RPN |

CC120 never resets another sounding/releasing voice. It disables the target's
waveform and asserts TEST. The common filter/DC tail can remain while another
voice is active; a single shared filter cannot be reset independently per part.
When all oscillators are hard-muted, the core is reset and output is exactly
silent unless a continuing EXT IN signal is supplied. A subsequent note restores
a MIDI voice; a manual hold off/on edge restores a manual voice.

MIDI System Reset (0xFF), editor Panic, host reset, transport playing→stop,
prepare/reprepare and valid state restore clear all channels, sustain and bend.
Global resets intentionally cut release tails. UI/reset requests use an atomic
flag; only the audio thread touches the prepared engine during processing.
Preparation/core/resampler allocation take place outside `processBlock`.

## Stage 4 editor performance state

The Rack retains all 67 IDs/version hints and schema 3. Its MIDI-source audition
keyboard sends one monophonic note and an explicit voice mask through a
lock-free atomic intent mailbox; UI events take effect at the next callback
boundary. Host MIDI still runs at its sample offsets. Audition overrides only
its target voice(s) and release returns to any still-owned host note/bend.
Manual-source voices ignore keyboard audition and retain raw frequency/Hold.
Reset/recall/source/routing/model/clock/stop cancel intent and advance an epoch,
so stale editor key releases/requests cannot rearm a reset note. CC120 affecting
an audition target cancels audition; other host parts retain channel isolation.

Audition state, live gates, target selection, Advanced visibility and editor size
are transient and absent from patch XML. The editor reads only atomic parameter
values and a block-end gate snapshot; the audio callback never accesses UI or
`MidiKeyboardState`. Editor close releases audition without clearing host-owned
notes. See `RACK.md` for numeric units and user-facing behavior.


## Stage 5 host contract

Version 0.5.0 preserves all 67 parameter IDs, their version hints and schema 3.
The host tail declaration is 30 seconds, allowing the roughly 24-second maximum
hardware release plus ADSR/filter settling. Note-off retains that release;
Panic/reset and transport stop still intentionally clear it. The declaration is
conservative for short patches and does not imply a held gate will end itself.
Advanced-panel focus is requested only when its destination is showing.
