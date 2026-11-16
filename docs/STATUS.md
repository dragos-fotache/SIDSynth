# Current status

Updated: 16 November 2026.

Repository: the user initialized local Git on 16 November, after Stage 5, and
requested the initial commit and push to `origin`:
`git@github.com:dragos-fotache/SIDSynth.git` (branch `master`). The initial
checkpoint includes source, designs, notices, documentation and the demo
instruments/song. Generated build/work directories, dependency downloads and
machine-local settings are excluded in `.gitignore`. No further tests are run
for this checkpoint; the numeric-entry correction awaits user validation.

## Confirmed by the user

- Linux only; open-source plugin; initial format VST3; first host Renoise.
- Selected interface: Rack. Its design files are unchanged by Stages 1 and 2.
- Implement in stages with durable project context.
- User requested Stage 5 implementation on 16 November; preview release below.
- User requested GitHub Actions Linux builds and a release workflow that they
  will run manually. Keep automated tests disabled in these workflows.
- On 16 November the user reports Stage 5 works fine, and identifies white text
  during numeric entry. The user will test this UI correction manually and
  explicitly requests no agent tests or new tests.
- On 16 November the user confirms Stage 2 human listening is fine and the
  display looks good at their current setup. Other scales are untested. UI
  Panic was not exercised: Renoise releases computer-keyboard notes when the
  plugin window gains focus. Confidence in the button is not a host test.
- User authorized Stage 4 implementation on 16 November; completed below.
- On 16 November the user confirms the Stage 4 Rack looks and behaves well
  at their current setup. Detailed sound-quality audition is not separately recorded.
- User authorized Stage 3 implementation, completed below. Stage 2 results
  remain historical evidence for the prior 0.2.0 build.
- User confirms SIDSynth loads and plays in Renoise, but reports a host crash
  after playing Vital and switching back to SIDSynth in the Plugin tab.
- User directs us to leave Renoise/Vital bugs alone and move on if the fault
  is external. The isolated tests below reproduce the crash with Vital alone.

## GitHub Actions setup

Added a Linux build workflow for pushes to `master`, pull requests targeting
`master` and manual runs. It uses Ubuntu 24.04, `BUILD_TESTING=OFF`, the pinned
source archives, two parallel compile jobs and seven-day downloadable binary/
source artifacts. The source package now includes `.github/`.

The separate **Manual release** workflow has only `workflow_dispatch`. It
defaults to a draft preview release and is left for the user to invoke.
Only its final release job has repository write permission. No test suite,
validator, sanitizer, host check or release workflow is run for this setup.
See `docs/CI.md` for the exact triggers, Action commit pins and user procedure.
The first automatic cloud build is pending publication of this setup.

GitHub is installed through the app connector but its MCP tools are not exposed
to this active chat. Plugin Management confirmed that connection; the already
authenticated GitHub CLI is used for repository/workflow operations.

## Numeric-entry colour correction — manual validation pending

Following the user's 16 November report, numeric entry now explicitly uses
Rack ink for both editing and selected text, a translucent green selection
background and an ink caret. JUCE's separate Label editing colour was still
using its default light text even though the ordinary value label was dark.
The slider's text-box selection colour also needs its own override because
JUCE copies it onto the editing component.

Only the editor palette changes; parameters, schema and audio code are unchanged.
No tests are added or run, and no host checks are performed, at the user's
explicit request. Build command:

```sh
cmake --build build-clean --target SIDSynth_VST3 --parallel 4
```

Release VST3 rebuild succeeded; log: `work/ui-text-colour-build.log`. The
installed module was replaced atomically so the running Renoise keeps its
existing mapping until restart. No song was operated and Renoise was not
restarted by the agent. Restart Renoise to load the correction. Previous module
backup: `work/sidsynth-before-ui-text-colour.so`.

New built/installed module SHA-256:
`cfc6dca0f6790a900dde4e063887c02875f9561905cfee0ff9696deb208c124b`.
This module has build-only verification and awaits the user's manual check.
The archived 0.5.0 packages and their recorded validation remain the earlier
Stage 5 checkpoint; those test results do not claim validation of this UI patch.

## Stage 5 implementation — first Linux preview

**Stage 5 is implemented as version 0.5.0, installed locally, with the available
checks passing.** Final host validation uses **PID 81061**. The dedicated host
is left stopped on `work/renoise-host/sidsynth-stage5-audition.xrns`, with the
Rack open, no manual holds and its speaker links restored. The sample-rate/
buffer matrix inside Renoise and licensed-host offline export remain explicit
acceptance follow-ups. The user now reports Stage 5 works fine. This confirms their current workflow;
individual demo sound-quality judgments were not itemized.

- Preserved one SID core, plugin identity, all **67 IDs/version hints** and
  **schema 3**. No additional format, operating system or musical subsystem.
- Fixed hidden-editor Advanced-panel focus requests exposed by the Debug test.
  The host now receives a conservative **30-second tail**, rather than one
  second, for the roughly 24-second maximum SID release. Panic/stop still cut
  immediately. A regression proves the slow release remains audible beyond
  one second with its gate off.
- Added `SIDSYNTH_SANITIZERS`, Stage 5 timing/reconfiguration/isolation tests,
  `--benchmark`, the PID-scoped host runner, a normal scratch-window close tool
  and deterministic binary/source packaging. Build/packaging commands and exact
  tool pins are in `docs/BUILD.md`; no runtime dependency pins changed.
- Three native XRNI patches and a documented 64-line three-part alias song are
  in `examples/`. Each native patch save/load restores all 67 displayed values.
  The example is reloaded and its one-instance/two-alias links rechecked.
- Native patch-recall tests use empty slot 03. Loading a patch over Renoise's
  primary instrument drops its aliases; the final runner explicitly avoids
  that and verifies the final saved example. Early exported examples were
  replaced before packaging.
