# Renoise MCP setup and host checks

Current validation: **16 November 2026**, SIDSynth **0.5.0**, Renoise **3.5.4
Demo**, JACK/PipeWire **48 kHz / 1024 frames**. Final dedicated scratch host:
**PID 81061**, stopped on `sidsynth-stage5-audition.xrns`, Rack open, manual holds
off, speaker links restored. Stage 2 listening and Stage 4 Rack behavior are
user-confirmed. New demo sound-quality audition is not separately recorded.

## Stage 5 results and commands

`tools/check_renoise_stage5.py` reuses the Stage 4 ready-main-window guard,
PID-checked MCP client, scoped stereo recorder and temporary speaker isolation.
It records only that Renoise PID; other playback remains independent. System
audio settings are unchanged.

```sh
python3 tools/check_renoise_stage5.py --pid 81061
```

Passed on the exact installed release module:

- One instance/two aliases with independent release, native cutoff automation.
- A second independent instance, five editor cycles per instance, complete
  two-instance song recall, silence on recall and three stop/start cycles.
- Three native XRNI demo instruments save/load and restore all 67 displayed
  values, with finite bounded recorded playback.
- The native three-part song is reloaded, then its one-instance/two-alias links
  are explicitly checked again before recording the final example.
- The Demo render guard returns unavailable without calling the disabled host
  renderer. This is not a successful offline-render check.

The extension adds scratch-only `save_instrument` and `load_instrument`, using
Renoise's documented [Application API](https://renoise.github.io/xrnx/API/renoise/renoise.Application.html).
**Loading an XRNI over instrument 00 drops its Renoise aliases.** The final
runner tests patch recall in the empty independent slot 03 and unloads it after
capture. The exported three-part example retains exactly one real plugin plus
aliases 01/02; it is checked again after native song reload. Earlier test exports
with dropped aliases were replaced.

`--resume-examples` requires results from the same PID and exact module hash,
including the preceding passed multiple-instance/recall/stop checks. The final
release run uses the complete runner. Do not resume from a fixture whose
primary instance was replaced and aliases lost.

Normal window-close checks use:

```sh
python3 tools/close_renoise_scratch.py --pid 81061 --name sidsynth-stage5-checkpoint
```

This requires the ready scratch window and matching MCP PID, stops/saves the
song, sends WM_DELETE_WINDOW and waits at most 20 seconds. It never force-kills.
Four saved normal shutdowns/restarts passed: PIDs **65130 / 76089 / 78211 /
80042**, about three seconds each, including an open Rack on the final module.
The current host 81061 is retained for audition rather than closed again.

Evidence: `stage5-host-results.json`, `stage5-mcp-transcript.jsonl`,
`stage5-host-release.log`, `stage5-*.wav`, `stage5-shutdown-*.log` and
`renoise-console-stage5*.log`, all under `work/renoise-host/`.
Native examples ship in `examples/`. Final module SHA-256:
`7795624b57b326eeb5a404153e346dcfcd96bcac12f7f22039351edb324f6794`.
Broader rate/buffer transitions run offline/in pluginval; actual Renoise remains
at 48 kHz / 1024. See [release limits](RELEASE.md).

## Stage 4 results and commands (historical 0.4.0)

`tools/check_renoise_stage4.py` runs actual X11/XTest editor interactions and
reuses the PID-scoped ReMCP/playback recorder. Requires libX11, libXtst,
ImageMagick `import`, NumPy/SciPy and PipeWire tools. It checks the scratch PID,
song and plugin before mutations, detaches only that PID's monitor links while
recording, then clears holds/notes and restores the links, including on failure.
It never records a microphone/system mix or operates another song.

**Do not inspect through MCP while Renoise is starting or showing recovery.**
The first Stage 4 attempt queued a read of `song.rendering` during
`OnSetNewSong`, producing a stack in Renoise's `TRenoiseLuaSong::Rendering`.
The user reported recovery Yes crashing again. The separately saved
`sidsynth-before-stage4.xrns` allowed recovery No and safe resumption. The
runner first requires a scratch `.xrns - Renoise` main-window title, then waits
one second before API access. A deferred song load likewise waits across the
main-window transition. Save the altered fixture before requesting reload, so
Renoise does not stop at a Save changes dialog.

After the main GUI is fully loaded:

```sh
python3 tools/renoise_mcp.py call sidsynth_host_check '{"operation":"inspect"}'
python3 tools/check_renoise_stage4.py --pid 65130
```

