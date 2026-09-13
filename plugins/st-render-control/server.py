#!/usr/bin/env python3
"""Dependency-free local MCP server for ST_Render_Manager."""

from __future__ import annotations

import base64
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import uuid
from typing import Any


SERVER_NAME = "st-render-control"
SERVER_VERSION = "0.1.0"
SUPPORTED_PROTOCOLS = ("2025-06-18", "2024-11-05")
PROJECT_ROOT = Path(__file__).resolve().parents[2]


class ToolFailure(RuntimeError):
    pass


def _candidate_executables(name: str) -> list[Path]:
    override = os.environ.get("ST_RENDER_MANAGER_PATH" if "Manager" in name else "ST_RENDER_DUMP_PATH")
    candidates: list[Path] = []
    if override:
        candidates.append(Path(override).expanduser())
    candidates.extend(
        [
            PROJECT_ROOT / "build" / "bin" / name,
            PROJECT_ROOT / "build" / "bin" / "Debug" / name,
            PROJECT_ROOT / "build" / "bin" / "Release" / name,
            PROJECT_ROOT / "build" / "bin" / "RelWithDebInfo" / name,
            PROJECT_ROOT / "build" / "debug" / "bin" / "Debug" / name,
            PROJECT_ROOT / "build" / "release" / "bin" / "RelWithDebInfo" / name,
        ]
    )
    return candidates


def _find_executable(name: str) -> Path:
    candidates = _candidate_executables(name)
    if candidates and os.environ.get("ST_RENDER_MANAGER_PATH" if "Manager" in name else "ST_RENDER_DUMP_PATH"):
        if candidates[0].is_file():
            return candidates[0].resolve()
    existing = [candidate for candidate in candidates if candidate.is_file()]
    if existing:
        return max(existing, key=lambda path: path.stat().st_mtime).resolve()
    searched = ", ".join(str(path) for path in candidates)
    raise ToolFailure(f"{name} was not found. Build the project first. Searched: {searched}")


def _runtime_candidates() -> list[Path]:
    override = os.environ.get("ST_RENDER_MCP_DIR")
    candidates: list[Path] = []
    if override:
        candidates.append(Path(override).expanduser())
    candidates.extend(
        [
            PROJECT_ROOT / "build" / "bin" / "McpBridge",
            PROJECT_ROOT / "build" / "bin" / "Debug" / "McpBridge",
            PROJECT_ROOT / "build" / "bin" / "Release" / "McpBridge",
            PROJECT_ROOT / "build" / "bin" / "RelWithDebInfo" / "McpBridge",
            PROJECT_ROOT / "build" / "debug" / "bin" / "Debug" / "McpBridge",
            PROJECT_ROOT / "build" / "release" / "bin" / "RelWithDebInfo" / "McpBridge",
        ]
    )
    return candidates


