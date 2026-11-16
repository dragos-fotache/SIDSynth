# Linux build, validation and release

For the manually invoked GitHub Linux build and release workflows,
see [CI.md](CI.md). These workflows compile/package only, with tests disabled.

## Build

Requires CMake 3.24+, Ninja, a C++17 compiler, pkg-config and Linux development
headers/libraries for ALSA, FreeType, Fontconfig and X11 (including Xext,
Xinerama, Xrandr, Xcursor, Xrender, Xcomposite and Xfixes). JUCE's configure step
may discover other optional system libraries; browser and curl support are
disabled in this instrument. No autotools installation is needed for libresidfp.

From the project root:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 6
ctest --test-dir build --output-on-failure
./build/sidsynth_offline "$PWD/build/stage5-a4.wav" "$PWD/build/editor-preview.png"
```

The first configure downloads the exact JUCE and libresidfp archives listed in
`cmake/Dependencies.cmake`, verifying SHA-256 before extraction. Later builds
reuse them. These are source dependencies, statically linked into the VST3.
All generated files stay in the project's ignored `build` / `build-*` folders.

Outputs:

- `build/SIDSynth_artefacts/Release/VST3/SIDSynth.vst3/`
- `build/sidsynth_offline`
- `build/stage5-a4.wav`: 48 kHz mono PCM16; MIDI A4 starts at sample 113,
  note-off at sample 38437, total length two seconds.
- `build/stage2-three-parts.wav` and `build/stage2-layered.wav`: two-second
  48 kHz mono examples exercising overlap, fallback, sustain, bend and release.
- `build/editor-preview.png`: optional software-rendered Rack editor, with additional
  `-80`, `-100`, `-125`, `-150` and `-advanced` PNGs.

The offline executable runs the actual processor with MIDI/audio buffers. It
checks both modes, 128 held pitches, counted repeats, last-note fallback,
sustain and bend isolation, shared/channel-16 assignments, mode/channel reset,
three-voice audio equivalence, per-oscillator patch retention, A4 pitch/release,
nonzero offsets, exact timelines across blocks 64/257/1024 at 44.1/48/96 kHz,
schema-1/2 migration, routing/state restoration, malformed input, MIDI/UI/host
reset, transport stop and ignored large SysEx. Callback guards count ordinary C++ `new`/`new[]` and wrapped C allocator/free/
pthread mutex calls. Their scope is explained below.

The Stage 4 editor is the selected native **1120×780 Rack**, scalable from
80–150%. It exposes all 67 parameters, live gate indicators, numeric entry,
manual Hold and an explicit audition target. See [the Rack guide](RACK.md).
Integration checks cover every parameter attachment in both directions,
typed/clamped numbers, advanced-panel visibility, editor-close release,
shared-channel audition isolation, host-note fallback and reset/recall epochs.

Additional checks cover all hardware register groups, manual/MIDI ownership,
manual disarming on resets/recall, live model/clock switches, exact cached-core
versus public-upstream samples, sync/ring with voice 3's direct output off,
filtered voice 3, the actual editor Panic action, schema 2 migration, and mono/
stereo external input. Combined filter/sync/ring timelines match at blocks
64/257/1024 for all 6581/8580 + PAL/NTSC combinations at 44.1/48/96 kHz.
External input timelines also match across those blocks at 48 kHz.

The test executable wraps C malloc/calloc/realloc/free and pthread_mutex_lock
calls from linked code in addition to ordinary C++ new/new[]. These guards are
bounded linked-call checks, rather than a whole-process profiler. See the
Stage 5 measurements in RELEASE.md.
The cached model and clock configuration path is covered while notes are live.
Noise-recovery regressions cover all three voices, MIDI/Manual ownership,
filtered/bypassed output, both models, all three rates and exact callback-size
consistency. They compare healthy noise, combined waveforms, explicit TEST
and another voice's sync/ring/filter history against the pinned upstream core.

`stage3-filter-sync-ring.wav` is an additional offline audition example. The
retained three-part/layered files still use their historical `stage2-` names.

## Reproduce the verified clean build without downloading again

This workspace also retains the two original, verified archives under
`build/dependency-downloads/`. A separate build from those archives was used to
check the CMake adapter and clean compilation:

```sh
cmake -S . -B build-clean -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSIDSYNTH_DEPENDENCY_ARCHIVE_DIR="$PWD/build/dependency-downloads"
cmake --build build-clean --parallel 6
ctest --test-dir build-clean --output-on-failure
./build-clean/sidsynth_offline "$PWD/build-clean/stage5-a4.wav" "$PWD/build-clean/editor-preview.png"
```

The local-archive option still verifies both pinned hashes. For a new offline
machine, copy those two archives with the project, or omit the option and allow
CMake to download them. The first development build used explicit
`FETCHCONTENT_SOURCE_DIR_JUCE` / `FETCHCONTENT_SOURCE_DIR_LIBRESIDFP` overrides
pointing to exact-commit source checkouts in `build/_deps/`; the independent
`build-clean` build uses the hash-verified archives without those overrides.

## Install for Renoise

Close any loaded SIDSynth instance before replacing the bundle. Install only
this plugin, including its license and notices, to the standard per-user VST3
location (no sudo):

```sh
cmake --install build-clean --prefix "$HOME/.vst3" --component Plugin
```

For the normal `build` directory, substitute `build` above. Remove just
`~/.vst3/SIDSynth.vst3` to uninstall. A build alone does not install anything.

In Renoise:

1. Open **Edit → Preferences → Plugins/Misc**, enable VST3 and press **Rescan**.
2. Select an empty instrument, open its **Plugin** tab and select **SIDSynth**.
   The default **3 parts** mode uses MIDI channels **1**, **2** and **3**.
3. Open the editor. Play a middle-register note from Renoise's keyboard or MIDI
   input. MIDI note 69 should sound at approximately 440 Hz; host octave labels
   can differ. Release it and listen for the short envelope release.
4. Hold a note and press **Panic / reset**. It should stop immediately. Play
   another note to confirm recovery. Check playback stop as well.
5. Hold one note, play another and release the newer note. The first should
   return. Check sustain pedal-up, pitch bend (fixed ±2 semitones), and repeated
   notes. Positive velocities have a fixed level in this stage.
6. Choose **Layered** and its input channel. One note should play all three saw
   oscillators. Changing a mode/channel selector cuts held notes. Try channel
   16 and confirm the old channel no longer triggers layered notes.
7. Change Output and routing, save a temporary song, reload it and check that
   the settings return while no held/sustained note returns. These are host
   checks, separate from the automated state tests.

## SID controls and audition

- Each voice strip exposes independent Triangle,
  Saw, Pulse and Noise switches combine hardware waveform bits. Filter LP/BP/HP
  switches also combine; routing a voice into a filter with no mode selected
  removes that voice from the mix.
- Each voice defaults to **MIDI** source: note + tuning + bend owns frequency
  and gate. **Manual** source uses the exact 0–65535 frequency register and its
  **manual hold** switch. Tuning is MIDI-only. Raw SID controls show their register ranges.
- Manual hold supports sustained audition without Renoise keyboard focus. After
  Panic, transport stop, routing/model/clock changes or song recall, toggle hold
  off/on to rearm. An on-looking saved hold does not restart itself after reset.
- Try voice 1 sync/ring with voice 3 at another pitch, and voice 3 direct output
  off. Its modulator remains active. Ring modulation acts on Triangle.
- Model/clock edits clear performance. Both clocks keep MIDI concert pitch by
  recalculating the register; Manual raw frequency changes pitch with the clock.
- Use Renoise's native **Instr. Automation** device to select any plugin
  parameter, then create pattern automation. Hardware values intentionally step
  at callback boundaries; Output alone has a 10 ms ramp.
- Save/reload a temporary patch with oscillator, filter, model and clock edits.
  Confirm all settings return without live MIDI notes or an armed manual hold.

The optional **EXT IN** bus supports mono/stereo in a host that exposes an
instrument audio input; its signal passes through the shared SID filter/master
volume. The tested Renoise 3.5.4 native Sidechain workflow offers track/effect
destinations and no instrument input destination. There is no working native
input route documented for this host; the bus remains optional and disabled by
default. See `docs/RENOISE_MCP.md` for the actual probe and its limits.

## Three parts with one instance and two aliases

1. Load SIDSynth once into instrument **00**. In its editor select **3 parts**,
   leaving voice channels at **1 / 2 / 3**. Set Renoise's Plugin **Channel** to 1.
2. Select empty instrument **01** and open its Plugin instrument list. Under
   **VST Aliases**, choose the existing SIDSynth in instrument 00. Set this
   alias's **Channel** to 2.
3. Repeat in instrument **02**, aliasing the same instance and setting
   **Channel** to 3. Do not select a fresh SIDSynth instance from the plugin list.
4. Sequence notes using instruments 00, 01 and 02. Hold overlapping notes and
   release each part separately. Each instrument should control its assigned
   oscillator; fallback, sustain and bend must remain independent.

Aliases share the instance's patch and Output, while their Channel/Transpose
settings are separate. SIDSynth has one mixed output from one shared chip and
filter. The alias procedure follows the [Renoise Plugin manual](https://tutorials.renoise.com/wiki/Plugin#Multi-Timbral_Plugins).
The agent exercised this one-instance/two-alias routing through MCP and
recorded real host playback. Alias links were derived in a scratch XRNS because
the Lua alias property is read-only; the GUI selection clicks were not tested.
See [MCP setup and host results](RENOISE_MCP.md) for commands and retained evidence.

Renoise requires VST3 bundles in the standard VST3 directories. Its scan settings
are described in the [Renoise Preferences manual](https://tutorials.renoise.com/wiki/Preferences#VST3_Plugins_.28Windows.2C_Mac_.26_Linux.29).

Audition the offline WAV in an audio player as a separate listening reference;
on this machine `pw-play build-clean/stage5-a4.wav` is available. Mathematical
audio checks and a software editor preview do not establish audible soundcard
output, Renoise compatibility, or editor behavior inside Renoise. Record actual
listening and host results in `docs/STATUS.md`, identifying who performed them.

## Recorded local baseline (16 November 2026)

Arch Linux, x86_64, kernel `7.3.4-arch1-1`; AMD Ryzen 5 3600X, 12 logical CPUs,
32 GiB RAM. CMake 4.5.0, Ninja 1.13.2, GCC/G++ 16.2.1 (`20260810`), Clang
23.1.5 (available, not the verification compiler), pkg-config 3.0.10.

Installed host: **Renoise Demo 3.5.4**, Arch package `renoise-demo 3.5.4-1`,
`/usr/bin/renoise`; version checked with `pacman -Qi renoise-demo` and its
installed manual. `renoise --version` is not a supported version-query command.

Detected pkg-config versions: ALSA 1.2.16.1, FreeType module 26.7.20,
Fontconfig 2.18.3, X11 1.8.14, Xext 1.3.7, Xinerama 1.1.6, Xrandr 1.5.5 and
Xcursor 1.2.3. This is a local build baseline, not a promise of binary
compatibility with older Linux distributions.


## Stage 5 sanitizers and CPU measurement

The sanitizer option instruments the plugin, SID core, tests and compiled JUCE
modules. Keep its output separate from the release build:

```sh
cmake -S . -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS_DEBUG='-O1 -g1' -DCMAKE_C_FLAGS_DEBUG='-O1 -g1' \
  -DSIDSYNTH_SANITIZERS=ON \
  -DSIDSYNTH_DEPENDENCY_ARCHIVE_DIR="$PWD/build/dependency-downloads"
