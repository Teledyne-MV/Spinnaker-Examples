//=============================================================================
// Copyright (c) 2024 FLIR Integrated Imaging Solutions, Inc. All Rights Reserved.
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
 *  @brief AcquisitionCCM.cpp shows how to use host side color correction for image
 *  post processing. Images captured can be processed using the Spinnaker color
 *  correction features to improve color accuracy. This example relies on information
 *  provided in the Acquisition example.
 *
 *  This example demonstrates camera configurations to ensure no camera side color
 *  processing is done on the images captured. Then, the example shows how to select
 *  the color correction options through the CCMSettings struct, and how to perform
 *  color correction on an image through the ImageUtilityCCM class.
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
#include <regex>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

// Set UseExampleImageFile to true to load the example image from file and apply color correction
// on the image. This will save the example image before color correction and after color
// correction for comparison purposes.
const bool UseExampleImageFile = false;
const string kExampleImageFileName = "Cloudy_6500k.raw";
const CCMColorTemperature kExampleColorTemperature = SPINNAKER_CCM_COLOR_TEMP_CLOUDY_6500K;
const CCMApplication kExampleApplication = SPINNAKER_CCM_APPLICATION_GENERIC;
const uint64_t kExampleImageWidth = 2448;
const uint64_t kExampleImageHeight = 2048;
const uint64_t kExampleImageOffsetX = 0;
const uint64_t kExampleImageOffsetY = 0;
const uint64_t kExampleImageSize = kExampleImageWidth * kExampleImageHeight * 3;
const PixelFormatEnums kExampleImagePixelFormat = PixelFormat_BGR8;

// Set UseExampleCCMCode to true to use the example custom CCM code below instead of going with the
// pre-defined color correction matrix settings provided in the CCMSettings. This is intended to demonstrate
// how to load a custom color correction matrix provided by FLIR moving forward. The specific custom code provided
// here represents the same pre-defined color correction settings for the IMX250 sensor, CLOUDY_6500K temperature
// and the POLYNOMIAL_9X3 matrix.
const bool UseExampleCCMCode = false;
const string kExampleCCMCode = "a3dc9a59f6c7e43ef86ed3c410f6374633d3fbeac484c65e008be07b436d9c053b76408a1f4a39c"
"e81c75d3377017f33b815dc2666f431c5f776c8ea03a30ad1728dcee90cd5fe55020611a1e1153fbaec9c2110752a4df72b48c8afc151b"
"1a28a15b25ec38dccd9e336b8f0e53bf486ba024734ea74eb8e5539ff3e738ff4b19370f5958b6529f9751359e6ef1da1f9e34f75d20ac"
"1dcb4ed864d573ee03d1af0a326533d871ad73821bdd6b378dc7f88d848a8598e1d0a38e05289b42b226ab6ac86d707e0dbee3f552c";


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

int ResetGVCPHeartbeat(CameraPtr pCam)
{
    return ConfigureGVCPHeartbeat(pCam, true);
}

int DisableGVCPHeartbeat(CameraPtr pCam)
{
    return ConfigureGVCPHeartbeat(pCam, false);
}

string ColorTemperatureToFileNameSuffix(CCMColorTemperature temperature)
{
    std::regex pattern("_[A-Za-z0-9]+$"); // matches alphanumeric characters after underscore at the end of the string
    std::smatch match;

    std::string temperatureString = ImageUtilityCCM::ColorTemperatureToString(temperature);

    if (std::regex_search(temperatureString, match, pattern))
    {
        std::string result = match.str();
        result.erase(result.begin()); // remove the underscore from the beginning
        return "-" + result;
    }
    else
    {
        return "-Unknown" ;
    }
}

