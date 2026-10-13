"""Conservative OBSERVED-context gate; it never authorizes a physics write.

The positive single-player discriminator comes from this build's own UI:
useMainState/getIsSingleplayer -> navigation.state.settingMode == SETTING_GRANDPRIX.
This gate does not supply the missing simulation-thread revalidation or checkpoint.
"""

import math


def result(decision, reason, singleplayer=False):
    return {
        "context_decision": decision,
        "reason": reason,
        "singleplayer_evidence": singleplayer,
        "reset_permitted": False,
    }


def assess(before, after, game, telemetry, now_ms, started_ms, *, observation_build=14130):
    # The old physics tools keep their default 14130 gate. 14150/14200 are available
    # only to the separately verified, read-only telemetry wrapper.
    if type(observation_build) is not int or observation_build not in (14130, 14150, 14200):
        return result("unknown", "unsupported_observation_profile")
    if not all(isinstance(x, dict) for x in (before, after, game, telemetry)):
        return result("unknown", "missing_or_malformed_source")
    if not all(type(x) in (int, float) and math.isfinite(x) for x in (now_ms, started_ms)):
        return result("unknown", "invalid_clock")
    if not 0 <= now_ms - started_ms <= 500:
        return result("unknown", "rest_collection_too_slow")
    tick = telemetry.get("host_tick_ms")
    if type(tick) is not int or not 0 <= now_ms - tick <= 350:
        return result("unknown", "stale_native_sample")
    keys = ("appBuild", "settingMode", "gameSession", "gameState", "navigationState", "gamePhase")
    if any(k not in before or k not in after for k in keys):
        return result("unknown", "missing_navigation_fields")
    if any(before[k] != after[k] for k in keys):
        return result("unknown", "context_changed_during_collection")
    for name in (
        "game_version",
        "session",
        "game_mode",
        "phase",
        "control",
        "vehicles",
        "active_vehicles",
    ):
        if type(telemetry.get(name)) is not int:
            return result("unknown", "invalid_native_integer_field")
    if (
        type(after["appBuild"]) is not int
        or after["appBuild"] != observation_build
        or telemetry.get("game_version") != observation_build
    ):
        return result("unknown", "unsupported_build")
    mode = after["settingMode"]
    if mode in ("SETTING_MULTIPLAYER", "SETTING_REPLAY_PLAYBACK"):
        return result("forbidden", "multiplayer_or_replay")
    if mode != "SETTING_GRANDPRIX":
        return result("unknown", "unrecognized_setting_mode")
    session = after["gameSession"]
    if session not in ("PRACTICE1", "PRACTICE2", "PRACTICE3", "PRACTICE4"):
        known = (
            "TESTDAY",
            "TEST_DAY",
            "WARMUP",
            "RACE1",
            "RACE2",
            "RACE3",
            "RACE4",
            "QUALIFY1",
            "QUALIFY2",
            "QUALIFY3",
            "QUALIFY4",
            "QUALIFICATION",
        )
        return result("forbidden" if session in known else "unknown", "non_practice_session", True)
    if type(telemetry.get("session")) is not int or telemetry["session"] != int(session[-1]):
        return result("unknown", "session_sources_disagree", True)
    if telemetry.get("game_mode") != 0:
        return result("forbidden", "network_scoring_conflicts_with_singleplayer", True)
    if after["gameState"] != "GSTATE_DYN" or after["navigationState"] not in (
        "NAV_EVENT",
        "NAV_REALTIME",
    ):
        return result("unknown", "not_in_active_event", True)
    for name in ("bounds_ok", "fresh_observed", "player_found", "telemetry_matched"):
        if telemetry.get(name) is not True:
            return result("unknown", "native_sample_incomplete_or_not_advancing", True)
    for name in ("inMonitor", "isReplayActive", "inControlOfVehicle", "playerVehicleLoaded"):
        if type(game.get(name)) is not bool:
            return result("unknown", "missing_game_state_flags", True)
    if game["isReplayActive"]:
        return result("forbidden", "replay_active", True)
    if game["inMonitor"] or telemetry.get("in_realtime") is not True:
        return result("forbidden", "monitor_or_not_realtime", True)
    if after["navigationState"] != "NAV_REALTIME":
        return result("unknown", "driving_navigation_not_confirmed", True)
    if game.get("MultiStintState") != "DRIVING":
        return result("unknown", "not_verified_driving_state", True)
    if after["gamePhase"] != "GREEN" or telemetry.get("phase") != 5:
        return result("forbidden", "not_green_driving_phase", True)
    if (
        not game["inControlOfVehicle"]
        or not game["playerVehicleLoaded"]
        or telemetry.get("control") != 0
    ):
        return result("forbidden", "player_not_in_control", True)
    if telemetry.get("vehicles") != 1 or telemetry.get("active_vehicles") != 1:
        return result("forbidden", "initial_scope_requires_single_vehicle", True)
    return result("allowed", "observed_singleplayer_practice_only", True)


def assess_player_observation(
    before, after, game, telemetry, now_ms, started_ms, *, observation_build=14200
):
    """Read-only player observer may coexist with AI in offline Practice.

    All original source/freshness/control checks still apply. This separate
    entry point is not used by any physical writer and never permits reset.
    """
    decision = assess(
        before, after, game, telemetry, now_ms, started_ms, observation_build=observation_build
    )
    if decision["reason"] == "initial_scope_requires_single_vehicle":
        if 1 <= telemetry["active_vehicles"] <= telemetry["vehicles"] <= 128:
            return result("allowed", "observed_offline_player_only", True)
    return decision
