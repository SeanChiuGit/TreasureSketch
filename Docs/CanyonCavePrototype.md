# Fixed through-mountain cave prototype

This is a local geometry test for one cave. Place `ACanyonCavePrototype` in an
empty Unreal level to inspect it. The shape is deterministic and represents the
fixed seed 1000 review stage. Normal Canyon rounds now use the same solid
mesh builder for their through-mountain cave region.

The actor extracts **one solid boundary** into one procedural mesh. Its exterior
hill, both entrance faces, tunnel wall and ceiling, and continuous tunnel floor
all come from the same solid field. A low surface valley south of the hill is
the exterior bypass. The mesh uses fixed tunnel width and height at this stage.

`TreasureSketch.Canyon.FixedMountainPrototype` saves four views under
`Saved/CanyonCavePrototype/` and checks collision along the tunnel and bypass.
The fixed actor remains a small reference scene. Normal Canyon rounds supply a
curved path and varying width and height to the shared builder.

## Why the old Canyon mesh could not simply be patched

The live Canyon `IslandMesh` is a single-height terrain grid. A tunnel needs a
floor and a roof at the same XY position, so that grid cannot represent the
solid with a void. The old implementation deleted whole terrain cells whose
corners touched the cave, then added a separately sampled cave shell. The
deleted cell edges and cave cross sections did not share boundaries.

## Integration in normal Canyon rounds

The terrain grid reserves a rectangular patch snapped to its cell boundaries.
The `CanyonSolidMesh` builder samples the same exterior height on those grid
positions, then extracts the mountain exterior, entrances, tunnel surfaces,
and floor from one solid field. The old separate shell and corner-based terrain
deletion are removed. The surrounding terrain and route graph stay in place.