- The local preview archives contain the VST3, license/notices, docs and demos,
  plus a matching complete source archive with both original dependency source
  archives and offline build instructions. Release archives remain local.
  See `docs/RELEASE.md` and `docs/validation-0.5.0.json`.

### Stage 5 checks

**Automated by the agent:** Release CTest **1/1 passes (10.07 s)**; final
ASan/UBSan/LeakSanitizer CTest **1/1 passes (96.16 s)** without suppressions.
The instrumented build includes the core, processor/editor and compiled JUCE
modules. Initial LeakSanitizer inspection failed inside the restricted sandbox;
normal process access allowed the complete leak-enabled run. No production
sanitizer or callback suppression was added.

New tests cover exact variable/empty callback timelines at **44.1/48/96 kHz**,
rate/buffer preparation and pitch recovery, recall while live, a slow release
and independent interleaved cores under model/clock/reset changes. All previous
MIDI/register/audio/state/editor checks remain. Callback guards still report
**0 C++ allocations, 0 wrapped C allocations, 0 frees, 0 pthread mutex locks**.
These are bounded linked-call checks, not a whole-system blocking profiler.

**pluginval 1.0.4:** strictness **10**, fixed seed **20949**, all three rates,
blocks **1/64/257/1024**, no disabled tests: **SUCCESS** for the final module.
Includes state restoration, background state, parameter thread safety, fuzzing,
bus layouts, automation and editor processing/lifetime. Its optional separate
Steinberg validator is absent/skipped; pluginval itself validates the VST3.
Five additional standalone SIDSynth-only native editor/unload cycles pass.

**Release CPU measurement:** 45 rows, all three rates × blocks **64/257/1024**
× fresh idle / filtered 6581 / filtered 8580 / stereo EXT IN / released notes.
Active CPU is **4.95–6.73% of one Ryzen 5 3600X core**; after normal note release
it remains **4.94–5.31%**, since the chip keeps running. Fresh idle is
**0.004–0.157%**. The largest measured callback took **18.0%** of its deadline.
This is a short normal-desktop sample, not a real-time guarantee or host CPU
reading. Sanitizer builds were not used for these measurements. Raw CSV:
`work/stage5-cpu-release.csv`.

**Actual Renoise 3.5.4 Demo, automated by the agent:** final-module checks pass
for scan/load, one instance/two aliases, independent channel release, native
cutoff automation, two independent instances, five editor cycles per instance,
complete two-instance patch recall with silence, three stop/start cycles and
all native demo patch save/load/playback. Output is finite, bounded and stereo
consistent after recorder setup. Only the scratch PID's output is recorded;
its speaker links are detached during tests and restored after silence. System
audio settings were left alone, allowing the user's other playback to continue.

Normal scratch-window close passes with the plugin loaded, including an open
Rack: **PID 65130: 3.21 s; 76089: 3.00 s; 78211: 3.00 s; final module PID
80042: 3.00 s**. Each has a saved checkpoint and subsequent restart; no forced
termination was used. The earlier Stage 4 hang remains unexplained, but did not
recur in these normal shutdown/reload checks. The final audition PID is 81061.

Actual Renoise remains at **48 kHz / 1024 frames**. Its Demo render probe returns
unavailable without calling the disabled renderer. Other Renoise rates/buffers
and native EXT IN routing remain unverified as documented in RELEASE.md.
The user reports Stage 5 works fine; detailed demo-by-demo listening was not
itemized. Numeric-entry correction validation is left to the user.

Final build/installed module SHA-256:
`7795624b57b326eeb5a404153e346dcfcd96bcac12f7f22039351edb324f6794`.
The extracted source archive configured from its included verified dependency
archives, built cleanly in `build-source-check/build/`, and passed CTest
**1/1 (9.92 s)**. Its C++/tests/CMake files match the final source. Final binary
and corresponding source archives are under `build-release/`; SHA256SUMS is
verified. Two identical packaging runs produced matching archive checksums.

### Stage 5 working commands

```sh
cmake --build build-clean --parallel 4
ctest --test-dir build-clean --output-on-failure
./build-clean/sidsynth_offline --benchmark > work/stage5-cpu-release.csv
python3 tools/check_renoise_stage5.py --pid 81061
python3 tools/package_release.py
cmake -E chdir build-release sha256sum -c SHA256SUMS
```

The normal close command saves then sends WM_DELETE_WINDOW,
waits at most 20 seconds and never force-kills:

```sh
python3 tools/close_renoise_scratch.py --pid 81061 --name sidsynth-stage5-checkpoint
```

Never inspect MCP during startup/recovery; require the ready scratch main-window
title first. Do not close the final audition host merely to rerun these commands.
The sanitizer, validator and clean-source build commands are in BUILD.md.
Evidence: `work/stage5-*`, `work/pluginval/stage5-tail*`,
`work/lifetime-stage5-sid-only.log`, `work/renoise-host/stage5-*` and
`work/renoise-host/renoise-console-stage5*.log`. No Git repository was initialized
at Stage 5 completion; the initial Git checkpoint was requested later, as noted
at the top of this file.

## Stage 4 implementation (historical 0.4.0)

**Stage 4 — Rack interface is implemented. Version 0.4.0 is built, tested and
installed.** The dedicated Renoise **PID 65130** is stopped with
`work/renoise-host/sidsynth-stage4-audition.xrns` and the Rack editor open.
Speaker monitor connections are restored, with all manual holds off and no
held notes. The user confirms the Rack looks and behaves well at their current
setup; detailed sound-quality audition is not separately recorded.

- Replaced the scrolling workbench with the selected cream/green Rack: three
  voice strips, one shared filter, explicit mode/model/clock, editable channels,
  output gain and separate SID volume. Every one of the **67 existing** saved/
  automatable parameters has a native attachment; IDs, version hints and
  **schema 3** are unchanged. Exactly one SID core remains.
