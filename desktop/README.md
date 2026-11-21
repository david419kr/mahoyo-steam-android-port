# Mahoyo — Windows

Extract the complete folder, then open **Mahoyo.exe**. On first launch, select
your Steam game folder containing `WoH.exe`. Keep the bundled subfolders together.
Java does not need to be installed separately. Windows x64 and OpenGL 3.3 are required.

The launcher reads the Steam 1.1 game files directly. Its icon is extracted from
the original Steam executable when building; other game assets are not bundled.
Existing game files are not modified. Settings include adaptive/original
display, dialogue placement, ambient margins, native-resolution text and render scale.

| Control | Action |
|---|---|
| Left click / Enter | Advance dialogue |
| Right click / Escape | Show or hide the menu |
| Space | Show or hide dialogue |
| Hold Ctrl | Fast-forward; release to stop |
| Wheel up / Up arrow | History |
| Alt+Enter | Borderless fullscreen / restore window |
| Shift+Enter | Original 16:9 / adaptive display |

During fullscreen play, the mouse pointer hides after two seconds of inactivity
or immediately when Enter advances dialogue. Moving the mouse restores it.
Menus and windowed mode keep the pointer visible.

## Controller

DualSense, DualShock 4 and standard Xbox/XInput controllers use the same layout.
Connect before or during play; no extra controller software is required for a
device recognized by GLFW's built-in gamepad mappings.

| Xbox position | PlayStation | Action during play |
|---|---|---|
| A (bottom) | Cross | Advance / confirm |
| B (right) | Circle | Cancel / close |
| Y (top) | Triangle | Toggle menu |
| X (left) | Square | History |
| D-pad up | D-pad up | History |
| D-pad left / right | D-pad left / right | Save / load |
| D-pad down | D-pad down | Settings |
| Hold RB | Hold R1 | Skip until released |
| View / Back / Select | Create / Share / Select | Show or hide dialogue |

In menus, use the D-pad to select, the bottom button to confirm and the right
button to cancel. Left/right adjust a focused settings slider. Losing focus or
disconnecting the controller stops its held skip. A button held while a menu
opens must be released before it can activate that menu.

Korean patch: in Settings, select the existing `patch_KR` folder (or its parent)
containing `mahoyo_kr.dll` and the three `mahoyo_KR*.dat` files, then restart.
The patch DLL is only read as data; it is not executed. Keep that folder available.

## Android save synchronization

Save your current progress, then open **SaveSync.exe**. Enable USB debugging and
authorize this PC on the phone. The tool checks USB devices and the reverse tunnel
at `127.0.0.1:17555`, and detects the installed unofficial Steam port.

ADB must be installed: Android SDK Platform-Tools on PATH, in the Android SDK,
`C:\Program Files\platform-tools`, or `C:\platform-tools` are detected automatically.
Download it from <https://developer.android.com/tools/releases/platform-tools> if needed.

An Android update supporting this protocol is included in `android/`. The tool
offers to install it if needed. Updating closes the game; existing saves remain.
Close open game menus before synchronization. The PC player may stay running.

- Each slot keeps the newer saved version, based on its file modification time.
- Read flags, CG and clear progress are merged by union. Thumbnails follow their saves.
- Missing slots are copied. Deleting a slot on one device does **not** delete it on the other.
- Both originals are backed up before changes. Hashes are checked on both sides afterward.
- Unsaved in-game progress and installation/account records are not transferred.

Windows saves and settings: `%LOCALAPPDATA%\MahoyoUnofficial`.
Backups: its `save-sync-backups` subfolder. Logs: its `logs` subfolder.
Phone-side backups are retained inside the app's private storage as well.

This is a local test build. It has not been published as a Windows release.
