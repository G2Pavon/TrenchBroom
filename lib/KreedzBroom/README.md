# KreedzBroom

Everything KreedzBroom adds to TrenchBroom: play mode, which runs Counter-Strike 1.6
movement in the 3D view.

| Library | What it holds |
|---|---|
| `TbCsLib` | The movement itself: ReGameDLL's `pm_shared` and the engine code it needs, ported from ReHLDS. No Qt, no TrenchBroom dependencies. |
| `TbKzbLib` | Play mode in the editor: the controller, mouse capture, sounds, showpre, the collision world built from the map, and the KreedzBroom preference pane. |

## Hooks in TrenchBroom

TrenchBroom has no plugin interface, so a few small hooks connect it to `TbKzbLib`.
These are the only places where KreedzBroom changes TrenchBroom's own files. Check them
first when merging upstream.

| File | Hook |
|---|---|
| `app/TrenchBroom/src/Main.cpp` | Calls `kzb::initialize()`, which turns vsync off and installs the two hooks below. |
| `lib/TbUiLib/.../MapView3D.{h,cpp}` | `MapView3DPlayMode` and `setPlayModeFactory`: each 3D view gets a play mode, which takes over updating and drawing the view while it is active. Fly mode paces its frames itself because vsync is off. |
| `lib/TbUiLib/.../PreferenceDialog.{h,cpp}` | `addPane` adds the KreedzBroom pane. |
| `lib/TbUiLib/src/ActionManager.cpp` | The Run menu entries "Play" (F7) and "Play From Camera" (Shift+F7). |
| `lib/CMakeLists.txt`, `app/TrenchBroom/CMakeLists.txt` | Build this folder and link `TbKzbLib` into the app. |

Play mode's settings are declared in `TbKzbLib/include/kzb/Preferences.h`, in
TrenchBroom's `Preferences` namespace. Its input reaches it through an event filter on
the view, so `MapView3D`'s event handlers stay as they are upstream.
