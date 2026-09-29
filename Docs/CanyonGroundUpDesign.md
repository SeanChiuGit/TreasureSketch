# Canyon map redesign: gameplay before geometry

Status: design contract for the new Canyon branch. The previous `feature/gameplay-first-canyon` implementation is an experiment, not the base for this work.

## Player task

One player sees the treasure and later draws a clue from memory. Another player interprets that drawing while navigating. A successful generated map gives them a **small set of useful facts**: a memorable overall shape, a route decision, recognizable landmarks, and a treasure location described by a spatial relationship. Visual variety alone does not count.

The generator must produce a clue plan before it produces terrain. Example: “From the starting ledge, the needle is visible. At the fork, take the low route under the arch. The treasure is below the broken crossing.” Every item in that plan must correspond to a reachable, visible, drawable fact in the world.

## New generation contract

1. **Choose a navigation problem.** Examples: two routes with different visibility, a loop with a shortcut, a hub with two misleading branches, a high route crossing a low route, or a sequence of landmarks with a hidden side pocket. This is a grammar of decisions and connections, not a list of fixed cell maps.
2. **Create a semantic graph.** Nodes represent arrival spaces, decisions, viewpoints, crossings, landmarks, and treasure areas. Edges describe traversal, travel cost, visibility, elevation change, and whether the route can be seen from another route. The graph contains a valid clue path and optional alternatives.
3. **Choose a treasure relationship.** The treasure is behind, below, between, aligned with, or after named world features. The relationship must be expressible in a quick sketch and verified in the generated space.
4. **Embed the graph in continuous space.** Solve distances, angles, branch separation, crossing clearance, and approach directions without grid cells or fixed module lengths. Curved route centerlines and irregular region boundaries are outputs of this step.
5. **Reserve landmark sites.** Each site records silhouette, viewing direction, footprint, clearance, required walk-through space, and expected sightlines. Placeholder geometry is sufficient until a matching asset exists.
6. **Build traversable graybox terrain.** Carve routes and spaces from the embedded plan. Add cliffs, mesas, walls, and vertical links where they support the navigation problem. The geometry must be walkable and must preserve intended sightlines and occlusion.
7. **Validate and repair.** Test actual traversal, slopes, route choices, landmark visibility and separation, treasure concealment, and whether the clue plan remains true. Reject or repair failures before decoration.
8. **Resolve art.** Select or commission meshes to fit the reserved sites. Secondary rocks and materials enrich the scene only after the graybox passes gameplay review.

## Canyon vertical slice

The first reviewable result is a set of about 20 **asset-free graybox seeds**. Show the graph, a top-down route sketch, the clue plan, and a playable UE map for representative seeds. A seed passes only if another player can explain its route choice and treasure relationship from the sketch. We should compare player-facing structure, not merely count different hashes or mesh arrangements.

The first implemented navigation problems should cover: fork and rejoin, loop and shortcut, high versus low crossing, and hub with a hidden side pocket. Each can vary in length, orientation, branch angle, height, side chambers, and clue order. The generator should not guarantee variety by cycling seed modulo a list of fixed layouts.

## Landmark and asset interface

A landmark slot is specified by what it does in the map. An asset may fill a slot only if its geometry satisfies the contract. Required fields are:

- identity and sketch silhouette (for example arch, needle, split peak);
- world dimensions, pivot, collision, and walk-through clearance;
- preferred approach/view directions and minimum useful viewing distance;
- footprint and decoration exclusion radius;
- whether a route can pass through, between, below, or above it.

The Giant Stone Arch being made in the other chat is the first candidate for an `Underpass` slot. It remains independent of this generator until its dimensions and collision are tested. Its existing look is a candidate visual style, not a structural constraint.

## Acceptance checks

- Spawn to treasure and each intended route are physically traversable.
- Every decision promised by the clue plan presents distinguishable alternatives.
- A sketchable landmark appears early, and at least two landmarks have distinct silhouettes and meaningful relative positions.
- The target treasure relationship is physically true and observable during scouting, while the treasure is not trivially visible from spawn.
- The map has enough visual space around major landmarks for their silhouettes to read.
- Across 20 seeds, maps require different route descriptions or different spatial clues. Rotate/mirror/retint of the same structure does not count as a distinct navigation problem.

## Boundary with existing code

The current `AProceduralIsland` remains the Island/Forest implementation while the new Canyon path is developed separately. The new graph, embedding, graybox geometry, and validator must not depend on its terrain noise, decoration scatter, 5×5 cell coordinates, or 24 m mesh sockets. We can reuse engine plumbing such as seed replication and procedural mesh components after the gameplay contract is proven.

## First design probe

`Scripts/prototype_canyon_plan.py` generated the 20 sketches in `Docs/CanyonGraybox20.svg` from seeds 1000–1019. This is an asset-free graph probe, not a UE map. Its first version produced mostly the same fork/rejoin shape despite different labels; that version was rejected. The current version includes six navigation problems and places some treasure goals on loops, alcoves, or one branch of a high/low split.

All 1,000 seeds from 1000–1999 passed the prototype's graph connectivity, landmark count, and abstract edge-grade checks. Those checks do **not** establish terrain walkability, landmark visibility, or the truth of clues in a 3D world. The 20-seed sample also overrepresents high/low maps (9 of 20), and several clue sentences still repeat the same structure. The next iteration must improve clue and topology diversity before UE terrain or commissioned assets are fitted to this system.

## Playable Unreal graybox

The Canyon branch now has a separate route layout generator and uses it to carve a continuous heightfield: lower corridors with variable widths, steep boundary walls, access ramps, and an upper wall-top route with its own loop. Local detours and dead ends add choices to the six navigation problems. Landmark silhouettes are still engine primitives. The solo menu exposes Canyon graybox testing at five times normal movement speed, with flight toggled by `V`; `R` replaces the current solo map with a new seed. See `Docs/CanyonGrayboxPlaytest.md` for the launch and feedback workflow. This is a navigation playtest; the Python sketches and UE implementation are two design probes, not yet one shared serialized map format.
