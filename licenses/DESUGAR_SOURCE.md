# Source for the Android Java compatibility library

The APK uses `com.android.tools:desugar_jdk_libs:2.1.5`, a subset of OpenJDK adapted for Android. Its GPLv2 Classpath exception permits linking it with independently licensed MIT project code. The library and its original source remain under their upstream licenses.

- Upstream source: [google/desugar_jdk_libs at release-preparation commit 73170c345e6a762fc6a1f0301bb15218850023ef](https://github.com/google/desugar_jdk_libs/tree/73170c345e6a762fc6a1f0301bb15218850023ef). `VERSION_JDK11.txt` identifies version 2.1.5.
- [Source archive hosted alongside the APK](https://github.com/david419kr/mahoyo-steam-android-port/releases/latest/download/desugar-jdk-libs-2.1.5-source.zip). It contains the unmodified upstream source, copyright headers, licensing information and build scripts.
- [Independent upstream archive](https://codeload.github.com/google/desugar_jdk_libs/zip/73170c345e6a762fc6a1f0301bb15218850023ef).
- Source archive SHA-256: `cf0b48cb046b6517ed8e2a816c125116a907e13e48e341882ba6f4016a155d9f`.

The binary identity and source mapping are recorded in [runtime-components.json](runtime-components.json). This project does not edit the library's source. D8/L8 performs the normal Android bytecode transformation and shrinking; the application build scripts are in `android/`, with Gradle 8.14 and Android Gradle Plugin versions documented in `BUILDING.md` and `android/build.gradle`.

The separately supplied desugaring configuration is BSD-3-Clause software from [R8](https://r8.googlesource.com/r8), not part of the project's MIT code. Its license is retained in [runtime/r8-LICENSE.txt](runtime/r8-LICENSE.txt).