- Independent waveform/filter switches, 0.01-semitone tuning, alternate raw
  manual-frequency entry, hardware ranges, editable numeric values, tooltips,
  native focus/navigation and compact Advanced TEST/EXT IN controls. Actual
  gate-bit lamps are separate from saved Hold. Panic acts **on press**, with
  brief Reset feedback; its in-host manual-hold recording is exactly silent.
- Audition explicitly targets V1/V2/V3 in 3 parts, including shared channels;
  Layered targets MIDI-source voices together. A lock-free epoch-tagged mailbox
  keeps UI key ownership out of saved patches and the callback. Release returns
  to still-owned host notes/bend. Reset/recall/routing/source/model/clock/stop
  reject stale audition intent. Editor close/target changes/Escape release it.
  UI intent applies at callback boundaries; host MIDI retains sample offsets.
- Native scalable components at **1120×780** default, **896×624–1680×1170**
  requested limits. The four size choices are 80/100/125/150%; the corner keeps
  the aspect ratio. Reopen defaults to 100%. No bitmap panel is stretched.
- Read the live Figcraft document, then edited only the project Rack and added
  its advanced-hardware detail. Verified and saved
  `design/sid-interface-proposals.figcraft`, PNG and SVG; Console is retained.
  `docs/RACK.md` explains the resolved design and audition behavior.
- JUCE remains pinned at 9.0.5. Its audio-utils keyboard also builds audio-format
  support; retained the additional pinned-source BSD codec notices and updated
  `third_party/NOTICES.md`. No new source release or dependency pin changed.

### Stage 4 checks

**Automated by the agent:** Release build passed in the existing hash-archive
`build-clean/`. CTest **1/1 passed (8.69 s)**. Prior engine/MIDI/state tests pass,
plus all 67 UI-to-parameter and host-to-UI attachments, typed/clamped register
entry, advanced visibility, shared-channel audition targeting, host-note
fallback/release, manual ownership, reset/recall epochs and editor-close release.
Callback guards remain **0 C++ allocations, 0 wrapped C allocations, 0 frees,
0 pthread mutex locks**, including audition. These remain bounded linker-wrap
checks, not a complete system profiler. Five final SIDSynth-only X11
attach/event-loop/remove/terminate/unload cycles pass. Package installation and
license resources pass in `build-clean/install-check/` and the local VST3 path.

**Agent visual inspection:** rendered the Rack and Advanced panel offline;
inspected actual Renoise captures at 80/100/125/150%. Labels and values fit,
Manual frequency replaces tuning, mode/channel availability is clear, and
selected Hold remains visibly distinct from GATE OFF after disarming. Tested
one Linux display/compositor setup; arbitrary DPI/compositor combinations are
not established. At 80%, native window rounding produced **897×623** for the
896×624 request (within one pixel); larger tested sizes were exact.

**Actual Renoise 3.5.4 Demo, automated by the agent (PID 65130):**

- Real waveform clicks preserve independent Saw/Triangle selection; typed
  cutoff **820** reaches the host. Audition plays only Voice 2 despite V1/V2
  sharing channel 1; the Voice 2 gate lamp agrees and recorded A4 is audible.
- Actual Manual source/Hold plays; UI Panic silences it with recorded RMS
  **0.0**, without editing saved Hold. Advanced TEST toggles reach the host.
- Four size choices and five native editor reopen cycles pass.
- Saving a nontrivial patch, changing it, and loading the saved fixture restores
  all **67 displayed values**, including on-valued Hold, with silence on recall.
  The changed fixture was saved separately before reload to avoid a native
  save-changes prompt. Normal pattern playback is captured and retained as
  `stage4-host-audition.wav`; Demo offline rendering was not invoked.
- Completed interaction checks and layout/recall resumes are combined in
  `stage4-host-results.json` with matching final module SHA. Recorder setup
  excludes its first 100 ms from stereo/analysis comparisons because the two
  live ports are linked sequentially; complete raw WAVs remain available.
- Early failed manual checks left a tone sounding; the user stopped it. Further
  recordings detached **only this test PID's** two speaker-monitor links, then
  restored them after clearing all notes/holds. The runner now does this
  automatically, including error cleanup. This was not a sound-quality pass.

### Startup/test-session findings

The first restart entered Renoise's recovery dialog. The user chose Yes and
reported another crash. Its stack is in `TRenoiseLuaSong::Rendering` while
`OnSetNewSong` is still loading: a queued MCP inspection read `song.rendering`
before the main GUI was ready. Recovery **No** plus the separately saved
`sidsynth-before-stage4.xrns` restored the test session. The Stage 4 runner now
requires a scratch main-window title before initial API access and waits across
song replacement. Do not send inspect calls during startup/recovery. This is
recorded evidence of the test-client/host timing failure, not a plugin DSP crash.

The intermediate scratch PID 61828 became unresponsive during the saved,
plugin-unloaded restart and did not exit on SIGTERM. Only that test process was
killed after its checkpoint was confirmed; no other Renoise song was operated.
The cause is unresolved. Full host shutdown/validator checks belong in Stage 5;
the final standalone SIDSynth lifecycle probe and actual editor reopen checks
pass. The attempted live debugger attachment was denied by the OS; no ptrace
settings were changed. External Vital investigation remains out of scope.

Final built/installed module SHA-256: `d86bdbf4c54568df52b0d8b1dd107607f32c028180eafb8d42db44c2198649d0`.
Evidence: `build-clean/stage4-*`, `work/lifetime-stage4-sid-only.log`,
`work/renoise-host/stage4-*`, and `renoise-console-stage4*.log`.

## Stage 3 implementation (historical 0.3.0)

**Stage 3 — Complete SID controls, automation and recall is implemented.** Version
**0.3.0** is built, tested and installed. All offline checks and actual Renoise
Stage 3 checks pass. New sound-quality listening judgments and the in-host UI
Panic click remain unconfirmed; Stage 2 listening/display are user-confirmed.

- Exactly **one** libresidfp SID core still supplies all three oscillators, their
  shared clock, native sync/ring interactions and one shared filter.
