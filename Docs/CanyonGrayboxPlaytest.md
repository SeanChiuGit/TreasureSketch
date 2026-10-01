# Canyon graybox playtest

This branch runs an asset-free Canyon map inside Unreal. The lower paths, steep canyon walls, ramps, and upper wall-top paths are generated from a route graph. Some lower passages narrow to roughly three character widths. Stone arch, pillars, split peak, broken bridge, needle, and stone ring are temporary geometric placeholders. The independent Blender landmark work is not required for this test.

Fixed seed **1000** previews the original shorter two-mouth shortcut. Fixed seed **1001 at 1× area** previews a roughly 150 m winding cave that descends beneath surface paths and climbs back to its exit. Fixed seed **1002 at 1× area** previews three cave mouths converging on one hall. Its passages take roughly 50–100 m to reach the hall, with one longer winding approach. The two new cave layouts are reserved for these fixed previews until review; other settings use the shorter type. All cave voids, floors, walls, and ceilings are generated within one terrain solid. A surface route remains available around the mountain. The rock is still graybox geometry rather than final art.

The upper level now alternates between a branching dead-end lookout, a through route, and a through route with another choice. Surface detours are placed on different edges rather than repeatedly near the first fork, and 1× maps gain an outer fork. Increasing the room's area setting adds more outer branches and crossings: at 2× there is one extra region beyond 1×, rising to four extra at 5×. Compare the same seed at 1× and 5× to judge whether the added travel and decisions feel worthwhile.

At 1× map area the Canyon terrain spans **125.4 × 125.4 m**, matching Beach's square extent; Forest spans **94.05 × 94.05 m**. The playable Canyon routes occupy only part of that square.

## Start

- Double-click `Scripts/PlayCanyonGraybox.cmd` for a random Canyon seed.
- To replay an exact map, run `Scripts/PlayCanyonGraybox.cmd -IslandSeed=1002` from a terminal, or enter the seed beside **单人测试 → 测试峡谷路线灰盒** in the game's menu.
- In the map, movement is **5×** speed. Press **V** to toggle flying; while flying, **Space** climbs and **Left Ctrl** descends. Press **V** again to walk. Press **R** for a new random seed. The displayed seed identifies the map you just tried. **Esc** opens the normal pause menu.
- The standard solo map test shows the treasure marker so the route can be inspected. The `-CanyonGrayboxPreview` launch flag starts directly in this mode.

## Play in a normal round

- Open `TreasureSketch.uproject` and start the game normally. In the room lobby, Canyon is enabled in the map pool by default alongside Beach and Forest. The host can leave only **峡谷** selected to test it every round.
- Canyon is available in **合作寻宝** (one mapmaker), **多地图师** (one explorer), and **探索者对抗**. The room's 0.5–5 area setting scales Canyon as well.
- Mapmakers can press **B** once during the ground-view drawing phase to take a clue photo. Open the sketch with **M** and click **查看照片** to inspect it. The photo stays with that mapmaker's sketch for the explorer and round review.
- The 5× movement speed, flight toggle, visible treasure marker, and **R** map refresh belong to the solo Canyon graybox test. Normal rounds use their usual movement and round rules.

## What to judge

1. At the first decision, do the routes create an interesting choice or only extra walking?
2. Can you understand where each route leads using the landmark placeholders and terrain?
3. Does the treasure location make you remember or draw a distinct relationship?
4. Are the narrow passages comfortable for three players? Do the ramp and upper routes offer a useful choice?
5. Are any routes blocked, too steep, too dark, or easy to bypass over a wall?
6. From a distance, does the cave read as an opening in continuous rock? At the mouth and inside, are the transitions comfortable, and is the through route more appealing than the surface detour?

Please note the **seed** and describe the junction or landmark when giving feedback. Geometry, lighting, and clue variety are still graybox quality; this test is for navigation, not final art.