The native connector's transport was stale after the first process restart;
this task used the same local HTTP/MCP client. The full interaction run plus
layout/recall resumes retain passing checks for the identical final module.
Resume only with the existing matching `stage4-host-results.json`:

```sh
python3 tools/check_renoise_stage4.py --pid 65130 --resume-layout
python3 tools/check_renoise_stage4.py --pid 65130 --resume-recall
```

**Passed, automated by the agent inside Renoise:**

- Real Triangle/Saw combination click, typed cutoff **820**, and editable
  audition target. With V1/V2 on channel 1, the mouse keyboard auditions only
  V2; its live gate lights and a recorded A4 is present.
- Manual source, raw frequency display and actual Hold click; Panic on press
  records **0.0 RMS**, while the saved Hold parameter remains selected.
- Actual Advanced panel and V1 TEST clicks reach the host.
- Native sizes 80/100/125/150%, visually inspected by the agent. Actual X11
  sizes were **897×623 / 1120×780 / 1400×975 / 1680×1170**. The smallest size
  differs by one pixel per axis from its request. Five editor reopen cycles
  return to **1120×780**. These are one display/compositor setup, not every DPI.
- Song recall restores all **67 displayed parameter values**, including a saved
  on-valued but disarmed manual Hold, and records silence until fresh input.
- Normal pattern playback is captured, finite/bounded and stereo-consistent
  after recorder setup. The first 100 ms is excluded from stereo/analysis checks
  because linking two already-sounding output ports is sequential; original
  complete WAVs are retained. No Demo offline-render operation is made.
- `stage4-host-results.json` has `passed: true`. Final module SHA-256 is
  `d86bdbf4c54568df52b0d8b1dd107607f32c028180eafb8d42db44c2198649d0`.

During early failed checks the user stopped an irritating sustained test tone.
That is evidence of hearing the test, not a sound-quality approval. The runner
now isolates the scratch monitor while recording and cleans up on failure.
Renoise's initial mouse-up Panic did not silence the host in the test; the final
Panic acts on press and was proven silent with the actual host recording.
Several driver assumptions were corrected: wait for window activation, select
size-menu rows directly (mouse hover affected arrow selection), allow one-pixel
host rounding, and save changed fixtures before loading the reference.

An intermediate saved/plugin-unloaded scratch host (PID 61828) became
unresponsive during restart and did not exit on SIGTERM. Only that process was
killed after its checkpoint was confirmed. Its cause remains unresolved for
Stage 5 host shutdown checks. Final SIDSynth-only standalone lifecycle and five
actual host editor reopen cycles pass; do not equate these with a full validator.
No external Vital investigation or ptrace-setting change was performed.

Evidence stays in ignored `work/renoise-host/`: `stage4-host-results.json`,
`stage4-mcp-transcript.jsonl`, `stage4-host-{checks,layout,recall}.log`, actual
`stage4-*.png` editor/menu captures, and `stage4-host-*.wav` recordings.
Saved fixtures: `sidsynth-before-stage4.xrns`, `sidsynth-stage4-checkpoint.xrns`,
`sidsynth-stage4-recall.xrns`, `sidsynth-stage4-before-recall.xrns` and stopped
`sidsynth-stage4-audition.xrns`. Startup logs are `renoise-console-stage4*.log`.

## Historical Stage 3 results and commands

`tools/check_renoise_stage3.py` reuses the PID-scoped playback recorder from
`tools/check_renoise_host.py`. Its MCP helper checks both the scratch file root
and that the endpoint's `/proc/self/stat` PID matches the requested recorder PID.
It requires one instance plus two aliases and exactly 67 exposed parameters.
Source: `tools/renoise/SIDSynthHostChecks.lua` adds native automation, unload and
input-inspection operations to the existing scratch-only extension; no general
Lua execution operation or unrelated song mutation was introduced.

```sh
python3 tools/renoise_mcp.py call sidsynth_host_check '{"operation":"inspect"}'
python3 tools/check_renoise_stage3.py --pid 55689
```

The runner changes only the dedicated scratch song, preserves separate recall
and audition fixtures, records normal playback from only that PID's output
ports and stops playback after each recording. It makes no Demo render request.
The previous user audition settings were saved separately as
`sidsynth-before-stage3.xrns` before the old plugin was unloaded/restarted.
The new host loaded that schema-2 song, retaining gain/routing and defaulting all
new SID fields. A separate 0.3.0 install-check bundle and manifest were verified.

**Passed, automated by the agent inside Renoise:**

