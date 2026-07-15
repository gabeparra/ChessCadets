"""
ChessKids - Assign the translucent holographic material to every board so the
grid/scan overlay stops rendering as an opaque sheet over the board.

ROOT CAUSE (confirmed via board_diag): GridOverlayMaterial and
HolographicScanMaterial are null on both BP_ChessBoard and the C++ class, so the
overlay planes fall back to the opaque /Engine WorldGridMaterial and cover the
board. The project DOES contain a translucent material, /Game/GeneratedTextures/
translucentmat (BLEND_TRANSLUCENT) -- it just was never wired up.

WHAT IT DOES (idempotent, no rebuild needed)
--------------------------------------------
  - Tries to set GridOverlayMaterial + HolographicScanMaterial on the
    BP_ChessBoard default (so future spawns are correct), only if unset.
  - For each level (L_Field + the five piece arenas): finds every ChessBoard,
    and for any whose overlay material is unset:
        * sets the instance GridOverlayMaterial/HolographicScanMaterial props
          (persists; OnConstruction rebuilds the planes translucent), and
        * sets the live GridOverlay/ScanPlane component materials directly
          (immediate visual), then saves the level.
  - Leaves square materials alone (they show the engine grid, not the bug).

RUN ONCE: Tools -> Execute Python Script... -> this file.
(Click "Don't Save" on any open-level prompt.)
"""

import unreal

MAPS_DIR = "/Game/Maps"
HOLO_PATH = "/Game/GeneratedTextures/translucentmat"
LEVELS = [
    "/Game/Maps/L_Field",
    "/Game/Maps/L_Pawn",
    "/Game/Maps/L_Knight",
    "/Game/Maps/L_Bishop",
    "/Game/Maps/L_Rook",
    "/Game/Maps/L_Royalty",
]
OVERLAY_PROPS = ("GridOverlayMaterial", "HolographicScanMaterial")
OVERLAY_COMPS = ("GridOverlay", "ScanPlane")

editor_assets = unreal.EditorAssetLibrary
level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(m):
    unreal.log("[ChessKids holofix] " + str(m))


def warn(m):
    unreal.log_warning("[ChessKids holofix] " + str(m))


def set_overlay_on(obj, holo):
    """Set the two overlay material props on obj (instance or CDO) where unset.
    Returns number of props changed."""
    changed = 0
    for p in OVERLAY_PROPS:
        try:
            if obj.get_editor_property(p) is None:
                obj.set_editor_property(p, holo)
                changed += 1
        except Exception as exc:
            warn("  prop {0}: {1}".format(p, exc))
    return changed


def set_overlay_components(actor, holo):
    """Directly retint the live overlay plane components for immediate effect."""
    try:
        comps = actor.get_components_by_class(unreal.StaticMeshComponent)
    except Exception:
        return
    for c in comps:
        try:
            if c.get_name() in OVERLAY_COMPS:
                c.set_material(0, holo)
        except Exception:
            pass


def main():
    holo = editor_assets.load_asset(HOLO_PATH)
    if not holo:
        warn("could not load " + HOLO_PATH + " -- aborting")
        return
    log("holographic material: " + holo.get_path_name())

    # 1. Configure BP_ChessBoard defaults (best-effort).
    try:
        bp = unreal.load_class(None, "/Game/Blueprints/BP_ChessBoard.BP_ChessBoard_C")
        if bp:
            cdo = unreal.get_default_object(bp)
            if set_overlay_on(cdo, holo):
                editor_assets.save_asset("/Game/Blueprints/BP_ChessBoard", only_if_is_dirty=False)
                log("BP_ChessBoard defaults updated + saved")
            else:
                log("BP_ChessBoard already had overlay materials")
    except Exception as exc:
        warn("BP default update failed (continuing): {0}".format(exc))

    # 2. Fix every board instance in every level.
    for pkg in LEVELS:
        try:
            if not editor_assets.does_asset_exist(pkg):
                warn("missing level: " + pkg)
                continue
            if not level_subsystem.load_level(pkg):
                warn("could not load " + pkg)
                continue
            boards = [a for a in actor_subsystem.get_all_level_actors()
                      if isinstance(a, unreal.ChessBoard)]
            if not boards:
                warn(pkg + ": no ChessBoard found")
                continue
            touched = 0
            for b in boards:
                changed = set_overlay_on(b, holo)
                set_overlay_components(b, holo)   # immediate visual regardless
                touched += changed
            level_subsystem.save_current_level()
            editor_assets.save_directory(MAPS_DIR, recursive=True, only_if_is_dirty=True)
            log("{0}: {1} board(s), {2} overlay prop(s) set, saved".format(
                pkg, len(boards), touched))
        except Exception as exc:
            warn("error on {0}: {1}".format(pkg, exc))

    log("Done. Overlay should now be translucent. Reload a level (or it will "
        "rebuild on next open) to see the persisted result.")


main()
