# Implementation plan

Status: Stage 5 implementation is available as the first Linux preview,
**0.5.0**, retaining the native Rack, 67 parameters and schema 3. Available
checks pass: pluginval level 10, Release tests, ASan/UBSan/leak detection,
callback guards and actual Renoise workflows. Packaging and examples are
included. Renoise Demo offline export and additional in-Renoise device rates/
buffers remain environment-limited acceptance checks; see docs/RELEASE.md and
docs/STATUS.md. The user reports Stage 5 works fine. A subsequent numeric-entry
colour correction is rebuilt/installed and left for user testing; no tests
were run or added for that correction, as requested.
The subsequent noise-lock recovery fix is described in `docs/STATUS.md`;
returning to Noise alone now reseeds a locked generator in isolation, retaining
the existing parameter/state contract and combined-waveform behavior.
The current source version is **0.5.1** for that patch; the 0.5.0 validation
records and local archives remain historical checkpoints.

## Working agreement

Each stage should finish with something concrete to build, hear or inspect.
Keep scope small enough that a stage can be resumed from the repository files
in a fresh chat. Stages are milestones, not promises about chat length or hours.
Complete the active stage and its checks before broadening the implementation.

The first useful result is a SID note playing in Renoise. A polished editor
comes after the audio and MIDI foundations have been proven.

## Architecture

`Renoise note events + automated parameters -> MIDI/performance state -> SID
register updates -> one SID core -> audio output`.

Use small modules with clear responsibilities:

- **SidEngine:** wraps the selected library, its clock, model, register access,
  audio input/output and sample-rate conversion. Has no editor dependency.
- **MidiRouter:** turns channel-aware note events into three oscillator states,
  with explicit monophonic note priority and release behavior.
- **Parameter definitions:** stable IDs, defaults, ranges and conversion to SID
  register values. Shared by processor, editor and serialization.
- **PluginProcessor:** connects host events, automation, audio buses and state.
- **RackEditor:** reusable controls following the approved layout.

Keep one core for the entire chip. Creating a complete SID per MIDI note or per
part would change the agreed oscillator interactions and shared-filter behavior.

## Stage 1 — First sound in Renoise

- [x] Record the local Linux architecture/toolchain and installed Renoise version.
- [x] Verify JUCE + libresidfp integration and pin exact dependency releases or
  commits. Select the project license and retain third-party notices.
- [x] Establish CMake targets for the Linux VST3 and a small offline audio test.
- [x] Produce one audible oscillator with a simple initial patch, note-on,
  note-off and a minimal development editor.
- [x] Include a safe reset/panic path and an initial stable parameter/state schema.
- [x] Document build and local installation commands. Keep generated builds
  inside the project; install into the user's VST3 location only as needed for
  the authorized host test.

**Done when:** a clean build succeeds, an offline note render contains valid
audio with the expected pitch and release, and the user can load the bundle and
play a note in Renoise. Record the host check separately if user audition is pending.

## Stage 2 — The two playing modes

- [x] Implement **3 parts**, defaulting to MIDI channels 1, 2 and 3, with a channel
  selector for each voice.
- [x] Implement **Layered**, with one selected input channel driving all three
  oscillators and retaining each oscillator's tuning and patch settings.
- [x] Define last-note priority, held-note fallback, repeated pitches, note-off
  matching, and note-on velocity zero behavior. Decide/document any initial
  sustain-pedal and pitch-bend support rather than silently accepting events.
- [x] Clear held-note ownership safely on mode/channel changes and host reset.
- [x] Respect within-block event timing and preserve fractional SID clock time
  across host blocks.
- [x] Document a Renoise setup using one instance plus two plugin aliases,
  assigned to channels 1–3.

**Done when:** three simultaneous parts stay independent, releasing one does
not release another, layered mode plays the intended combined patch, and
equivalent event timelines remain consistent across different block sizes.

## Stage 3 — Complete SID controls, automation and recall

- [x] Per oscillator: all waveform bits and combinations, frequency/tuning,
  12-bit pulse width, four envelope parameters, gate, sync and ring modulation.
- [x] Shared filter: 11-bit cutoff, resonance, routing for all voices and the
  external input, and independent low/band/high-pass mode bits.
- [x] Global/advanced: 4-bit SID master volume, 6581/8580 model, PAL/NTSC clock,
  per-oscillator test/reset and voice 3's direct-output switch.
- [x] Define MIDI-driven versus manual-frequency/gate behavior so competing
  controls have clear ownership. Keep hardware resolution in register writes.
