# AcquisitionOpenCV

## Overview
 
This example is a modification of the Spinnaker SDK Acquisition Example, that demonstrates how to convert 
a Spinnaker Image to an OpenCV Mat object and display the result using OpenCV HighGUI; tested with OpenCV 4.1.0.

## How to Configure:

* Download OpenCV 4.1.0 from https://opencv.org/releases/ and extract to C:/
* In Visual Studio, Go to Project > Properties
* Ensure that the Property Page shows "Configuration: Debug", and "Platform: x64"
* Ensure that in the Property Page -> Configuration Properties > C/C++ > General > Additional Include Directories, Add "C:\opencv\build\include"; click apply
* In the Property Page, Configuration Properties > Linker > General > Additional Library Directories, add "C:\opencv\build\x64\vc14\lib"; click apply
* In the Property Page, Configuration Properties > Linker > Input > Additional Dependencies, add "opencv_world410d.lib"; click OK

The example code is tested with Emgu.CV.runtime.windows version 4.4.0.4061 target framework .net 4.6.1 and spinnaker 




