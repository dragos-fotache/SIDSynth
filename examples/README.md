# SIDSynth demonstration patches

Requires SIDSynth 0.5.0 (Linux x86_64 VST3) and Renoise 3.5.4 or a compatible
newer version. Install the plugin first, then open
`sidsynth-three-parts-example.xrns` and press Play. It is a short 120 BPM,
4 LPB, 64-line pattern saved with playback stopped and no held notes.

The song contains exactly **one** SIDSynth instance in instrument **00**, plus
aliases **01/02** on channels **2/3**. The three tracks sequence a saw bass,
pulse lead and triangle counterline through the same chip and filter. Aliases
share the patch; independent instances have independent chips.

Load an instrument file into an empty Renoise instrument slot:

| File | Playing mode | Sound |
| --- | --- | --- |
| `sidsynth-three-parts.xrni` | 3 parts, channels 1/2/3 | Saw / pulse / triangle, shared low-pass |
| `sidsynth-layered-brass.xrni` | Layered, channel 1 | Three detuned saws, gentle attack and resonant low-pass |
| `sidsynth-ring-bell.xrni` | Layered, channel 1 | Decaying triangle ring-modulated by voice 3 a fifth above |

All presets use MIDI source, no manual Hold or TEST, and −18 dB Output.
The bell's modulator has no direct output. Its ongoing oscillator supplies
ring modulation through the same core.

`patches.json` is the readable recipe source. It lists Renoise-normalized
values; omitted controls use the plugin defaults. Each native XRNI contains
the complete schema-3 patch. To regenerate them, use the dedicated scratch
host procedure in [RENOISE_MCP.md](../docs/RENOISE_MCP.md), then run
`tools/check_renoise_stage5.py`. The runner records playback and verifies all
67 recalled values after each instrument save/load.

Agent checks establish signal, routing and recall. Subjective sound-quality
approval of these demonstration sounds remains a user listening check.
