import unreal

# Run from the project root with:
# UnrealEditor-Cmd.exe GrandCityMobile.uproject -run=pythonscript
#   -script=Tools/import_crouch_assets.py -unattended -nop4 -nosplash
#   -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities,MoverExamples

SOURCE_ROOT = "/MoverExamples/Characters/Mannequins/Animations/Manny"
SOURCE_NAMES = (
    "MM_Unarmed_Crouch_Entry",
    "MM_Unarmed_Crouch_Exit",
    "MM_Unarmed_Crouch_Idle",
    "MM_Unarmed_Crouch_Walk_Fwd",
    "MM_Unarmed_Crouch_Walk_Bwd",
    "MM_Unarmed_Crouch_Walk_Left",
    "MM_Unarmed_Crouch_Walk_Right",
)
TARGET_ROOT = "/Game/Characters/Mannequins/Anims/CrouchFixed"


source_mesh = unreal.load_asset(
    "/MoverExamples/Characters/Mannequins/Meshes/SKM_Manny_Simple"
)
target_mesh = unreal.load_asset(
    "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"
)
retargeter = unreal.load_asset(
    "/MoverExamples/Characters/Mannequins/Rigs/RTG_Mannequin"
)

# Epic's retargeter resolves its target rig from
# /Game/Characters/Mannequins/Rigs/IK_Mannequin. That project asset must stay
# beside the generated clips; without it the batch operation silently writes
# reference-pose tracks, so keep the copied rig at that exact project path.

if not source_mesh or not target_mesh or not retargeter:
    raise RuntimeError(
        "Could not load the source mesh, Quinn target mesh, or Manny IK retargeter."
    )

asset_data = []
asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
asset_registry.scan_paths_synchronous(
    [SOURCE_ROOT],
    force_rescan=True,
    ignore_deny_list_scan_filters=True,
)
for source_name in SOURCE_NAMES:
    source_path = f"{SOURCE_ROOT}/{source_name}"
    source_asset = unreal.load_asset(source_path)
    if not source_asset:
        raise RuntimeError(f"Could not load crouch source animation: {source_path}")
    # UE 5.8 exposes the legacy Name overload to Python; using the complete
    # object path is required even though the C++ overload is deprecated.
    data = asset_registry.get_asset_by_object_path(
        unreal.SoftObjectPath(source_asset.get_path_name())
    )
    if not data or not data.is_valid():
        raise RuntimeError(f"Missing crouch source animation: {source_path}")
    asset_data.append(data)

inputs = unreal.IKRetargetBatchOperationInputs()
inputs.set_editor_property("assets_to_retarget", asset_data)
inputs.set_editor_property("source_mesh", source_mesh)
inputs.set_editor_property("target_mesh", target_mesh)
inputs.set_editor_property("ik_retarget_asset", retargeter)
inputs.set_editor_property("target_path", TARGET_ROOT)
inputs.set_editor_property("use_source_path", False)
inputs.set_editor_property("include_referenced_assets", False)
inputs.set_editor_property("overwrite_existing_files", False)
inputs.set_editor_property("retain_additive_flags", True)

results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
if len(results) != len(SOURCE_NAMES):
    raise RuntimeError(
        f"Expected {len(SOURCE_NAMES)} retargeted animations, got {len(results)}."
    )

for result in results:
    asset = result.get_asset()
    if not asset:
        raise RuntimeError(f"Retarget output could not be loaded: {result.package_name}")

    # CharacterMovement drives the capsule. Keep every crouch clip in-place so
    # the animated root cannot move the mesh away from the collision capsule or
    # add a second movement source on top of CharacterMovement.
    asset.set_editor_property("enable_root_motion", False)
    asset.set_editor_property("force_root_lock", True)

    data_model = asset.get_editor_property("data_model_interface")
    if not data_model:
        raise RuntimeError(f"Retarget output has no animation data: {asset.get_path_name()}")

    middle_frame = max(0, data_model.get_number_of_frames() // 2)
    pose_deltas = []
    for evaluation_type, evaluation_name in (
        (unreal.AnimDataEvalType.RAW, "raw"),
        (unreal.AnimDataEvalType.COMPRESSED, "compressed"),
    ):
        options = unreal.AnimPoseEvaluationOptions()
        options.set_editor_property("evaluation_type", evaluation_type)
        options.set_editor_property("optional_skeletal_mesh", target_mesh)
        pose = asset.get_anim_pose_at_frame(middle_frame, options)
        pelvis = pose.get_relative_to_ref_pose_transform(
            "pelvis", unreal.AnimPoseSpaces.LOCAL
        )
        thigh = pose.get_relative_to_ref_pose_transform(
            "thigh_l", unreal.AnimPoseSpaces.LOCAL
        )
        delta = (
            abs(pelvis.translation.x)
            + abs(pelvis.translation.y)
            + abs(pelvis.translation.z)
            + abs(thigh.rotation.x)
            + abs(thigh.rotation.y)
            + abs(thigh.rotation.z)
        )
        pose_deltas.append(delta)
        unreal.log_warning(
            f"CROUCH_POSE output={asset.get_path_name()} "
            f"type={evaluation_name} frame={middle_frame} delta={delta:.6f}"
        )

    if min(pose_deltas) < 0.05:
        raise RuntimeError(
            f"Retarget output is still a reference pose: {asset.get_path_name()}"
        )

    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    skeleton = asset.get_editor_property("skeleton")
    if asset.get_editor_property("enable_root_motion"):
        raise RuntimeError(f"Root motion is still enabled on {asset.get_path_name()}")
    if not asset.get_editor_property("force_root_lock"):
        raise RuntimeError(f"Root lock is disabled on {asset.get_path_name()}")
    unreal.log_warning(
        "CROUCH_IMPORT "
        f"output={asset.get_path_name()} "
        f"skeleton={skeleton.get_path_name()} "
        "root_motion=False root_lock=True"
    )

unreal.log_warning("CROUCH_IMPORT_DONE")
