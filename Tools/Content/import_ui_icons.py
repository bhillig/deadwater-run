# Copyright Brandon Hillig 2026
# Run by Tools/RenderIcons.ps1 -Import inside UnrealEditor-Cmd. Imports every T_ICON_*.png from the folder named by
# DW_UI_ICON_DIR (the icon studio's output) into /Game/DeadwaterRun/UI/Icons as UI textures: no mips, UI LOD group,
# never streamed, UserInterface2D compression. Re-imports changed files in place, so references survive re-renders.
import os

import unreal

LOG = []
ERRORS = []
ROOT = "/Game/DeadwaterRun/UI/Icons"
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    LOG.append(msg)


def fail(msg):
    ERRORS.append(msg)
    LOG.append("ERROR " + msg)


source_dir = os.environ.get("DW_UI_ICON_DIR", "")
files = sorted(f for f in os.listdir(source_dir) if f.startswith("T_ICON_") and f.endswith(".png")) if os.path.isdir(source_dir) else []
if not files:
    fail("no T_ICON_*.png in DW_UI_ICON_DIR (%s); run Tools/RenderIcons.ps1 first" % source_dir)

wanted = {
    "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
    "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
    "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
    "never_stream": True,
    "srgb": True,
}
imported = 0
for name in files:
    asset = os.path.splitext(name)[0]
    path = "%s/%s" % (ROOT, asset)
    source = os.path.join(source_dir, name)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", ROOT)
    task.set_editor_property("destination_name", asset)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tools.import_asset_tasks([task])
    texture = eal.load_asset(path)
    if texture is None or not isinstance(texture, unreal.Texture2D):
        fail("import failed for %s" % path)
        continue
    for prop, value in wanted.items():
        texture.set_editor_property(prop, value)
    try:
        # Committed assets carry the file name only, never a local path.
        texture.get_editor_property("asset_import_data").scripted_add_filename(name, 0, "")
    except Exception as exc:
        fail("could not scrub import path of %s (%s)" % (asset, exc))
    eal.save_asset(path, only_if_is_dirty=False)
    imported += 1
    log("imported %s (%dx%d)" % (path, texture.blueprint_get_size_x(), texture.blueprint_get_size_y()))

log("summary: imported %d of %d" % (imported, len(files)))
print("DWCONTENT-BEGIN")
for line in LOG:
    print("DWCONTENT " + line)
print("DWCONTENT " + ("CONTENT PASS" if not ERRORS else "CONTENT FAIL"))
print("DWCONTENT-END")
