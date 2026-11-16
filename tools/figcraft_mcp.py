"""Small stdio MCP client for the installed FigCraft CLI."""
import argparse
import json
import selectors
import subprocess
import sys
import time
from pathlib import Path


class FigCraft:
    def __init__(self):
        self.proc = subprocess.Popen(
            ["/usr/bin/figcraft-cli", "mcp", "--attach", "7890"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1,
        )
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.proc.stdout, selectors.EVENT_READ)
        self.selector.register(self.proc.stderr, selectors.EVENT_READ)
        self.serial = 0

    def send(self, message):
        self.proc.stdin.write(json.dumps(message) + "\n")
        self.proc.stdin.flush()

    def request(self, method, params, timeout=20):
        self.serial += 1
        request_id = self.serial
        self.send({"jsonrpc": "2.0", "id": request_id, "method": method, "params": params})
        deadline = time.monotonic() + timeout
        errors = []
        while time.monotonic() < deadline:
            for key, _ in self.selector.select(max(0, min(1, deadline - time.monotonic()))):
                line = key.fileobj.readline()
                if not line:
                    self.selector.unregister(key.fileobj)
                    continue
                if key.fileobj is self.proc.stderr:
                    errors.append(line.strip())
                    continue
                message = json.loads(line)
                if message.get("id") == request_id:
                    if "error" in message:
                        raise RuntimeError(json.dumps(message["error"]))
                    return message["result"]
            if self.proc.poll() is not None:
                raise RuntimeError("FigCraft MCP exited: " + "; ".join(errors))
        raise TimeoutError(method + ": " + "; ".join(errors))

    def initialize(self):
        result = self.request("initialize", {
            "protocolVersion": "2024-11-05", "capabilities": {},
            "clientInfo": {"name": "codex-figcraft", "version": "1.0"},
        })
        self.send({"jsonrpc": "2.0", "method": "notifications/initialized"})
        return result

    def close(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.proc.kill()
        self.selector.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["catalog", "call"])
    parser.add_argument("tool", nargs="?")
    parser.add_argument("arguments", nargs="?", default="{}")
    args = parser.parse_args()
    client = FigCraft()
    try:
        initialized = client.initialize()
        if args.action == "catalog":
            catalog = client.request("tools/list", {})
            destination = Path(__file__).resolve().parents[1] / "work" / "figcraft-catalog.json"
            destination.write_text(json.dumps({"initialize": initialized, **catalog}, indent=2))
            print(json.dumps({"initialize": initialized,
                "tools": [{"name": t["name"], "description": t.get("description", "")}
                          for t in catalog.get("tools", [])]}, indent=2))
        else:
            if not args.tool:
                parser.error("call requires a tool name")
            print(json.dumps(client.request("tools/call", {
                "name": args.tool, "arguments": json.loads(args.arguments),
            }, timeout=90), indent=2))
    finally:
        client.close()


if __name__ == "__main__":
    main()
