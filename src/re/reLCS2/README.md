# reLCS2 — Liberty City Stories

This folder owns the game's runtime policies, mission commands, HUD, frontend adapters, vehicle behavior and native rendering sources used by Storyland Play Game and Play Test.

The common bytecode interpreter, shared simulation, asset readers and platform hosts remain under `src/`. They consume this module directly; there is no duplicated game engine or second set of save structures.

Mission command fragments under `script/` are included in their established declaration order by the shared interpreter. Pure HUD state/layout code is consolidated separately from adapters that depend on the complete mission environment.

Shader aliases are recorded in `cmake/shader_sources.json`; aliases retain the exact original shader text and do not change a rendering profile.
