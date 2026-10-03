# Canyon generation performance

Performance-only changes after v0.13.0. Layout generation, random streams,
sampling resolution, tunnel shapes and map scale formulas remain unchanged.

- Compute terrain height/normal rows and solid-field samples concurrently.
- Triangulate consecutive Z ranges independently, then append in original order.
- Skip solid-field cells whose eight corners all have the same sign.
- Avoid unused tunnel projection work for grouped cave networks.
- Cook the four terrain material sections together once instead of rebuilding
  their combined collision after each section. Construction still completes
  collision synchronously before gameplay uses the island.

## Measured results

UE 5.6 Win64 Development Editor, headless (`-nullrhi`), MapScale=1. Timed
`FinishSpawning` on this development machine; these are individual samples,
not a cross-machine benchmark or rendered frame measurements.

| Seed | Before | After | Reduction |
| --- | ---: | ---: | ---: |
| 1050 | 3.233251 s | 2.106516 s | 34.8% |
| 18232 | 4.053947 s | 2.498988 s | 38.4% |

Both seeds preserve the full procedural mesh CRC: every position, normal,
UV0, color, tangent and triangle index, in original order. All nonempty mesh
sections retain collision. Logs: `Saved/Logs/CanyonPerfBefore.log` and
`Saved/Logs/CanyonPerfFinal.log`.

The AllGameModes smoke run completed round/spawn/treasure checks but failed
the capsule traversal check for random seed 784343 at step 28. This run is
not an all-game-modes pass. A targeted comparison checks that batched terrain
collision and repeated original-style collision cooking produce identical
floor and capsule query results for this seed; this comparison passed in
`Saved/Logs/CanyonPerfCollision.log`. Existing cave shape problems
are outside this performance change.

Map construction still waits for mesh assembly and collision cooking. These
changes shorten the pause; they do not make generation asynchronous. The
downloadable v0.13.0 package does not contain this optimization yet.
