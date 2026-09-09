# Vendored NRD sources

This directory contains NVIDIA NRD **v4.17.3** (30 April 2026), pinned to
[the published release](https://github.com/NVIDIA-RTX/NRD/releases/tag/v4.17.3).
The upstream licenses are retained in each package.

| Package | Revision | Source archive SHA-256 |
| --- | --- | --- |
| NRD | `v4.17.3` | `c3a71eb0c3577f664f6f8bf3be7c585a39911fbf2895c144af10c58b861ad6ba` |
| ShaderMake | `18f5a344e7ca8fa65daaf079d07bc8ce38453e05` | `35de2547e28cf10f18a0e2782cf154a3d1610212d00d829fa8e73a18210623b6` |
| MathLib | `v11` | `9bd668daa3770a684ca86ccfbd15dd5e1927f259bc531034a4fd943e1f98f5c9` |

The archives are from `https://codeload.github.com/NVIDIA-RTX/<package>/zip/`:
`refs/tags/v4.17.3`, `18f5a344e7ca8fa65daaf079d07bc8ce38453e05`, and
`refs/tags/v11`, respectively. ShaderMake and MathLib are the exact revisions
requested by the upstream NRD CMake file. They are stored under `External/`
so configuring the renderer does not download dependencies.

Local integration changes are limited to CMake dependency wiring. The renderer
builds embedded SPIR-V with the DXC from the installed Vulkan SDK, disables
unused D3D shader variants and NRI, and uses its own RHI executor. Normal
encoding is `R10_G10_B10_A2_UNORM` (`2`); roughness is `LINEAR` (`1`). CMake
generates `Shaders/NRDConfig.hlsli` with the same settings used by the library.
The upstream shader and C++ algorithms are unmodified.
