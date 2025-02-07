# Overview

This repository is made up of additional examples to the ones provided in the Spinnaker SDK, meant to be used with Teledyne FLIR Machine Vision Cameras.  Users are expected to be fairly familiar with the Spinnaker SDK and API before using the examples from this list.

**Please Note:**
If you have any technical questions regarding these examples, please feel free to add a comment, or go to flir.custhelp.com and select “Ask a question”.
 
## How to run examples (Windows C++)
* Install Spinnaker SDK.  
* Navigate to the src folder of the Spinnaker directory (default location; C:\Program Files\FLIR Systems\Spinnaker\src), then to the Acquisition Folder
* Rename the Acquisition cpp file (for example, "AcquisitionOriginal")
* Using the cpp file for the example you want to run from the repository, rename the file to "Acquisition" and place into the Acquisition Folder
* Compile the example in Visual Studio as normal

## How to run examples (Windows C#)
* Follow the same steps as shown above for Windows C++, except replace the cs file rather than cpp file

## How to run examples (Windows Python)
* Install Spinnaker SDK.  
* Install Spinnaker Python Wrapper
* Navigate to the examples folder of the Spinnaker Python directory (default location for cp35; C:\Users\USERNAME\AppData\Local\Programs\Python\Python35\PySpin\Examples)
* Add the py file for the example you want to run from the repository
* Run the example in command prompt (see readme that comes included with PySpin for further installation/setup instructions)

## How to run examples (Linux C++/Python)
* Follow the steps mentioned in the instructions above, navigating to the location you installed Spinnaker or the Spinnaker Python wrapper, in Ubuntu

## List of Examples

### AcquisitionCCM (C++)
* This example shows how to use host side color correction for image post processing. Images captured can be processed using the Spinnaker color correction features to improve color accuracy. This example relies on information provided in the Acquisition example.

### AcquisitionOpenCV (C++, C#)
* This example is a modification of the Spinnaker SDK Example "Acquisition" which demonstrates how to convert a Spinnaker Image to an OpenCV Mat object and display the result using OpenCV HighGUI (Tested with OpenCV 4.1.0)

### CameraTimeToPCTime (C++, C#, Python)
* This example converts camera's image timestamp to PC system time and saves 10 images, using the PC System time as a part of the file name for each image file.

### DeviceReset (C++)
* This example shows how to reset cameras for a system with one or more cameras.

### FileAccessUserset (C++, C#, Python)
* This example sets the exposure to a desired value, then save the custom settings to the camera's "UserSet0" or "UserSet1" userset, then saving those custom settings to the current folder.  The example can also be used to upload the saved file's settings back to the camera.

### Logic Block and Counter Examples: AlternatingStrobe (C++, C#)
* Shows how to use counters and logic blocks in order to apply a custom strob edelay to every second frame

### Logic Block and Counter Examples: BurstStrobeThenTrigger (C++, C#)
* This example uses counters and logic blocks order to achive a burst trigger with the strobe preceding exposure.

### Logic Block and Counter Examples: EnableAndHardwareTrigger (C++)
* A logic block example in which the criteria for triggering is Hardware Trigger Input Signal (Rising edge, Line0) AND Enable Input Signal (Level High, Line2).

### Logic Block and Counter Examples: ExposureStartToAcquisitionEnd (C++)
* This examples uses logic blocks to Set the output Strobe High when the start of exposure occurs for the first image captured, and sets it to low upon Acquisition End

### Logic Block and Counter Examples: ExtendedTriggerDelay (C++)
* Shows how to use Counters and Logic Blocks in order to delay exposure by up to 2^32 microseconds after trigger, much longer than the 65535 limit for the TriggerDelay node.

### Logic Block and Counter Examples: StrobeBeforeExposure (C++)
* This example shows how to output a strobe signal before beginning exposure.

### RawToProcessed (C++, C#)
* This example shows how to convert saved raw image files to spinnaker image objects, then save them in a processed image file format, as saved.  

### SaveToUserSet (C++, C#, Python)
* shows how to save custom settings to User Set. By default, it modifies the exposure time to 2000 microseconds, then saves this change to User Set 1.  It can also be configured to reset the camera to factory default settings.

### ShadingCorrection (C++)
* This example creates and saves a calibration file which is required for camera shading correction.  Options are provided to deploy a calibration file (if already created) from the disk to the camera, or download and save the file from the camera to disk.

### SpinPictureBox (C#)
* shows how to provide a simple GUI that shows a live stream of the camera feed.

### StereoOpenCV (Python)
* shows how to acquire stereo images from a Teledyne BumbleBee X camera using the PySpin SDK, process rectified and disparity images, and display and save the results.

### Synchronized (C++)
* shows how to setup multiple FLIR Machine Vision cameras in a primary/secondary configuration, synchronizing image capture.  It relies on users to have followed the hardware layout defined on the FLIR IIS article, "Configuring Synchronized Capture with Multiple Cameras".

### TimeSync (C++)
* This example is a simplified version of the "actioncommand" example, where the application will synchronize the camera's clock with each other, but does not do action commands.



