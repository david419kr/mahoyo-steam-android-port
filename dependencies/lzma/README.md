# LZMA decoder

Unmodified public-domain decoder by Igor Pavlov, from https://github.com/ip7z/7zip.
The exact upstream commit and SHA-256 hashes are recorded in `source.json`.
LzmaDec.c and the ARM64 LzmaDecOpt.S fast path are compiled. The decoder is used for Steam VZip depot chunks;
final decoded length and Steam Adler-32 remain checked by JavaSteam.
