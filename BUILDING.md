# Building the Android application

## Tools

The Windows build scripts require:

- Python **3.12 x64** or newer, with `venv` and `pip`.
- JDK **21**; set `JAVA_HOME` (Android Studio's JBR works). The Android app targets Java 17 bytecode; the pinned JavaSteam source build needs JDK 21.
- Gradle **8.14** on PATH, or pass its `gradle.bat` path with `-Gradle`.
- Android SDK platform **35**, build-tools **36.0.0**, NDK **29.0.13113456**, and SDK CMake **3.22.1**.

## Setup

Run from the repository root. Paths below are examples, not required installation locations.

```powershell
.\scripts\setup.ps1 -Python python
```

Setup creates `.venv/` and `.deps/`; neither is committed.

## Build

```powershell
$env:JAVA_HOME = 'C:\Program Files\Android\Android Studio\jbr'
.\scripts\build-android.ps1 `
  -SdkRoot 'C:\Android\Sdk' `
  -Gradle 'C:\Tools\gradle-8.14\bin\gradle.bat'
```

The script checks out JavaSteam at the commit in `dependencies/steam.json`, applies the checksum-verified Android download patches recorded there, then builds it as a source dependency. The patches bound pending chunks, use platform AES/Adler32, and enable the bundled native LZMA decoder while retaining chunk integrity checks. Setup verifies both patched files and rejects unrelated local edits. The public-domain decoder sources and their upstream hashes are tracked in `dependencies/lzma/`. A fresh clone needs Git and network access for dependency setup. No game installation or private development directory is needed to build the APK.

The APK contains the project's custom icon and interface, not original game artwork, text, palettes or extracted engine tables. After an authenticated Steam download, the application reads those resources from the user's installed game files. Data-only metadata interpretation runs in a bounded reader; it never loads or executes the Windows game. The build verifies an explicit APK asset allowlist to catch accidental bundling.

The public build and synthetic checks do not require Unicorn, Capstone, pefile, Steamless, or a game installation. Python dependencies are limited to Pillow and NumPy for the image/codec checks. Third-party license texts and notices are copied into the APK; `stageLicenseAssets` checks the resolved runtime and desugaring dependencies against `licenses/runtime-components.json`. Update that inventory and the applicable notices when changing dependency versions. Preserve the desugared Java library source access described in `licenses/DESUGAR_SOURCE.md` when distributing an APK.

Outputs:

- `android/app/build/outputs/apk/release/app-release.apk` — unified `org.mahoyo.port.unofficial` application

The script runs installer unit tests and verifies signatures, 16 KiB ZIP alignment, release flags, and package identity. It does not install an APK or interact with a device. The separate `src/androidTest` instrumentation is not included in the release APK.

## Signing

By default, a local key is created at `.local/signing/port-release.jks`. Its default local-build password is `local-port`; this is a convenience key, not a published release credential. For controlled release signing, pass `-SigningKey` or set `WOH_KEYSTORE`, `WOH_STORE_PASSWORD`, and `WOH_KEY_PASSWORD`; the alias is `woh-port`.

Keep the same signing key for updates. An APK signed with a different key cannot update an existing installation of the same package.

## Host checks

No game files or Android device are required for the synthetic tests:

```powershell
.\.venv\Scripts\python.exe -B -m unittest discover -s tests -p 'test_*.py'
cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build/host
ctest --test-dir build/host --output-on-failure
.\scripts\check-android.ps1
```

The Java checks need `JAVA_HOME`; the slot check also needs `clang++` on PATH. Host checks validate logic and file formats; they do not replace a device rendering/audio test.
