# OBB builder

Supply an owned Steam **game 1.1** installation, either original or with the **Korean patch 1.2** already installed. The builder recognizes the supported source using archive identities; it does not obtain or install a Korean patch.

After the Python/tool setup in [BUILDING.md](BUILDING.md):

```powershell
.\pack-obb.cmd 'D:\Games\WITCH ON THE HOLY NIGHT'
```

The prompt language is English for an original installation and Korean for a Korean installation. Choose:

1. **Original quality — final size over 20 GB.** No additional lossy compression; less processing time.
2. **AVIF — final size approximately 5 GB.** More efficient image/video encoding; longer processing time. Dimensions are preserved, with lossless handling for UI/masks and alpha.

한국어 패치판은 한국어로 안내합니다. **원본 화질 유지(20GB 이상)** 또는 **AVIF 고효율 압축(약 5GB, 제작 시간 증가)**을 선택할 수 있습니다.

For explicit paths and options:

```powershell
.\pack-obb.cmd 'D:\Games\WITCH ON THE HOLY NIGHT' --mode avif --output 'E:\MahoyoAndroid' --cache 'E:\MahoyoCache' --workers 4
```

Use `--mode original` for original quality. Run `pack-obb.cmd --help` for all options.

The original folder is read-only. Choose output/cache directories outside it. Allow approximately **65 GiB** of working space for original mode or **75 GiB** for a first AVIF run, including intermediate data. Final OBB size is smaller than total working space.

To resume after interruption, repeat the same command with the same source, mode, and output/cache paths. Completed members and conversion checkpoints are reused. Changed inputs are rejected rather than mixed into an earlier build. SHA-256 sums are written beside the finished OBB.

Output is grouped by edition and mode, for example:

```text
output/vanilla/avif/main.1.org.mahoyo.port.obb
output/korean/avif/main.1.org.mahoyo.port.kr.obb
```

Copy the matching OBB to the Android location listed in [README.md](README.md). Both encoding modes work with the same edition APK. Packaging checks source compatibility, not Steam account ownership.
