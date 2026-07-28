"""Diagnostic: dump board material configuration to a file (no log noise)."""
import unreal

OUT = "D:/unrealprojects/chesskids/Scripts/board_diag.txt"
PROPS = ["LightSquareMaterial", "DarkSquareMaterial", "GridOverlayMaterial",
         "HolographicScanMaterial", "LegalMoveMaterial", "SelectionMaterial",
         "HoverMaterial"]
lines = []


def rec(s):
    lines.append(str(s))


def dump_cdo(title, cls):
    rec("== " + title + " ==")
    if not cls:
        rec("  <class not found>")
        return
    try:
        cdo = unreal.get_default_object(cls)
    except Exception as e:
        rec("  get_default_object ERR: %s" % e)
        return
    for p in PROPS:
        try:
            v = cdo.get_editor_property(p)
            rec("  %s = %s" % (p, v.get_path_name() if v else "None"))
        except Exception as e:
            rec("  %s ERR: %s" % (p, e))


# 1. BP_ChessBoard defaults
bp = unreal.load_class(None, "/Game/Blueprints/BP_ChessBoard.BP_ChessBoard_C")
dump_cdo("BP_ChessBoard CDO", bp)

# 2. Raw C++ ChessBoard defaults
dump_cdo("C++ ChessBoard CDO", unreal.ChessBoard.static_class())

# 3. Blend mode / class of candidate material assets
rec("== candidate material assets ==")
for mp in ["/Game/GeneratedTextures/translucentmat",
           "/Game/GeneratedTextures/T_DarkSquare_Mat",
           "/Game/GeneratedTextures/IMG_3483_Mat",
           "/Game/GeneratedTextures/IMG_3486_Mat"]:
    if not unreal.EditorAssetLibrary.does_asset_exist(mp):
        rec("  %s : MISSING" % mp)
        continue
    m = unreal.EditorAssetLibrary.load_asset(mp)
    info = "  %s : class=%s" % (mp, type(m).__name__)
    for prop in ("blend_mode", "shading_model", "two_sided"):
        try:
            info += " %s=%s" % (prop, m.get_editor_property(prop))
        except Exception:
            pass
    rec(info)

# 4. Everything under GeneratedTextures (to spot any material we missed)
rec("== /Game/GeneratedTextures assets ==")
try:
    for a in unreal.EditorAssetLibrary.list_assets("/Game/GeneratedTextures", recursive=True, include_folder=False):
        rec("  " + a)
except Exception as e:
    rec("  list ERR: %s" % e)

with open(OUT, "w") as f:
    f.write("\n".join(lines))
unreal.log("[board_diag] wrote " + OUT)
