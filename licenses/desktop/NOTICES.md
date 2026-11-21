# Windows distribution notices

The port's own code is MIT licensed. See LICENSE.txt. Game and Korean patch files
are supplied by the user. The Windows launcher icon is extracted from the user's
Steam executable at build time and remains the property of the game's rights
holders. Other game assets and Korean patch data are not bundled.

Bundled components:

| Component | Version | License / notice |
|---|---|---|
| LWJGL, including Windows and JAWT bindings | 3.3.6 | BSD-3-Clause, LWJGL.txt |
| GLFW | bundled with LWJGL 3.3.6 | Zlib, GLFW.txt |
| stb (Vorbis decoder) | bundled with LWJGL 3.3.6 | MIT/public domain, STB.txt |
| iced-x86 | 1.21.0 | MIT, iced-x86-LICENSE.txt |
| JSON-java | 20260719 | Public domain, JSON.txt |
| Eclipse Temurin JRE | 21.0.13.1+2 | GPLv2 with Classpath Exception and module-specific notices, runtime/legal and runtime/NOTICE |
| FFmpeg shared libraries and tools | n8.1.3-39-g660b5aa7db, LGPL build | LGPLv3, FFmpeg-LGPL.txt and FFmpeg-GPL.txt |

Pinned binary download URLs and SHA-256 hashes are recorded in dependencies.json.
The runtime and FFmpeg are unmodified. The FFmpeg libraries are dynamically linked
from `media/`; compatible replacement libraries can be used in that directory.
Reverse engineering of this application for debugging modifications to those
LGPL libraries is permitted.

Corresponding source and build recipes:

- LWJGL: <https://github.com/LWJGL/lwjgl3/tree/3.3.6>
- GLFW: <https://github.com/glfw/glfw>
- stb: <https://github.com/nothings/stb>
- iced-x86: <https://github.com/icedland/iced/tree/v1.21.0>
- JSON-java: <https://github.com/stleary/JSON-java/tree/20260719>
- Temurin source: <https://github.com/adoptium/jdk21u/commit/8799347eca78>
- Temurin build recipes: <https://github.com/adoptium/temurin-build/tree/8d9fd6fa96e5066571a13823b07c2934e5958d8b>
- FFmpeg source: <https://github.com/FFmpeg/FFmpeg/commit/660b5aa7db>
- FFmpeg binary supplier/build recipes: <https://github.com/FFmpeg-Builds/FFmpeg-Builds/releases/tag/autobuild-2026-11-20-12-40>

Additional JRE module notices remain in the bundled `runtime/legal` tree.
FFmpeg's configure flags can be inspected with `media/ffmpeg.exe -buildconf`.
ADB is detected from the user's existing installation and is not redistributed.
