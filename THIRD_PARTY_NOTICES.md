# Third-party components

Original game media, executable files, translations and extracted artwork are not bundled in the application. The README demonstration photo is retained separately. The application reads original data from the user's Steam installation after download.

External dependencies retain their own licenses. This notice does not replace them.

| Component | Project | License / included notice |
| --- | --- | --- |
| Python | https://www.python.org/ | PSF license |
| Pillow | https://python-pillow.org/ | HPND and bundled library notices |
| NumPy | https://numpy.org/ | BSD-3-Clause and bundled library notices |
| pefile | https://github.com/erocarrera/pefile | MIT |
| Capstone | https://www.capstone-engine.org/ | BSD-3-Clause |
| iced-x86 Java | https://github.com/icedland/iced | MIT; included APK notice |
| Unicorn | https://www.unicorn-engine.org/ | GPL-2.0; see upstream component notices |
| Android Gradle Plugin / Gradle | https://developer.android.com/build / https://gradle.org/ | Upstream licenses |
| JavaSteam and depot downloader | https://github.com/joshuatam/JavaSteam | MIT; source pinned in `dependencies/steam.json` |
| Kotlin and kotlinx.coroutines | https://github.com/JetBrains/kotlin / https://github.com/Kotlin/kotlinx.coroutines | Apache-2.0 |
| ZXing | https://github.com/zxing/zxing | Apache-2.0 |
| Protocol Buffers Java | https://github.com/protocolbuffers/protobuf | BSD-3-Clause |
| Bouncy Castle | https://www.bouncycastle.org/ | MIT |
| XZ for Java | https://tukaani.org/xz/java.html | 0BSD |
| LZMA decoder (7-Zip) | https://github.com/ip7z/7zip | Public domain; source and hashes in `dependencies/lzma` |
| zstd-jni | https://github.com/luben/zstd-jni | BSD-2-Clause; upstream Zstandard notices |
| OkHttp / Okio | https://github.com/square/okhttp / https://github.com/square/okio | Apache-2.0 |

Native decoder notices are also included in the APK. Do not remove license files when preparing a dependency bundle. No general source-code license is assigned by this export; an explicit project license can be chosen before public distribution.
