# SIDSynth

An open-source, Linux-only VST3 instrument exposing one emulated Commodore 64
SID chip through a compact Rack interface. Renoise is the first validation host.

Stage 5 provides the first Linux preview release, **0.5.0**: the native Rack,
67 stable parameters, schema-3 recall, hardened editor focus and a host tail
long enough for the slow SID release. Validation, CPU measurements and remaining
host limits are recorded in [the release notes](docs/RELEASE.md).

Three demo instruments and a one-instance/two-alias Renoise song are in
[examples/](examples/README.md). Build, sanitizer, benchmark and packaging
commands are in [docs/BUILD.md](docs/BUILD.md). The optional EXT IN bus works
offline; the tested Renoise workflow exposes no native instrument input route.
Renoise Demo cannot render offline. External Vital issues remain out of scope.

## Product decisions

- Linux only. VST3 is the initial plugin format.
- Use an existing open-source SID emulation library.
- Preserve one chip's three oscillators and single shared filter.
- **3 parts:** independent monophonic parts, initially on MIDI channels 1–3.
- **Layered:** one monophonic part drives all three oscillators with individual
  waveform, tuning and envelope settings.
- The selected interface is **Rack**, with horizontal voice strips and a shared
  filter on the right. The Console proposal is retained only as design history.
- Source repository: [dragos-fotache/SIDSynth](https://github.com/dragos-fotache/SIDSynth).

## Implementation

Uses C++17/CMake, **JUCE 9.0.5** for the plugin/editor and **libresidfp 1.3.0**
for the one SID core. Exact source commits and archive hashes are pinned in
`cmake/Dependencies.cmake`. The project license is **AGPL-3.0-only**, using
JUCE's open-source route. See [LICENSE](LICENSE) and
[third-party notices](third_party/NOTICES.md).

Build, install and audition instructions are in [docs/BUILD.md](docs/BUILD.md).
The initial patch uses one 8580/PAL chip with three saw oscillators. It defaults
to 3 parts on MIDI channels 1–3; Layered uses input channel 1. All SID controls
are editable in the Rack editor.

Keep the chip engine, MIDI behavior and parameter definitions separate from
the editor. The Rack design is an editable reference, not a bitmap to stretch
over the plugin window.

## Project map

- [Implementation plan](docs/PLAN.md): staged scope and acceptance criteria.
- [Current status](docs/STATUS.md): completed work, next task and handoff prompt.
- [Build and audition](docs/BUILD.md): exact commands and local toolchain.
- [Renoise MCP](docs/RENOISE_MCP.md): local connection, fixture and recorded host checks.
- [Rack editor guide](docs/RACK.md): controls, audition, resizing and recall.
- [Parameter/state contract](docs/STATE.md): stable IDs, schema 3 and schema 1/2 migration.
- `src/`: chip engine, fixed-storage MIDI router, processor, parameters and
  native Rack editor.
- `tests/OfflineTests.cpp`: MIDI/audio/state checks and a WAV render.
- [Rack preview](design/sid-rack.png) and [editable SVG](design/sid-rack.svg).
- [Native Figcraft document](design/sid-interface-proposals.figcraft): both
  original proposals, with Rack selected for implementation.
- `tools/figcraft_mcp.py`: bridge to the running Figcraft app's stdio MCP server.

## Development workflow

Implement one milestone at a time. End each milestone with a buildable result,
relevant automated checks, explicit Renoise listening checks, and an updated
`docs/STATUS.md`. Record a Git checkpoint when a repository is initialized.
Future chats should start by reading `AGENTS.md` and `docs/STATUS.md`.

## Primary references

- [libresidfp source and license](https://github.com/libsidplayfp/libresidfp)
- [JUCE source and license](https://github.com/juce-framework/JUCE)
- [Renoise plugin preferences](https://tutorials.renoise.com/wiki/Preferences)
- [Renoise manual, plugin aliases](https://files.renoise.com/manual/Renoise%20User%20Manual.pdf)
