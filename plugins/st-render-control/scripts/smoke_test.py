#!/usr/bin/env python3
"""Small protocol smoke test; does not require ST_Render_Manager to be open."""

import json
from pathlib import Path
import subprocess
import sys


plugin_root = Path(__file__).resolve().parents[1]
process = subprocess.Popen(
    [sys.executable, str(plugin_root / "server.py")],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    text=True,
)


def request(payload):
    process.stdin.write(json.dumps(payload) + "\n")
    process.stdin.flush()
    response = json.loads(process.stdout.readline())
    assert response.get("id") == payload["id"], response
    return response


initialized = request(
    {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {"protocolVersion": "2025-06-18", "capabilities": {}, "clientInfo": {"name": "smoke", "version": "1"}},
    }
)
assert initialized["result"]["serverInfo"]["name"] == "st-render-control"

tools = request({"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}})
tool_names = {tool["name"] for tool in tools["result"]["tools"]}
assert {"launch_app", "list_modules", "select_module", "capture_canvas", "run_render_dump"} <= tool_names

process.stdin.close()
process.wait(timeout=5)
print(f"MCP smoke test passed with {len(tool_names)} tools")
