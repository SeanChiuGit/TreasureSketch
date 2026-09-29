# KayKit Mage Explorer

This is a rigged KayKit Adventurers Mage variant prepared as an explorer/player-character prototype.

## Gear states

- Default: `PROP_Book_Back` and the long green-gem `PROP_MagicStaff_Back` are visible on the character's back.
- Read Book: hide `PROP_Book_Back`, show `PROP_Book_Open`.
- Wand Dig: hide `PROP_MagicStaff_Back`, show `PROP_MagicStaff_Hand`.
- The gear objects are separate so Unreal Blueprint or an Animation Notify can switch visibility without changing the character mesh.

## Actions

- `A_MageExplorer_ReadBook` - 60-frame looping book-reading gesture.
- `A_MageExplorer_WandDig` - 36-frame raise, downward dig/strike, and recovery action.
- KayKit base actions are also retained, including Idle, Run, Walk, Jump, Interact, PickUp, Throw, Use Item, Hit, Spawn, and Death.
- A shove/push action is intentionally not included in this prototype.

## Files

- `KayKitMageExplorer_Rigged.blend` - editable rig, props, actions, and NLA tracks.
- `SK_KayKitMageExplorer.fbx` - character-only skeletal export for Unreal; gear is imported separately and attached at runtime.
- `SK_KayKitMageExplorer.glb` - portable preview/export.
- `MageExplorer_*.gif` and `MageExplorer_*.png` - action and pose previews.
- `KayKit_License.txt` - original KayKit CC0 license.

This asset has not yet been imported or runtime-tested in Unreal Engine. The next integration step is to import the FBX, create animation sequences, and connect prop visibility to the custom action states.
