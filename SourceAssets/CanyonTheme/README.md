# Canyon theme prop library

Ten approved static props imported under `/Game/IslandAssets/Canyon/Props`.
Each mesh has its own folder and imported materials. Mesh origins preserve the
authored ground placement; FBX exports use Blender metre units converted to UE
centimetres. This library does not alter canyon generation or enable prop hunt.

New Blender models:
- `SM_SupplyCrate`: supply crate
- `SM_WaterBarrel`: water barrel
- `SM_MineCart`: mining cart
- `SM_Cactus`: cactus
- `SM_OreCluster`: ore cluster

Reused project models:
- `SM_RockCluster`: Island/SM_RockCluster_A.glb
- `SM_ThreeStoneStack`: MistForestLandmarks/SM_MistForestThreeStoneStack_A.glb
- `SM_Campfire`: PirateLandmarks/SM_CampfireRuins_A.glb (includes flame geometry)
- `SM_FallenLog`: JungleNature/SM_FallenJungleLog_A.glb (includes moss)
- `SM_SkullIdol`: PirateLandmarks/SM_SkullIdol_A.glb

Editable new models and previews are in `SourceAssets/CanyonNewProps`.
Rebuild the FBX files using `Scripts/export_canyon_props.py` in Blender.
