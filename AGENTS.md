# Project instructions

Read `README.md`, `docs/STATUS.md`, and the active milestone in `docs/PLAN.md`
before implementation. These files are the durable project context; keep them
accurate instead of relying on a previous chat's transcript.

## Established scope

- Linux only; open-source plugin; initial format VST3; first host Renoise.
- Implement the selected **Rack** design in `design/`.
- Exactly one SID core per plugin instance. Its three oscillators must share
  the same clock, inter-oscillator sync/ring behavior and single filter.
- Two MIDI modes: layered monophonic and three independent monophonic parts.
- Work in stages. Do not expand into other operating systems, additional
  plugin formats, a tracker, a C64 player or SID-file export without user direction.

## Implementation discipline

- Pin dependency versions/commits. Document licenses from those pinned sources.
- Define stable parameter IDs and versioned saved state before treating builds
  as persistent instruments in users' songs.
- Keep MIDI performance state separate from saved patch parameters.
- Process note events at their offsets within audio blocks. Avoid allocation,
  blocking locks, logging, filesystem access and UI access in the audio callback.
- Test meaningful behavior: event timing, note ownership, channel isolation,
  block-size consistency, register ranges and state restoration. Avoid tests
  that merely mirror code structure.
- Use real listening and Renoise checks alongside automated tests. Record
  whether each check was automated, performed by the agent, or confirmed by the
  user. Never claim host validation from compilation alone.
- At each milestone update `docs/STATUS.md` with changes, checks, unresolved
  issues, exact working commands and the next concrete task.

## Design tooling

Use Figcraft for design changes, as requested by the user. The working project
copy is `design/sid-interface-proposals.figcraft`. Native MCP tools are preferred
when exposed. If absent, `tools/figcraft_mcp.py` connects through
`/usr/bin/figcraft-cli mcp --attach 7890`; the app must be running with
`figcraft --control 7890`. Read the live document before editing, verify changes,
and save back to the project copy. Never overwrite unrelated open designs.