int ConstructCCMSettings(CCMSettings& ccmSettings, string& fileNameSuffix)
{
    //
    // Construct CCMSettings
    //
    // *** NOTES ***
    //
    // ColorTemperature: Users may want to choose a different color temperature depending on the lighting
    // condition and the desired output color tone. The default is: SPINNAKER_CCM_COLOR_TEMP_LED_4649K.
    //
    // Sensor: Color correction can still be applied to images not captured by the selected sensor,
    // however, the color accuracy may be undesireable in such situations. Please refer to the CCMSensor
    // enum for a list of supported sensors.
    //
    // Type: Users may select either the Linear 3x3 color correction matrices or the Polynomial 9x3
    // color correction matrices. For the microscopy application, the Linear 3x3 CCMs offer better
    // performance and accuracy.
    //
    // CustomCCMCode: The custom CCM code should not be set when choosing pre-defined CCM settings since the
    // pre-defined settings are ignored when a CustomCCMCode is specified, aside from the color temperature.
    //
    if (!UseExampleCCMCode)
    {
        ccmSettings.Sensor = SPINNAKER_CCM_SENSOR_IMX250;
        ccmSettings.Type = SPINNAKER_CCM_TYPE_LINEAR;
        ccmSettings.ColorSpace = SPINNAKER_CCM_COLOR_SPACE_SRGB;
        fileNameSuffix = ColorTemperatureToFileNameSuffix(ccmSettings.ColorTemperature);

        if (!UseExampleImageFile)
        {
            ccmSettings.Application = SPINNAKER_CCM_APPLICATION_MICROSCOPY;
            ccmSettings.ColorTemperature = SPINNAKER_CCM_COLOR_TEMP_LED_4649K;
        }
        else
        {
            ccmSettings.Application = kExampleApplication;
            ccmSettings.ColorTemperature = kExampleColorTemperature;
        }
    }
    else
    {
        ccmSettings.CustomCCMCode = kExampleCCMCode;
        fileNameSuffix = "-CustomCCM";
    }

    return 0;
}

// This function compares the device sensor type to the sensor type specified by the user in the CCM settings. 
int CheckCCMSensorSetting(CCMSettings& ccmSettings, INodeMap& nodeMap)
{
    std::regex pattern("IMX\\d{3}");
    std::smatch match;

    std::string deviceSensor;
    CStringPtr sensorDesc = nodeMap.GetNode("SensorDescription");
    if (IsAvailable(sensorDesc) && IsReadable(sensorDesc))
    {
        deviceSensor = sensorDesc->GetValue().c_str();
    }
    else
    {
        cout << "Failed to get device sensor information." << endl;
        cout << "Cannot verify that CCM sensor settings match device sensor type." << endl;
        return -1;
    }

    std::string deviceResult;
    if (std::regex_search(deviceSensor, match, pattern))
    {
        deviceResult = match.str();
    }
    else
    {
        cout << "Device sensor information does not match expected format." << endl;
        cout << "Cannot verify that CCM sensor settings match device sensor type." << endl;
        return -1;
    }

    std::string settingsResult;
    std::string settingsSensor = ImageUtilityCCM::SensorToString(ccmSettings.Sensor);
    if (std::regex_search(settingsSensor, match, pattern))
    {
        settingsResult = match.str();
    }
    else
    {
        cout << "CCM settings sensor information does not match expected format." << endl;
        cout << "Cannot verify that CCM sensor settings match device sensor type." << endl;
        return -1;
    }

    if (deviceResult != settingsResult)
    {
        cout << endl << "WARNING: CCM sensor setting does not match device sensor!" << endl << endl;
        cout << "   Device sensor type: " << deviceResult << " (" << deviceSensor << ")" << endl;
        cout << "   CCM sensor setting: " << settingsResult << " (" << settingsSensor << ")" << endl;
        cout << "   Better color accuracy can be achieved by using the correct CCM settings." << endl;
    }

    return 0;
}