- 67 parameters exposed with the existing IDs/identity preserved.
- Sync and ring change recorded audio with voice 3 direct output off. Relative
  spectral differences against controls: **1.2502 / 1.2639**.
- A real native **Instr. Automation** device targets Filter cutoff. Its point
  envelope at lines **1 / 9 / 17** sends **1 / 0.015 / 1** normalized cutoff.
  Open / closed / reopened RMS: **0.006571 / 0.001209 / 0.006569**.
- Both models and both clocks play A4; all four combinations were captured.
- Manual hold sounds without MIDI/keyboard focus, transport stop disarms it,
  an unrelated pulse-width edit stays silent, and an explicit hold edge rearms.
- A nontrivial patch saves/restores **all 67 displayed values**, including all
  oscillator/filter controls, model/clock and an on-valued manual hold. The
  recalled patch is silent until a fresh note or explicit hold edge.
- Five native editor open/close cycles. The software workbench snapshot was
  inspected separately; other in-host scales remain untested.
- A stopped MIDI Layered audition song and short playable pattern are retained
  as `sidsynth-stage3-audition.xrns` / `stage3-host-audition.wav`.

Two test assumptions were corrected without changing production code. A rapid
manual Hold off/on can fall within one callback and collapse to the original
value, so the runner now waits **80 ms** before rearming. Also, Renoise can retain
an unsnapped normalized parameter value it sent (cutoff 0.46), while the SID saves
the exact rounded hardware value (942/2047). The recall assertion compares exact
displayed values at their defined hardware/musical resolution. Resume commands
retained earlier passing captures instead of repeating unrelated checks:

```sh
python3 tools/check_renoise_stage3.py --pid 55689 --resume-from-manual
python3 tools/check_renoise_stage3.py --pid 55689 --resume-recall
python3 tools/check_renoise_stage3.py --pid 55689 --audition-only
```

The last command only rebuilds/records the audition fixture. Resume modes require
the preceding retained results/fixtures and do not imply a fresh complete run.
The actual final `stage3-host-results.json` has `passed: true` with those combined
checks, the module SHA, measured values, input probe and source port metadata.

### EXT IN host limitation

