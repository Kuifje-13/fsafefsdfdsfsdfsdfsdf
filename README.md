# Input Buffer (v2, from scratch)

Settings: enable, starting preset, press buffer ticks (0-16), wave release delay (0-8), Player 2, logging. F6 in a level cycles Custom / 2-2 / 4-4 / 8-8 / 16-8 / Off.

Build: install Geode SDK + CLI, set GEODE_SDK, then `geode build`.
For practice/casual use. Don't use it on records where input assists are banned.

## Getting the .geode without a local toolchain
Push this folder to a GitHub repo (branch `main`). The workflow in `.github/workflows/build.yml`
builds it on a Windows runner; download the `.geode` from the run's Artifacts, then drop it in
`Geometry Dash/geode/mods/`.
