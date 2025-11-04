
# Stereo Calibration Check

This repository provides a Python script for verifying stereo calibration quality on a Bumblebee X stereo camera.
The script measures vertical alignment (row offset) between rectified left and right image pairs using AKAZE feature matching and RANSAC.
It helps identify potential misalignment or calibration drift in stereo setup.

## Features

- Acquire rectified left/right images directly from the Bumblebee X stereo camera using the PySpin SDK.
- Compute average vertical row offset across 10 captured pairs.
- Report whether calibration is still valid based on offset threshold (±0.5 px).
- Optional visualization of feature matches for inspection.
- Also supports file mode for testing with saved left/right image pairs.

## Requirements

- Python 3.5+
- PySpin SDK
- OpenCV
- NumPy

## Installation


1. Install the PySpin SDK following FLIR's official documentation.  
   [Download the Spinnaker SDK](https://www.teledynevisionsolutions.com/support/support-center/software-firmware-downloads/iis/spinnaker-sdk-download/spinnaker-sdk--download-files/?pn=Spinnaker+SDK&vn=Spinnaker+SDK)

2. Install the other dependencies:

   ```bash
   pip install -r requirements.txt
   ```

---

## Usage

### A) **Camera Mode**
Runs live from the Bumblebee X stereo camera and averages offsets across 10 frames.

```bash
python3 StereoCalibrationCheck.py
```

Example output:
```
Getting average row offset across 10 frames
[1/10] offset=0.042315 (inliers=278)
[2/10] offset=0.037902 (inliers=291)
...
Average row offset across 10 valid pairs: 0.041112
Verdict: Camera appears well calibrated.
```

### B) **File Mode**
Compute row offset using saved rectified stereo images:
```bash
python3 StereoCalibrationCheck.py --left left_rect.png --right right_rect.png --show
```
Displays matched features and prints average offset.

---

## Interpretation

| **Average Row Offset (px)** | **Interpretation** |
|------------------------------|--------------------|
| ≤ 0.5 | ✅ Camera appears well calibrated |
| > 0.5 | ⚠️ Possible calibration drift or misalignment |

---

## Troubleshooting

- **“No cameras found.”**  
  Ensure the camera is connected, powered, and visible in **SpinView**.

- **“ModuleNotFoundError: PySpin”**  
  Install the correct Spinnaker Python wheel inside your virtual environment.

- **Low inlier count**  
  Ensure the scene has sufficient texture; flat or featureless areas won’t produce enough AKAZE matches.

---

## License
This code is provided “as is” without warranty of any kind. Use at your own risk.
