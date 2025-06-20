
# ImageUtilityStereo

ImageUtilityStereo shows how to acquire image sets from the BumbleBee stereo camera, compute 3D information using ImageUtilityStereo functions, and display a depth image (created via CreateDepthImage) along with interactive measurements.

## Requirements 
1. For both C++ and Python versions

- Spinnaker (tested with Spinnaker 4.2.0.83)
- OpenCV (tested with OpenCV 4.11.0)

2. For Python only
- Python (tested with Python 3.10)
- PySpin (tested with Spinnaker 4.2.0.83 64bit)
- NumPy (tested with version 2.2.5)

## Installation

1. Install the Spinnaker SDK and (if using the Python version) the PySpin wrapper from the following link:  
   [Download the Spinnaker SDK](https://www.teledynevisionsolutions.com/support/support-center/software-firmware-downloads/iis/spinnaker-sdk-download/spinnaker-sdk--download-files/?pn=Spinnaker+SDK&vn=Spinnaker+SDK)

2. (c++ only) Modify the visual studio project file to include opencv
(This set of instructions assume that you have installed opencv at C:\opencv)

Step 1: Set Up Environment Variables (Optional but helpful)
- Open System Properties → Environment Variables.
- Add a new system variable "Name: OPENCV_DIR" (without quotes)
- Value: C:\opencv\build
- Add %OPENCV_DIR%\x64\vc16\bin to your system Path.

Step 2: Configure Visual Studio Project
- Right-click your project → Properties, and Under Configuration Properties → VC++ Directories:
	- Include Directories: C:\opencv\build\include
	- Library Directories: C:\opencv\build\x64\vc16\lib
-Under Linker → Input → Additional Dependencies:
	-opencv_world4110.lib (replace 4110 with the version of opencv you are using)
	-opencv_world4110d.lib (for Debug)

3. (Python only) install required  dependencies:
- pip install opencv-python
- pip install Numpy

## Usage

1. Ensure your BumbleBee X stereo camera is connected and powered before running the example.
2. The example will:
   - Detect connected cameras.
   - Continuously acquire images.
   - Display rectified left and disparity images.
   - Displays the pixel positioning under mouse cursor (x/y/z components), as well as the calculated straight distance from camera (specifically, the center of the lens on the left).
   - Press `s` to save the current images.
   - Press `Esc` to stop acquisition and exit.

## Troubleshooting

- If no cameras are detected, ensure the camera drivers are correctly installed and that the camera is powered and connected.
- If images cannot be saved, check write permissions in the current directory.

## License

This code is provided "as is" without warranty of any kind. Use at your own risk.