The optional audio input is implemented and tested offline with mono/stereo
layouts, input-only sound, shared routing/volume and block-size consistency.
The actual Renoise native `#Sidechain` probe exposes only **Amount, Panning,
Dest. Track and Dest. Effect**. Available track output routes are **Master** and
**Output 01 Left/Right**, with no plugin instrument input destination. This agrees
with the [Renoise Sidechain documentation](https://tutorials.renoise.com/wiki/Routing_Devices#Sidechain),
which describes an effect in another track or FX chain, and the
[Instrument Automation documentation](https://tutorials.renoise.com/wiki/Meta_Devices#Instrument_Automation)
provides control rather than audio routing.

**No working native instrument-input route was exposed through this tested
native/API workflow.** GUI receiver-list selection and third-party routing
wrappers were not exercised. Do not claim live EXT IN validation in Renoise or
silently add an effect-format build to work around it. The optional input remains
disabled by default, with host support required to activate it.

Evidence stays in ignored `work/renoise-host/`:

- `stage3-host-results.json`, `stage3-mcp-transcript.jsonl`.
- `stage3-host-checks.log`, `stage3-host-checks-manual.log`,
  `stage3-host-checks-recall.log`, `stage3-host-audition.log`.
- `stage3-host-*.wav`, `stage3-input-probe.json`, `stage3-recall-expected.json`.
- `sidsynth-before-stage3.xrns`, `sidsynth-stage3-recall.xrns`,
  `sidsynth-stage3-before-recall.xrns`, `sidsynth-stage3-audition.xrns`.
- `renoise-console-stage3.log`, `renoise-stage3.pid`.

Historical Stage 3 build/installed/host-tested module SHA-256:
`6c8b5b9108b0fde5880f63a3adc45ecff9e3793aace22cdf69f85c8ecc11ebe5`.
Subjective listening for new controls, in-host UI Panic click, broader display
scales and full validator/profiling checks remain unconfirmed. The user has
confirmed Stage 2 listening/display, separately from these agent-automated checks.

## Connection

The user-requested [ReMCP tool](https://www.renoise.com/tools/renoise-mcp) is
installed at:

`/home/dragos/.config/Renoise/V3.5.4/Scripts/Tools/com.renoise.ReMCP.xrnx/`

Codex has a global MCP entry named **`renoise`** using Streamable HTTP at
**`http://127.0.0.1:19714/mcp`**. Check it with `codex mcp get renoise`.
This work used `tools/renoise_mcp.py` to send MCP initialize/tools calls
to that same server. No cloud service or npm bridge is involved.

The server runs inside Renoise, so keep the test Renoise instance open. The
current test instance is PID **81061**, with `sidsynth-stage5-audition.xrns`
loaded and playback stopped. PID 65130 is historical Stage 4. PID 55689 is historical Stage 3. PID **46878** was restarted after its
scratch state was preserved. The earlier unrelated instance was left alone. Always identify the instance hosting the endpoint before
editing a song. The extension refuses mutations outside the SIDSynth scratch
song; the test runner also checks its file location before starting.

Installed changes from the downloaded package:

- Bind both server creation paths to **127.0.0.1**, rather than every interface.
- Load tools and start port 19714 after the tool finishes loading. The normal
  **Tools → Renoise MCP...** panel remains available for server control.
- Add `tools/sidsynth_host_checks.lua`, sourced from
  `tools/renoise/SIDSynthHostChecks.lua`, plus a scratch-root configuration file.
  The extension exposes plugin loading/parameters/channels, native editor
  visibility, scratch song save/reload, and the native MIDI Control setup.
- Guard the render operation in Demo builds: return an unavailable result
  instead of asking the host to perform its disabled render operation.

The original Stage 2 MCP setup did not change production SIDSynth code. Stage 3
changes are documented above. Renoise audio preferences, Vital and system audio
configuration were left unchanged.

## Pinned tool package and reproducible install

Package: **ReMCP manifest version 0.1, API 6**. Its protocol handshake reports
server version 1.0.0; this is upstream metadata, not a different package.

Source archive:
`https://www.renoise.com/uploads/tools/renoise-mcp/com.renoise.ReMCP_v0.1_api6.xrnx`

SHA-256:
`a2cbf69680c3a1d39860433e28f3d4edb6bb7bf27260377ef77cea4f061c1015`

The archive is retained in ignored `work/renoise-mcp/`. It contains no license
file. ReMCP is an externally installed development tool and is not bundled with
the SIDSynth instrument or added to its runtime dependencies.

From the project root, for a machine without an existing ReMCP installation:

```sh
mkdir -p work/renoise-mcp
curl -fL https://www.renoise.com/uploads/tools/renoise-mcp/com.renoise.ReMCP_v0.1_api6.xrnx \
  -o work/renoise-mcp/com.renoise.ReMCP_v0.1_api6.xrnx
python3 tools/install_renoise_mcp.py work/renoise-mcp/com.renoise.ReMCP_v0.1_api6.xrnx
codex mcp add renoise --url http://127.0.0.1:19714/mcp
```

The installer verifies the pinned hash, applies the changes above and refuses
to overwrite an existing tool directory. Its default destination is the XDG
Renoise 3.5.4 Tools directory; `--destination` supports another installation or
an isolated staging check. A staging installation and Lua/Python syntax checks
were performed by the agent.

Start Renoise with a new, empty song, then check the connection:

```sh
python3 tools/renoise_mcp.py initialize
python3 tools/renoise_mcp.py list
python3 tools/renoise_mcp.py call sidsynth_host_check '{"operation":"inspect"}'
```

To update just the local extension after editing its project source, copy it to
`tools/sidsynth_host_checks.lua` in the installed bundle, then call
`sidsynth_host_check` with `{"operation":"reload_tools"}`. Do not replace an
unrelated tool or operate on a user's working song.

## Create a scratch alias fixture

On an empty song, the extension's prepare operation verifies that there are no
samples, loaded plugins or pattern data before tagging the song:

```sh
python3 tools/renoise_mcp.py call sidsynth_host_check '{"operation":"prepare"}'
python3 tools/renoise_mcp.py call sidsynth_host_check '{"operation":"load_plugin","instrument":0}'
python3 tools/renoise_mcp.py call sidsynth_host_check '{"operation":"save","name":"sidsynth-seed"}'
python3 tools/create_renoise_alias_fixture.py
python3 tools/renoise_mcp.py call sidsynth_host_check '{"operation":"load_song","name":"sidsynth-aliases"}'
```

Renoise exposes `alias_instrument_index` as read-only in its Lua API. The fixture
helper therefore changes only the two alias links in a scratch XRNS derived
from Renoise's own saved seed. It verifies that there is exactly one SIDSynth
plugin device, then links instruments 01/02 to 00 and sets channels 1/2/3.
After loading, the host API confirmed alias indices **0/1/1** (Lua indices),
channels **1/2/3**, and the same plugin device path for all three instruments.
There are no additional SID plugin instances in the fixture.

## Historical Stage 2 live checks (0.2.0)

Use the PID of this dedicated test Renoise process. Do not open a different song
in that instance while the runner is operating it.

```sh
python3 tools/check_renoise_host.py --pid 46878
```

The runner requires NumPy, SciPy, `pw-dump`, `pw-record` and `pw-link`. It maps
that PID to its PipeWire client/node and explicitly links only its two output
ports to a temporary recorder with autoconnection disabled. It does not record
the microphone, system output mix or the other Renoise instance. Capture links
are removed when the recorder exits. MIDI/parameter changes use MCP calls;
pattern playback is scheduled by Renoise. GUI-triggered calls are not a
sample-accurate event-timing measurement.

The actual work used an initial run, then `--resume-from-layered` after correcting
an invalid test assumption about oscillator phase, then `--midi-only` for the
native controller checks. Previously passed captures were retained instead of
rerunning unrelated cases. All completed results were combined in
`work/renoise-host/host-results.json`.

### Historical Stage 2 results

All of the following **passed, automated by the agent inside Renoise**:

- Instrument load and exposed gain/mode/channel parameters; normal note playback
  and envelope release. Pattern `A-5` is MIDI note 69 / 440 Hz in this host's
  octave convention. The solo held RMS was **0.00648**, tail **0.0000304**.
- One instance with two aliases: three simultaneous pitches, independent
  note-offs and continued audio in the remaining parts. Released components
  fell to about **0.000032** while held components remained about **0.00725**.
- Overlapping-note fallback and counted repeats using separate note columns.
- Layered mode compared with three parts assigned to the same channel: steady
  RMS ratio **0.99997**, with matching fundamental/harmonic magnitudes.
- Layered input channel 16 plays; the old input channel is ignored.
- Mode/channel changes during held notes and transport stop clear the sound.
- Native `*Instr. MIDI Control`: pitch bend moves A4 to B4 (+2 semitones) while
  another alias remains at its original pitch; recentering restores A4.
- Native CC64 holds the assigned channel after note-off while the other alias
  releases; pedal-up produces a quiet tail (RMS **0.0000408**).
- Saving while a note is playing, changing parameters, and reloading restores
  Output **−24 dB**, Layered, input channel **16** and voice channel assignments
  **16/10/15**, without restoring held notes. The reloaded patch plays again.
- Five native plugin-editor open/close cycles, queried through the host API.

libresidfp preserves oscillator accumulators on chip reset. After the voices
have played different pitches, three unison saws need not sum to three times a
solo voice's RMS. The first layered assertion assumed that and was rejected;
it was replaced with a shared-channel three-voice reference. Production audio
code was not changed to make the test pass.

### Limits and retained evidence

**At the Stage 2 checkpoint, not confirmed:** sound-quality/listening and
layout judgment (subsequently user-confirmed), clicking the UI Panic button,
editor layout at other display scales,
other host sample rates/buffer sizes, or a full plugin validator. Host editor
visibility is confirmed by API, not by a screenshot or visual inspection.

**Offline host rendering is unavailable in Renoise Demo.** The initial attempt
logged `Rendering is disabled in the demo version of Renoise.` and then the
host crashed (PID 46046) while recursively centering its error dialog. The core
report's faulting stack is in Renoise's window/X11 code, not a plugin callback;
this does not independently establish every contributing cause. No host or
Vital fix was attempted. The extension now detects Demo and avoids the call.
Normal live playback capture subsequently completed the checks above.

Evidence stays in ignored `work/renoise-host/`:

- `host-results.json`, `mcp-transcript.jsonl` and the three `host-checks*.log`
  files contain results, tool calls and progress.
- `host-*.wav` files contain the live captures. `host-three-parts.wav` and
  `host-layered.wav` are useful audition references.
- `sidsynth-recall.xrns` is the stopped recall example left open in Renoise;
  `sidsynth-midi-tests.xrns` retains the controller setup and test pattern.
- `render-denial-core-info.txt` records the single failed render attempt.
- `renoise-console*.log` records launch/load context and host audio settings.

Historical 0.2.0 module SHA-256 (superseded by 0.3.0 above):
`f0e46d309c86f0a4696036cc4bc3255891bd5dd772d128440dff717943e9e166`.
