# Super+E Wayland Debug Plan

Goal: stop Super+E from closing or blanking the Wayland app window; ensure unfocus only clears focus and rendering persists.

1) Reproduce + Baseline
- Run `tests/controls_spec --gtest_filter=*SuperE*` and `--gtest_filter=*SuperHotkeysCycleWaylandApps*` to confirm current behavior and log outputs.
- Manual repro via `./launch --in-wm` (wlroots) and hit Super+E while a Wayland client is focused; note if surface destroys or just loses focus/visibility.
- Before each run, truncate `/tmp/matrix-wlroots-output.log` and `/tmp/matrix-wlroots-wm.log` for clean markers.

2) Trace Input Path
- `src/wayland/wlr_compositor.cpp`: instrument `process_key_sym` around Super combos (`replaySuperActive`, `mods`, `waylandFocusActive`, seat focus clears) and verify we only unfocus, not destroy.
- `src/WindowManager/WindowManager.cpp`: confirm `handleHotkeySym` and `unfocusApp` behavior in `waylandMode`; ensure we don’t hit X11 close paths or destroy entities.
- Verify replay logic (`pendingReplaySuper*`) isn’t dropping modifier state and firing extra keyups.

3) Validate Surface/Entity Lifetime
- Track Wayland surface map (`surface_map`, `pending_wl_actions`) and `WaylandApp::handle_destroy` to see if Super+E triggers unmap/destroy.
- Watch compositor logs for `mapped`/`destroy` around Super+E to detect unintended teardown.
- Use `ENABLE_RENDER_TMP_LOGS` and `WLROOTS_DEBUG_LOGS` to capture texture IDs, sizes, and directRender toggles when focused/unfocused.

4) Rendering/Shader Checks
- In `src/renderer.cpp`, confirm Wayland branch continues to draw in-world quad when unfocused and only disables directRender path. Log `appNumber`, texture ID/unit, size, and model matrix after Super+E.
- If black, temporarily visualize UVs or solid color in `shaders/fragment.glsl` for Wayland apps to isolate texture vs. pipeline issues.
- Ensure vertex shader `directRender` path is only active for focused app; check model/bootableScale values after unfocus.

5) Engine/Controls Flow
- Confirm `Engine` tick keeps camera and entity placement alive in Wayland mode after unfocus. Use API status (`current_status()`) to compare `wayland_apps` and camera position before/after Super+E.
- Verify `wlr_seat_keyboard_notify_clear_focus`/`pointer_notify_clear_focus` don’t block re-focus or drop rendering.

6) Fix + Regression Pass
- Implement minimal fix once root cause is found (input handling, seat focus, renderer state, or shader sampling).
- Re-run `tests/controls_spec --gtest_filter=*SuperE*`, `tests/controls_spec --gtest_filter=*SuperHotkeysCycleWaylandApps*`, and `tests/wayland_basic --all` (if time) to guard Wayland path.
