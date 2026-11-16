# SIDSynth 0.5.0 — first Linux preview

16 November 2026. Linux x86_64, VST3, Renoise first. Local release archives
include corresponding project/dependency sources; no public upload was made.
The 67 parameter IDs, plugin identity and schema 3 are preserved.

The native Rack retains three voices on one SID core and one shared filter.
Stage 5 adds validation/build tooling, CPU measurements, native Renoise demos
and two host fixes: hidden editors no longer request keyboard focus, and the
host tail is 30 seconds so slow SID releases can continue beyond one second.
Panic and transport stop retain their deliberate immediate reset behavior.

## Verification

All results below were automated by the agent unless stated otherwise.

| Check | Coverage / outcome |
| --- | --- |
| Release CTest | Engine, MIDI, state, UI attachments, EXT IN and new Stage 5 cases pass |
| Variable callback timing | Blocks 1/64/0/257/31/1024/3/128 match an exact fixed-block reference at all three rates |
| Rate reconfiguration | 48 → 96 → 44.1 → 48 kHz clears ownership, restores concert pitch and accepts fresh input |
| Multiple cores | Another instance's notes, reset, model and clock changes leave the reference instance's samples identical |
| Callback guards | Zero tracked C++ allocations, wrapped C allocator calls/frees and pthread mutex locks |
| pluginval 1.0.4 | Level 10, seed 20949, 44.1/48/96 kHz, blocks 1/64/257/1024; no disabled tests |
| Sanitizers | ASan + UBSan + LeakSanitizer, instrumented core/JUCE/processor/editor, no suppressions |
| Actual Renoise | Scan/load, two aliases, channel isolation, filter automation, two instances, ten editor cycles, full recall and three stop/start cycles |
| Demonstrations | Three native XRNI files save/load with all 67 displayed values restored; finite bounded playback recorded |
| Host lifetime | Normal saved-scratch shutdown with plugin/editor loaded, followed by restart/reload; exact runs in STATUS.md |
| Packaging | Staging install includes licenses; source archive retains the hash-verified pinned archives; clean extracted-source Release build and CTest pass |

The callback guards cover linked calls and ordinary C++ new/new[], not every
internal shared-library path. Pluginval's optional separate Steinberg-validator
subtest is skipped because that executable is absent. Sanitizers are separate
from release performance measurements.

The user confirmed Stage 2 listening and the Stage 4 Rack's appearance and
behavior. New demo sound-quality approval is not separately recorded.

## CPU measurement

The Release benchmark measures fresh idle, released notes, filtered 6581/8580 (all voices plus
sync/ring), and stereo EXT IN at 44.1/48/96 kHz and blocks 64/257/1024.
It reports CPU time as a fraction of one core and wall-time percentiles.
Active processing measured **4.95–6.73% of one core**; after normal release it
remained **4.94–5.31%**, since the chip continues running. Fresh idle measured
**0.004–0.157%**. The longest observed callback used **18.0%** of its deadline.
The [raw CSV](validation-0.5.0-cpu.csv) and `validation-0.5.0.json` retain the
measured ranges and module identity,
and [BUILD.md](BUILD.md) for the repeatable command.

This machine is an AMD Ryzen 5 3600X on Arch Linux. Measurements are local
desktop samples. Other audio/UI work can affect scheduling; they establish
neither hard real-time guarantees nor compatibility with older distributions.

## Remaining host limits

- Actual Renoise playback uses JACK/PipeWire at **48 kHz / 1024 frames**.
  The broader matrix and reconfiguration checks run in the real processor and
  pluginval. Changing the shared audio server during the user's other playback
  was deliberately avoided; other rates/buffers inside Renoise remain unverified.
- Renoise **3.5.4 Demo disables offline rendering**. The guarded host probe
  reports unavailable; no disabled render call is made. Real Renoise offline
  export remains a check for a licensed host.
- The tested Renoise native/API workflow exposes no instrument EXT IN
  destination. The optional mono/stereo input works offline and stays disabled
  by default.
- Stage 4's earlier scratch shutdown hang remains unexplained. The Stage 5
  normal shutdown/restart checks pass; this does not establish every shutdown
  scenario. External Vital faults remain outside this project.
- Binary baseline is this rolling Arch toolchain. A broader Linux compatibility
  build/test baseline is a later release decision.

These limits keep 0.5.0 a preview. The implementation and available checks are
complete; licensed-host offline export, the additional Renoise device matrix
and subjective demo listening remain explicit acceptance follow-ups.

## Install, remove and rebuild

Close loaded SIDSynth instances, extract the binary archive and copy
`SIDSynth.vst3` to `~/.vst3/`. Rescan VST3 in Renoise. Remove only
`~/.vst3/SIDSynth.vst3` to uninstall. Examples are documented
[beside their files](../examples/README.md).

Extract the matching source archive and follow `BUILD-OFFLINE.txt` or
[BUILD.md](BUILD.md). JUCE 9.0.5 and libresidfp 1.3.0 remain pinned by full commit
and SHA-256 in `cmake/Dependencies.cmake`; their sources/licenses are included.
Project license: AGPL-3.0-only. [Third-party notices](../third_party/NOTICES.md).
