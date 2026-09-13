# PBR test material

`metal_plate_02/` is the 1K PNG version of Poly Haven's **Metal Plate 02** texture set.
Poly Haven publishes this asset under the CC0 license:

https://polyhaven.com/a/metal_plate_02

Map assignment used by ST_Render:

- `metal_plate_02_diff_1k.png`: Base Color / Diffuse
- `metal_plate_02_nor_dx_1k.png`: Normal (DirectX)
- `metal_plate_02_rough_1k.png`: Roughness
- `metal_plate_02_metal_1k.png`: Metallic

The optional environment map is `environment/studio_small_01.jpg`, the
Tonemapped JPG download of Poly Haven's Studio Small 01 HDRI. It is used as
an LDR equirectangular reflection source until the renderer gains HDR image
decoding.

Source: https://polyhaven.com/a/studio_small_01

## Vault door PBR test

`Data/ScenePrefab/vault_door_pbr.scene.json` uses the existing 1K **Metal Plate
02** PBR set for the circular vault-door model. This matches the model's metal
construction and avoids stretching a wood texture around a round door.

`rough_pine_door/` is also included as an optional 1K JPG PBR set for testing a
painted-wood material on models with rectangular door UVs. It is not the
default vault-door material.

Poly Haven publishes this asset under the CC0 license:

https://polyhaven.com/a/rough_pine_door

The model is the CC0 **Vault Door** asset from OpenGameArt:

https://opengameart.org/content/vault-door
