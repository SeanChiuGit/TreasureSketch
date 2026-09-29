# Canyon graybox playtest

This branch runs an asset-free Canyon map inside Unreal. The floor and steep canyon walls are generated from a route graph. Stone arch, pillars, split peak, broken bridge, needle, and stone ring are temporary geometric placeholders. The independent Blender landmark work is not required for this test.

## Start

- Double-click `Scripts/PlayCanyonGraybox.cmd` for a random Canyon seed.
- To replay an exact map, run `Scripts/PlayCanyonGraybox.cmd -IslandSeed=1000` from a terminal, or enter the seed beside **单人测试 → 测试峡谷路线灰盒** in the game's menu.
- In the map, press **R** for a new random seed. The displayed seed identifies the map you just tried. **Esc** opens the normal pause menu.
- The standard solo map test shows the treasure marker so the route can be inspected. The `-CanyonGrayboxPreview` launch flag starts directly in this mode.

## What to judge

1. At the first decision, do the routes create an interesting choice or only extra walking?
2. Can you understand where each route leads using the landmark placeholders and terrain?
3. Does the treasure location make you remember or draw a distinct relationship?
4. Are any routes blocked, too steep, too dark, or easy to bypass over a wall?

Please note the **seed** and describe the junction or landmark when giving feedback. Geometry, lighting, and clue variety are still graybox quality; this test is for navigation, not final art.
