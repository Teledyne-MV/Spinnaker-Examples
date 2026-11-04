//=============================================================================
// Copyright (c) 2026 FLIR Integrated Imaging Solutions, Inc. All Rights Reserved.
//
// This software is the confidential and proprietary information of FLIR
// Integrated Imaging Solutions, Inc. ("Confidential Information"). You
// shall not disclose such Confidential Information and shall use it only in
// accordance with the terms of the license agreement you entered into
// with FLIR Integrated Imaging Solutions, Inc. (FLIR).
//
// FLIR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. FLIR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================

/**
 *  @example AcquisitionCCM.cpp
 *
 *  @brief AcquisitionCCM.cpp shows how to use host-side color correction for image
 *  post processing, as well as acquiring color correction parameters from the camera. 
 *
 *  This example demonstrates camera configurations to ensure no camera side color
 *  processing is done on the images captured. The acquired image is then color corrected
 *  through each ImageUtilityCCM::CreateColorCorrected overload:
 *  - Using CCMSettings built from the pre-defined sensor, type, color space, application
 *    and color temperature options.
 *  - Using CCMSettings carrying a custom CCM code produced by EncryptColorCorrectionMatrix.
 *  - Using ColorTransformationValues read back from the camera for the current color transformation selector state
 *
 *  Please leave us feedback at: https://www.surveymonkey.com/r/TDYMVAPI
 *  More source code examples at: https://github.com/Teledyne-MV/Spinnaker-Examples
 *  Need help? Check out our forum at: https://teledynevisionsolutions.zendesk.com/hc/en-us/community/topics
 */

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include "ImageUtilityCCM.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>
#include <vector>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

// Set UseExampleImageFile to true to load the example image from file and use it in place of an
// acquired image. A camera is still required so that the camera side color correction matrices can
// be read back through ColorTransformationValues. The image will be saved before
// color correction and after color correction for comparison purposes.
const bool UseExampleImageFile = false;
const string kExampleImageFileName = "Cloudy_6500k.raw";
// The microscopy application has calibrated matrices for every Pregius Gen2 and Gen3 sensor, but only
// for a limited set of color temperatures. 2765K, 3188K, 4649K, 4665K and 5034K are common to both.
const CCMColorTemperature kExampleColorTemperature = SPINNAKER_CCM_COLOR_TEMP_CLOUDY_6500K;
const CCMApplication kExampleApplication = SPINNAKER_CCM_APPLICATION_GENERIC;
const uint64_t kExampleImageWidth = 2448;
const uint64_t kExampleImageHeight = 2048;
const uint64_t kExampleImageOffsetX = 0;
const uint64_t kExampleImageOffsetY = 0;
const uint64_t kExampleImageSize = kExampleImageWidth * kExampleImageHeight * 3;
const PixelFormatEnums kExampleImagePixelFormat = PixelFormat_BGR8;

// The custom CCM scenario below demonstrates loading a matrix encrypted from known entries. The encrypted
// matrix represents the IMX250 sensor, CLOUDY_6500K temperature, and the Advanced 9x3 matrix.
const string kExample9x3Matrix = "1.7383,0.0868,0.3592,"
                                 "3.4637,-1.0023,3.0529,"
                                 "-1.8875,-3.4389,-1.6702,"
                                 "-0.5477,2.9908,-0.2124,"
                                 "2.4390,-0.6077,2.7653,"
                                 "-0.4504,-4.5349,-1.1504,"
                                 "0.1248,-0.7580,3.0707,"
                                 "2.2196,-1.1149,5.9171,"
                                 "-0.5653,-3.3670,-4.9294";

