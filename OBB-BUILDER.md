# OBB builder

Supply an owned Steam **game 1.1** installation, either original or with the **Korean patch 1.2** already installed. The builder recognizes the supported source using archive identities; it does not obtain or install a Korean patch.

After the Python/tool setup in [BUILDING.md](BUILDING.md):

```powershell
.\pack-obb.cmd 'D:\Games\WITCH ON THE HOLY NIGHT'
```

Guidance is English for an original installation and Korean for a Korean installation. The public builder produces **AVIF OBBs only — final size approximately 5 GB**, with longer processing time. Dimensions are preserved, with lossless handling for UI/masks and alpha. Older original-quality OBBs remain readable by the APK.

한국어 패치판은 한국어로 안내합니다. **AVIF 고효율 압축(최종 용량 약 5GB, 제작 시간 증가)**으로 작성합니다. 원본 화질 모드 선택은 제거되었습니다.

For explicit paths and options:

```powershell
.\pack-obb.cmd 'D:\Games\WITCH ON THE HOLY NIGHT' --mode avif --output 'E:\MahoyoAndroid' --cache 'E:\MahoyoCache' --workers 4
```

`--mode avif` is optional and retained for existing commands. Run `pack-obb.cmd --help` for all options.

The original folder is read-only. Choose output/cache directories outside it. Allow approximately **75 GiB** of working space for the first run, including intermediate data. Final OBB size is smaller than total working space.

To resume after interruption, repeat the same command with the same source, mode, and output/cache paths. Completed members and conversion checkpoints are reused. Changed inputs are rejected rather than mixed into an earlier build. SHA-256 sums are written beside the finished OBB.

Output is grouped by edition and mode, for example:

```text
output/vanilla/avif/main.1.org.mahoyo.port.unofficial.obb
output/korean/avif/main.1.org.mahoyo.port.unofficial.obb
```

Copy the OBB to an accessible phone folder and select it using **Import AVIF OBB** in the unified APK. No manual `Android/obb` folder manipulation is needed. The APK detects the edition from the content. Packaging checks source compatibility; first-run Steam ownership verification happens in the APK.