- All per-voice waveform bits/combinations, MIDI tuning, exact manual frequency,
  12-bit pulse width, four ADSR nibbles, manual gate/hold, sync, ring and TEST are
  exposed. TEST is the actual hardware bit, including oscillator/noise reset.
- Shared 11-bit cutoff, resonance, four input routing bits, independent LP/BP/HP
  bits, SID master volume, 6581/8580, PAL/NTSC and voice 3 direct-output switch
  are exposed. Direct-off preserves voice 3's filter and modulation paths.
- **67 saved/automatable parameters**; all original IDs/version hints and plugin
  identity are preserved. Schema **3** saves the complete patch and explicitly
  migrates schemas 1/2 with compatible sound defaults. Invalid known numeric
  fields reject the whole patch; no live MIDI/chip/armed-hold state is saved.
- Explicit per-voice **MIDI / Manual** source settles ownership. MIDI owns gate
  and note+tuning+bend frequency. Manual owns the exact raw 16-bit frequency and
  explicit hold; tuning/bend/notes do not compete. Source edits clear old MIDI
  ownership. Panic/reset/stop/routing/model/clock/recall disarm manual holds;
  an explicit hold off/on edge rearms them. See `docs/STATE.md` for the policy.
- Hardware parameter targets step at callback boundaries through JUCE/APVTS;
  MIDI retains its within-block offsets. Only post-chip Output is smoothed.
  Model/clock changes clear performance instead of continuing a live envelope.
- `SidBackend` prepares both model DAC/waveform tables at construction and both
  clock resamplers during preparation. Playback copies fixed arrays and swaps
  retained resamplers without upstream's allocating/locking setters. Those
  caches contain no extra SID cores. CMake makes a build-tree source copy with
  an explicit friend declaration in SID.h; pinned downloads and licenses remain
  unchanged. The adapter is documented in `third_party/NOTICES.md`.
- Optional **EXT IN** bus, disabled by default, accepts mono/stereo and folds
  stereo to mono. Finite/clamped signed-16-bit input is held through the SID
  cycles for each host frame, passing through the actual shared chip input,
  routing and volume. Nonfinite input becomes zero. Mono/stereo output works.
- **Renoise limitation:** the tested native Sidechain device targets tracks and
  effects, with no instrument destination; track output routes offer Master or
  hardware. No working instrument-input route was exposed through this native/
  API workflow. EXT IN is implemented and offline-tested; native Renoise input
  playback is unvalidated/unavailable in that workflow. No extra FX plugin or
  change of product scope was added. See the exact probe in `RENOISE_MCP.md`.
- Resizable **700×760 native parameter workbench** exposes all controls with a
  scrolling list and fixed Panic. This is Stage 3 scaffolding. The approved
  Figcraft/Rack design files are unchanged; the Rack editor remains Stage 4.

### Stage 3 checks

**Automated by the agent:** Release build in the existing hash-archive-configured
`build-clean/` passed, with manifest version **0.3.0**. CTest **1/1 passed (8.51 s)**.
The original Stage 1/2 behavior checks remain and pass. New checks include:

- Register bit combinations, exact 11/12/16-bit boundaries, ADSR nibble packing,
  independent shared switches and per-voice changes without gate loss.
- Manual/MIDI ownership, live tuning, no stale bend/note ownership on source
  changes, manual disarming after Panic/restore, and intentional rearming.
- Full 67-parameter state round trip, schema-2 defaults (plus existing schema-1
  migration), malformed late-field atomic rejection and hardware-range clamps.
- All 6581/8580 × PAL/NTSC configurations at 44.1/48/96 kHz. Combined shared
  filter, sync/ring, MIDI and boundary-automation timelines match **exactly** at
  blocks **64/257/1024**. EXT IN timelines also match at those blocks at 48 kHz.
- Cached-backend samples match the pinned upstream public API **exactly** for
  all four model/clock configurations, repeated after live switches.
- Audio proves voice 3 direct-off removes its direct contribution, keeps its
  filtered path, and can still sync/ring-modulate voice 1. Input-only signal
  needs no note and responds to SID volume. Mono input/output layout works.
- The actual development editor Panic callback silences the processor offline.
  This is a UI-action wiring check, not a click inside Renoise.
- Callback guards recorded **0 ordinary C++ new/new[] allocations, 0 wrapped
  malloc/calloc/realloc calls, 0 frees and 0 pthread mutex locks**, including live
  model/clock changes. Linker wrapping covers calls from linked code, not every
  internal operation of shared system libraries. Stage 5 profiling remains.

The final module passed **five standalone X11 editor attach/event-loop/remove/
terminate/unload cycles** in the existing SIDSynth-only lifecycle probe.
CMake package installation passed inside `build-clean/install-check/` with
licenses/notices, then the bundle was installed to
**`/home/dragos/.vst3/SIDSynth.vst3`**. The build and installed module SHA-256:
`6c8b5b9108b0fde5880f63a3adc45ecff9e3793aace22cdf69f85c8ecc11ebe5`.
The downloaded source pin/hash values did not change.

**Agent visual inspection:** software-rendered the 700×760 parameter workbench;
visible labels/values fit, shared hardware toggles are independent, and the
parameter list scrolls within the editor with Panic fixed above it. This does
not establish other host display scales or user judgment of the new workbench.

**Actual Renoise, automated by the agent through ReMCP:**

- Preserved the user's latest Stage 2 scratch settings as
  `work/renoise-host/sidsynth-before-stage3.xrns`. Unloaded the old plugin and
  restarted only the dedicated scratch host so its cached module was released.
  No Vital process/binary/settings or unrelated Renoise song was operated.
- New scratch Renoise **PID 55689**, version **3.5.4 Demo**, JACK/PipeWire,
  reported **48 kHz / 1024-frame buffers**. Opening the schema-2 song preserved
  its gain/routing and exposed **67** parameters with default new sound controls.
