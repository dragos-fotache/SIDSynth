#!/usr/bin/env python3
"""Install the pinned ReMCP archive with local SIDSynth host-check support.

Download the archive URL below first. Existing tool directories are never
overwritten. This script does not change Renoise's other tools or preferences.
"""
import argparse
import hashlib
import json
import os
import shutil
import zipfile
from pathlib import Path

ARCHIVE_URL = "https://www.renoise.com/uploads/tools/renoise-mcp/com.renoise.ReMCP_v0.1_api6.xrnx"
ARCHIVE_SHA256 = "a2cbf69680c3a1d39860433e28f3d4edb6bb7bf27260377ef77cea4f061c1015"
PROJECT = Path(__file__).resolve().parent.parent
STARTUP = '''
-- Local setup: start the loopback-only MCP endpoint after this tool loads.
renoise.tool().tool_finished_loading_observable:add_notifier(function()
  local router = require("mcp.router")
  local n, errors = router.load_tools_dir(renoise.tool().bundle_path)
  for _, message in ipairs(errors) do print("[ReMCP] " .. message) end
  local ok, message = require("mcp.server").start(19714)
  print("[ReMCP] " .. (ok and ("Started on 127.0.0.1:19714; tools=" .. n) or tostring(message)))
end)
'''


def install(archive, destination):
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == ARCHIVE_SHA256, "Archive hash mismatch"
    assert not destination.exists(), "Tool directory already exists; inspect it before replacing anything"
    with zipfile.ZipFile(archive) as source:
        names = [name for name in source.namelist() if not name.startswith("__MACOSX/")]
        for name in names:
            path = Path(name)
            assert not path.is_absolute() and ".." not in path.parts, "Unsafe archive path"
        for name in names:
            source.extract(name, destination)
    server_path = destination / "mcp/server.lua"
    server = server_path.read_text()
    original = "renoise.Socket.create_server(M.port, renoise.Socket.PROTOCOL_TCP)"
    assert server.count(original) == 2, "Unexpected server source"
    server_path.write_text(server.replace(original,
        'renoise.Socket.create_server("127.0.0.1", M.port, renoise.Socket.PROTOCOL_TCP)'))
    main = destination / "main.lua"
    main.write_text(main.read_text() + STARTUP)
    shutil.copyfile(PROJECT / "tools/renoise/SIDSynthHostChecks.lua",
                    destination / "tools/sidsynth_host_checks.lua")
    scratch = PROJECT / "work/renoise-host"
    scratch.mkdir(parents=True, exist_ok=True)
    (destination / "sidsynth-host-root.txt").write_text(str(scratch) + "\n")
    (destination / "sidsynth-install.json").write_text(json.dumps({
        "archive_url": ARCHIVE_URL, "archive_sha256": ARCHIVE_SHA256,
        "listen": "127.0.0.1:19714", "scratch": str(scratch)}, indent=2) + "\n")
    print(destination)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    config = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    parser.add_argument("--destination", type=Path,
        default=config / "Renoise/V3.5.4/Scripts/Tools/com.renoise.ReMCP.xrnx")
    args = parser.parse_args()
    install(args.archive, args.destination)