- [x] Add the optional external audio-input bus and verify how Renoise routes
  audio to it. If the intended host cannot expose it, record the limitation
  before changing the scope or interface.
- [x] Expose musical controls to host automation. Preserve intentional stepped
  hardware behavior rather than applying indiscriminate smoothing.
- [x] Restore versioned patch state on song reload, keeping transient MIDI
  note ownership separate. Handle missing/unknown state fields safely.

**Done when:** meaningful register-mapping and timing tests pass; automation and
saved patch state work; shared routing, sync and ring modulation have been
auditioned; and host saving/reloading restores the patch without stuck notes.

## Stage 4 — Rack interface

- [x] Implement the selected layout with native, scalable controls: three voice
  strips, one shared filter module, top-level play mode/model controls and the
  audition keyboard.
- [x] Make MIDI channel assignments editable. Make the selected chip model and
  the current mode unmistakable.
- [x] Connect every visible control to its real parameter or performance action.
  Preserve independent waveform and filter-mode switches.
- [x] Give advanced hardware controls a compact, discoverable area. Distinguish
  a live gate indicator from an explicit manual hold control.
- [x] Support numeric entry, clear units/ranges, keyboard navigation and useful
  tooltips. Give the audition keyboard an explicit target voice in 3-part mode.
- [x] Verify actual sizes, display scaling and editor reopen behavior in Renoise.
- [x] Update the Figcraft design when implementation resolves a design question.

**Done when:** the user can make and recall sounds through the Rack editor,
every displayed control works, and there are no clipped labels or ambiguous
state indicators at the tested Linux display scales.

## Stage 5 — Host hardening and first release

- [x] Exercise Renoise scan/load, aliases/channels, automation, stop/start,
  song reload, multiple instances and editor open/close. Record the unavailable
  Demo offline render and untested Renoise rate/buffer matrix explicitly.
- [x] Check 44.1/48/96 kHz, representative and changing callback sizes,
  finite samples, exact timing/continuity, note ownership and recall in the
  real processor and pluginval. Advertise a tail long enough for slow releases.
- [x] Run pluginval level 10 and instrument the core, processor, editor and
  compiled JUCE modules with ASan/UBSan plus leak detection.
- [x] Measure Release CPU use and retain bounded callback allocation/lock guards.
  Include fresh idle, released notes, both models and stereo EXT IN.
- [x] Package the Linux VST3 with install/removal instructions, exact dependency
  pins, corresponding source archives, offline build instructions and licenses.
- [x] Add three native demonstration patches and a documented one-instance/
  two-alias Renoise example, verifying native recall and actual playback.

**Implementation outcome:** 0.5.0 is a local preview release with the available
checks complete. The user has confirmed the prior Renoise playing workflow and
Rack behavior. New demo sound-quality approval, licensed Renoise offline export
and other sample-rate/buffer configurations inside Renoise remain explicit
acceptance follow-ups. No additional product scope or public upload is implied.

## Scope for a later decision

Additional plugin formats, modulation tables/arpeggiators, extended polyphony,
effects, a SID-file player/exporter and additional operating systems are outside
this implementation plan. The user has explicitly chosen Linux only.

## Questions to settle during the relevant stage

These do not block the planning baseline:

- Stage 1: exact dependency versions, project license and Linux build baseline.
- Stage 2 (settled): fixed positive velocity level, ±2-semitone bend, CC64
  sustain, counted repeated pitches, last-note priority and held-note fallback.
  See `docs/STATE.md` for exact policies.
- Stage 3 (settled): explicit MIDI/Manual source, exact manual frequency/hold,
  disarming on reset/recall, callback-boundary hardware automation and optional
  EXT IN. Native Renoise input routing is unavailable in the tested workflow.
  See `docs/STATE.md` and `docs/RENOISE_MCP.md`.
- Stage 4 (settled): 1120×780 default, 80–150% native scaling; targeted
  transient audition with host-note fallback and reset epochs. See `docs/RACK.md`.

## Primary references

- [libresidfp](https://github.com/libsidplayfp/libresidfp)
- [JUCE licensing](https://github.com/juce-framework/JUCE/blob/master/LICENSE.md)
- [Renoise VST3 preferences](https://tutorials.renoise.com/wiki/Preferences)
- [Renoise manual: plugin aliases and routing](https://files.renoise.com/manual/Renoise%20User%20Manual.pdf)
- [VST3 note-on event fields](https://steinbergmedia.github.io/vst3_doc/vstinterfaces/structSteinberg_1_1Vst_1_1NoteOnEvent.html)