- Passed sync and ring audio changes with voice 3 direct output disabled.
  Relative spectral differences versus controls were **1.2502 / 1.2639**.
- Passed native **Instr. Automation** cutoff point envelope during playback.
  Recorded open / closed / reopened RMS: **0.006571 / 0.001209 / 0.006569**.
- All four model/clock combinations played A4 in the host. Manual hold sounded
  without a keyboard note; transport stop disarmed it, an unrelated pulse-width
  edit stayed silent, and an explicit hold edge recovered the sound.
- Saved a nontrivial complete patch including an on-valued manual hold; altered
  settings; reloaded; all **67 displayed values** matched at their defined
  resolution and the recalled patch remained silent. Five editor open/close
  cycles passed through the native host API.
- Two test assumptions were corrected, without production code changes: manual
  off/on calls need separate callbacks (the runner now waits 80 ms); and Renoise
  can cache unsnapped normalized values such as cutoff 0.46 while the SID stores
  register 942. Recall compares exact displayed hardware/musical values, rather
  than demanding equality between 0.46 and 942/2047.
- Recordings capture only this PID's two output ports. All captured samples are
  finite/bounded and stereo matches. No microphone or system mix was recorded.
  Normal playback is used; no disabled Demo offline-render request was made.
- Retained the input-routing probe and separate full-recall fixture. The handoff
  song is the stopped **`sidsynth-stage3-audition.xrns`**, with a playable MIDI
  Layered patch on channel 1 and a short pattern, plus its recorded WAV.

Evidence and exact reproduction/resume commands are in `docs/RENOISE_MCP.md`.
Logs/results/fixtures remain in ignored `work/renoise-host/stage3-*` and
`build-clean/stage3-*`. **Pending:** personal sound-quality judgment for the new
controls, in-host UI Panic click, workbench scaling at other display scales and
Stage 5 validator/profiling checks. The selected Rack editor is next.

## Stage 2 implementation (historical 0.2.0)

**Stage 2 — The two playing modes is implemented.** Offline checks and actual
Renoise MCP host checks pass, including recorded three-part/Layered audio,
controllers, routing transitions, song recall and editor open/close. Human
listening/display were pending at that checkpoint and are now user-confirmed
above; in-host UI Panic remains untested. Host results are agent-automated.

- `MidiRouter` uses fixed storage for 16 channels, all 128 MIDI pitches and
  counted repeats, with last-note priority and legato held-note fallback.
- **3 parts** defaults to channels 1/2/3; all three voice channels are selectable
  from 1–16. Shared assignments deliberately play the assigned voices together.
- **Layered** uses one selectable channel for all three oscillators, without
  replacing their individual engine patch/tuning data. The plugin's initial
  sound is three fixed saw patches; full patch controls remain Stage 3.
- Exactly one existing libresidfp core is clocked for the combined sound. MIDI
  events retain sample offsets and the persistent resampler's fractional clock.
- Sustain CC64, fixed ±2-semitone pitch bend, CC121 controller reset, channel
  CC120 hard mute and CC123 normal note release are implemented. Positive
  velocity remains fixed-level. Complete policy is in `docs/STATE.md`.
- Any mode/channel parameter change, global Panic/reset, transport stop or valid
  state restore clears held notes, repeats, pedal and bend. A channel-specific
  mute does not reset other sounding or releasing voices.
- Added stable routing parameter IDs and schema **2**, with explicit schema-1
  migration. Saving excludes live performance state. Restore validates the whole
  patch before applying it, using locale-independent numeric parsing.
- Version **0.2.0**; the 600×360 development editor exposes mode, layered channel,
  three part channels, Output and Panic. Inactive-mode selectors are disabled.
  This is implementation scaffolding; the approved Figcraft/Rack design files
  have not been changed and the full editor remains Stage 4.
- `docs/BUILD.md` documents one Renoise instance plus two aliases on channels
  1–3, with a host audition checklist.

### Stage 2 checks

**Automated by the agent:** final Release Linux VST3 build succeeded in the
existing hash-archive-configured `build-clean/`; CTest **1/1 passed (4.90 s)**,
with **0 ordinary C++ callback `new`/`new[]` allocations**. The final build
regenerated the VST3 manifest with version 0.2.0. The offline suite passed.
The suite covers both modes, all 128 held pitches, repeated-pitch matching,
unmatched/velocity-zero note-offs, held-note and sustain fallback, controller
reset, independent bend, channel 16/shared assignments, routing transitions,
per-oscillator waveform/tuning/envelope retention (including retuning a held
note down to frequency-register zero), and saved routing/schema-1
migration with invalid/unknown/default/clamped state handling.

Three-part and layered event timelines are compared sample-for-sample at
blocks **64, 257 and 1024**, at **44.1, 48 and 96 kHz**. Layered audio is compared
to simultaneous notes on the three independent channels. Each part's A4 pitch
and continued audio after other parts release/mute are checked. Callback
`new`/`new[]` guarding remains active, including transitions and ignored SysEx.
This does not replace C allocator/locking profiling or a host validator.

The final SIDSynth binary passed **five editor create/attach/event-loop/remove/
terminate/unload cycles** using the existing X11/IRunLoop lifecycle probe. No
Vital or Renoise process was operated. This is an automated plugin lifecycle
check, not Renoise audio/alias validation. The initial sandboxed attempt could
not access X; the final run used the local display successfully.

CMake package installation passed in `build-clean/install-check/`, including
licenses/notices. With Renoise closed, version **0.2.0** was then installed at
**`/home/dragos/.vst3/SIDSynth.vst3`**. Its module SHA-256 matches the final build
and install-check copy:
`f0e46d309c86f0a4696036cc4bc3255891bd5dd772d128440dff717943e9e166`.

**Agent visual inspection:** software-rendered the 600×360 development editor;
all routing, Output and Panic labels/controls fit. The default 3-parts mode and
channels 1/2/3 are visible, with the inactive layered selector disabled. This
preview does not establish in-host interaction or display scaling.

