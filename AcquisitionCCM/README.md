# AcquisitionCCM

## Overview 

AcquisitionCCM.cpp shows how to use host-side color correction for image post processing, as well as acquiring color correction parameters from the camera. This example demonstrates camera configurations to ensure no camera side color processing is done on the images captured. The acquired image is then color correctedthrough each ImageUtilityCCM::CreateColorCorrected overload:
* Using CCMSettings built from the pre-defined sensor, type, color space, application and color temperature options.
* Using CCMSettings carrying a custom CCM code produced by EncryptColorCorrectionMatrix.
* Using ColorTransformationValues read back from the camera for the current color transformation selector state


## How to Run the Application

To successfully run this example, please double-check the following:
* Add raw image file "Cloudy_6500k.raw" to the folder location of the compiled executable of the example.