// This function prints the CCM settings information, as constructed in the ConstructCCMSettings function.
int PrintCCMSettings(CCMSettings& ccmSettings, string& fileNameSuffix)
{
    cout << endl << "*** CCM SETTINGS ***" << endl << endl;

    cout << "Application: " << ImageUtilityCCM::ApplicationToString(ccmSettings.Application) << endl;
    cout << "Color Temperature: " << ImageUtilityCCM::ColorTemperatureToString(ccmSettings.ColorTemperature) << endl;
    cout << "Sensor: " << ImageUtilityCCM::SensorToString(ccmSettings.Sensor) << endl;
    cout << "Type: " << ImageUtilityCCM::TypeToString(ccmSettings.Type) << endl;
    cout << "Color Space: " << ImageUtilityCCM::ColorSpaceToString(ccmSettings.ColorSpace) << endl;
    cout << "Custom CCM Code: " << (ccmSettings.CustomCCMCode) << endl;
    cout << "File suffix: " << fileNameSuffix << endl;

    return 0;
}

int RunColorCorrectionOnExampleImage()
{
    int result = 0;

    //
    // Construct CCMSettings
    //
    // *** NOTES ***
    //
    // See the ConstructCCMSettings function for detailed notes about each color correction setting.
    //
    CCMSettings ccmSettings;
    string fileNameSuffix;

    ConstructCCMSettings(ccmSettings, fileNameSuffix);
    PrintCCMSettings(ccmSettings, fileNameSuffix);

    std::shared_ptr<char> imageBuffer = std::shared_ptr<char>(new char[kExampleImageSize]);

    cout << endl << endl << "*** EXAMPLE IMAGE - COLOR CORRECTION ***" << endl << endl;

    try
    {
        ImagePtr sourceImage;
        cout << "Reading image: " << kExampleImageFileName << endl;
        ifstream file(kExampleImageFileName, ios::binary | ios::in);
        if (file)
        {
            // Read data as a block:
            file.read(imageBuffer.get(), kExampleImageSize);
            file.close();

            // Create a Spinnaker Image object from a raw data buffer with known image parameters
            cout << "Creating image source image" << endl;
            sourceImage = Image::Create(
                kExampleImageWidth,
                kExampleImageHeight,
                kExampleImageOffsetX,
                kExampleImageOffsetY,
                kExampleImagePixelFormat,
                imageBuffer.get());
        }
        else
        {
            cout << kExampleImageFileName << " not found in the working directory" << endl;
            return -1;
        }

        // Save the image before color correction to be compared later.
        const string beforeImageFileName = "AcquisitionCCM-Example-Before.jpg";
        sourceImage->Save(beforeImageFileName.c_str());
        cout << "Before image saved at " << beforeImageFileName << endl;

        ImagePtr colorCorrectedImage = ImageUtilityCCM::CreateColorCorrected(sourceImage, ccmSettings);

        // Create a unique filename
        ostringstream filename;
        filename << "AcquisitionCCM-Example" << fileNameSuffix << ".jpg";

        // Save image
        colorCorrectedImage->Save(filename.str().c_str());
        cout << "Image saved at " << filename.str() << endl;
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

// This function acquires, performs color correction on and saves 10 images from a device.
int AcquireImages(CameraPtr pCam, INodeMap& nodeMap, INodeMap& nodeMapTLDevice)
{
    int result = 0;
    
    //
    // Construct CCMSettings
    //
    // *** NOTES ***
    //
    // See the ConstructCCMSettings function for detailed notes about each color correction setting.
    //
    CCMSettings ccmSettings;
    string fileNameSuffix;

    ConstructCCMSettings(ccmSettings, fileNameSuffix);
    PrintCCMSettings(ccmSettings, fileNameSuffix);
    CheckCCMSensorSetting(ccmSettings, nodeMap);

    cout << endl << endl << "*** IMAGE ACQUISITION ***" << endl << endl;

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

        // Retrieve integer value from entry node
        const int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();

        // Set integer value from entry node as new value of enumeration node
        ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

        cout << "Acquisition mode set to continuous..." << endl;

        //
        // Disable camera side color processing
        //
        // *** NOTES ***
        //
        // When using host side color correction features, it is important to disable any camera side color processing
        // to ensure the best color accuracy, and as a nice side effect of faster image acquisition.
        //
        CBooleanPtr ptrColorTransformationEnable = nodeMap.GetNode("ColorTransformationEnable");
        if (IsAvailable(ptrColorTransformationEnable) && IsWritable(ptrColorTransformationEnable))
        {
            ptrColorTransformationEnable->SetValue(false);
            cout << "ColorTransformationEnable set to false..." << endl;
        }

        //
        // Disable  gamma correction
        //
        // *** NOTES ***
        // Gamma correction should be disabled, to ensure the intended color accuracy.
        CBooleanPtr ptrGammaEnable = nodeMap.GetNode("GammaEnabled");
        if (IsAvailable(ptrGammaEnable) && IsWritable(ptrGammaEnable))
        {
            ptrGammaEnable->SetValue(false);
            cout << "GammaEnabled set to false..." << endl;
        }

        //
        // Configure camera white balance
        //
        // *** NOTES ***
        // White balance needs to be set properly according to the scene and light source color temperature.
        // This can be done by either of the following:
        // - Setting the Balance White Auto to continuous (demonstrated below)
        // - Manually by setting the red and blue balance ratio through BalanceRatioSelector and BalanceRatio nodes
        // - Manually by setting the RGB Transform light source to the appropriate light source color temperature
        //   through RgbTransformLightSource node
        CEnumerationPtr ptrBalanceWhiteAuto = nodeMap.GetNode("BalanceWhiteAuto");
        if (IsAvailable(ptrBalanceWhiteAuto) && IsWritable(ptrBalanceWhiteAuto))
        {
            CEnumEntryPtr ptrBalanceWhiteAutoContinuous = ptrBalanceWhiteAuto->GetEntryByName("Continuous");
            if (IsAvailable(ptrBalanceWhiteAutoContinuous) && IsReadable(ptrBalanceWhiteAutoContinuous))
            {
                ptrBalanceWhiteAuto->SetIntValue(ptrBalanceWhiteAutoContinuous->GetValue());
                cout << "White Balance Auto set to continuous..." << endl;
            }
        }

        // Begin acquiring images
        pCam->BeginAcquisition();

        cout << "Acquiring images..." << endl;

        // Retrieve device serial number for filename
        gcstring deviceSerialNumber("");
        CStringPtr ptrStringSerial = nodeMapTLDevice.GetNode("DeviceSerialNumber");
        if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
        {
            deviceSerialNumber = ptrStringSerial->GetValue();

            cout << "Device serial number retrieved as " << deviceSerialNumber << "..." << endl;
        }
        cout << endl;

        // Retrieve, convert, and save images
        const unsigned int k_numImages = 10;

        //
        // Create ImageProcessor instance for post processing images
        //
        ImageProcessor processor;

        //
        // Set default image processor color processing method
        //
        // *** NOTES ***
        // By default, if no specific color processing algorithm is set, the image
        // processor will default to NEAREST_NEIGHBOR method.
        //
        processor.SetColorProcessing(SPINNAKER_COLOR_PROCESSING_ALGORITHM_HQ_LINEAR);

        for (unsigned int imageCnt = 0; imageCnt < k_numImages; imageCnt++)
        {
            try
            {
                // Retrieve next received image
                ImagePtr pResultImage = pCam->GetNextImage(1000);

                // Ensure image completion
                if (pResultImage->IsIncomplete())
                {
                    // Retrieve and print the image status description
                    cout << "Image incomplete: " << Image::GetImageStatusDescription(pResultImage->GetImageStatus())
                        << "..." << endl
                        << endl;
                }
                else
                {
                    // Print image information; height and width recorded in pixels

                    const size_t width = pResultImage->GetWidth();

                    const size_t height = pResultImage->GetHeight();

                    cout << "Grabbed image " << imageCnt << ", width = " << width << ", height = " << height << endl;

                    // Convert image to BGR 8
                    ImagePtr convertedImage = processor.Convert(pResultImage, PixelFormat_BGR8);

                    // Create a unique filename
                    ostringstream filenameBefore;

                    filenameBefore << "AcquisitionCCM-";
                    if (!deviceSerialNumber.empty())
                    {
                        filenameBefore << deviceSerialNumber.c_str() << "-";
                    }
                    filenameBefore << imageCnt << "-Before.jpg";

                    // Save image
                    convertedImage->Save(filenameBefore.str().c_str());

                    cout << "Image saved at " << filenameBefore.str() << endl;

                    ImagePtr colorCorrectedImage = ImageUtilityCCM::CreateColorCorrected(
                        convertedImage,
                        ccmSettings);

                    // Create a unique filename
                    ostringstream filename;

                    filename << "AcquisitionCCM-";
                    if (!deviceSerialNumber.empty())
                    {
                        filename << deviceSerialNumber.c_str() << "-";
                    }
                    filename << imageCnt << fileNameSuffix << ".jpg";

                    // Save image
                    colorCorrectedImage->Save(filename.str().c_str());

                    cout << "Image saved at " << filename.str() << endl;
                }

                // Release image
                pResultImage->Release();

                cout << endl;
            }
            catch (Spinnaker::Exception& e)
            {
                cout << "Error: " << e.what() << endl;
                result = -1;
            }
        }

        // End acquisition
        pCam->EndAcquisition();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

// This function prints the device information of the camera from the transport
// layer; please see NodeMapInfo example for more in-depth comments on printing
// device information from the nodemap.
int PrintDeviceInfo(INodeMap& nodeMap)
{
    cout << endl << "*** DEVICE INFORMATION ***" << endl << endl;
    int result = 0;

    try
    {
        FeatureList_t features;
        const CCategoryPtr category = nodeMap.GetNode("DeviceInformation");
        if (IsAvailable(category) && IsReadable(category))
        {
            category->GetFeatures(features);

            for (auto it = features.begin(); it != features.end(); ++it)
            {
                const CNodePtr pfeatureNode = *it;
                cout << pfeatureNode->GetName() << " : ";
                CValuePtr pValue = static_cast<CValuePtr>(pfeatureNode);
                cout << (IsReadable(pValue) ? pValue->ToString() : "Node not readable");
                cout << endl;
            }
        }
        else
        {
            cout << "Device control information not available." << endl;
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

    try
    {
        // Retrieve TL device nodemap and print device information
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

        result = PrintDeviceInfo(nodeMapTLDevice);

        // Initialize camera
        pCam->Init();

        // Retrieve GenICam nodemap
        INodeMap& nodeMap = pCam->GetNodeMap();

        // Configure heartbeat for GEV camera
#ifdef _DEBUG
        result = result | DisableGVCPHeartbeat(pCam);
#else
        result = result | ResetGVCPHeartbeat(pCam);
#endif

        // Acquire images
        result = result | AcquireImages(pCam, nodeMap, nodeMapTLDevice);

#ifdef _DEBUG
        // Reset heartbeat for GEV camera
        result = result | ResetGVCPHeartbeat(pCam);
#endif

        // Deinitialize camera
        pCam->DeInit();
    }
    catch (Spinnaker::Exception& e)
    {
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

    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    // Print out current library version
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
        << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << endl
        << endl;

    int result = 0;
    if (UseExampleImageFile)
    {
        result = RunColorCorrectionOnExampleImage();
    }
    else
    {
        // Retrieve list of cameras from the system
        CameraList camList = system->GetCameras();

        const unsigned int numCameras = camList.GetSize();

        cout << "Number of cameras detected: " << numCameras << endl << endl;

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

            cout << endl << "Running example for camera " << i << "..." << endl;

            // Run example
            result = result | RunSingleCamera(pCam);

            cout << "Camera " << i << " example complete..." << endl << endl;
        }

        // Release reference to the camera
        pCam = nullptr;

        // Clear camera list before releasing system
        camList.Clear();
    }

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return result;
}