def _read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ToolFailure(f"Unable to read {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ToolFailure(f"Expected a JSON object in {path}")
    return value


def _active_runtime(max_age_seconds: float = 3.0) -> tuple[Path, dict[str, Any]]:
    now_ms = int(time.time() * 1000)
    live: list[tuple[int, Path, dict[str, Any]]] = []
    for runtime in _runtime_candidates():
        state_path = runtime / "state.json"
        if not state_path.is_file():
            continue
        try:
            state = _read_json(state_path)
            heartbeat = int(state.get("heartbeatUnixMs", 0))
        except (ToolFailure, TypeError, ValueError):
            continue
        if heartbeat > 0 and now_ms - heartbeat <= int(max_age_seconds * 1000):
            live.append((heartbeat, runtime, state))
    if not live:
        raise ToolFailure("ST_Render_Manager is not running or its MCP bridge is not responding. Call launch_app first.")
    _, runtime, state = max(live, key=lambda item: item[0])
    return runtime, state


def _send_app_command(command: str, params: dict[str, Any] | None = None, timeout: float = 8.0) -> dict[str, Any]:
    runtime, _ = _active_runtime()
    requests_dir = runtime / "requests"
    responses_dir = runtime / "responses"
    requests_dir.mkdir(parents=True, exist_ok=True)
    responses_dir.mkdir(parents=True, exist_ok=True)

    request_id = uuid.uuid4().hex
    request = {"id": request_id, "command": command, "params": params or {}}
    temporary = requests_dir / f"{request_id}.tmp"
    request_path = requests_dir / f"{request_id}.json"
    response_path = responses_dir / f"{request_id}.json"
    temporary.write_text(json.dumps(request, ensure_ascii=False), encoding="utf-8")
    os.replace(temporary, request_path)

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if response_path.is_file():
            try:
                response = _read_json(response_path)
            finally:
                response_path.unlink(missing_ok=True)
            if not response.get("ok"):
                raise ToolFailure(str(response.get("error", "The application rejected the command")))
            result = response.get("result", {})
            return result if isinstance(result, dict) else {"value": result}
        time.sleep(0.025)

    request_path.unlink(missing_ok=True)
    raise ToolFailure(f"Timed out waiting for ST_Render_Manager to handle {command}")


TOOLS: list[dict[str, Any]] = [
    {
        "name": "get_status",
        "title": "Get ST Render status",
        "description": "Check whether ST_Render_Manager is running and inspect its selected module and canvas state.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": True, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "list_modules",
        "title": "List ST Render modules",
        "description": "List the test and review modules exposed by the running ST_Render_Manager application.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": True, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "select_module",
        "title": "Select an ST Render module",
        "description": "Switch the running application to a module by exact case-insensitive name or by the index returned from list_modules.",
        "inputSchema": {
            "type": "object",
            "properties": {"name": {"type": "string"}, "index": {"type": "integer", "minimum": 0}},
            "additionalProperties": False,
        },
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "rerender",
        "title": "Rerender the current module",
        "description": "Ask ST_Render_Manager to redraw the selected module immediately.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "set_resolution",
        "title": "Set render resolution",
        "description": "Change the active render-target resolution using a preset: 0=640x480, 1=800x600, 2=960x540, 3=1280x720.",
        "inputSchema": {"type": "object", "properties": {"preset": {"type": "integer", "minimum": 0, "maximum": 3}}, "required": ["preset"], "additionalProperties": False},
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "save_scene",
        "title": "Save 3D scene",
        "description": "Save the current 3D Render scene as a versioned JSON file. Omit path to use the current scene path.",
        "inputSchema": {"type": "object", "properties": {"path": {"type": "string"}}, "additionalProperties": False},
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "load_scene",
        "title": "Load 3D scene",
        "description": "Load a versioned JSON scene file into 3D Render. Omit path to use the current scene path.",
        "inputSchema": {"type": "object", "properties": {"path": {"type": "string"}}, "additionalProperties": False},
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "list_shaders",
        "title": "List 3D Render shaders",
        "description": "List shader scripts available to the 3D Render module.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": True, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "select_shader",
        "title": "Select a 3D Render shader",
        "description": "Select a repository shader by its catalog index, or use index -1 for the built-in Blinn-Phong shader.",
        "inputSchema": {"type": "object", "properties": {"index": {"type": "integer", "minimum": -1}}, "required": ["index"], "additionalProperties": False},
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "list_models",
        "title": "List 3D Render models",
        "description": "List OBJ model assets available to the 3D Render module.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": True, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "select_model",
        "title": "Select a 3D Render model",
        "description": "Select a repository OBJ model by catalog index and rerender 3D Render.",
        "inputSchema": {"type": "object", "properties": {"index": {"type": "integer", "minimum": 0}}, "required": ["index"], "additionalProperties": False},
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "list_textures",
        "title": "List 3D Render textures",
        "description": "List repository image assets available for material texture slots.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": True, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "select_texture",
        "title": "Select a material texture",
        "description": "Assign a repository texture to the selected object's diffuse, roughness, metallic, or normal slot. Use index -1 to clear a slot.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "slot": {"type": "string", "enum": ["diffuse", "baseColor", "roughness", "metallic", "normal"]},
                "index": {"type": "integer", "minimum": -1}
            },
            "required": ["slot", "index"],
            "additionalProperties": False
        },
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "list_scene_objects",
        "title": "List 3D scene objects",
        "description": "List model objects in the 3D Render scene, including transforms and selection.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": True, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "add_scene_object",
        "title": "Add a 3D scene object",
        "description": "Create a scene object from a model catalog index and select it.",
        "inputSchema": {"type": "object", "properties": {"modelIndex": {"type": "integer", "minimum": 0}}, "required": ["modelIndex"], "additionalProperties": False},
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "select_scene_object",
        "title": "Select a 3D scene object",
        "description": "Select a scene object by its zero-based hierarchy index.",
        "inputSchema": {"type": "object", "properties": {"index": {"type": "integer", "minimum": 0}}, "required": ["index"], "additionalProperties": False},
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "duplicate_scene_object",
        "title": "Duplicate selected scene object",
        "description": "Duplicate the selected scene object and offset the copy slightly.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "delete_scene_object",
        "title": "Delete selected scene object",
        "description": "Delete the currently selected scene object.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": False, "destructiveHint": True, "openWorldHint": False},
    },
    {
        "name": "set_scene_object_transform",
        "title": "Set a 3D scene object transform",
        "description": "Set position, Euler rotation in degrees, and/or scale for a scene object.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "index": {"type": "integer", "minimum": 0},
                "position": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
                "rotation": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
                "scale": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3}
            },
            "additionalProperties": False
        },
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "get_light",
        "title": "Read 3D Render light",
        "description": "Read the active direction light settings from 3D Render.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": True, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "set_light",
        "title": "Set 3D Render light",
        "description": "Set the active direction light direction and/or intensity.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "direction": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
                "intensity": {"type": "number", "minimum": 0, "maximum": 5},
            },
            "additionalProperties": False,
        },
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "get_console_output",
        "title": "Read ST Render console output",
        "description": "Read the console text produced by the currently selected module.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": True, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "capture_canvas",
        "title": "Capture the ST Render canvas",
        "description": "Rerender and return the current application canvas at the active render resolution as an image.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": True, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "launch_app",
        "title": "Launch ST Render",
        "description": "Start the locally built ST_Render_Manager application and wait for its control bridge.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "shutdown_app",
        "title": "Close ST Render",
        "description": "Request a clean shutdown of the running ST_Render_Manager application.",
        "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False},
        "annotations": {"readOnlyHint": False, "destructiveHint": False, "openWorldHint": False},
    },
    {
        "name": "run_render_dump",
        "title": "Run the offline render diagnostic",
        "description": "Run ST_Render_Dump.exe and return its pixel statistics and ASCII diagnostic output.",
        "inputSchema": {
            "type": "object",
            "properties": {"timeout_seconds": {"type": "number", "minimum": 1, "maximum": 120, "default": 30}},
            "additionalProperties": False,
        },
        "annotations": {"readOnlyHint": True, "destructiveHint": False, "openWorldHint": False},
    },
]


