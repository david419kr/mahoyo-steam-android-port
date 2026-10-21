# mahoyo-steam-android-port

An ARM64 Android port of **WITCH ON THE HOLY NIGHT**, using files from a user-owned Steam installation.

The installer has been checked on an ARM64 Android phone with real QR approval and ownership verification, a complete fixed-version Steam download, offline playback, legacy AVIF OBB import, and Korean patch ZIP application. Broader device and interruption testing is still in progress.

## Features

- Steam game **1.1**, with optional Korean patch **1.2** resources.
- One unofficial APK that detects original or Korean installation data.
- Steam password/Guard and QR login, ownership verification, and optional direct Steam download.
- Permanent offline playback after installation; no periodic login, token renewal, expiry, or network check.
- Original edition: Auto / Japanese / English / Simplified Chinese / Traditional Chinese.
- Korean edition: Auto / Korean / Japanese / Simplified Chinese / Traditional Chinese.
- Dynamic layouts for phones and foldables, original 16:9 mode, fit/crop images, and ambient margins.
- Full-screen, separated, or automatic dialogue layout; native-resolution text and adjustable typography.
- Save/load, history with voice replay, automatic reading, gestures, and on-screen help.
- OBB builder with progress reporting, source verification, and resumable conversion/packing.

## Install

Android **9+**, **ARM64**, and **OpenGL ES 3.0** are required.

Build the APK using [BUILDING.md](BUILDING.md). Its package is `org.mahoyo.port.unofficial`; it can coexist with the older development APKs. This project is not affiliated with Valve or the game's publisher.

On first use, sign in to Steam with a password/Steam Guard or scan the QR using Steam Mobile. The account must own WITCH ON THE HOLY NIGHT. Then choose:

- **Import an AVIF OBB, approximately 5 GB.** Make it on a PC using [OBB-BUILDER.md](OBB-BUILDER.md), copy it anywhere accessible on the phone, and select it in the app. Older original-quality OBB files are also accepted. The app copies and verifies the chosen file; keep enough free space for the copy.
- **Download directly from Steam.** No PC conversion is required. The supported original Steam files occupy approximately **17.7 GB**, plus cache and preparation space. The installer displays the current network download and installed-size estimates when available. Files remain in their original HFA archives. Downloads request the fixed game 1.1 manifest, independently of the current public branch. Steam must still grant the account access to that manifest.

For a direct Steam installation, choose a user-supplied **Korean patch 1.2 ZIP** in game data management. The app finds and verifies the required files regardless of ZIP folder layout; Windows code is never executed. A Korean OBB is detected automatically. Korean installations replace the English language slot with Korean.

After successful setup, ordinary launches use only local data. App updates retain the installation record and saves. Uninstalling the app or clearing its data removes that record. Steam tokens and passwords are never saved. Steam download resumption requires another online login; playing an already installed game does not.

No game files, Korean patch, credentials, or finished OBBs are supplied here. The public PC builder produces AVIF OBBs only, with progress reporting and resumable checkpoints.

## Repository

- `android/`: Android application, rendering, audio, controls, and JNI.
- `runtime/`: script VM and game runtime.
- `tools/`: source readers, asset conversion, compression, and OBB packing.
- `scripts/`: dependency setup, Android build, and host checks.
- `tests/`: synthetic runtime and tool regressions; no game media.
- `data/`: compatibility profiles and UI translations.

See [third-party notices](THIRD_PARTY_NOTICES.md). Generated artwork, external binaries, signing keys, caches, APKs, and OBBs stay outside Git. This repository is a curated source import, not a record of the original development process.
