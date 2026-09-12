# ST Render MCP Control

`st-render-control` is a local stdio MCP server for controlling
`ST_Render_Manager.exe`. It has no third-party Python dependencies.

## Architecture

```text
Codex / ChatGPT desktop
        | MCP over stdio
        v
plugins/st-render-control/server.py
        | atomic JSON request/response files
        v
build/bin/McpBridge
        | polled on the SDL main thread
        v
ST_Render_Manager.exe
```

The application processes commands from its normal SDL loop. No worker thread
touches SDL, ImGui, modules, or the canvas.

## Tools

| Tool | Purpose |
|---|---|
| `launch_app` | Start the built application and wait for the bridge |
| `get_status` | Read application, selected module, and canvas status |
| `list_modules` | List selectable modules |
| `select_module` | Select a module by name or index |
| `rerender` | Redraw the selected module |
| `get_console_output` | Read the selected module's console text |
| `capture_canvas` | Return the canvas as a BMP image |
| `shutdown_app` | Request a clean application shutdown |
| `run_render_dump` | Execute the offline renderer diagnostic |

## Development test

```powershell
python plugins/st-render-control/scripts/smoke_test.py
```

For a complete application launch/select/capture/shutdown check:

```powershell
python plugins/st-render-control/scripts/live_smoke_test.py
```

After building `ST_Render_Manager`, the server can also be added directly to a
Codex host without installing the plugin:

```powershell
codex mcp add st-render-control -- python E:\Project\C++\ST_Render\plugins\st-render-control\server.py
```

Use `codex mcp list` to verify the connection. Restart the Codex desktop app or
IDE extension after changing MCP configuration.

## Runtime files

The application creates `McpBridge/` beside its executable. Requests and
responses are single-use. `state.json` is refreshed twice per second and is
removed during a clean shutdown. Canvas captures are kept in
`McpBridge/captures/` for inspection.

Environment overrides supported by the MCP server:

- `ST_RENDER_MANAGER_PATH`
- `ST_RENDER_DUMP_PATH`
- `ST_RENDER_MCP_DIR`
