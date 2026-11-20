# Copyright Brandon Hillig 2026
# Run by Tools/ImportAudio.ps1 inside UnrealEditor-Cmd. Imports the approved SFX pick list (DW-311) as SW_* sound waves.
# The plan (resolved source paths, destination, per-asset settings) comes from the JSON file named by the
# DW_AUDIO_IMPORT_PLAN environment variable; no pack path lives in this script or in the content.
# Idempotent: an existing asset is not re-imported (unless the plan says force) but its settings are re-applied.
import json
import os

import unreal

LOG = []
ERRORS = []
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

LOADING = {
    "RetainOnLoad": unreal.SoundWaveLoadingBehavior.RETAIN_ON_LOAD,
    "PrimeOnLoad": unreal.SoundWaveLoadingBehavior.PRIME_ON_LOAD,
    "LoadOnDemand": unreal.SoundWaveLoadingBehavior.LOAD_ON_DEMAND,
}


def log(msg):
    LOG.append(msg)


def fail(msg):
    ERRORS.append(msg)
    LOG.append("ERROR " + msg)


def same(a, b):
    if isinstance(a, float) or isinstance(b, float):
        return abs(float(a) - float(b)) < 1e-4
    return a == b


def load_dep(path):
    asset = eal.load_asset(path) if path else None
    if path and asset is None:
        fail("missing dependency %s (run Tools/CreateAudioContent.ps1 first)" % path)
    return asset


def scrub_source_path(wave, source):
    """Replaces the absolute pack path the importer records with the bare file name, so committed assets carry no local path."""
    try:
        data = wave.get_editor_property("asset_import_data")
        data.scripted_add_filename(os.path.basename(source), 0, "")
    except Exception as exc:
        fail("could not scrub import path of %s (%s)" % (wave.get_name(), exc))


plan_path = os.environ.get("DW_AUDIO_IMPORT_PLAN")
if not plan_path or not os.path.isfile(plan_path):
    fail("DW_AUDIO_IMPORT_PLAN is not set or the plan file is missing")
    plan = {"root": "", "items": []}
else:
    with open(plan_path, "r", encoding="utf-8-sig") as handle:
        plan = json.load(handle)

root = plan["root"]
force = bool(plan.get("force", False))
deps = {}
imported = existing = reconfigured = 0

for item in plan["items"]:
    name = item["asset"]
    folder = "%s/%s" % (root, item["category"]) if item["category"] else root
    path = "%s/%s" % (folder, name)
    sound_class = deps.setdefault(item["sound_class"], load_dep("/Game/DeadwaterRun/Audio/Classes/" + item["sound_class"]))
    concurrency = None
    if item["concurrency"]:
        concurrency = deps.setdefault(item["concurrency"], load_dep("/Game/DeadwaterRun/Audio/Concurrency/" + item["concurrency"]))
    if ERRORS:
        break

    created = False
    if force or not eal.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", item["source"])
        task.set_editor_property("destination_path", folder)
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        created = True
    wave = eal.load_asset(path)
    if wave is None or not isinstance(wave, unreal.SoundWave):
        fail("import failed for %s" % path)
        continue

    changed = created
    wanted = {
        "looping": bool(item["loop"]),
        "compression_quality": int(item["quality"]),
        "loading_behavior": LOADING[item["loading"]],
        "sound_class_object": sound_class,
        "override_concurrency": False,
    }
    for prop, value in wanted.items():
        if not same(wave.get_editor_property(prop), value):
            wave.set_editor_property(prop, value)
            changed = True
    if item["loading"] == "LoadOnDemand":
        # Music streams from disk in chunks. LoadOnDemand is the streaming loading behaviour; older engines also
        # carry a bStreaming flag, set it when the property exists.
        try:
            if not wave.get_editor_property("streaming"):
                wave.set_editor_property("streaming", True)
                changed = True
        except Exception:
            pass
    want_set = {concurrency} if concurrency else set()
    if set(wave.get_editor_property("concurrency_set")) != want_set:
        wave.set_editor_property("concurrency_set", want_set)
        changed = True
    if created:
        scrub_source_path(wave, item["source"])
    if changed:
        eal.save_asset(path, only_if_is_dirty=False)
        if created:
            imported += 1
        else:
            reconfigured += 1
        log("%s %s (%.2fs, %dch, %s, %s)" % ("imported" if created else "reconfigured", path, wave.get_editor_property("duration"),
                                           wave.get_editor_property("num_channels"), item["sound_class"], item["concurrency"] or "no concurrency"))
    else:
        existing += 1

log("summary: imported %d, reconfigured %d, unchanged %d, planned %d" % (imported, reconfigured, existing, len(plan["items"])))

print("DWCONTENT-BEGIN")
for line in LOG:
    print("DWCONTENT " + line)
print("DWCONTENT " + ("CONTENT PASS" if not ERRORS else "CONTENT FAIL"))
print("DWCONTENT-END")
