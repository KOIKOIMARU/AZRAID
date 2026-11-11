# CG2 Development Rules

## Default working style

- Work autonomously until the requested outcome is genuinely complete.
- Do not pause for permission before normal, reversible actions inside this workspace, including investigation, source edits, builds, game launches, smoke tests, screenshots, temporary diagnostics, and cleanup of files created only for verification.
- When a detail is ambiguous but a safe and reversible choice exists, choose the best-supported option, state the assumption in a progress update, and continue.
- Ask the user only when an action is irreversible, publishes or pushes data externally, requires credentials or payment, or would materially change the requested product direction.
- Platform-enforced approval prompts cannot be bypassed. When escalation is required, request the narrowest reusable permission that covers the recurring build or test action.
- Prefer implementation and evidence over long explanations. Keep progress updates concise and report the final outcome, validation, risks, and remaining limitations.

## Git and preservation

- At the beginning of work, always inspect `git status`, the current branch, and the current uncommitted diff.
- Existing user changes must never be discarded, reset, reverted, overwritten, cleaned, or stashed away without explicit direction.
- Work with unrelated changes in place and keep feature edits narrowly scoped.
- Before a risky or wide refactor, autonomously create a clearly named local checkpoint or `codex/` branch when it materially improves recoverability. A checkpoint may preserve the whole current working state, but feature commits must not silently absorb unrelated user work.
- Local checkpoint commits are allowed. Never push, publish, open a pull request, rewrite history, or delete branches without explicit user direction.
- Never use destructive Git commands such as `git reset --hard`, `git clean`, or forced checkout to solve a conflict.

## Protected paths and editing

- Never read, modify, generate into, move, or delete anything under `project/enc_temp_folder/`.
- Use `rg` or `rg --files` for searches.
- Use `apply_patch` for manual source, shader, project, and configuration edits.
- Dedicated converters, formatters, and generators may write generated assets or perform bulk mechanical transformations.
- Keep edited text files as UTF-8 without BOM.
- Preserve existing line-ending conventions when practical and always run `git diff --check` before completion.

## Implementation and verification cadence

- Make small, logically coherent changes. Build after each meaningful compile-affecting unit, not after every individual numeric or textual tweak.
- During iteration, use the Debug build as the primary compile check.
- For visual or shader changes, run the game and inspect the actual frame; compilation alone is not sufficient.
- At completion, build both Debug and Release and require zero warnings and zero errors.
- After changes that affect gameplay, rendering, resources, loading, lifetime, or threading, launch the Release build, enter gameplay, and run a 15–30 second smoke test unless a longer test is warranted.
- Remove only temporary verification artifacts created by the current task. Do not remove user assets or unrelated files.
- Verify modified text files have no UTF-8 BOM and confirm the final Git status.

## Build commands

Debug:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' project\CG2.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

Release:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' project\CG2.vcxproj /p:Configuration=Release /p:Platform=x64 /m
```

Automated Release smoke test (build, auto-start, monitor, log, and clean exit):

```powershell
& .\project\tools\RunSmokeTest.ps1 -Configuration Release -GameplaySeconds 15
```

The Debug smoke test also verifies that the D3D12 Debug Layer, DRED
breadcrumbs/page-fault reporting, and the InfoQueue are active. It fails when
the Debug Layer records a warning, error, or corruption message. Application,
build, and D3D12 diagnostic logs are stored under `generated/smoke-tests/`.

## DirectX 12 and rendering safety

- Do not force a rendering change when resource states, descriptor lifetime, command-list ordering, synchronization, or GPU ownership are uncertain. Investigate the path, use the Debug Layer/DRED or focused diagnostics when available, and choose a reversible implementation.
- Never reconnect the player-engine exhaust to `ParticleManager::Draw()` unless the generic GPU particle path has been deliberately repaired and validated. Keep the current stable `Object3d` exhaust path until then.
- Avoid runtime allocation in hot rendering or gameplay paths when a fixed pool or preallocation is suitable.
- Treat a successful build as necessary but not sufficient: rendering work requires an actual runtime check.

## Quality direction

- The current visual target is a polished anime-style presentation: approximately 70% Honkai: Star Rail influence for combat readability, VFX, and UI, and 30% Azur Promilia influence for bright atmosphere and color.
- Prefer selective bloom, controlled highlights, readable silhouettes, coherent color scripting, strong feedback, and clean HUD hierarchy over uniformly increasing brightness or effect intensity.
- When visual direction is underspecified, implement a restrained, reversible first pass and validate it in the actual game before further tuning.
