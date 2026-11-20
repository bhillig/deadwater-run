# Copyright Brandon Hillig 2026
# Run by Tools/CreateAudioContent.ps1 inside UnrealEditor-Cmd (DW-113, the parts that do not need DW-114).
# Creates or updates, idempotently: sound classes SC_Master/Music/SFX/UI, sound mixes SM_Base and SM_Pause,
# and concurrency assets SCON_Hits/Cannon/UI under /Game/DeadwaterRun/Audio/.
# Prints its log between markers the PowerShell wrapper extracts. Assets are saved only when something changed.
import unreal

BASE = "/Game/DeadwaterRun/Audio"
LOG = []
ERRORS = []
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    LOG.append(msg)


def fail(msg):
    ERRORS.append(msg)
    LOG.append("ERROR " + msg)


def get_or_create(folder, name, asset_class, factory):
    path = "%s/%s" % (folder, name)
    if eal.does_asset_exist(path):
        return eal.load_asset(path), False
    asset = tools.create_asset(name, folder, asset_class, factory)
    if asset is None:
        fail("could not create %s" % path)
    return asset, True


def same(a, b):
    """Float-safe equality (UE stores float32, so 0.6 reads back as 0.6000000238)."""
    if isinstance(a, float) or isinstance(b, float):
        return abs(float(a) - float(b)) < 1e-4
    return a == b


def set_if_changed(asset, prop, value):
    """Sets an editor property; returns True when the value actually changed."""
    try:
        current = asset.get_editor_property(prop)
    except Exception as exc:  # property missing in this engine version
        fail("%s has no property %s (%s)" % (asset.get_name(), prop, exc))
        return False
    if same(current, value):
        return False
    asset.set_editor_property(prop, value)
    return True


def finish(asset, created, changed):
    path = asset.get_path_name().split(".")[0]
    if created or changed:
        eal.save_asset(path, only_if_is_dirty=False)
        log("%s %s" % ("created" if created else "updated", path))
    else:
        log("unchanged %s" % path)


# --- Sound classes ---------------------------------------------------------------------------------------------
# Placeholder balance until the DW-403 mix pass: music sits under the effects, UI a touch under SFX.
# Settings sliders set the SM_Base adjusters (UGameplayStatics::SetSoundMixClassOverride) on top of these.
# is_ui_sound keeps a class playing while the world is paused: UI yes (menus), Music yes (it ducks via SM_Pause
# instead of stopping), SFX no (SFX keep their state under pause, 15-Audio-VFX.md).
CLASSES = [
    # name, parent, volume, is_music, is_ui_sound
    ("SC_Master", None, 1.0, False, False),
    ("SC_Music", "SC_Master", 0.6, True, True),
    ("SC_SFX", "SC_Master", 1.0, False, False),
    ("SC_UI", "SC_Master", 0.9, False, True),
]

class_assets = {}
for name, parent, volume, is_music, is_ui in CLASSES:
    asset, created = get_or_create(BASE + "/Classes", name, unreal.SoundClass, unreal.SoundClassFactory())
    if asset is None:
        continue
    class_assets[name] = asset
    changed = False
    props = asset.get_editor_property("properties")
    for prop, value in (("volume", volume), ("is_music", is_music), ("is_ui_sound", is_ui)):
        if not same(props.get_editor_property(prop), value):
            props.set_editor_property(prop, value)
            changed = True
    if changed:
        asset.set_editor_property("properties", props)
    if parent is not None:
        parent_asset = class_assets.get(parent)
        if parent_asset is None:
            fail("%s: parent %s missing" % (name, parent))
        elif asset.get_editor_property("parent_class") != parent_asset:
            asset.set_editor_property("parent_class", parent_asset)
            changed = True
    finish(asset, created, changed)

# --- Mixes -----------------------------------------------------------------------------------------------------
def make_adjuster(sound_class, volume, apply_to_children):
    adj = unreal.SoundClassAdjuster()
    adj.set_editor_property("sound_class_object", sound_class)
    adj.set_editor_property("volume_adjuster", volume)
    adj.set_editor_property("pitch_adjuster", 1.0)
    # The class hierarchy already multiplies a parent's volume into its children, so adjusters do not cascade.
    adj.set_editor_property("apply_to_children", apply_to_children)
    return adj


