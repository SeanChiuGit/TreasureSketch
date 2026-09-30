# Fixed through-mountain cave prototype

This is a local geometry test for one cave. Place `ACanyonCavePrototype` in an
empty Unreal level to inspect it. The shape is deterministic and represents the
fixed seed 1000 review stage; it is not yet used by normal Canyon rounds.

The actor extracts **one solid boundary** into one procedural mesh. Its exterior
hill, both entrance faces, tunnel wall and ceiling, and continuous tunnel floor
all come from the same solid field. A low surface valley south of the hill is
the exterior bypass. The mesh uses fixed tunnel width and height at this stage.

`TreasureSketch.Canyon.FixedMountainPrototype` saves four views under
`Saved/CanyonCavePrototype/` and checks collision along the tunnel and bypass.
The fixed stage is ready for visual review before bend, width variation, rock
detail, or random seeds are added.

## Why the current Canyon mesh cannot simply be patched

The live Canyon `IslandMesh` is a single-height terrain grid. A tunnel needs a
floor and a roof at the same XY position, so that grid cannot represent the
solid with a void. The current live implementation deletes whole terrain cells
whose corners touch the cave, then adds a separately sampled cave shell. The
deleted cell edges and cave cross sections do not share vertices. This causes
the entrance gaps and visible shell boundary in the live map.

## Smallest integration change after fixed-scene approval

Reserve one local terrain patch around a selected mountain crossing. Stop the
surrounding grid at that patch's explicit boundary, using the terrain grid's
boundary positions and heights as input. Generate the patch's exterior, tunnel,
entrances, and floor together with the same solid-boundary method as this
prototype. Its outer edge must reuse those input boundary vertices; the
surrounding grid must reuse them too. Replace the live `BuildCanyonCaves` shell
and whole-cell `IsCaveVoid` trimming for this patch. The rest of the Canyon
terrain and route graph can stay as they are.
