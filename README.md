# mahoyo-steam-android-port

An ARM64 Android port of **WITCH ON THE HOLY NIGHT**, using files from a user-owned Steam installation.

## Features

- Steam game **1.1**, with optional Korean patch **1.2** resources.
- Separate original and Korean editions, installable side by side.
- Original edition: Auto / Japanese / English / Simplified Chinese / Traditional Chinese.
- Korean edition: Auto / Korean / Japanese / Simplified Chinese / Traditional Chinese.
- Dynamic layouts for phones and foldables, original 16:9 mode, fit/crop images, and ambient margins.
- Full-screen, separated, or automatic dialogue layout; native-resolution text and adjustable typography.
- Save/load, history with voice replay, automatic reading, gestures, and on-screen help.
- OBB builder with progress reporting, source verification, and resumable conversion/packing.

## Install

Android **9+**, **ARM64**, and **OpenGL ES 3.0** are required.

| Edition | APK package | OBB path on the device |
| --- | --- | --- |
| Original | `org.mahoyo.port` | `Android/obb/org.mahoyo.port/main.1.org.mahoyo.port.obb` |
| Korean | `org.mahoyo.port.kr` | `Android/obb/org.mahoyo.port.kr/main.1.org.mahoyo.port.kr.obb` |

Build the APKs using [BUILDING.md](BUILDING.md). Create the OBB using [OBB-BUILDER.md](OBB-BUILDER.md), then copy it to the matching directory above. The APK reads either original-quality or AVIF OBBs. No game files or finished OBBs are included here.

## OBB modes

| Mode | Expected final size | Processing |
| --- | --- | --- |
| Original quality | **Over 20 GB** | No additional lossy compression; faster |
| AVIF | **Approximately 5 GB** | Higher-efficiency image/video compression; slower |

The builder detects supported original/Korean installations and presents English/Korean guidance accordingly. Actual sizes depend on the supported source and encoding results.

## Repository

- `android/`: Android application, rendering, audio, controls, and JNI.
- `runtime/`: script VM and game runtime.
- `tools/`: source readers, asset conversion, compression, and OBB packing.
- `scripts/`: dependency setup, Android build, and host checks.
- `tests/`: synthetic runtime and tool regressions; no game media.
- `data/`: compatibility profiles and UI translations.

See [third-party notices](THIRD_PARTY_NOTICES.md). Generated artwork, external binaries, signing keys, caches, APKs, and OBBs stay outside Git. This repository is a curated source import, not a record of the original development process.