**Actual Renoise checks, automated by the agent through ReMCP:**

- Installed the user-requested ReMCP 0.1/API-6 package, pinned by archive SHA-256,
  with loopback-only autostart. Registered Codex MCP server `renoise` at
  `http://127.0.0.1:19714/mcp`. The current chat used the Python HTTP/MCP client.
- Added development tools for installation, a scratch alias fixture, scoped host
  operations and playback recording/analysis. These are external test tools;
  no production SIDSynth code or dependency versions changed in this MCP task.
- Renoise **3.5.4 Demo**, JACK/PipeWire, reported **48 kHz / 1024-frame buffer**.
  The dedicated test instance (PID 46878) is stopped with
  `work/renoise-host/sidsynth-recall.xrns` loaded. The pre-existing Renoise
  instance (PID 32946) and its song were left alone.
- Verified one actual plugin plus two aliases, with host API alias indices
  0/1/1 and channels 1/2/3. Since the Lua alias property is read-only, links were
  created in a scratch XRNS derived from the host's saved seed and verified
  after loading; the GUI's alias-selection clicks were not exercised.
- Captured only this test process's two JACK output ports, using PID-to-PipeWire
  client/node mapping and explicit recorder links. All recorded samples are
  finite/bounded and the stereo channels match. No microphone or system mix
  was captured. MIDI note 69 is pattern A-5 in Renoise's octave convention.
- Passed: independent three-part releases, held-note fallback, counted repeats,
  Layered versus a shared-channel three-voice reference, Layered channel 16
  acceptance/old-channel rejection, and held-note clearing on mode/channel
  changes and transport stop.
- Passed native host pitch bend to +2 semitones with another alias unaffected,
  recentering, CC64 holding only its assigned channel, and pedal-up release.
- Passed song save while a note plays, restoration of gain −24 dB, Layered,
  layered channel 16 and voice channels 16/10/15, no restored held note, and
  successful playback of the restored patch. Five editor open/close cycles
  passed through Renoise's native plugin API.
- One test assumption was corrected: upstream reset preserves oscillator
  accumulators, so previously played unison oscillators need not sum to 3× solo
  RMS. Layered matches the shared-channel reference (RMS ratio **0.99997**).
  Production audio code was not changed to force phase alignment.
- The first offline host-render request was denied by Renoise Demo, then the
  host crashed while centering the denial dialog (PID 46046; recursive Renoise
  window/X11 stack). No contributing-cause claim or external fix is made from
  that stack alone. The extension now detects Demo and avoids that call; the
  normal playback captures above completed successfully afterward.

See `docs/RENOISE_MCP.md` for the pinned archive, setup and exact commands,
results/limitations and current connection. Evidence is in ignored
`work/renoise-host/host-results.json`, `mcp-transcript.jsonl`, the three
`host-checks*.log` files, `host-*.wav`, the XRNS fixtures and the render-denial
core report. The installed module still matches the SHA-256 above.

**At the Stage 2 checkpoint, pending:** a person's listening judgment and display
check (subsequently user-confirmed above), clicking UI Panic, other host rates/
buffers and the
Stage 5 validator/profiler checks. Offline host rendering remains unavailable
in this Demo edition. Editor visibility/open-close and numerical recorded audio
are confirmed; they are not a visual or subjective listening judgment.

## Stage 1 implementation (historical)

**Stage 1 — First sound in Renoise is complete:** the clean build/offline checks
pass and the user confirms the installed instrument loads and plays in Renoise.
The reported switching crash was isolated to Vital's unload path; no SIDSynth
code change was indicated. Renoise and Vital were left unchanged as requested.

- C++17/CMake Linux VST3 target `SIDSynth_VST3` and offline processor/audio test
  `sidsynth_offline`, registered with CTest.
- Exactly one libresidfp core per instance, containing the three oscillators
  and shared filter. Voice 1 plays a fixed 8580/PAL saw patch on MIDI channel 1;
  voices 2 and 3 are silent. Note-on/off respect event offsets.
- Basic latest-note ownership, unmatched/channel-isolated note-off handling,
  velocity-zero note-off and 16-bit pitch saturation. Full playing modes and
  held-note policies were deferred to Stage 2, now implemented above.
- Minimal 500×240 development editor with Output and Panic. No Rack redesign
  or Stage 4 controls were added.
- Panic via UI, CC120/123 on channel 1, MIDI System Reset, host reset and
  transport playing→stop; valid state restore also requests a reset. Callback
  resets reuse the existing chip and allocate no replacement engine.
- Stable parameter ID `output.gainDb`, parameter version hint 1, versioned
  `SIDSynthPatch` schema 1. Saved patch state excludes MIDI/chip performance
  state. Missing/default, unknown-version and malformed-state behavior is tested.
- Source commits and archive SHA-256 values pinned for **JUCE 9.0.5**
  (`d60b7a78030661574a615b586d9325ba7a7fa7dd`) and **libresidfp 1.3.0**
  (`5393471a02a505ff43aa710148b2d35289b72c31`). CMake adapts upstream's autotools
  source list and generated headers without modifying its sources.
- Project license selected: **AGPL-3.0-only**, JUCE open-source route.
  Exact upstream license files, vendored notices and JUCE SPDX inventory retained
  under `third_party/` and included by the local install target.
- Build/install/audition instructions and environment in `docs/BUILD.md`.

## Stage 1 checks (historical)

**Automated, run by the agent:**

- Independent Release build in `build-clean/`, configured from the two pinned,
  hash-verified archives with no source-directory overrides. CMake/Ninja built
  the engine, offline executable and Linux VST3 successfully. Final source
  updates were rebuilt and retested in that directory.
- CTest: **1/1 passed**, containing the full offline behavior suite (about
  1.9 seconds). Equivalent timelines at blocks **64, 257 and 1024** are exactly
  equal at **44.1, 48 and 96 kHz**.
