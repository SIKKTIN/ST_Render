# M1911 test asset

`source/NonRigged_M1911.fbx` is the first FBX model used by the renderer. The
default prefab loads only this main pistol body; the magazine and ammunition
FBX files remain available for future scene composition.

The matching PBR maps live under `pbr_textures/PBR_Textures/M1911/` and are
bound per FBX material (`Barrel`, `Frame`, `Grip`, and `Slide`). The software
renderer downsamples these 2048px maps to a 512px working copy when importing
the model so the editor remains responsive while preserving the original
resources on disk.