def _text_result(text: str, structured: dict[str, Any] | None = None) -> dict[str, Any]:
    result: dict[str, Any] = {"content": [{"type": "text", "text": text}]}
    if structured is not None:
        result["structuredContent"] = structured
    return result


def _launch_app() -> dict[str, Any]:
    try:
        _, state = _active_runtime()
        return {"alreadyRunning": True, "state": state}
    except ToolFailure:
        pass

    executable = _find_executable("ST_Render_Manager.exe")
    creation_flags = 0
    if os.name == "nt":
        creation_flags = subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NO_WINDOW
    process = subprocess.Popen(
        [str(executable)],
        cwd=str(executable.parent),
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        creationflags=creation_flags,
    )

    deadline = time.monotonic() + 12.0
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise ToolFailure(f"ST_Render_Manager exited during startup with code {process.returncode}")
        try:
            _, state = _active_runtime(max_age_seconds=5.0)
            return {"alreadyRunning": False, "pid": process.pid, "state": state}
        except ToolFailure:
            time.sleep(0.1)
    raise ToolFailure("ST_Render_Manager launched, but its MCP bridge did not become ready within 12 seconds")


def _call_tool(name: str, arguments: dict[str, Any]) -> dict[str, Any]:
    if name == "launch_app":
        result = _launch_app()
        return _text_result("ST_Render_Manager is ready.", result)
    if name == "get_status":
        _, state = _active_runtime()
        return _text_result(
            f"ST_Render_Manager is running; selected module: {state.get('selectedModule', '')}.", state
        )
    if name == "list_modules":
        result = _send_app_command("list_modules")
        modules = result.get("modules", [])
        return _text_result(f"Found {len(modules)} modules.", result)
    if name == "select_module":
        if "name" not in arguments and "index" not in arguments:
            raise ToolFailure("Provide either name or index")
        result = _send_app_command("select_module", arguments)
        return _text_result(f"Selected module: {result.get('selectedModule', '')}.", result)
    if name == "rerender":
        result = _send_app_command("rerender")
        return _text_result(f"Rerendered {result.get('selectedModule', '')}.", result)
    if name == "set_resolution":
        result = _send_app_command("set_resolution", arguments)
        canvas = result.get("canvas", {})
        return _text_result(
            f"Set render resolution to {canvas.get('width', '?')}x{canvas.get('height', '?')}.", result
        )
    if name == "save_scene":
        result = _send_app_command("save_scene", arguments)
        return _text_result(f"Saved scene to {result.get('path', '')}.", result)
    if name == "load_scene":
        result = _send_app_command("load_scene", arguments)
        return _text_result(f"Loaded scene from {result.get('path', '')}.", result)
    if name == "list_shaders":
        result = _send_app_command("list_shaders")
        return _text_result(f"Found {len(result.get('shaders', []))} shaders.", result)
    if name == "select_shader":
        result = _send_app_command("select_shader", arguments)
        return _text_result(f"Selected shader index {result.get('selectedShader', '')}.", result)
    if name == "list_models":
        result = _send_app_command("list_models")
        return _text_result(f"Found {len(result.get('models', []))} models.", result)
    if name == "select_model":
        result = _send_app_command("select_model", arguments)
        return _text_result(f"Selected model index {result.get('selectedModel', '')}.", result)
    if name == "list_textures":
        result = _send_app_command("list_textures")
        return _text_result(f"Found {len(result.get('textures', []))} textures.", result)
    if name == "select_texture":
        result = _send_app_command("select_texture", arguments)
        return _text_result(f"Updated {arguments.get('slot', '')} texture slot.", result)
    if name == "list_scene_objects":
        result = _send_app_command("list_scene_objects")
        return _text_result(f"Found {len(result.get('objects', []))} scene objects.", result)
    if name == "add_scene_object":
        result = _send_app_command("add_scene_object", arguments)
        return _text_result(f"Added scene object {result.get('selectedObject', '')}.", result)
    if name == "select_scene_object":
        result = _send_app_command("select_scene_object", arguments)
        return _text_result(f"Selected scene object {result.get('selectedObject', '')}.", result)
    if name == "duplicate_scene_object":
        result = _send_app_command("duplicate_scene_object")
        return _text_result(f"Duplicated scene object as {result.get('selectedObject', '')}.", result)
    if name == "delete_scene_object":
        result = _send_app_command("delete_scene_object")
        return _text_result("Deleted the selected scene object.", result)
    if name == "set_scene_object_transform":
        result = _send_app_command("set_scene_object_transform", arguments)
        return _text_result(f"Updated scene object {result.get('selectedObject', '')}.", result)
    if name == "get_light":
        result = _send_app_command("get_light")
        return _text_result("Read the active 3D Render light.", result)
    if name == "set_light":
        result = _send_app_command("set_light", arguments)
        return _text_result("Updated the active 3D Render light.", result)
    if name == "get_console_output":
        result = _send_app_command("get_console_output")
        output = str(result.get("output", ""))
        return _text_result(output or "The selected module produced no console output.", result)
    if name == "capture_canvas":
        result = _send_app_command("capture_canvas", timeout=15.0)
        image_path = Path(str(result.get("path", "")))
        if not image_path.is_file():
            raise ToolFailure(f"The application did not create the capture: {image_path}")
        encoded = base64.b64encode(image_path.read_bytes()).decode("ascii")
        return {
            "content": [
                {"type": "text", "text": f"Captured {result.get('module', '')} to {image_path}."},
                {"type": "image", "data": encoded, "mimeType": "image/bmp"},
            ],
            "structuredContent": result,
        }
    if name == "shutdown_app":
        result = _send_app_command("shutdown")
        return _text_result("ST_Render_Manager accepted the shutdown request.", result)
    if name == "run_render_dump":
        timeout = float(arguments.get("timeout_seconds", 30))
        if not 1 <= timeout <= 120:
            raise ToolFailure("timeout_seconds must be between 1 and 120")
        executable = _find_executable("ST_Render_Dump.exe")
        completed = subprocess.run(
            [str(executable)],
            cwd=str(executable.parent),
            capture_output=True,
            text=True,
            timeout=timeout,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
        )
        structured = {
            "exitCode": completed.returncode,
            "stdout": completed.stdout,
            "stderr": completed.stderr,
        }
        return _text_result(completed.stdout or completed.stderr or "The diagnostic produced no output.", structured)
    raise ToolFailure(f"Unknown tool: {name}")


