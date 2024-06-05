# AcquisitionCCM

## Overview 

AcquisitionCCM.cpp shows how to use host side color correction for image post processing. Images captured can be processed using the Spinnaker color correction features to improve color accuracy. This example relies on information provided in the Acquisition example.This example demonstrates camera configurations to ensure no camera side color processing is done on the images captured. Then, the example shows how to select the color correction options through the CCMSettings struct, and how to perform color correction on an image through the ImageUtilityCCM class.


## How to Run the Application

To successfully run this example, please double-check the following:
* Add raw image file "Cloudy_6500k.raw" to the folder location of the compiled executable of the example;
