import json
import os

import unreal

# Retargets the Mixamo "Entering Car" / "Exiting Car" clips onto the UE5
# Mannequin (SKM_Quinn_Simple). Run from the project root with the editor
# either closed or open (new assets are picked up by the asset registry):
# UnrealEditor-Cmd.exe GrandCityMobile.uproject -run=pythonscript
#   -script=Tools/import_vehicle_animations.py -unattended -nop4 -nosplash
#   -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities
#
# Each Mixamo FBX was imported with its own skeleton, so every clip gets its
# own source IK Rig + retargeter. Mixamo exports a T-pose; the retarget pose
# auto-aligns the A-posed Mannequin to it.

MIXAMO_ROOT = "/Game/Mixamo"
RIG_ROOT = "/Game/Characters/Mannequins/Rigs/Mixamo"
ANIM_ROOT = "/Game/Characters/Mannequins/Anims/Vehicle"
TARGET_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"
TARGET_RIG = "/Game/Characters/Mannequins/Rigs/IK_Mannequin"
REPORT_PATH = os.path.join(
    unreal.Paths.project_saved_dir(), "VehicleAnimationReport.json")

CLIPS = (
    # (source mesh, source anim, output name)
    ("Entering_Car", "Entering_Car_Anim", "MM_Car_Enter"),
    ("Exiting_Car", "Exiting_Car_Anim", "MM_Car_Exit"),
)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
editor_assets = unreal.EditorAssetLibrary


def load(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError(f"Could not load {path}")
    return asset


def recreate(name, folder, asset_class, factory):
    path = f"{folder}/{name}"
    if editor_assets.does_asset_exist(path):
        editor_assets.delete_asset(path)
    asset = asset_tools.create_asset(name, folder, asset_class, factory)
    if not asset:
        raise RuntimeError(f"Could not create {path}")
    return asset


def sample_bone(anim, bones, time):
    # Mixamo bone names vary between exports ("mixamorig:Hips", "mixamorig1:Hips").
    for bone in (bones if isinstance(bones, tuple) else (bones,)):
        try:
            xform = unreal.AnimationLibrary.get_bone_pose_for_time(anim, bone, time, False)
            break
        except Exception:
            xform = None
    if xform is None:
        return None
    loc = xform.translation
    rot = xform.rotation.rotator()
    return {"t": round(time, 3), "loc": [round(loc.x, 1), round(loc.y, 1), round(loc.z, 1)],
            "rot": [round(rot.pitch, 1), round(rot.yaw, 1), round(rot.roll, 1)]}


target_mesh = load(TARGET_MESH)
target_rig = load(TARGET_RIG)
report = {}

for mesh_name, anim_name, output_name in CLIPS:
    source_mesh = load(f"{MIXAMO_ROOT}/{mesh_name}")
    source_anim = load(f"{MIXAMO_ROOT}/{anim_name}")

    rig = recreate(f"IK_Mixamo_{mesh_name}", RIG_ROOT,
                   unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    rig_controller = unreal.IKRigController.get_controller(rig)
    if not rig_controller.set_skeletal_mesh(source_mesh):
        raise RuntimeError(f"{mesh_name} is not usable as an IK Rig mesh")
    if not rig_controller.apply_auto_generated_retarget_definition():
        raise RuntimeError(f"{mesh_name} was not recognized as a Mixamo skeleton")

    retargeter = recreate(f"RTG_Mixamo_{mesh_name}", RIG_ROOT,
                          unreal.IKRetargeter, unreal.IKRetargetFactory())
    rtg = unreal.IKRetargeterController.get_controller(retargeter)
    rtg.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, rig)
    rtg.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    rtg.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, source_mesh)
    rtg.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, target_mesh)
    rtg.add_default_ops()
    rtg.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, rig)
    rtg.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    rtg.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    rtg.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)

    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = [unreal.AssetRegistryHelpers.create_asset_data(source_anim)]
    inputs.source_mesh = source_mesh
    inputs.target_mesh = target_mesh
    inputs.ik_retarget_asset = retargeter
    inputs.search = anim_name
    inputs.replace = output_name
    inputs.target_path = ANIM_ROOT
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = True
    results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    if not results:
        raise RuntimeError(f"Retarget produced no assets for {anim_name}")

    output = load(f"{ANIM_ROOT}/{output_name}")
    length = output.get_play_length()
    samples = [length * i / 8.0 for i in range(9)]
    report[output_name] = {
        "length": round(length, 3),
        "source_hips": [sample_bone(source_anim, ("mixamorig:Hips", "mixamorig1:Hips", "Hips"), t) for t in samples],
        "pelvis": [sample_bone(output, "pelvis", t) for t in samples],
        "root": [sample_bone(output, "root", t) for t in samples],
    }

    editor_assets.save_directory(RIG_ROOT, only_if_is_dirty=True, recursive=True)
    editor_assets.save_directory(ANIM_ROOT, only_if_is_dirty=True, recursive=True)

with open(REPORT_PATH, "w") as report_file:
    json.dump(report, report_file, indent=1)
unreal.log(f"Vehicle animation retarget done. Report: {REPORT_PATH}")
