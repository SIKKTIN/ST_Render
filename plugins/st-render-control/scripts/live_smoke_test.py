#!/usr/bin/env python3
"""End-to-end test for the MCP server and the running application bridge."""

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
next_id = 1


def request(method, params=None):
    global next_id
    payload = {"jsonrpc": "2.0", "id": next_id, "method": method, "params": params or {}}
    next_id += 1
    process.stdin.write(json.dumps(payload) + "\n")
    process.stdin.flush()
    response = json.loads(process.stdout.readline())
    assert response.get("id") == payload["id"], response
    return response["result"]


request(
    "initialize",
    {"protocolVersion": "2025-06-18", "capabilities": {}, "clientInfo": {"name": "live-smoke", "version": "1"}},
)
dumped = request("tools/call", {"name": "run_render_dump", "arguments": {}})
assert dumped["structuredContent"]["exitCode"] == 0, dumped

launched = request("tools/call", {"name": "launch_app", "arguments": {}})
assert not launched.get("isError"), launched
launch_data = launched["structuredContent"]
started_by_test = not launch_data.get("alreadyRunning", False)

try:
    listed = request("tools/call", {"name": "list_modules", "arguments": {}})
    assert not listed.get("isError"), listed
    names = [module["name"] for module in listed["structuredContent"]["modules"]]
    assert "3D Render" in names, names
    test_components = [
        module for module in listed["structuredContent"]["modules"]
        if module.get("category") == "Test Components"
    ]
    assert {module["name"] for module in test_components} == {
        "FrameBuffer Test", "Rasterizer Test", "Texture Test", "Shader Test"
    }, test_components

    selected = request("tools/call", {"name": "select_module", "arguments": {"name": "3D Render"}})
    assert selected["structuredContent"]["selectedModule"] == "3D Render", selected

    models = request("tools/call", {"name": "list_models", "arguments": {}})
    model_entries = models["structuredContent"]["models"]
    assert model_entries, models

    scene = request("tools/call", {"name": "list_scene_objects", "arguments": {}})
    assert len(scene["structuredContent"]["objects"]) == 1, scene

    added = request("tools/call", {
        "name": "add_scene_object",
        "arguments": {"modelIndex": model_entries[min(1, len(model_entries) - 1)]["index"]},
    })
    assert len(added["structuredContent"]["objects"]) == 2, added
    moved = request("tools/call", {
        "name": "set_scene_object_transform",
        "arguments": {"position": [-1.5, 0.0, 0.0], "rotation": [0.0, 25.0, 0.0]},
    })
    assert moved["structuredContent"]["objects"][1]["position"] == [-1.5, 0.0, 0.0], moved

    duplicated = request("tools/call", {"name": "duplicate_scene_object", "arguments": {}})
    assert len(duplicated["structuredContent"]["objects"]) == 3, duplicated
    deleted = request("tools/call", {"name": "delete_scene_object", "arguments": {}})
    assert len(deleted["structuredContent"]["objects"]) == 2, deleted

    captured = request("tools/call", {"name": "capture_canvas", "arguments": {}})
    assert any(item.get("type") == "image" for item in captured["content"]), captured
    capture_path = Path(captured["structuredContent"]["path"])
    assert capture_path.is_file() and capture_path.stat().st_size > 0, capture_path
    print(f"Live MCP test passed; modules={len(names)}, capture={capture_path}")
finally:
    if started_by_test:
        request("tools/call", {"name": "shutdown_app", "arguments": {}})
    process.stdin.close()
    process.wait(timeout=5)