def _handle_request(message: dict[str, Any]) -> dict[str, Any] | None:
    request_id = message.get("id")
    method = message.get("method")
    params = message.get("params") or {}
    if request_id is None:
        return None

    try:
        if method == "initialize":
            requested = str(params.get("protocolVersion", SUPPORTED_PROTOCOLS[0]))
            protocol = requested if requested in SUPPORTED_PROTOCOLS else SUPPORTED_PROTOCOLS[0]
            result = {
                "protocolVersion": protocol,
                "capabilities": {"tools": {"listChanged": False}},
                "serverInfo": {"name": SERVER_NAME, "version": SERVER_VERSION},
                "instructions": (
                    "Use launch_app before live application tools when ST_Render_Manager is not running. "
                    "Call list_modules before select_module when the desired module name is uncertain."
                ),
            }
        elif method == "ping":
            result = {}
        elif method == "tools/list":
            result = {"tools": TOOLS}
        elif method == "tools/call":
            tool_name = str(params.get("name", ""))
            arguments = params.get("arguments") or {}
            if not isinstance(arguments, dict):
                raise ToolFailure("Tool arguments must be an object")
            result = _call_tool(tool_name, arguments)
        else:
            return {
                "jsonrpc": "2.0",
                "id": request_id,
                "error": {"code": -32601, "message": f"Method not found: {method}"},
            }
        return {"jsonrpc": "2.0", "id": request_id, "result": result}
    except ToolFailure as exc:
        return {
            "jsonrpc": "2.0",
            "id": request_id,
            "result": {"content": [{"type": "text", "text": str(exc)}], "isError": True},
        }
    except subprocess.TimeoutExpired as exc:
        return {
            "jsonrpc": "2.0",
            "id": request_id,
            "result": {"content": [{"type": "text", "text": f"Command timed out: {exc}"}], "isError": True},
        }


def main() -> int:
    for raw_line in sys.stdin.buffer:
        if not raw_line.strip():
            continue
        try:
            message = json.loads(raw_line)
            if not isinstance(message, dict):
                raise ValueError("JSON-RPC message must be an object")
            response = _handle_request(message)
        except (json.JSONDecodeError, ValueError) as exc:
            response = {
                "jsonrpc": "2.0",
                "id": None,
                "error": {"code": -32700, "message": f"Parse error: {exc}"},
            }
        if response is not None:
            sys.stdout.write(json.dumps(response, ensure_ascii=False, separators=(",", ":")) + "\n")
            sys.stdout.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
