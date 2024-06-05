# Shading Correction

## Overview 

This example creates and saves a calibration file which is required for camera shading correction.  Options are provided to deploy a calibration file (if already created) from the disk to the camera, or download and save the file from the camera to disk. It also provides debug messages when debug mode is turned on, giving more detailed progress status and error messages to the user. Further details on Shading Correction can be found in the article, "Using Lens Shading Correction"; https://www.flir.ca/support-center/iis/machine-vision/application-note/using-lens-shading-correction/.


## How to Run the Application:

Build and run the executable file.  Interact with the program with specified options:
	
* -c : Create and deploy calibration file to camera
* -d : Download calibration file from camera to system disk (with given file name)
* -u : Upload calibration file (with given file name) to camera
* -a : Acquire images using on camera calibration file after all operations are complete
* -f string: location of file to download/upload

Example:
        To upload existing "CalibrationFile" file from current working directory: ShadingCorrection.exe -u -v -f CalibrationFile

## Applicable Products
Shading Correction is a feature only available for certain camera models; to see the full list of models, see our article, "Using Lens Shadding Correction";
https://www.flir.ca/support-center/iis/machine-vision/application-note/using-lens-shading-correction/

