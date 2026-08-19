# RapidOCR build dependency

The `vocekit-rapidocr.exe` helper uses the official RapidOcrOnnx 1.2.2
complete project package.

Download:

https://github.com/RapidAI/RapidOcrOnnx/releases/tag/1.2.2

Required archive:

`Project_RapidOcrOnnx-1.2.2.7z`

Pinned archive checks:

```text
83183975 bytes
SHA-256 5049D4C9CAF0143A9F35E618F80F7E5946B8DF1E57A90B3CA8E9CC105FC8A6AE
```

The machine-readable lock is `rapidocr-lock.json`. In addition to the archive
checks, it pins a deterministic SHA-256 fingerprint covering all 1,445
extracted paths, file sizes, and file contents. Existing local SDK directories
are never trusted only because a few expected files exist.

Extract it to:

`vocekit-qt6/Project_RapidOcrOnnx-1.2.2`

The preferred preparation command downloads the exact pinned release asset,
checks its byte length and SHA-256, extracts it in a temporary directory, and
validates the required SDK libraries and model files before installation:

```powershell
.\scripts\fetch-rapidocr.ps1
```

For an already downloaded archive, pass its path explicitly:

```powershell
.\scripts\fetch-rapidocr.ps1 -ArchivePath "C:\Downloads\Project_RapidOcrOnnx-1.2.2.7z"
```

Then run:

```powershell
.\scripts\build-ocr-helpers.ps1
```

The downloaded SDK is intentionally excluded from Git because it is about
810 MB. The deploy script packages only the helper executable, four model
files, and the upstream Apache-2.0 license.

Expected model SHA-256 values:

```text
3439588C030FAEA393A54515F51E983D8E155B19A2E8ABA7891934C1CF0DE526  ch_PP-OCRv3_det_infer.onnx
897A3EDEDB38FEE0DAE2C1CCEE38241F37DF202C9509E3ABCA02E9217C5EE615  ch_PP-OCRv3_rec_infer.onnx
E47ACEDF663230F8863FF1AB0E64DD2D82B838FCEB5957146DAB185A89D6215C  ch_ppocr_mobile_v2.0_cls_infer.onnx
28B2362AD4AB2DC38769AA72FEB535E3A9DDB3FD2A7585A05920E6393B1DC7F7  ppocr_keys_v1.txt
```
