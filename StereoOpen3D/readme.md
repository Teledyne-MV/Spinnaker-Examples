
# Stereo Camera Point Cloud computation and Visualization

This repository contains a Python script for acquiring stereo images from a Teledyne BumbleBee X camera using the PySpin SDK, computing a point cloud, and displaying the results.

## Features

- Acquire rectified and disparity images from stereo cameras using the PySpin SDK.
- Compute 3D point clouds using PySpin.
- Visualize 3D point clouds using Open3D.

## Requirements

- Python 3.8+
- PySpin SDK
- NumPy
- Open3D

## Installation

1. Install the PySpin SDK following FLIR's official documentation.
   [Download the Spinnaker SDK](https://www.teledynevisionsolutions.com/support/support-center/software-firmware-downloads/iis/spinnaker-sdk-download/spinnaker-sdk--download-files/?pn=Spinnaker+SDK&vn=Spinnaker+SDK)

2. Install Python dependencies:

   ```bash
   pip install -r requirements.txt
   ```

## Usage

1. Connect your FLIR stereo camera and ensure it is powered on.
2. Run the script:

   ```bash
   python3 StereoOpen3D.py
   ```

3. The script will:
   - Continuously acquire images and compute 3D point clouds.
   - Display the point clouds in an Open3D visualization window.
   
## Troubleshooting

- Ensure the camera is correctly connected and drivers are installed if no cameras are detected.
- Check directory permissions if images or point clouds cannot be saved.

## License

This project is provided "as is" without any warranty. Use at your own risk.