// Disables or enables heartbeat on GEV cameras so debugging does not incur timeout errors
int ConfigureGVCPHeartbeat(CameraPtr pCam, bool enableHeartbeat)
{
    //
    // Write to boolean node controlling the camera's heartbeat
    //
    // *** NOTES ***
    // This applies only to GEV cameras.
    //
    // GEV cameras have a heartbeat built in, but when debugging applications the
    // camera may time out due to its heartbeat. Disabling the heartbeat prevents
    // this timeout from occurring, enabling us to continue with any necessary
    // debugging.
    //
    // *** LATER ***
    // Make sure that the heartbeat is reset upon completion of the debugging.
    // If the application is terminated unexpectedly, the camera may not locked
    // to Spinnaker indefinitely due to the the timeout being disabled.  When that
    // happens, a camera power cycle will reset the heartbeat to its default setting.

    // Retrieve TL device nodemap
    INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

    // Retrieve GenICam nodemap
    INodeMap& nodeMap = pCam->GetNodeMap();

    CEnumerationPtr ptrDeviceType = nodeMapTLDevice.GetNode("DeviceType");
    if (!IsReadable(ptrDeviceType))
    {
        return -1;
    }

    if (ptrDeviceType->GetIntValue() != DeviceType_GigEVision)
    {
        return 0;
    }

    if (enableHeartbeat)
    {
        cout << endl << "Resetting heartbeat..." << endl << endl;
    }
    else
    {
        cout << endl << "Disabling heartbeat..." << endl << endl;
    }

    CBooleanPtr ptrDeviceHeartbeat = nodeMap.GetNode("GevGVCPHeartbeatDisable");
    if (!IsWritable(ptrDeviceHeartbeat))
    {
        cout << "Unable to configure heartbeat. Continuing with execution as this may be non-fatal..." << endl << endl;
    }
    else
    {
        ptrDeviceHeartbeat->SetValue(!enableHeartbeat);

        if (!enableHeartbeat)
        {
            cout << "WARNING: Heartbeat has been disabled for the rest of this example run." << endl;
            cout << "         Heartbeat will be reset upon the completion of this run.  If the " << endl;
            cout << "         example is aborted unexpectedly before the heartbeat is reset, the" << endl;
            cout << "         camera may need to be power cycled to reset the heartbeat." << endl << endl;
        }
        else
        {
            cout << "Heartbeat has been reset." << endl;
        }
    }

    return 0;
}

static void SaveImage(
    const ImagePtr& image,
    const std::string& deviceSerialNumber,
    const std::string& tag)
{
    std::ostringstream filename;
    filename << "AcquisitionCCM";
    if (!deviceSerialNumber.empty())
    {
        filename << "-" << deviceSerialNumber.c_str();
    }
    filename << "-" << tag << ".jpg";

    image->Save(filename.str().c_str());
    cout << "   Image Saved: " << filename.str() << endl;
}

static bool TrySetBooleanNode(INodeMap& nodeMap, const char* nodeName, bool value)
{
    CBooleanPtr ptr = nodeMap.GetNode(nodeName);
    if (IsAvailable(ptr) && IsReadable(ptr) && IsWritable(ptr))
    {
        ptr->SetValue(value);
        return true;
    }
    return false;
}

static bool TrySetEnumNode(INodeMap& nodeMap, const char* nodeName, const char* entryName)
{
    CEnumerationPtr ptrEnum = nodeMap.GetNode(nodeName);
    if (!IsAvailable(ptrEnum) || !IsReadable(ptrEnum) || !IsWritable(ptrEnum))
    {
        return false;
    }

    CEnumEntryPtr ptrEntry = ptrEnum->GetEntryByName(entryName);
    if (!IsAvailable(ptrEntry) || !IsReadable(ptrEntry))
    {
        return false;
    }

    ptrEnum->SetIntValue(ptrEntry->GetValue());
    return true;
}

// Loads the example image from disk. The returned image is a deep copy so that it remains valid
// after the local file buffer goes out of scope.
static ImagePtr LoadExampleImageFromFile()
{
    ImagePtr loadedImage;

    cout << "Reading image: " << kExampleImageFileName << endl;

    ifstream file(kExampleImageFileName, ios::binary | ios::in);
    if (!file)
    {
        cout << kExampleImageFileName << " not found in the working directory" << endl;
        return loadedImage;
    }

    vector<char> imageBuffer(static_cast<size_t>(kExampleImageSize));
    file.read(imageBuffer.data(), static_cast<streamsize>(kExampleImageSize));
    if (file.gcount() != static_cast<streamsize>(kExampleImageSize))
    {
        cout << "Failed to read full image from " << kExampleImageFileName << " (expected "
             << kExampleImageSize << " bytes, got " << file.gcount() << " bytes)" << endl;
        file.close();
        return loadedImage;
    }
    file.close();

    ImagePtr wrappedImage = Image::Create(
        kExampleImageWidth,
        kExampleImageHeight,
        kExampleImageOffsetX,
        kExampleImageOffsetY,
        kExampleImagePixelFormat,
        imageBuffer.data());

    return Image::Create(wrappedImage);
}

