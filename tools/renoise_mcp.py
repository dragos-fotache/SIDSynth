#!/usr/bin/env python3
"""Small dependency-free client for the local ReMCP Streamable HTTP endpoint."""
import argparse
import json
import urllib.request


class RenoiseMCP:
    def __init__(self, url="http://127.0.0.1:19714/mcp", timeout=30):
        self.url = url
        self.timeout = timeout
        self.request_id = 0

    def request(self, method, params=None):
        self.request_id += 1
        payload = {"jsonrpc": "2.0", "id": self.request_id, "method": method}
        if params is not None:
            payload["params"] = params
        request = urllib.request.Request(
            self.url, data=json.dumps(payload).encode(),
            headers={"Content-Type": "application/json", "Accept": "application/json, text/event-stream"},
        )
        with urllib.request.urlopen(request, timeout=self.timeout) as response:
            result = json.load(response)
        if "error" in result:
            raise RuntimeError(result["error"])
        result = result["result"]
        if result.get("isError"):
            raise RuntimeError(result.get("content"))
        return result

    def initialize(self):
        return self.request("initialize", {
            "protocolVersion": "2025-11-25", "capabilities": {},
            "clientInfo": {"name": "sidsynth-host-checks", "version": "1"},
        })

    def call(self, name, arguments=None):
        return self.request("tools/call", {"name": name, "arguments": arguments or {}})


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["initialize", "list", "call"])
    parser.add_argument("name", nargs="?")
    parser.add_argument("arguments", nargs="?", default="{}", help="JSON object")
    parser.add_argument("--url", default="http://127.0.0.1:19714/mcp")
    args = parser.parse_args()
    client = RenoiseMCP(args.url)
    initialization = client.initialize()
    if args.command == "initialize":
        result = initialization
    elif args.command == "list":
        result = client.request("tools/list")
    else:
        if not args.name:
            parser.error("call requires a tool name")
        result = client.call(args.name, json.loads(args.arguments))
    print(json.dumps(result, indent=2))
