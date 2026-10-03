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

## Cave cover constraints

Canyon layout now builds one shared list of tunnel segments, rooms and actual
portals. Both the mountain height envelope and solid-field void sampler use it.
Interior columns reserve at least 200 cm of rock above the carved space (or a
larger value at increased map scale). Descending passages retain at least
260 cm of target headroom where terrain needs to rise. Ground-level and uphill
passages request their full designed clearance from the same mountain envelope.

Only actual exterior portals permit an opening through the envelope. Exterior
road cuts stop before the protected interior; the transition has a buffer for
mesh sampling. Uphill portal approaches have a longer height transition and the
outside bypass leaves more space around the mountain. Raised exterior roads use
shared junction heights and a 0.30 grade guide. Solid extraction uses a minimum
25 cm vertical cell size to retain the reserved rock layer.

The collision test checks passage floors and player capsule clearance, then
samples center and side positions for ceiling and actual rock thickness. Seed
1050 remains the uphill review scene; 1051 is its level comparison. These tests
check geometry and collision, not the visual quality of the pictured location.
Current verification: Editor build succeeds. CanyonCoverRelease.log records
passing passage clearance, ceiling thickness, portal height and surface grade
assertions for the sampled layouts, including 1050 and 1051. Both suites still
report failure on cave-count assertions: the conservative placement filter
rejects additional caves on some maps, lowering mixed-cave frequency. This
placement-rate limitation is retained rather than bypassing safety checks.