- The original `build/` directory was also rebuilt with final sources and its
  CTest suite passed **1/1**. The installed module's SHA-256 matches the verified
  `build-clean` module (`4bdc746f6139b9f50aebf2d9e698cc36424c1042045fa777473c2df3f16e81b2`).
- Finite/bounded samples, identical stereo channels, silence before note-on,
  nonzero event timing, preserved release followed by a quiet tail, channel
  isolation/unmatched note-offs, velocity zero, register pitch saturation,
  state restore/defaults/version rejection/malformed input, panic/reset and
  transport stop all pass. A 64 kB ignored SysEx passes without callback `new`.
- **0 ordinary C++ `new`/`new[]` allocations** observed inside the guarded
  callbacks, including core rendering and reset. This does not replace a full
  C allocator/locking profiler or host validator (Stage 5).
- A4 measured by autocorrelation: **440.079 Hz at 44.1 kHz**, **439.998 Hz at
  48 kHz**, **439.856 Hz at 96 kHz**. Held RMS is about **0.01293**; released
  tail RMS about **0.000059** with the default −12 dB output. These are offline
  numerical measurements, not listening judgments.
- VST3 manifest helper loaded the built module and generated its factory/class
  manifest. Exported `GetPluginFactory`, `ModuleEntry`, `ModuleExit` are present;
  `ldd` reports no unresolved shared-library dependencies. This is not a Renoise
  scan/load test.
- CMake installation tested inside `build-clean/install-check/`, including
  license/notices; then the bundle was installed for the authorized host audition
  at **`/home/dragos/.vst3/SIDSynth.vst3`**. Existing Vital was untouched.

**Agent visual inspection:**

- Software-rendered the 500×240 development editor. Output value/control and
  Panic button fit. Corrected non-ASCII text rendering in the development label.
  This preview is not an editor-open/close or scaling test inside Renoise.

**Listening and Renoise (user report and agent crash isolation):**

- User reports the loaded instrument works mostly fine and plays notes.
  At that Stage 1 checkpoint, detailed sound quality, release, Panic and song
  recall were unconfirmed; the new automated host checks are recorded above.
- Renoise Demo **3.5.4** is installed, verified from `renoise-demo 3.5.4-1` and
  its installed manual. Load/play was user-confirmed. Switching from Vital back
  to SIDSynth can crash the whole host after a few notes.
- Agent found three Renoise core dumps (PIDs 30861, 32037, 32522). The latest
  crash is on an older JUCE Timer thread immediately after Vital is released.
  Renoise's crash handler recursively faults; the original instruction address
  matches Vital's `juce::WaitableEvent::wait(int)` return from a condition wait,
  in a region no longer present in the dump's loaded-module map. SIDSynth's own
  newer JUCE timer thread is separately present and waiting normally.
- A standalone VST3 lifecycle probe reproduced the matching original fault
  under GDB after the Vital module unloaded, with an X11 parent and Linux host
  `IRunLoop`. It also exits with SIGSEGV (139) when loading/unloading **Vital
  alone**, without loading SIDSynth. **SIDSynth alone passed five full
  create/editor-attach/event-loop/editor-remove/terminate/unload cycles** in
  the same probe. No audio playback was required to trigger Vital's failure.
- This establishes a reproducible failure outside SIDSynth. Per the user's
  instruction, stop that investigation and proceed with SIDSynth's next stage.
  No changes were made to either plugin binary, Renoise settings, or Vital.
  No framework/dependency upgrade or speculative SIDSynth fix was applied.
- Probe source/logs: `work/plugin-lifetime.cpp`, `work/lifetime-gdb-before.txt`,
  `work/lifetime-vital-only.log`, `work/lifetime-sid-only.log`. Build/run commands
  and scope are recorded below. The tested Vital binary has SHA-256
  `74b8fd96f94e0788e4cdd4a6a2c3638f2bd23d7affd33defaa1ac7b48ea8b7dd`.
- Diagnostic core, extracted stacks, log copy and standalone lifecycle harness
  are in ignored `work/`. No user configuration or Vital files have been edited.

## Historical Stage 4 working locations and commands

Working directory: `/home/dragos/work/SIDSynth`.

```sh
cmake -S . -B build-clean -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSIDSYNTH_DEPENDENCY_ARCHIVE_DIR="$PWD/build/dependency-downloads"
cmake --build build-clean --parallel 6
ctest --test-dir build-clean --output-on-failure
./build-clean/sidsynth_offline "$PWD/build-clean/stage4-a4.wav" "$PWD/build-clean/stage4-editor.png"
cmake --install build-clean --prefix "$HOME/.vst3" --component Plugin
```

- Build bundle: `build-clean/SIDSynth_artefacts/Release/VST3/SIDSynth.vst3/`.
- Historical 0.4.0 installation (now replaced by 0.5.0): `/home/dragos/.vst3/SIDSynth.vst3/`.
- Audition render: `build-clean/stage4-a4.wav` (two-second, 48 kHz mono PCM16).
- Additional renders: `build-clean/stage2-three-parts.wav` and
  `build-clean/stage2-layered.wav` (two-second 48 kHz mono PCM16).
- Editor preview: `build-clean/stage4-editor.png`; size/Advanced variants beside it.
- Stage 4 logs: `build-clean/stage4-{configure,build,ctest,offline,install-check,local-install}.log`.
- Stage 3 logs: `build-clean/stage3-build.log`, `build-clean/stage3-ctest.log`,
  `build-clean/stage3-offline.log`, `build-clean/stage3-install-check.log`,
  `build-clean/stage3-local-install.log`, `work/lifetime-stage3-sid-only.log`.
- Actual Stage 4 host audition: `work/renoise-host/stage4-host-audition.wav`.
- Additional core example: `build-clean/stage3-filter-sync-ring.wav`.
- Historical Stage 2 logs: `build-clean/stage2-build.log`, `build-clean/stage2-ctest.log`,
  `build-clean/stage2-offline.log`, `build-clean/stage2-install-check.log`,
  `build-clean/stage2-local-install.log`, `work/lifetime-stage2-sid-only.log`.
