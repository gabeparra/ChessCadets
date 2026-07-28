# Editor Automation Tools

Python scripts that drive the **running** Unreal Editor via Python Remote Execution.
No editor restart needed — they edit the currently open level/assets live, exactly like
clicking through the UI.

## Setup (once per machine)

1. The project already enables the **Python Editor Script Plugin** with Remote Execution.
   Verify: Editor → Project Settings → Plugins → Python → "Enable Remote Execution" is on.
2. You need a system Python 3.x on PATH (no extra packages required — the driver reuses
   UE's own `remote_execution.py`).
3. If your engine is not at `D:\UE_5.7`, edit the `sys.path.append(...)` line at the top
   of `ue_remote.py` to point at your engine's
   `Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python` folder.

## Usage

Open the editor with the map you want to work on, then from the project root:

```
python Tools/ue_remote.py Tools/board_diag.py
```

The script runs *inside* the editor process; its `print()` output is relayed back to your
terminal.

## Scripts

| Script | What it does | Safe to run? |
|---|---|---|
| `ue_remote.py` | The driver — sends any `.py` file to the running editor. | n/a |
| `board_diag.py` | Prints the ChessBoard actor's material slots for the open map. Use when highlights/glow look wrong on a map. | Yes — read-only |
| `fix_board_materials.py` | Repairs a board whose material slots point at junk assets. | Mostly — review before running |
| `fix_boards.py` | Board actor cleanup pass. | Mostly — review before running |
| `build_chess_maps.py` | Recipe for building an arena map skeleton. | **No on existing maps** — creates/overwrites actors |
| `decorate_maps.py` | Procedural decoration pass for arena maps. | **No on existing maps** — spawns many actors |

## Rules of thumb

- **Read-only diags: run freely.** Anything else: read the script first — several passes
  delete-and-rebuild actors or material graphs, which destroys manual edits on the map.
- One editor instance at a time; the driver connects to whichever editor it discovers.
- Editing a map you have open works and marks it dirty — the script (or you) must save.
- Never run a pass against a map someone else is actively editing on their machine.
- Ad-hoc / one-shot session scripts belong in `/Scripts/` (gitignored), not here. Promote
  a script to `Tools/` only when it's reusable and its destructive parts are documented.
