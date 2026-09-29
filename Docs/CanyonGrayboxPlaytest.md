# Canyon graybox playtest

This branch runs an asset-free Canyon map inside Unreal. The lower paths, steep canyon walls, ramps, and upper wall-top paths are generated from a route graph. Some lower passages narrow to roughly three character widths. Stone arch, pillars, split peak, broken bridge, needle, and stone ring are temporary geometric placeholders. The independent Blender landmark work is not required for this test.

## Start

- Double-click `Scripts/PlayCanyonGraybox.cmd` for a random Canyon seed.
- To replay an exact map, run `Scripts/PlayCanyonGraybox.cmd -IslandSeed=1000` from a terminal, or enter the seed beside **单人测试 → 测试峡谷路线灰盒** in the game's menu.
- In the map, movement is **5×** speed. Press **V** to toggle flying; while flying, **Space** climbs and **Left Ctrl** descends. Press **V** again to walk. Press **R** for a new random seed. The displayed seed identifies the map you just tried. **Esc** opens the normal pause menu.
- The standard solo map test shows the treasure marker so the route can be inspected. The `-CanyonGrayboxPreview` launch flag starts directly in this mode.

## What to judge

1. At the first decision, do the routes create an interesting choice or only extra walking?
2. Can you understand where each route leads using the landmark placeholders and terrain?
3. Does the treasure location make you remember or draw a distinct relationship?
4. Are the narrow passages comfortable for three players? Do the ramp and upper routes offer a useful choice?
5. Are any routes blocked, too steep, too dark, or easy to bypass over a wall?

Please note the **seed** and describe the junction or landmark when giving feedback. Geometry, lighting, and clue variety are still graybox quality; this test is for navigation, not final art.
