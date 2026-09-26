"""Civilian population tests (PED-T01..T04), run inside the editor through Tools/uepy.py during PIE.

Written without access to the project: check it against Tools/policetest.py conventions before first use and keep
whatever that file does for PIE control and the world lookup.

A Python loop would block the game thread, so measurements are split across uepy calls (PROJECT_OVERVIEW §3):

    python Tools/uepy.py -c "import civtest; civtest.stats()"
    python Tools/uepy.py -c "import civtest; civtest.watch_start()"      # registers a post-tick sampler
    (drive / walk / swing the camera for N minutes)
    python Tools/uepy.py -c "import civtest; civtest.watch_report()"     # unregisters, prints PED-T02..T04

(ADAPT: how policetest.py imports helpers from Tools/ inside the editor — sys.path append or exec of the file.)

The visible-pop check is independent of the subsystem's own visibility test: it uses the renderer.
AActor.was_recently_rendered() is true when any primitive of the actor was drawn in the last frames (occlusion
culling included). A pop = a civilian that is rendered in its first frames after appearing, or was rendered just
before disappearing. Caveat: dynamic shadows count as rendered — if that produces false positives, filter by
distance to the camera frustum instead and note it in the report.
"""

import builtins
import unreal

RENDER_TOLERANCE_S = 0.1  # "recently" window for was_recently_rendered: ~6 frames at 60 FPS


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def _pop():
    world = _world()
    if world is None:
        raise RuntimeError("not in PIE")
    return unreal.SubsystemBlueprintLibrary.get_world_subsystem(world, unreal.CivilianPopulationSubsystem)


def stats():
    s = _pop().get_stats()
    print(f"active {s.active}/{s.target} pinned {s.pinned} pooled {s.pooled} | new {s.spawned_new} "
          f"reuse {s.reused_from_pool} despawn {s.despawned} | rejected vis {s.rejected_visible} "
          f"nav {s.rejected_nav} crowd {s.rejected_crowded} fail {s.spawn_failed}")
    return s


def watch_start():
    pop = _pop()
    st = {
        "prev": {},            # actor name -> was rendered last tick
        "age": {},             # actor name -> ticks since it appeared
        "pops_in": [],         # (time, name)
        "pops_out": [],
        "max_active": 0,
        "target_hits": 0,
        "ticks": 0,
        "t0": unreal.GameplayStatics.get_time_seconds(_world()),
        "first": pop.get_stats(),
    }

    def tick(dt):
        world = _world()
        if world is None:
            return
        now = unreal.GameplayStatics.get_time_seconds(world)
        civs = _pop().get_active_civilians()
        cur = {}
        for p in civs:
            name = p.get_name()
            rendered = p.was_recently_rendered(RENDER_TOLERANCE_S)
            cur[name] = rendered
            age = st["age"].get(name, 0)
            if name not in st["prev"]:
                age = 0
            # appeared this tick or the next two, and already on screen -> pop in
            if age <= 2 and rendered and name not in [n for _, n in st["pops_in"]]:
                st["pops_in"].append((now, name))
            st["age"][name] = age + 1
        for name, was_rendered in st["prev"].items():
            if name not in cur and was_rendered:
                st["pops_out"].append((now, name))
        st["prev"] = cur
        s = _pop().get_stats()
        st["max_active"] = max(st["max_active"], s.active)
        st["ticks"] += 1

    builtins.CIVTEST = st
    builtins.CIVTEST_HANDLE = unreal.register_slate_post_tick_callback(tick)
    print("civtest: watching")


def watch_report():
    unreal.unregister_slate_post_tick_callback(builtins.CIVTEST_HANDLE)
    st = builtins.CIVTEST
    first, last = st["first"], _pop().get_stats()
    dur = unreal.GameplayStatics.get_time_seconds(_world()) - st["t0"]
    cap = unreal.get_default_object(unreal.CivilianPopulationSettings).get_editor_property("max_civilians")
    new_after_fill = last.spawned_new - first.spawned_new
    reused = last.reused_from_pool - first.reused_from_pool

    print(f"duration {dur:.0f} s, {st['ticks']} ticks")
    print(f"PED-T02 visible pops: in {len(st['pops_in'])}, out {len(st['pops_out'])} -> "
          f"{'PASS' if not st['pops_in'] and not st['pops_out'] else 'FAIL'}")
    for t, n in (st["pops_in"] + st["pops_out"])[:10]:
        print(f"   {t:.1f} s {n}")
    print(f"PED-T03 budget: max active {st['max_active']} / cap {cap} -> {'PASS' if st['max_active'] <= cap else 'FAIL'}")
    print(f"PED-T04 pool: new bodies {new_after_fill}, reused {reused} during the watch "
          f"(PASS if reuse dominates after the first fill)")
    stats()
