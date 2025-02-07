
# Stereo Camera Image Acquisition and Processing

This repository contains a Python script for acquiring stereo images from a Teledyne BumbleBee X camera using the PySpin SDK, processing rectified and disparity images, and displaying and saving the results.

## Features

- Acquire images from the BumbleBee X stereo cameras using the PySpin SDK.
- Retrieve and print stereo camera parameters.
- Display rectified left and disparity images in real-time.
- Save acquired images by pressing the `s` key.
- Exit the acquisition loop by pressing the `Esc` key.

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

## Usage

1. Ensure your BumbleBee X stereo camera is connected and powered.
2. Run the script:

   ```bash
   python3 StereoOpenCV.py
   ```

3. The script will:
   - Detect connected cameras.
   - Continuously acquire images.
   - Display rectified left and disparity images.
   - Press `s` to save the current images.
   - Press `Esc` to stop acquisition and exit.

## Troubleshooting

- If no cameras are detected, ensure the camera drivers are correctly installed and that the camera is powered and connected.
- If images cannot be saved, check write permissions in the current directory.

## License

This code is provided "as is" without warranty of any kind. Use at your own risk.

