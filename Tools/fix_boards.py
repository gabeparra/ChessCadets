"""
ChessKids - Replace the bare C++ ChessBoard in each piece level with the
configured BP_ChessBoard (which carries the translucent holographic materials,
square materials, neon edge lights, etc.).

WHY
---
build_chess_maps.py spawned the raw C++ `ChessBoard` class, whose material
properties (LightSquareMaterial / DarkSquareMaterial / GridOverlayMaterial /
HolographicScanMaterial) are all null. With null materials the board's planes
fall back to the opaque /Engine WorldGridMaterial, so the holographic overlay
renders as a solid sheet covering the board. BP_ChessBoard has those materials
assigned, so simply using it fixes the look.

WHAT IT DOES (per piece level, idempotent)
------------------------------------------
  1. find the board actor; if it is already a BP_ChessBoard, skip,
  2. otherwise record its transform + theme index, delete it,
  3. spawn BP_ChessBoard in its place, restamp the theme index,
  4. re-wire ChessManager.Board and ChessCamera.TargetBoard to the new board,
  5. save (World-Partition-safe directory flush).

RUN ONCE: Tools -> Execute Python Script... -> this file.
(Click "Don't Save" on any open-level prompt.)
"""

import unreal

MAPS_DIR = "/Game/Maps"
BP_CLASS_PATH = "/Game/Blueprints/BP_ChessBoard.BP_ChessBoard_C"

LEVELS = [
    ("/Game/Maps/L_Pawn",    0),
    ("/Game/Maps/L_Knight",  1),
    ("/Game/Maps/L_Bishop",  2),
    ("/Game/Maps/L_Rook",    3),
    ("/Game/Maps/L_Royalty", 4),
]

editor_assets = unreal.EditorAssetLibrary
level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(m):
    unreal.log("[ChessKids fixboards] " + str(m))


def warn(m):
    unreal.log_warning("[ChessKids fixboards] " + str(m))


def load_bp_class():
    bp = unreal.load_class(None, BP_CLASS_PATH)
    if not bp:
        warn("could not load " + BP_CLASS_PATH)
        return None
    # Sanity: report the BP's default holographic / square materials so we can
    # confirm it is actually configured (and not itself blank).
    try:
        cdo = unreal.get_default_object(bp)
        for prop in ("GridOverlayMaterial", "HolographicScanMaterial",
                     "LightSquareMaterial", "DarkSquareMaterial"):
            val = cdo.get_editor_property(prop)
            log("  BP default {0} = {1}".format(prop, val.get_name() if val else "None"))
    except Exception as exc:
        warn("could not read BP defaults: {0}".format(exc))
    return bp


def find_actors():
    board = manager = camera = None
    native_board_cls = unreal.ChessBoard.static_class()
    for a in actor_subsystem.get_all_level_actors():
        if isinstance(a, unreal.ChessBoard):
            board = a
        elif isinstance(a, unreal.ChessManager):
            manager = a
        elif isinstance(a, unreal.ChessCamera):
            camera = a
    return board, manager, camera, native_board_cls


def save_level_and_actors():
    level_subsystem.save_current_level()
    editor_assets.save_directory(MAPS_DIR, recursive=True, only_if_is_dirty=True)


def main():
    bp_class = load_bp_class()
    if not bp_class:
        return

    for pkg, theme in LEVELS:
        try:
            if not editor_assets.does_asset_exist(pkg):
                warn("missing level: " + pkg)
                continue
            if not level_subsystem.load_level(pkg):
                warn("could not load " + pkg)
                continue

            board, manager, camera, native_cls = find_actors()
            if board is None:
                warn(pkg + ": no ChessBoard found, skipping")
                continue
            if board.get_class() == bp_class:
                log(pkg + ": board already BP_ChessBoard, skipping")
                continue

            xf = board.get_actor_transform()
            actor_subsystem.destroy_actor(board)

            new_board = actor_subsystem.spawn_actor_from_class(
                bp_class, xf.translation, xf.rotation.rotator())
            if not new_board:
                warn(pkg + ": failed to spawn BP_ChessBoard")
                continue
            new_board.set_actor_label("ChessBoard")
            try:
                new_board.set_editor_property("CurrentThemeIndex", int(theme))
            except Exception:
                pass

            if manager:
                manager.set_editor_property("Board", new_board)
            else:
                warn(pkg + ": no ChessManager to re-wire")
            if camera:
                camera.set_editor_property("TargetBoard", new_board)

            save_level_and_actors()
            log(pkg + ": board replaced with BP_ChessBoard + re-wired + saved")
        except Exception as exc:
            warn("error on {0}: {1}".format(pkg, exc))

    log("Done.")


main()
