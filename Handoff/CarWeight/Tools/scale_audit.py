"""
scale_audit.py — is the car small, or is the world big? Run inside the Unreal editor (Python), through MCP or the
Output Log:   py "Handoff/CarWeight/Tools/scale_audit.py"            (report only)
              py "Handoff/CarWeight/Tools/scale_audit.py" --camera    (also write the camera targets into every
                                                                        VehicleDefinition asset; saves only those)

What it measures:
  1. every actor in the open level whose class name contains "Vehicle" (the cars): bounds vs a Dacia 1300;
  2. the player character class (a person, 178 cm) if one is placed or selected;
  3. lane width under each car (UMurdarRoadTools.nearest_lane_point -> OutWidth) vs 340 cm;
  4. anything SELECTED in the level: prints its size so you can compare a bloc, a door, a kerb by hand;
  5. every VehicleDefinition asset: FOV at rest / at speed, arm length, pivot, pitch vs the targets.
Never saves the level. With --camera it saves only the VehicleDefinition assets it changed (§Assets rule).
ADAPT: property names are the C++ ones in snake_case (fov_at_rest ...); if the Python API differs, print dir(asset).
"""
import sys
import unreal

REF = {
    "car_length": 435.0, "car_width": 165.0, "car_height": 144.0,
    "person": 178.0, "lane": 340.0,
}
CAMERA_TARGET = {
    "fov_at_rest": 70.0, "fov_at_speed": 80.0,
    "camera_distance": 480.0, "camera_distance_at_speed": 560.0,
    "camera_pitch": -7.0,
}
TOL = 0.12


def verdict(measured, expected):
    if measured <= 0:
        return "?"
    f = expected / measured
    if measured < expected * (1 - TOL):
        return "TOO SMALL (x%.2f)" % f
    if measured > expected * (1 + TOL):
        return "TOO BIG (x%.2f)" % f
    return "ok"


def actors():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return sub.get_all_level_actors(), sub.get_selected_level_actors()


def size_of(actor):
    origin, extent = actor.get_actor_bounds(False)
    # Bounds are world-axis aligned: for a car, rotate-proof by taking the longer horizontal side as length.
    x, y, z = extent.x * 2, extent.y * 2, extent.z * 2
    return max(x, y), min(x, y), z, origin


def report():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    all_actors, selected = actors()
    lines = []
    cars = [a for a in all_actors if "Vehicle" in a.get_class().get_name() and "Definition" not in a.get_class().get_name()]
    lines.append("== Cars (%d) ==" % len(cars))
    lane_widths = []
    for car in cars[:10]:
        length, width, height, origin = size_of(car)
        lines.append("%-40s L %6.0f %-18s W %5.0f %-18s H %5.0f %s" % (
            car.get_actor_label()[:40], length, verdict(length, REF["car_length"]),
            width, verdict(width, REF["car_width"]), height, verdict(height, REF["car_height"])))
        try:
            ok, _pos, _dir, lane_w = unreal.MurdarRoadTools.nearest_lane_point(world, origin, 800.0, unreal.Vector(0, 0, 0))
            if ok and lane_w > 0:
                lane_widths.append(lane_w)
                lines.append("    lane under it: %5.0f cm %s" % (lane_w, verdict(lane_w, REF["lane"])))
        except Exception as e:  # ADAPT: the out-params come back as a tuple in this order in most versions
            lines.append("    lane: could not query (%s)" % e)
    people = [a for a in all_actors if "Character" in a.get_class().get_name()]
    if people:
        _l, _w, h, _o = size_of(people[0])
        lines.append("== Person: %s height %.0f cm %s" % (people[0].get_actor_label(), h, verdict(h, REF["person"])))
    if selected:
        lines.append("== Selected ==")
        for a in selected:
            l, w, h, _o = size_of(a)
            lines.append("%-40s %6.0f x %6.0f x %6.0f cm" % (a.get_actor_label()[:40], l, w, h))
        lines.append("   compare: storey 270, door 210, kerb 15, dashed line 300, bloc P+4 ~15 m, P+10 ~30 m")
    # the diagnosis (same logic as MurdarWeight::Diagnose)
    if cars:
        length, _w, _h, _o = size_of(cars[0])
        car_ok = verdict(length, REF["car_length"]) == "ok"
        lane = sum(lane_widths) / len(lane_widths) if lane_widths else 0
        if not car_ok:
            lines.append("DIAGNOSIS: car mesh scale is wrong - fix the car first (x%.2f)" % (REF["car_length"] / length))
        elif lane and lane > REF["lane"] * (1 + TOL):
            lines.append("DIAGNOSIS: roads too wide for the car - narrow the lanes (x%.2f)" % (REF["lane"] / lane))
        else:
            lines.append("DIAGNOSIS: sizes are right - the small look is the camera and the empty surroundings")
    return lines


def camera(apply_changes):
    lines = ["== VehicleDefinition cameras =="]
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    # ADAPT: the class path of UVehicleDefinition in this project
    assets = registry.get_assets_by_class(unreal.TopLevelAssetPath("/Script/Murdar_GameDev", "VehicleDefinition"))
    changed = []
    for data in assets:
        asset = data.get_asset()
        vals = {k: asset.get_editor_property(k) for k in CAMERA_TARGET}
        lines.append("%-30s " % asset.get_name() + "  ".join("%s %.0f" % (k, v) for k, v in vals.items()))
        if apply_changes:
            for k, v in CAMERA_TARGET.items():
                asset.set_editor_property(k, v)
            changed.append(asset)
    if apply_changes and changed:
        for a in changed:
            unreal.EditorAssetLibrary.save_loaded_asset(a, only_if_is_dirty=True)
        lines.append("camera targets written to %d asset(s) (level NOT saved)" % len(changed))
        for a in changed:
            lines.append("  read back %s: fov_at_speed %.0f camera_distance %.0f" % (a.get_name(), a.get_editor_property("fov_at_speed"), a.get_editor_property("camera_distance")))
    return lines


if __name__ == "__main__":
    out = report() + camera("--camera" in sys.argv)
    for line in out:
        unreal.log(line)