def adjuster_signature(adjusters):
    return [(a.get_editor_property("sound_class_object"), round(a.get_editor_property("volume_adjuster"), 4),
             a.get_editor_property("apply_to_children")) for a in adjusters]


def build_mix(name, adjuster_specs, fade_in=None, fade_out=None):
    mix, created = get_or_create(BASE + "/Mix", name, unreal.SoundMix, unreal.SoundMixFactory())
    if mix is None:
        return
    wanted = []
    for class_name, volume in adjuster_specs:
        cls = class_assets.get(class_name)
        if cls is None:
            fail("%s: class %s missing" % (name, class_name))
            return
        wanted.append(make_adjuster(cls, volume, False))
    changed = adjuster_signature(mix.get_editor_property("sound_class_effects")) != adjuster_signature(wanted)
    if changed:
        mix.set_editor_property("sound_class_effects", wanted)
    for prop, value in (("fade_in_time", fade_in), ("fade_out_time", fade_out)):
        if value is not None:
            changed |= set_if_changed(mix, prop, value)
    finish(mix, created, changed)


# SM_Base: one adjuster per volume setting at unity; the settings screen overrides these at runtime.
# The mix must be pushed once at startup (PushSoundMixModifier) for the overrides to take effect (DW-114/DW-216).
build_mix("SM_Base", [("SC_Master", 1.0), ("SC_Music", 1.0), ("SC_SFX", 1.0), ("SC_UI", 1.0)])
# SM_Pause: pushed while the pause menu is open; ducks music only (15-Audio-VFX.md "Settings and mix").
build_mix("SM_Pause", [("SC_Music", 0.35)], fade_in=0.25, fade_out=0.4)

# --- Concurrency -----------------------------------------------------------------------------------------------
# Placeholder numbers sized from the catalogue; tuned in DW-403 by ear:
#  Hits:   Six Pistols (6) and Arms of Fury (3-5) land in one burst, plus deaths and Kraken slams.
#  Cannon: a broadside is a few shots; the 1-1.6 s tails must not pile up into a wall.
#  UI:     hover spam while sweeping a list.
CONCURRENCY = [
    # name, max_count, rule, retrigger_time, steal_release
    ("SCON_Hits", 8, unreal.MaxConcurrentResolutionRule.STOP_OLDEST, 0.03, 0.08),
    ("SCON_Cannon", 4, unreal.MaxConcurrentResolutionRule.STOP_OLDEST, 0.05, 0.15),
    ("SCON_UI", 3, unreal.MaxConcurrentResolutionRule.STOP_OLDEST, 0.02, 0.03),
]
for name, max_count, rule, retrigger, steal in CONCURRENCY:
    asset, created = get_or_create(BASE + "/Concurrency", name, unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
    if asset is None:
        continue
    settings = asset.get_editor_property("concurrency")
    changed = False
    for prop, value in (("max_count", max_count), ("resolution_rule", rule), ("retrigger_time", retrigger),
                        ("voice_steal_release_time", steal), ("limit_to_owner", False)):
        if not same(settings.get_editor_property(prop), value):
            settings.set_editor_property(prop, value)
            changed = True
    if changed:
        asset.set_editor_property("concurrency", settings)
    finish(asset, created, changed)

# --- Verify the hierarchy from the saved state ----------------------------------------------------------------
for name, parent, _v, _m, _u in CLASSES:
    cls = eal.load_asset(BASE + "/Classes/" + name)
    actual = cls.get_editor_property("parent_class") if cls else None
    actual_name = actual.get_name() if actual else None
    if actual_name != parent:
        fail("%s parent is %s, expected %s" % (name, actual_name, parent))
log("class hierarchy ok: Master > Music, SFX, UI")

print("DWCONTENT-BEGIN")
for line in LOG:
    print("DWCONTENT " + line)
print("DWCONTENT " + ("CONTENT PASS" if not ERRORS else "CONTENT FAIL"))
print("DWCONTENT-END")