// Creates an empty destination image matching the source image geometry and pixel format.
static ImagePtr CreateDestinationImage(const ImagePtr& srcImage)
{
    ImagePtr destImage = Image::Create();
    destImage->ResetImage(
        srcImage->GetWidth(),
        srcImage->GetHeight(),
        srcImage->GetXOffset(),
        srcImage->GetYOffset(),
        srcImage->GetPixelFormat());

    return destImage;
}

static bool CanReadColorTransformationValueSelector(INodeMap& nodeMap)
{
    CEnumerationPtr colorTransformationValueSelector = nodeMap.GetNode("ColorTransformationValueSelector");
    return IsAvailable(colorTransformationValueSelector) && IsReadable(colorTransformationValueSelector) &&
           IsWritable(colorTransformationValueSelector);
}

static void EnableCameraColorTransformationPrerequisites(INodeMap& nodeMap)
{
    CBooleanPtr ptrColorTransformEnable = nodeMap.GetNode("ColorTransformationEnable");
    CBooleanPtr ptrIspEnable = nodeMap.GetNode("IspEnable");
    if (IsAvailable(ptrColorTransformEnable) && IsReadable(ptrColorTransformEnable) &&
        IsWritable(ptrColorTransformEnable))
    {
        ptrColorTransformEnable->SetValue(true);
    }

    if (IsAvailable(ptrIspEnable) && IsReadable(ptrIspEnable) && IsWritable(ptrIspEnable))
    {
        ptrIspEnable->SetValue(true);
    }
}

static void DisableCameraColorTransformation(INodeMap& nodeMap)
{
    CBooleanPtr ptrColorTransformEnable = nodeMap.GetNode("ColorTransformationEnable");
    CBooleanPtr ptrIspEnable = nodeMap.GetNode("IspEnable");
    if (IsAvailable(ptrColorTransformEnable) && IsReadable(ptrColorTransformEnable) &&
        IsWritable(ptrColorTransformEnable))
    {
        ptrColorTransformEnable->SetValue(false);
    }

    if (IsAvailable(ptrIspEnable) && IsReadable(ptrIspEnable) && IsWritable(ptrIspEnable))
    {
        ptrIspEnable->SetValue(false);
    }
}

// This function prints the CCM settings that produced a color corrected image. It is called only
// after the settings are known to be supported, so an unsupported combination is never reported.
void PrintCCMSettings(const CCMSettings& ccmSettings)
{
    cout << "  CCM settings:" << endl;

    if (ccmSettings.CustomCCMCode.empty())
    {
        cout << "    Application: " << ImageUtilityCCM::ApplicationToString(ccmSettings.Application) << endl;
        cout << "    Color temperature: "
             << ImageUtilityCCM::ColorTemperatureToString(ccmSettings.ColorTemperature)
             << endl;
        cout << "    Sensor: " << ImageUtilityCCM::SensorToString(ccmSettings.Sensor) << endl;
        cout << "    Type: " << ImageUtilityCCM::TypeToString(ccmSettings.Type) << endl;
        cout << "    Color space: " << ImageUtilityCCM::ColorSpaceToString(ccmSettings.ColorSpace) << endl;
    }
    else
    {
        cout << "    Custom CCM code: " << ccmSettings.CustomCCMCode.substr(0, 10) << "..." << endl;
        cout << "    ColorTemperature: " << ImageUtilityCCM::ColorTemperatureToString(ccmSettings.ColorTemperature) << endl;
    }
}

// This function prints the entries of a color transformation matrix and any offsets.
void PrintColorTransformationValues(const ColorTransformationValues& colorTransformationValues)
{
    const std::vector<double> gains = colorTransformationValues.GetGains();
    if (gains.size() == 9)
    {
        for (unsigned int row = 0; row < 3; row++)
        {
            cout << "    [ " << gains[row * 3] << ", " << gains[row * 3 + 1] << ", " << gains[row * 3 + 2]
                 << " ]" << endl;
        }
    }
    else
    {
        cout << "    [";
        for (size_t i = 0; i < gains.size(); i++)
        {
            cout << " " << gains[i];
        }
        cout << " ]" << endl;
    }

    const std::vector<double> offsets = colorTransformationValues.GetOffsets();
    if (!offsets.empty())
    {
        cout << "  Offsets:" << endl << "    [";
        for (size_t i = 0; i < offsets.size(); i++)
        {
            cout << " " << offsets[i];
        }
        cout << " ]" << endl;
    }
}

