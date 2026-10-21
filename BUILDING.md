# Building the Android application

## Tools

The Windows build scripts require:

- Python **3.12 x64** or newer, with `venv` and `pip`.
- JDK **17+**; set `JAVA_HOME` (Android Studio's JBR works).
- Gradle **8.14** on PATH, or pass its `gradle.bat` path with `-Gradle`.
- Android SDK platform **35**, build-tools **36.0.0**, NDK **29.0.13113456**, and SDK CMake **3.22.1**.
- A supported Steam game 1.1 installation.
- [Steamless](https://github.com/atom0s/Steamless) CLI and plugins, supplied separately. Only a copied executable is unpacked for reading; the game program is not started.
- [vcpkg](https://github.com/microsoft/vcpkg) ARM64 Android packages for libavif **1.3.0#1**, dav1d **1.5.1#1**, and libyuv **1916**.

Optional FFmpeg/ffprobe executables are needed for AVIF-mode OBB video conversion, not for APK compilation. Keep the supplied tools' license files alongside them.

## Codec packages

A versioned vcpkg manifest is provided in `dependencies/`. With vcpkg and the Android NDK configured:

```powershell
$env:ANDROID_NDK_HOME = 'C:\Android\Sdk\ndk\29.0.13113456'
& C:\Tools\vcpkg\vcpkg.exe install --x-manifest-root=dependencies --x-install-root=.deps/vcpkg-installed --triplet arm64-android
```

Then pass that vcpkg checkout's `packages` directory to setup. Setup checks package versions, ARM64 object format, and pinned API headers; it records hashes of locally built libraries in ignored `.deps/` for the CMake integrity check. Compiler-dependent binary hashes do not need to match another developer's build.

## Setup

Run from the repository root. Paths below are examples, not required installation locations.

```powershell
.\scripts\setup.ps1 -Python python `
  -SteamlessDirectory 'C:\Tools\Steamless' `
  -MediaDirectory 'C:\Tools\ffmpeg\bin' `
  -AvifPackages 'C:\Tools\vcpkg\packages'
```

`MediaDirectory` may be omitted for APK-only builds. The FFmpeg directory should contain `ffmpeg.exe`, `ffprobe.exe`, and the distribution notices. Setup creates `.venv/` and `.deps/`; neither is committed.

## Build

```powershell
$env:JAVA_HOME = 'C:\Program Files\Android\Android Studio\jbr'
.\scripts\build-android.ps1 `
  -Source 'D:\Games\WITCH ON THE HOLY NIGHT' `
  -SdkRoot 'C:\Android\Sdk' `
  -Gradle 'C:\Tools\gradle-8.14\bin\gradle.bat' `
  -Edition all
```

`-Edition vanilla` or `korean` builds only that edition. APK UI artwork, the original icon, and exact layout/color tables are generated from the supplied game into ignored build directories. No private analysis directory is required.

Outputs:

- `android/app/build/outputs/apk/vanilla/release/app-vanilla-release.apk`
- `android/app/build/outputs/apk/korean/release/app-korean-release.apk`

The script verifies signatures, 16 KiB ZIP alignment, release flags, package identity, and edition-specific launcher languages. It does not install an APK or interact with a device.

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
