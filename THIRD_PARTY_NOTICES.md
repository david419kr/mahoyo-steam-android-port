# Third-party notices

Original project source code is licensed under the [MIT License](LICENSE), copyright 2026 david419kr. That license does not replace the terms of third-party components or grant rights to the original game, translations, trademarks, or the README demonstration screenshot. Game resources are obtained from the user's installation and are not bundled in the APK.

## Components distributed in the APK

The [runtime inventory](licenses/runtime-components.json) records all resolved artifact names, versions and notice paths, including transitive dependencies and core-library desugaring. The APK includes this inventory, this document, the project MIT license, and all files in [licenses/runtime](licenses/runtime). Upstream license texts and copyright notices are preserved. The build rejects changes to the dependency set until the inventory is updated.

| Component group / artifacts | Version | License | License text and notices |
| --- | --- | --- | --- |
| [androidx.annotation](https://android.googlesource.com/platform/frameworks/support/) — annotation-jvm | 1.9.1 | Apache-2.0 | [androidx-annotation-LICENSE.txt](licenses/runtime/androidx-annotation-LICENSE.txt) |
| [androidx.startup](https://android.googlesource.com/platform/frameworks/support/) — startup-runtime | 1.2.0 | Apache-2.0 | [androidx-startup-LICENSE.txt](licenses/runtime/androidx-startup-LICENSE.txt) |
| [androidx.tracing](https://android.googlesource.com/platform/frameworks/support/) — tracing | 1.0.0 | Apache-2.0 | [Apache-2.0.txt](licenses/runtime/Apache-2.0.txt) |
| [com.android.tools](https://github.com/google/desugar_jdk_libs) — desugar_jdk_libs | 2.1.5 | GPL-2.0-only WITH Classpath-exception-2.0 AND Apache-2.0 AND BSD-3-Clause | [desugar-jdk-libs-LICENSE.txt](licenses/runtime/desugar-jdk-libs-LICENSE.txt), [desugar-jdk-libs-ASSEMBLY_EXCEPTION.txt](licenses/runtime/desugar-jdk-libs-ASSEMBLY_EXCEPTION.txt), [desugar-jdk-libs-ADDITIONAL_LICENSE_INFO.txt](licenses/runtime/desugar-jdk-libs-ADDITIONAL_LICENSE_INFO.txt), [desugar-jdk-libs-SOURCE-NOTICES.txt](licenses/runtime/desugar-jdk-libs-SOURCE-NOTICES.txt) |
| [com.android.tools](https://r8.googlesource.com/r8) — desugar_jdk_libs_configuration | 2.1.5 | BSD-3-Clause | [r8-LICENSE.txt](licenses/runtime/r8-LICENSE.txt) |
| [com.github.luben](https://github.com/luben/zstd-jni) — zstd-jni | 1.5.7-4 | BSD-2-Clause AND BSD-3-Clause | [zstd-jni-LICENSE.txt](licenses/runtime/zstd-jni-LICENSE.txt), [zstd-LICENSE.txt](licenses/runtime/zstd-LICENSE.txt) |
| [com.google.protobuf](https://github.com/protocolbuffers/protobuf) — protobuf-java | 4.31.1 | BSD-3-Clause | [protobuf-LICENSE.txt](licenses/runtime/protobuf-LICENSE.txt) |
| [com.google.zxing](https://github.com/zxing/zxing) — core | 3.5.3 | Apache-2.0 | [Apache-2.0.txt](licenses/runtime/Apache-2.0.txt), [zxing-NOTICE.txt](licenses/runtime/zxing-NOTICE.txt) |
| [com.squareup.okhttp3](https://github.com/square/okhttp) — okhttp-android, okhttp-coroutines | 5.1.0 | Apache-2.0 | [okhttp-LICENSE.txt](licenses/runtime/okhttp-LICENSE.txt) |
| [com.squareup.okio](https://github.com/square/okio) — okio-jvm | 3.16.0 | Apache-2.0 | [okio-LICENSE.txt](licenses/runtime/okio-LICENSE.txt) |
| [io.github.icedland.iced](https://github.com/icedland/iced) — iced-x86 | 1.21.0 | MIT | [iced-x86-LICENSE.txt](licenses/runtime/iced-x86-LICENSE.txt) |
| [io.github.joshuatam](https://github.com/joshuatam/JavaSteam) — javasteam-depotdownloader, javasteam | 1.8.0.1-26-SNAPSHOT | MIT | [javasteam-LICENSE.txt](licenses/runtime/javasteam-LICENSE.txt) |
| [io.ktor](https://github.com/ktorio/ktor) — ktor-client-cio-jvm, ktor-client-core-jvm, ktor-client-websockets-jvm, ktor-events-jvm, ktor-http-cio-jvm, ktor-http-jvm, ktor-io-jvm, ktor-network-jvm, ktor-network-tls-jvm, ktor-serialization-jvm, ktor-sse-jvm, ktor-utils-jvm, ktor-websocket-serialization-jvm, ktor-websockets-jvm | 3.2.2 | Apache-2.0 | [ktor-LICENSE.txt](licenses/runtime/ktor-LICENSE.txt) |
| [org.apache.commons](https://commons.apache.org/proper/commons-lang/) — commons-lang3 | 3.18.0 | Apache-2.0 | [commons-lang3-LICENSE.txt](licenses/runtime/commons-lang3-LICENSE.txt), [commons-lang3-NOTICE.txt](licenses/runtime/commons-lang3-NOTICE.txt) |
| [org.bouncycastle](https://www.bouncycastle.org/) — bcprov-jdk18on | 1.81 | MIT | [bouncycastle-LICENSE.html](licenses/runtime/bouncycastle-LICENSE.html) |
| [org.jetbrains.kotlin](https://github.com/JetBrains/kotlin) — kotlin-stdlib-jdk7, kotlin-stdlib-jdk8, kotlin-stdlib | 2.2.0 | Apache-2.0 | [kotlin-LICENSE.txt](licenses/runtime/kotlin-LICENSE.txt), [kotlin-NOTICE.txt](licenses/runtime/kotlin-NOTICE.txt) |
| [org.jetbrains.kotlinx](https://github.com/Kotlin/kotlinx.coroutines) — kotlinx-coroutines-android, kotlinx-coroutines-core-jvm, kotlinx-coroutines-slf4j | 1.10.2 | Apache-2.0 | [coroutines-LICENSE.txt](licenses/runtime/coroutines-LICENSE.txt) |
| [org.jetbrains.kotlinx](https://github.com/Kotlin/kotlinx-io) — kotlinx-io-bytestring-jvm, kotlinx-io-core-jvm | 0.7.0 | Apache-2.0 | [kotlinx-io-LICENSE.txt](licenses/runtime/kotlinx-io-LICENSE.txt) |
| [org.jetbrains.kotlinx](https://github.com/Kotlin/kotlinx.serialization) — kotlinx-serialization-core-jvm, kotlinx-serialization-json-jvm | 1.9.0 | Apache-2.0 | [kotlinx-serialization-LICENSE.txt](licenses/runtime/kotlinx-serialization-LICENSE.txt) |
| [org.jetbrains](https://github.com/JetBrains/java-annotations) — annotations | 23.0.0 | Apache-2.0 | [jetbrains-annotations-LICENSE.txt](licenses/runtime/jetbrains-annotations-LICENSE.txt) |
| [org.slf4j](https://www.slf4j.org/) — slf4j-api | 2.0.17 | MIT | [slf4j-LICENSE.txt](licenses/runtime/slf4j-LICENSE.txt) |
| [org.tukaani](https://tukaani.org/xz/java.html) — xz | 1.10 | 0BSD | [xz-COPYING.txt](licenses/runtime/xz-COPYING.txt) |
| LZMA decoder and ARM64 assembly | 7-Zip commit `b88c6bc09339de32f7d18bbab900686b244f42ba` | Public domain | [Notice](licenses/runtime/lzma-NOTICE.txt), [source identities](https://github.com/david419kr/mahoyo-steam-android-port/blob/main/dependencies/lzma/source.json) |

JavaSteam and its depot downloader are built from commit `433f2ad15c36d5e690a4fe77401ec3f6b960641e`. Android downloader and codec modifications are recorded, with upstream and modified hashes, in [dependencies/steam.json](https://github.com/david419kr/mahoyo-steam-android-port/blob/main/dependencies/steam.json). Other runtime dependencies are used without project source modifications. D8/L8 performs the normal Android bytecode transformation.

The Public Suffix List data distributed with OkHttp is separately licensed under [MPL-2.0](licenses/runtime/MPL-2.0.txt); retain its [notice](licenses/runtime/public-suffix-list-NOTICE.txt). The [matching source list](https://github.com/david419kr/mahoyo-steam-android-port/releases/download/v0.6.3/public-suffix-list-5.1.0-source.txt) is provided alongside the APK. Its rules reproduce the packaged table byte for byte. Zstandard also retains the bundled [xxHash notice](licenses/runtime/xxhash-NOTICE.txt).

The OpenJDK library subset is **GPLv2 with the Classpath exception**, with additional per-file Apache/BSD terms. The exception permits linking it with independently licensed project code; it does not relicense the library under MIT. [Corresponding source and build information](licenses/DESUGAR_SOURCE.md) are supplied separately. Preserve that source access when redistributing an APK. The separate R8 desugaring configuration is BSD-3-Clause.

## Separately installed host tools

These tools are installed by the developer and are not bundled in the APK. Their upstream licenses remain in effect.

| Tool | License / upstream notice |
| --- | --- |
| Pillow 12.3.0 | [Distribution license and included library notices](licenses/tools/pillow/LICENSE) |
| NumPy 2.3.5 | [Distribution license and included library notices](licenses/tools/numpy/LICENSE.txt) |
| Python | [PSF license and notices](https://docs.python.org/3/license.html) |
| Gradle | [Apache-2.0 and dependency notices](https://github.com/gradle/gradle/blob/master/LICENSE) |
| Android Gradle Plugin / SDK / NDK / CMake / JDK / compiler | Separately obtained build tools; retain their vendors' licenses and notices. These tool distributions are not included in the APK or relicensed as project code. |

JUnit 4.13.2 (EPL-1.0), Hamcrest Core 1.3 (BSD-3-Clause), and org.json (upstream public-domain license) are test-only dependencies and are not included in the release APK. Their declarations remain in `android/app/build.gradle`.

No Unicorn or Capstone dependency is required by the public project. Obsolete PE/emulation analysis utilities have been removed. Files under `licenses/` retain their upstream terms and are not relicensed by the project's MIT license.