// This function acquires the source image, then applies and saves each color correction scenario.
int AcquireImages(CameraPtr pCam, INodeMap& nodeMap, INodeMap& nodeMapTLDevice)
{
    int result = 0;

    cout << endl << "*** IMAGE ACQUISITION ***" << endl;

    // Retrieve device serial number for filename
    string deviceSerialNumber;
    CStringPtr ptrStringSerial = nodeMapTLDevice.GetNode("DeviceSerialNumber");
    if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
    {
        deviceSerialNumber = string(ptrStringSerial->GetValue().c_str());

        cout << "  Device serial number: " << deviceSerialNumber << endl;
    }

    ImagePtr sourceImage;

    if (UseExampleImageFile)
    {
        sourceImage = LoadExampleImageFromFile();
        if (!sourceImage.IsValid())
        {
            return -1;
        }

        SaveImage(sourceImage, deviceSerialNumber, "Initial-Reference");
    }
    else
    {
        bool acquisitionStarted = false;
        ImagePtr pResultImage;
        CBooleanPtr ptrCaptureColorTransformation = nodeMap.GetNode("ColorTransformationEnable");
        CBooleanPtr ptrCaptureGamma = nodeMap.GetNode("GammaEnabled");
        const char* const autoNodeNames[] = {"BalanceWhiteAuto", "ExposureAuto", "GainAuto", "BlackLevelAuto"};
        CEnumerationPtr autoNodes[4];
        int64_t originalAutoValues[4] = {};
        bool autoValuesCaptured[4] = {};
        bool originalColorTransformation = false;
        bool originalGamma = false;
        const bool colorTransformationCaptured =
            IsAvailable(ptrCaptureColorTransformation) && IsReadable(ptrCaptureColorTransformation) &&
            IsWritable(ptrCaptureColorTransformation);
        const bool gammaCaptured =
            IsAvailable(ptrCaptureGamma) && IsReadable(ptrCaptureGamma) && IsWritable(ptrCaptureGamma);
        if (colorTransformationCaptured)
        {
            originalColorTransformation = ptrCaptureColorTransformation->GetValue();
        }
        if (gammaCaptured)
        {
            originalGamma = ptrCaptureGamma->GetValue();
        }
        for (unsigned int i = 0; i < 4; i++)
        {
            autoNodes[i] = nodeMap.GetNode(autoNodeNames[i]);
            if (IsAvailable(autoNodes[i]) && IsReadable(autoNodes[i]) && IsWritable(autoNodes[i]))
            {
                originalAutoValues[i] = autoNodes[i]->GetIntValue();
                autoValuesCaptured[i] = true;
            }
        }
        auto restoreCaptureSettings = [&]()
        {
            if (colorTransformationCaptured)
            {
                try
                {
                    ptrCaptureColorTransformation->SetValue(originalColorTransformation);
                }
                catch (...)
                {
                }
            }
            if (gammaCaptured)
            {
                try
                {
                    ptrCaptureGamma->SetValue(originalGamma);
                }
                catch (...)
                {
                }
            }
            for (unsigned int i = 0; i < 4; i++)
            {
                if (autoValuesCaptured[i])
                {
                    try
                    {
                        autoNodes[i]->SetIntValue(originalAutoValues[i]);
                    }
                    catch (...)
                    {
                    }
                }
            }
        };

        try
        {
            // Set acquisition mode to continuous

            // Retrieve enumeration node from nodemap
            CEnumerationPtr ptrAcquisitionMode = nodeMap.GetNode("AcquisitionMode");
            if (!IsReadable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
            {
                cout << "Unable to set acquisition mode to continuous (enum retrieval). Aborting..." << endl << endl;
                return -1;
            }

            // Retrieve entry node from enumeration node
            CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
            if (!IsReadable(ptrAcquisitionModeContinuous))
            {
                cout << "Unable to get or set acquisition mode to continuous (entry retrieval). Aborting..." << endl
                     << endl;
                return -1;
            }

            ptrAcquisitionMode->SetIntValue(ptrAcquisitionModeContinuous->GetValue());

            cout << "  Acquisition mode: Continuous" << endl;

            //
            // Disable camera side color processing
            //
            // *** NOTES ***
            //
            // When using host side color correction features, it is important to disable any camera side color
            // processing to ensure the best color accuracy, and as a nice side effect of faster image acquisition.
            //
            if (TrySetBooleanNode(nodeMap, "ColorTransformationEnable", false))
            {
                cout << "  Color transformation: disabled" << endl;
            }

            // Gamma correction should be disabled to ensure the intended color accuracy.
            if (TrySetBooleanNode(nodeMap, "GammaEnabled", false))
            {
                cout << "  Gamma: disabled" << endl;
            }

            //
            // Disable automatic camera adjustments
            //
            // *** NOTES ***
            // Auto white balance, exposure, gain and black level continuously alter the captured image,
            // which makes the effect of a color correction matrix hard to distinguish between the saved
            // images. Turning them off keeps the applied CCM the only variable between the outputs.
            //
            for (const char* autoNodeName : autoNodeNames)
            {
                if (TrySetEnumNode(nodeMap, autoNodeName, "Off"))
                {
                    cout << "  " << autoNodeName << ": Off" << endl;
                }
            }

            //
            // Create ImageProcessor instance for post processing images
            //
            // *** NOTES ***
            // By default, if no specific color processing algorithm is set, the image
            // processor will default to NEAREST_NEIGHBOR method.
            //
            ImageProcessor processor;
            processor.SetColorProcessing(SPINNAKER_COLOR_PROCESSING_ALGORITHM_HQ_LINEAR);

            // Acquire the reference image used by every color correction scenario below.
            pCam->BeginAcquisition();
            acquisitionStarted = true;

            cout << "  Capturing reference image..." << endl;

            pResultImage = pCam->GetNextImage(1000);

            // Ensure image completion
            if (pResultImage->IsIncomplete())
            {
                cout << "  Image incomplete: " << Image::GetImageStatusDescription(pResultImage->GetImageStatus())
                     << "..." << endl
                     << endl;
            }
            else
            {
                 cout << "  Captured image: " << pResultImage->GetWidth()
                     << ", height = " << pResultImage->GetHeight() << endl;

                // Convert image to BGR 8 and save it before color correction for comparison.
                sourceImage = processor.Convert(pResultImage, PixelFormat_BGR8);
                SaveImage(sourceImage, deviceSerialNumber, "Initial-Reference");
            }

            // Release image
            pResultImage->Release();
            pResultImage = nullptr;

            // End acquisition before performing host-side color correction.
            pCam->EndAcquisition();
            acquisitionStarted = false;
            restoreCaptureSettings();
        }
        catch (Spinnaker::Exception& e)
        {
            if (pResultImage.IsValid())
            {
                try
                {
                    pResultImage->Release();
                }
                catch (...)
                {
                }
            }
            if (acquisitionStarted)
            {
                try
                {
                    pCam->EndAcquisition();
                }
                catch (...)
                {
                }
            }
            restoreCaptureSettings();
            cout << "Error: " << e.what() << endl;
            return -1;
        }

        if (!sourceImage.IsValid())
        {
            return -1;
        }
    }

    cout << endl << "*** HOST SIDE CCM ***" << endl;

    CStringPtr sensorDescription = nodeMap.GetNode("SensorDescription");
    if (IsAvailable(sensorDescription) && IsReadable(sensorDescription))
    {
        cout << "  Current camera sensor: " << sensorDescription->GetValue() << endl;
    }
    else
    {
        cout << "  Current camera sensor: unavailable" << endl;
    }

    //
    // Using CCMSettings without CustomCCMCode
    //
    // *** NOTES ***
    // CreateColorCorrected is called with a CCMSettings describing which pre-defined color
    // correction matrix to apply. This overload allocates and returns the color corrected image,
    // whereas the overload used further below writes into a caller supplied destination image.
    //
    cout << endl << "  [1] Color Correction using Predefined CCMSettings parameters:" << endl;

    CCMSettings ccmSettings;

    // ColorTemperature selects the lighting condition and desired output color tone.
    // Sensor selects the calibration matrix; use the connected camera's reported sensor when possible.
    // Type selects the linear 3x3 matrix implementation.
    // ColorSpace controls whether the sRGB gamma transformation is applied.
    ccmSettings.Sensor = SPINNAKER_CCM_SENSOR_IMX250;
    ccmSettings.Type = SPINNAKER_CCM_TYPE_LINEAR;
    ccmSettings.ColorSpace = SPINNAKER_CCM_COLOR_SPACE_SRGB;
    ccmSettings.Application = kExampleApplication;
    ccmSettings.ColorTemperature = kExampleColorTemperature;

    ImagePtr colorCorrectedImage;

    try
    {
        PrintCCMSettings(ccmSettings);
        cout << " Creating color corrected image..." << endl;
        colorCorrectedImage = ImageUtilityCCM::CreateColorCorrected(sourceImage, ccmSettings);
        SaveImage(colorCorrectedImage, deviceSerialNumber, "Predefined-CCM");
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "  Error: " << e.what() << endl;
        cout << "  Update kExampleApplication and kExampleColorTemperature to a combination calibrated for this sensor."
             << endl;
        result = -1;
    }

    //
    // Using CCMSettings with CustomCCMCode
    //
    // *** NOTES ***
    // The destination image must match the source image dimensions and pixel format. The image
    // returned by the call above is reused here as the destination.
    //
    cout << endl << "  [2] Color Correction using custom encrypted CCM string" << endl;

    try
    {
        CCMSettings customCCMSettings;

        // CustomCCMCode overrides the predefined Application, Type and Sensor settings.
        // The encrypted matrix may contain either 3x3 linear or 9x3 advanced CCM values.
        customCCMSettings.CustomCCMCode =
            ImageUtilityCCM::EncryptColorCorrectionMatrix(kExample9x3Matrix);
        customCCMSettings.ColorTemperature = SPINNAKER_CCM_COLOR_TEMP_CLOUDY_6500K;

        // We reuse the previously allocated colorCorrectedImage as the destination image,
        // creating it only if it hasn't been allocated yet.
        if (!colorCorrectedImage.IsValid())
        {
            colorCorrectedImage = CreateDestinationImage(sourceImage);
        }
        PrintCCMSettings(customCCMSettings);

        // Example call using a previously allocated destination image. The source image is not modified.
        cout << " Creating color corrected image..." << endl;
        ImageUtilityCCM::CreateColorCorrected(sourceImage, colorCorrectedImage, customCCMSettings);

        SaveImage(colorCorrectedImage, deviceSerialNumber, "Custom-Encrypted-CCM");
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "  Error: " << e.what() << endl;
        result = -1;
    }

    //
    // Read and apply the color transformation values currently selected on the camera
    //
    // *** NOTES ***
    // Cameras that support on-device Color Transformation will only report correct ColorTransformationValueSelector values while
    // ColorTransformationEnable and IspEnable are enabled. The values are then read from the ColorTransformationValueSelector
    // and ColorTransformationValue nodes.

    cout << endl << "  [3] Color Correction using Color Transformation values from camera" << endl;

    try
    {
        const bool canReadColorTransformationValueSelector = CanReadColorTransformationValueSelector(nodeMap);
        if (!canReadColorTransformationValueSelector)
        {
            // Enable ColorTransformationValueSelector by enabling IspEnable and ColorTransformationEnable
            EnableCameraColorTransformationPrerequisites(nodeMap);
        }

        ImagePtr cameraCorrectedImage;

        // Fetch the color transformation values currently set on the camera.
        ColorTransformationValues cameraColorTransformationValues(pCam);

        cout << " ColorTransformation values from camera:" << endl;
        PrintColorTransformationValues(cameraColorTransformationValues);

        // Disable camera-side processing to ensure that if additional images are grabbed, host side color correction will be applied
        DisableCameraColorTransformation(nodeMap);

        cout << " Creating color corrected image..." << endl;
        cameraCorrectedImage = ImageUtilityCCM::CreateColorCorrected(sourceImage, cameraColorTransformationValues, ccmSettings.ColorSpace);
        SaveImage(cameraCorrectedImage, deviceSerialNumber, "ColorTransformationValues-From-Camera");
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "  Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}


// This function prints the device information of the camera from the transport
// layer; please see NodeMapInfo example for more in-depth comments on printing
// device information from the nodemap.
int PrintDeviceInfo(INodeMap& nodeMap)
{
    int result = 0;
    cout << endl << "*** DEVICE INFORMATION ***" << endl;

    try
    {
        FeatureList_t features;
        CCategoryPtr category = nodeMap.GetNode("DeviceInformation");
        if (IsReadable(category))
        {
            category->GetFeatures(features);

            FeatureList_t::const_iterator it;
            for (it = features.begin(); it != features.end(); ++it)
            {
                try
                {
                    CNodePtr pfeatureNode = *it;
                    cout << "  " << pfeatureNode->GetName() << " : ";
                    CValuePtr pValue = (CValuePtr)pfeatureNode;
                    cout << (IsReadable(pValue) ? std::string(pValue->ToString().c_str()) : std::string("Node not readable"));
                    cout << endl;
                }
                catch (Spinnaker::Exception&)
                {
                    cout << "Node not readable" << endl;
                }
            }
        }
        else
        {
            cout << "Device control information not readable." << endl;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

// This function acts as the body of the example; please see NodeMapInfo example
// for more in-depth comments on setting up cameras.
int RunSingleCamera(CameraPtr pCam)
{
    int result = 0;
    bool cameraInitialized = false;

    try
    {
        // Retrieve TL device nodemap and print device information
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();
        result = PrintDeviceInfo(nodeMapTLDevice);

        // Initialize camera
        pCam->Init();
        cameraInitialized = true;

        // Retrieve GenICam nodemap
        INodeMap& nodeMap = pCam->GetNodeMap();

        // Configure heartbeat for GEV camera
#ifdef _DEBUG
        result = result | ConfigureGVCPHeartbeat(pCam, false);
#else
        result = result | ConfigureGVCPHeartbeat(pCam, true);
#endif

        // Acquire images
        result = result | AcquireImages(pCam, nodeMap, nodeMapTLDevice);

#ifdef _DEBUG
        // Reset heartbeat for GEV camera
        result = result | ConfigureGVCPHeartbeat(pCam, true);
#endif

        // Deinitialize camera
        pCam->DeInit();
    }
    catch (Spinnaker::Exception& e)
    {
        if (cameraInitialized)
        {
#ifdef _DEBUG
            try
            {
                ConfigureGVCPHeartbeat(pCam, true);
            }
            catch (...)
            {
            }
#endif
            try
            {
                pCam->DeInit();
            }
            catch (...)
            {
            }
        }
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

// Example entry point; please see Enumeration example for more in-depth
// comments on preparing and cleaning up the system.
int main(int /*argc*/, char** /*argv*/)
{
    // Since this application saves images in the current folder
    // we must ensure that we have permission to write to this folder.
    // If we do not have permission, fail right away.

    FILE* tempFile = fopen("test.txt", "w+");
    if (tempFile == nullptr)
    {
        cout << "Failed to create file in current folder.  Please check "
                "permissions."
             << endl;
        cout << "Press Enter to exit..." << endl;
        getchar();
        return -1;
    }
    fclose(tempFile);
    remove("test.txt");

    cout << endl << "*** SYSTEM INFORMATION ***" << endl;

    // Print application build information
    cout << "  Application build date: " << __DATE__ << " " << __TIME__ << endl;

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    // Print out current library version
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "  Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
         << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << endl;

    // Dynamically set the number of threads to perform CCM.
    ImageUtility::SetNumThreads(std::thread::hardware_concurrency() / 2);
    unsigned int numThreads = ImageUtility::GetNumThreads();
    cout << "  Number of threads set for CCM: " << numThreads << " of " << std::thread::hardware_concurrency() << endl;

    int result = 0;

    // Retrieve list of cameras from the system
    CameraList camList = system->GetCameras();

    const unsigned int numCameras = camList.GetSize();

    cout << "  Number of cameras detected: " << numCameras << endl;

    // Finish if there are no cameras
    if (numCameras == 0)
    {
        // Clear camera list before releasing system
        camList.Clear();

        // Release system
        system->ReleaseInstance();

        cout << "Not enough cameras!" << endl;
        cout << "Done! Press Enter to exit..." << endl;
        getchar();

        return -1;
    }

    // Create shared pointer to camera
    CameraPtr pCam = nullptr;

    // Run example on each camera
    for (unsigned int i = 0; i < numCameras; i++)
    {
        // Select camera
        pCam = camList.GetByIndex(i);

        cout << endl << "Running Camera " << i << "..." << endl;

        // Run example
        result = result | RunSingleCamera(pCam);

        cout << endl << "*** Camera " << i << " Complete ***" << endl << endl;
    }

    // Release reference to the camera
    pCam = nullptr;

    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "*** COMPLETE ***" << endl;
    cout << "Press Enter to exit..." << endl;
    getchar();

    return result;
}