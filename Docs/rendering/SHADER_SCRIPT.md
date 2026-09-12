# ST_Render Shader Script

Shader scripts use two blocks: `vertex` and `fragment`.

```text
vertex {
    output.position = uniform.mvp * input.position;
    output.normal = normalize(input.normal);
    output.varying0 = vec4(input.uv, 0, 1);
}

fragment {
    output.color = input.color;
}
```

## Inputs

Vertex stage:

- `input.position`: `vec4`
- `input.normal`: `vec3`
- `input.uv`: `vec2`
- `input.color`: `vec4`
- `uniform.model`, `uniform.view`, `uniform.projection`, `uniform.mvp`, `uniform.normal`
- Named parameters supplied through `Renderer::setShaderFloat`, `setShaderVector2`, `setShaderVector3`, `setShaderVector4`, or `setShaderMatrix`

Fragment stage:

- `input.position`: interpolated fragment position
- `input.worldPosition`: `vec3`
- `input.normal`: normalized `vec3`
- `input.uv`: `vec2`
- `input.color`: `vec4`
- `input.varying0` ... `input.varying15`: `vec4`

## Outputs

- Vertex: `output.position`, `output.normal`, `output.uv`, `output.worldPosition`, `output.varying0` ... `output.varying15`
- Fragment: `output.color`

Supported expressions currently include vector constructors (`vec2`, `vec3`, `vec4`), `normalize`, `dot`, `clamp`, `saturate`, `mix`, and arithmetic operators.

Texture sampling is available in the fragment stage with `texture(input.uv)` once a texture has been supplied through `Renderer::setTexture`.

Scripts are parsed when loaded. A failed compile leaves the previous valid program available to the caller.

## Loading from C++

```cpp
std::string error;
auto program = ST::ScriptShaderProgram::fromFile("Data/Shaders/basic.stshader", error);
if (program) {
    renderer.setShaderProgram(program);
} else {
    std::cerr << error << std::endl;
}
```

For file-based hot reload, use the renderer-owned manager at a frame boundary:

```cpp
std::string error;
renderer.loadShaderFile("Data/Shaders/basic.stshader", error);

// Once per frame, before renderer.render(mesh):
if (!renderer.reloadShaderIfChanged(error) && !error.empty()) {
    std::cerr << "Shader reload failed: " << error << std::endl;
}
```

When a reload fails, the previous valid shader remains active.