- Historical Stage 1 logs: `build/clean-configure.log`, `build/clean-build.log`,
  `build/clean-final-build.log`, `build/local-install.log` (generated/ignored).
- Original development build: `build/`, initially configured with explicit
  source overrides for the exact-commit checkouts in `build/_deps/`.
- For a new network-enabled checkout, the standard configure is
  `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`; it downloads and
  verifies the pinned archives. Cached-archive configuration above was used
  for the independent clean build.
- No Git checkpoint: this project has no initialized repository metadata.

The local MCP endpoint and complete host runner can be inspected/run with:

```sh
codex mcp get renoise
python3 tools/renoise_mcp.py call sidsynth_host_check '{"operation":"inspect"}'
python3 tools/check_renoise_stage4.py --pid 65130
```

Use only the dedicated scratch Renoise PID hosting the endpoint. Actual runs
and resume commands, fixture preparation and retained evidence are detailed in
`docs/RENOISE_MCP.md`.

The Stage 4 final lifecycle command uses only the built SIDSynth module:

```sh
./work/plugin-lifetime \
  "$PWD/build-clean/SIDSynth_artefacts/Release/VST3/SIDSynth.vst3/Contents/x86_64-linux/SIDSynth.so" \
  "$PWD/build-clean/SIDSynth_artefacts/Release/VST3/SIDSynth.vst3/Contents/x86_64-linux/SIDSynth.so" sid-only
```

For a recorded CTest log, use
`ctest --test-dir build-clean --output-on-failure --output-log build-clean/stage4-ctest.log`.
The optional editor snapshot command above also emits the two Stage 2 examples
beside its A4 WAV. Stage 4 was rebuilt/tested in `build-clean/`; the historical
`build/` output is older.

Crash-isolation probe commands (requires access to the local X display; the
parent window is unmapped, and the probe does not operate Renoise):

```sh
c++ -std=c++17 -g \
  -Ibuild-clean/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK \
  work/plugin-lifetime.cpp \
  build-clean/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK/pluginterfaces/base/funknown.cpp \
  -ldl -lX11 -pthread -o work/plugin-lifetime
./work/plugin-lifetime \
  "$HOME/.vst3/SIDSynth.vst3/Contents/x86_64-linux/SIDSynth.so" \
  "$HOME/.vst3/SIDSynth.vst3/Contents/x86_64-linux/SIDSynth.so" sid-only
```

The final command passes five SIDSynth lifecycle cycles. The diagnostic probe
accepts a companion library as its second path; a third argument selects the
single-library test. The mixed and Vital-only failing runs were captured before
the user's request to leave external bugs alone; do not rerun or extend those
investigations without new direction. This is a focused lifecycle diagnostic,
not a full VST3 validator (it does not supply complete audio processing/host
services). The actual Renoise load/play result remains user-confirmed.

## Known limits and findings

- Hardware parameters step at callback boundaries; only MIDI retains within-
  block offsets. Velocity remains fixed-level, bend is ±2 semitones (RPN
  ignored). Stage 4 appearance/behavior and Stage 2 listening/display are
  user-confirmed; detailed audition of the new sounds is not separately recorded.
  Stage 4 UI Panic is now exercised in Renoise and produces silence.
- EXT IN is offline-tested; the tested Renoise native/API workflow exposes no
  instrument audio-input destination. The optional bus stays disabled by
  default. No new plugin format/category was introduced to work around it.
- The cached configuration adapter is coupled to the pinned libresidfp private
  layout. Exact-upstream audio comparison and allocation/mutex guards pass;
  source upgrades require deliberate adapter review.
- Normal release retains a small emulated analog/quantization noise floor.
  A 5 Hz wrapper DC blocker removes the core's residual DC. Preparation clocks
  one second of silent SID audio to settle its analog startup outside the
  callback. Panic is intentionally a hard cutoff and resets that core.
- Upstream libresidfp 1.3.0's `Noise::setLevel(0)` computes a negative shift;
  the wrapper uses the safe, minimal level **1** instead. Upstream source was
  not patched for this workaround. Background dither is deterministic for the
  automated timelines; the separate build-tree friend addition is noted above.
- GCC 16 emitted upstream `-Wmaybe-uninitialized` warnings in JUCE's
  ScopedBlendContext and vendored HarfBuzz. No project-source warnings or build
  failures were reported. Stage 5 sanitizer and pluginval checks now pass, as recorded above.
- The plugin uses this rolling Arch system's C++/graphics runtime. Older Linux
  distribution binary compatibility has not been established.

## Next concrete task

Manually check numeric entry after loading the colour correction. The
available Stage 5 implementation/checks and local preview packaging are complete.
Remaining acceptance checks need a licensed Renoise renderer and additional
Renoise device-rate/buffer configurations. New formats, platforms or musical
features require user direction. Keep external Vital investigation out of scope.

## Suggested prompt for a fresh chat

> Read AGENTS.md, README.md, docs/STATUS.md, docs/RELEASE.md and docs/PLAN.md.
> Stage 5 preview 0.5.0 is built/installed and packaged with pinned dependency
> sources, stable 67 IDs and schema 3. Release tests, ASan/UBSan/leak detection,
> level-10 pluginval and actual Renoise alias/automation/multiple-instance/recall/
> stop/start/editor checks pass. The dedicated scratch PID 81061 is stopped on
> the final audition song with its Rack open and monitors restored. Verify the
> ready main window before MCP; never inspect during startup/recovery. Do not
> replace instrument 00 when testing native XRNI recall: Renoise drops its
> aliases; use empty slot 03. Licensed-host offline render, other rates/buffers
> inside Renoise and subjective demo audition remain acceptance follow-ups.
> Do not expand scope or investigate external Vital faults without direction.