cmake --build build-sanitize --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build-sanitize --output-on-failure
```

LeakSanitizer needs process inspection access. This workspace's restricted
sandbox prevented that; the same test passed with normal desktop process access
and leak detection enabled. No sanitizer suppression file is used. Do not load a
sanitizer plugin into an ordinary host without arranging the sanitizer runtime.

Measure only the normal Release executable, with other test/build work stopped:

```sh
./build-clean/sidsynth_offline --benchmark > build-clean/cpu.csv
```

CSV rows cover 44.1/48/96 kHz × 64/257/1024 frames × fresh idle, released notes, filtered 6581,
filtered 8580 and stereo EXT IN. Each row warms 100 callbacks and measures about
three seconds of audio. CPU time is a percentage of one core; callback wall-time
percentiles include desktop scheduling. This is a local sample, not a real-time
deadline guarantee or an end-to-end Renoise CPU measurement.

Stage 5 also checks variable and empty callbacks against an exact sample
reference, repeated preparation at different rates, long release, live recall
and interleaved cores with independent resets/model/clock changes.

## Validator

The verified external development tool is [pluginval 1.0.4](https://github.com/Tracktion/pluginval/releases/tag/v1.0.4).
Its official Linux ZIP SHA-256 is
`c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352`.
Download/extract it to `work/pluginval/`, make `pluginval` executable, and run
with X11 display access:

```sh
work/pluginval/pluginval --strictness-level 10 --random-seed 20949 \
  --sample-rates 44100,48000,96000 --block-sizes 1,64,257,1024 \
  --output-dir "$PWD/work/pluginval" --output-filename stage5-tail \
  --validate "$PWD/build-clean/SIDSynth_artefacts/Release/VST3/SIDSynth.vst3"
```

No test is disabled. The separate Steinberg validator is not installed, so its
optional pluginval subtest is skipped. Pluginval itself exercises the actual
VST3 wrapper, including state/thread tests, parameter fuzzing and editor lifetime.

## Release archives

After all checks pass and the native example files are present:

```sh
python3 tools/package_release.py
cd build-release
sha256sum -c SHA256SUMS
```

For the current version, this creates `SIDSynth-0.5.1-linux-x86_64.tar.gz` and
`SIDSynth-0.5.1-source.tar.gz`. Rebuild the plugin after changing the project
version; packaging rejects a module whose version differs from the source.
The binary archive includes the complete VST3,
licenses/notices, documentation and examples. The source archive includes this
project and the two original verified dependency source archives, with
`BUILD-OFFLINE.txt` explaining a network-free configure/build. System development
packages are still required. Archives use stable ordering, timestamps, ownership
and gzip headers. Rebuilding the plugin is supported; bit-identical ELF output
across compilers or directories is not promised. A sanitizer build is refused
by the packaging script.
