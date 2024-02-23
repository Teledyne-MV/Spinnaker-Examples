//=============================================================================
// Copyright (c) 2001-2024 FLIR Systems, Inc. All Rights Reserved.
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
 *  @example ShadingCorrection.cpp
 *
 *  @brief LensShadingCorrection.cpp shows how to create calibration files to correct images with lens shading
 *  artifacts.
 *
 *  This example creates and saves a calibration file which is required for camera shading correction.
 *  Options are provided to deploy a calibration file (if already created) from the disk to the camera,
 *  or download and save the file from the camera to disk.
 *
 *  It also provides debug messages when debug mode is turned on, giving more
 *  detailed progress status and error messages to the user.
 *
 *  This example relies on information provided in the
 *  Enumeration, Acquisition, FileAccess_QuickSpin, and NodeMapInfo examples.
 *
 *  Please leave us feedback at: https://www.surveymonkey.com/r/TDYMVAPI
 *  More source code examples at: https://github.com/Teledyne-MV/Spinnaker-Examples
 *  Need help? Check out our forum at: https://teledynevisionsolutions.zendesk.com/hc/en-us/community/topics
 */

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>
#include <fstream>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

static bool _enableDebug = false;
static string filename = "";
static gcstring _fileSelector = "UserShadingCoeff1";

// Print out usage of the application
void PrintUsage()
{
    cout << "This program creates and saves the calibration file required for camera lens shading correction." << endl;
    cout << "Optionally deploy a saved calibration file from disk to camera, or download a calibration file from "
            "camera to disk."
         << endl
         << endl;
    cout << "Usage: LensShadingCorrection [options] COMMAND" << endl << endl;
    cout << "Option:\n\t";
    cout << "-v : Enable verbose output" << endl;

    cout << "Commands:\n\t";
    cout << "-c : Create and deploy calibration file to camera\n\t";
    cout << "-d : Download calibration file from camera to system disk (with given file name)\n\t";
    cout << "-u : Upload calibration file (with given file name) to camera\n\t";
    cout << "-a : Acquire images using on camera calibration file after all operations are complete\n\t";
    cout << "-f string: location of file to download/upload" << endl;

    cout << "Example:\n\t";
    cout << "To upload existing \"CalibrationFile\" file from current working directory: LensShadingCorrection.exe -u "
            "-v -f CalibrationFile"
         << endl
         << endl;
    cout << "Run LensShadingCorrection --help to print usage information" << endl;
    cout << endl << endl;
}

// Print out operation result message
static void PrintResultMessage(bool result)
{
    if (result)
    {
        cout << "\n*** OPERATION COMPLETE ***\n";
    }
    else
    {
        cout << "\n*** OPERATION FAILED ***\n";
    }
}

// Print out debug message
static void PrintDebugMessage(string msg)
{
#if DEBUG
    _enableDebug = true;
#endif
    if (_enableDebug)
    {
        cout << msg << endl;
    }
}

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
    // If the application is terminated unexpectedly, the camera may not be locked
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

int SwitchShadingCorrectionMode(CameraPtr pCam, bool mode)
{
    // Initialize camera
    pCam->Init();

    // Retrieve GenICam nodemap
    INodeMap& nodeMap = pCam->GetNodeMap();
    PrintDebugMessage("Configuring camera for lens shading correction with new calibration file");

    // Ensure lens shading correction mode is set to Active
    // Retrieve entry node from enumeration node

    CEnumerationPtr ptrLensShadingCorrectionMode = nodeMap.GetNode("LensShadingCorrectionMode");
    CEnumEntryPtr ptrLensShadingCorrectionModeSelected;
    gcstring modeString = mode ? "Active" : "Off";

    ptrLensShadingCorrectionModeSelected = ptrLensShadingCorrectionMode->GetEntryByName(modeString);

    if (!IsReadable(ptrLensShadingCorrectionModeSelected))
    {
        cout << "Unable to set Lens Shading Correction mode to " << modeString << " (entry retrieval). Aborting..."
             << endl
             << endl;
        return -1;
    }

    // Retrieve integer value from entry node
    const int64_t lensShadingCorrectionModeSelected = ptrLensShadingCorrectionModeSelected->GetValue();

    // Set integer value from entry node as new value of enumeration node
    ptrLensShadingCorrectionMode->SetIntValue(lensShadingCorrectionModeSelected);

    cout << "Lens Shading Correction mode set to " << modeString << "..." << endl;

    return 0;
}

// This function acquires and saves 10 images from a device.
int AcquireImages(CameraPtr pCam, INodeMap& nodeMap, INodeMap& nodeMapTLDevice)
{
    int result = 0;

    cout << endl << endl << "*** IMAGE ACQUISITION ***" << endl << endl;

    try
    {
        //
        // Set acquisition mode to continuous
        //
        // *** NOTES ***
        // Because the example acquires and saves 10 images, setting acquisition
        // mode to continuous lets the example finish. If set to single frame
        // or multiframe (at a lower number of images), the example would just
        // hang. This would happen because the example has been written to
        // acquire 10 images while the camera would have been programmed to
        // retrieve less than that.
        //
        // Setting the value of an enumeration node is slightly more complicated
        // than other node types. Two nodes must be retrieved: first, the
        // enumeration node is retrieved from the nodemap; and second, the entry
        // node is retrieved from the enumeration node. The integer value of the
        // entry node is then set as the new value of the enumeration node.
        //
        // Notice that both the enumeration and the entry nodes are checked for
        // availability and readability/writability. Enumeration nodes are
        // generally readable and writable whereas their entry nodes are only
        // ever readable.
        //
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

        // Configure heartbeat for GEV camera
#ifdef _DEBUG
        result = result | DisableGVCPHeartbeat(pCam);
#else
        result = result | ResetGVCPHeartbeat(pCam);
#endif
        //
        // Begin acquiring images
        //
        // *** NOTES ***
        // What happens when the camera begins acquiring images depends on the
        // acquisition mode. Single frame captures only a single image, multi
        // frame captures a set number of images, and continuous captures a
        // continuous stream of images. Because the example calls for the
        // retrieval of 10 images, continuous mode has been set.
        //
        // *** LATER ***
        // Image acquisition must be ended when no more images are needed.
        //
        pCam->BeginAcquisition();

        cout << "Acquiring images..." << endl;

        //
        // Retrieve device serial number for filename
        //
        // *** NOTES ***
        // The device serial number is retrieved in order to keep cameras from
        // overwriting one another. Grabbing image IDs could also accomplish
        // this.
        //
        gcstring deviceSerialNumber("");
        CStringPtr ptrStringSerial = nodeMapTLDevice.GetNode("DeviceSerialNumber");
        if (IsReadable(ptrStringSerial))
        {
            deviceSerialNumber = ptrStringSerial->GetValue();

            cout << "Device serial number retrieved as " << deviceSerialNumber << "..." << endl;
        }
        cout << endl;

        // Retrieve, convert, and save images
        const unsigned int k_numImages = 5;

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
                //
                // Retrieve next received image
                //
                // *** NOTES ***
                // Capturing an image houses images on the camera buffer. Trying
                // to capture an image that does not exist will hang the camera.
                //
                // *** LATER ***
                // Once an image from the buffer is saved and/or no longer
                // needed, the image must be released in order to keep the
                // buffer from filling up.
                //
                ImagePtr pResultImage = pCam->GetNextImage(1000);

                //
                // Ensure image completion
                //
                // *** NOTES ***
                // Images can easily be checked for completion. This should be
                // done whenever a complete image is expected or required.
                // Further, check image status for a little more insight into
                // why an image is incomplete.
                //
                if (pResultImage->IsIncomplete())
                {
                    // Retrieve and print the image status description
                    cout << "Image incomplete: " << Image::GetImageStatusDescription(pResultImage->GetImageStatus())
                         << "..." << endl
                         << endl;
                }
                else
                {
                    //
                    // Print image information; height and width recorded in pixels
                    //
                    // *** NOTES ***
                    // Images have quite a bit of available metadata including
                    // things such as CRC, image status, and offset values, to
                    // name a few.
                    //
                    const size_t width = pResultImage->GetWidth();

                    const size_t height = pResultImage->GetHeight();

                    cout << "Grabbed image " << imageCnt << ", width = " << width << ", height = " << height << endl;

                    //
                    // Convert image to mono 8
                    //
                    // *** NOTES ***
                    // Images can be converted between pixel formats by using
                    // the appropriate enumeration value. Unlike the original
                    // image, the converted one does not need to be released as
                    // it does not affect the camera buffer.
                    //
                    // When converting images, color processing algorithm is an
                    // optional parameter.
                    //
                    ImagePtr convertedImage = processor.Convert(pResultImage, PixelFormat_Mono8);

                    // Create a unique filename
                    ostringstream filename;

                    filename << "ShadingCorrection-";
                    if (!deviceSerialNumber.empty())
                    {
                        filename << deviceSerialNumber.c_str() << "-";
                    }
                    filename << imageCnt << ".jpg";

                    //
                    // Save image
                    //
                    // *** NOTES ***
                    // The standard practice of the examples is to use device
                    // serial numbers to keep images of one device from
                    // overwriting those of another.
                    //
                    convertedImage->Save(filename.str().c_str());

                    cout << "Image saved at " << filename.str() << endl;
                }

                //
                // Release image
                //
                // *** NOTES ***
                // Images retrieved directly from the camera (i.e. non-converted
                // images) need to be released in order to keep from filling the
                // buffer.
                //
                pResultImage->Release();

                cout << endl;
            }
            catch (Spinnaker::Exception& e)
            {
                cout << "Error: " << e.what() << endl;
                result = -1;
            }
        }

        //
        // End acquisition
        //
        // *** NOTES ***
        // Ending acquisition appropriately helps ensure that devices clean up
        // properly and do not need to be power-cycled to maintain integrity.
        //

        pCam->EndAcquisition();

#ifdef _DEBUG
        // Reset heartbeat for GEV camera
        result = result | ResetGVCPHeartbeat(pCam);
#endif
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        return -1;
    }

    return result;
}

// This function prints the device information of the camera from the transport
// layer; please see NodeMapInfo example for more in-depth comments on printing
// device information from the nodemap.
int PrintDeviceInfo(INodeMap& nodeMap)
{
    int result = 0;
    cout << endl << "*** DEVICE INFORMATION ***" << endl << endl;

    try
    {
        FeatureList_t features;
        const CCategoryPtr category = nodeMap.GetNode("DeviceInformation");
        if (IsReadable(category))
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

// Initialize the system
bool InitializeSystem(SystemPtr& system, CameraList& camList, CameraPtr& pCam)
{
    // Retrieve singleton reference to system object
    system = System::GetInstance();

    // Retrieve list of cameras from the system
    camList = system->GetCameras();

    const unsigned int numCameras = camList.GetSize();

    cout << "Number of cameras detected: " << numCameras << endl << endl;

    // Stop if there are no cameras
    if (numCameras == 0)
    {
        // Clear camera list before releasing system
        camList.Clear();

        // Release system
        system->ReleaseInstance();

        cout << "Not enough cameras!" << endl;
        cout << "Done! Press Enter to exit..." << endl;
        getchar();

        return false;
    }

    //
    // Create a shared pointer to camera
    //
    // *** NOTES ***
    // The CameraPtr object is a shared pointer, and will generally clean itself
    // up upon exiting its scope. However, if a shared pointer is created in the
    // same scope that a system object is explicitly released (i.e. this scope),
    // the reference to the shared point must be broken manually.
    //
    // *** LATER ***
    // Shared pointers can be terminated manually by assigning them to nullptr.
    // This keeps releasing the system from throwing an exception.
    //
    pCam = nullptr;

    // Run example on 1st camera
    pCam = camList.GetByIndex(0);

    return true;
}

// This function checks the lens shading correction status
// It returns true if the file is valid and calibration is correct and complete
bool LensShadingCorrectionCalibrationStatus(INodeMap& nodeMap)
{
    bool result = false;
    try
    {

        // Retrieve node name for shading status
        CStringPtr ptrLensShadingCalibrationStatus = nodeMap.GetNode("LensShadingCorrectionCalibrationStatus");
        if (!IsReadable(ptrLensShadingCalibrationStatus))
        {
            cout << "Unable to read Lens Shading Calibration Status(enum retrieval). Aborting..." << endl << endl;
            return result;
        }

        cout << ptrLensShadingCalibrationStatus->GetValue() << endl;

        if (ptrLensShadingCalibrationStatus->GetValue() == "Calibration Complete")
        {
            cout << "The calibration file was created successfully!" << endl;

            result = true;
        }
        else if (ptrLensShadingCalibrationStatus->GetValue() == "Calibration Saturated")
        {
            cout << "The image is too bright." << endl;

            cout << "Please adjust the exposure and/or gain settings and try again." << endl;

            cout << "Aborting..." << endl;

            result = false;
        }
        else if (ptrLensShadingCalibrationStatus->GetValue() == "Calibration Level Low")
        {
            cout << "The image has very low intensity." << endl;

            cout << "Please adjust the exposure and/or gain settings and try again." << endl;

            cout << "Aborting..." << endl;

            result = false;
        }
        else if (ptrLensShadingCalibrationStatus->GetValue() == "Calibration Region Low")
        {
            cout << "One or more regions has no signal." << endl;

            cout << "Please use a different lens to cover the entire image area and try again." << endl;

            cout << "Aborting..." << endl;

            result = false;
        }
        else if (ptrLensShadingCalibrationStatus->GetValue() == "Calibration Incomplete")
        {
            cout << "No calibration file has been created with this camera yet," << endl;

            cout << " or the current calibration was stopped without a successful result" << endl;

            cout << "Please try again. Aborting..." << endl;

            result = false;
        }
    }
    catch (Spinnaker::Exception& ex)
    {
        cout << "Unexpected Exception: " << ex.what() << endl;
        return false;
    }
    return result;
}

// Open camera file to read
bool OpenFileToRead(CameraPtr pCamera)
{
    cout << "Opening file for reading..." << endl;
    try
    {
        pCamera->FileOperationSelector.SetValue(FileOperationSelector_Open);
        pCamera->FileOpenMode.SetValue(FileOpenMode_Read);
        pCamera->FileOperationExecute.Execute();

        if (pCamera->FileOperationStatus.GetValue() != FileOperationStatus_Success)
        {
            cout << "Failed to open file for reading!" << endl;
            return false;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what() << endl;
        return false;
    }
    return true;
}

// Execute read operation
bool ExecuteReadCommand(CameraPtr pCamera)
{
    try
    {
        pCamera->FileOperationSelector.SetValue(FileOperationSelector_Read);
        pCamera->FileOperationExecute.Execute();

        if (pCamera->FileOperationStatus.GetValue() != FileOperationStatus_Success)
        {
            cout << "Failed to read file!" << endl;
            return false;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what() << endl;
        return false;
    }
    return true;
}

// Close the file
bool CloseFile(CameraPtr pCam)
{
    PrintDebugMessage("Closing file...");
    try
    {
        pCam->FileOperationSelector.SetValue(FileOperationSelector_Close);
        pCam->FileOperationExecute.Execute();
        if (pCam->FileOperationStatus.GetValue() != FileOperationStatus_Success)
        {
            cout << "Failed to close file!" << endl;
            return false;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what();
        return false;
    }
    return true;
}

// Execute delete operation
bool ExecuteDeleteCommand(CameraPtr pCam)
{
    PrintDebugMessage("Deleting file...");
    try
    {
        pCam->FileOperationSelector.SetValue(FileOperationSelector_Delete);
        pCam->FileOperationExecute.Execute();
        if (pCam->FileOperationStatus.GetValue() != FileOperationStatus_Success)
        {
            cout << "Failed to delete file!" << endl;
            return false;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what() << endl;
        return false;
    }
    return true;
}

// Open camera file for writing
bool OpenFileToWrite(CameraPtr pCam)
{
    PrintDebugMessage("Opening file for writing...");
    try
    {
        pCam->FileOperationSelector.SetValue(FileOperationSelector_Open);
        pCam->FileOpenMode.SetValue(FileOpenMode_Write);
        pCam->FileOperationExecute.Execute();
        if (pCam->FileOperationStatus.GetValue() != FileOperationStatus_Success)
        {
            cout << "Failed to open file for writing!" << endl;
            return false;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what();
        return false;
    }
    return true;
}

// Execute write operation
bool ExecuteWriteCommand(CameraPtr pCam)
{
    try
    {
        pCam->FileOperationSelector.SetValue(FileOperationSelector_Write);
        pCam->FileOperationExecute.Execute();
        if (pCam->FileOperationStatus.GetValue() != FileOperationStatus_Success)
        {
            cout << "Failed to write to file!" << endl;
            return false;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what();
        return false;
    }
    return true;
}

// Create and save a calibration file from camera memory
bool CreateCalibrationFile()
{
    try
    {
        SystemPtr system;
        CameraList camList;
        CameraPtr pCam;

        // Initialize System
        if (!InitializeSystem(system, camList, pCam))
        {
            PrintResultMessage(false);
            return false;
        }

        // Retrieve TL device nodemap and print device information
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();
        PrintDeviceInfo(nodeMapTLDevice);

        // Initialize camera
        pCam->Init();

        // Retrieve GenICam nodemap
        INodeMap& nodeMap = pCam->GetNodeMap();

        cout << endl << "*** CREATING CALIBRATION FILE ***" << endl;

        // Ensure the Lens Shading Correction Mode is set to Off before applying calibration
        CEnumerationPtr ptrLensShadingCorrectionMode = nodeMap.GetNode("LensShadingCorrectionMode");
        if (!IsReadable(ptrLensShadingCorrectionMode) || !IsWritable(ptrLensShadingCorrectionMode))
        {
            cout << "Unable to set Lens Shading Correction mode to off (enum retrieval). Aborting..." << endl << endl;
            return false;
        }

        // Retrieve entry node from enumeration node
        CEnumEntryPtr ptrLensShadingCorrectionModeOff = ptrLensShadingCorrectionMode->GetEntryByName("Off");
        if (!IsReadable(ptrLensShadingCorrectionModeOff))
        {
            cout << "Unable to set Lens Shading Correction mode to off (entry retrieval). Aborting..." << endl << endl;
            return false;
        }

        // Retrieve integer value from entry node
        const int64_t lensShadingCorrectionModeOff = ptrLensShadingCorrectionModeOff->GetValue();

        // Set integer value from entry node as new value of enumeration node
        ptrLensShadingCorrectionMode->SetIntValue(lensShadingCorrectionModeOff);

        cout << "Lens Shading Correction mode set to off..." << endl;

        // Select where to save the file from Lens Shading Coefficient Active Set
        CEnumerationPtr ptrLensShadingCoefficientActiveSet = nodeMap.GetNode("LensShadingCoefficientActiveSet");
        if (!IsReadable(ptrLensShadingCoefficientActiveSet) || !IsWritable(ptrLensShadingCoefficientActiveSet))
        {
            cout << "Unable to set Lens Shading Coefficient Active Set to User Shading Coeff (enum retrieval). "
                    "Aborting..."
                 << endl
                 << endl;
            return false;
        }

        // Retrieve entry node from enumeration node
        CEnumEntryPtr ptrLensShadingCoefficientActiveSetUserShadingCoeff =
            ptrLensShadingCoefficientActiveSet->GetEntryByName(_fileSelector);
        if (!IsReadable(ptrLensShadingCoefficientActiveSetUserShadingCoeff))
        {
            cout << "Unable to set Lens Shading Coefficient Active Set to User Shading Coeff (entry retrieval). "
                    "Aborting..."
                 << endl
                 << endl;
            return false;
        }

        // Retrieve integer value from entry node
        const int64_t lensShadingCoefficientActiveSetUserShadingCoeff =
            ptrLensShadingCoefficientActiveSetUserShadingCoeff->GetValue();

        // Set integer value from entry node as new value of enumeration node
        ptrLensShadingCoefficientActiveSet->SetIntValue(lensShadingCoefficientActiveSetUserShadingCoeff);

        cout << "Lens Shading Correction mode set to "
             << ptrLensShadingCoefficientActiveSet->GetCurrentEntry()->GetDisplayName() << "..." << endl;

        //
        // Execute Lens Shading correction Calibration Setup
        //
        // *** NOTES ***
        // This would change the camera settings as follows:
        // Maximum resolution
        // No binning or decimation
        // Auto gain enabled
        // Auto white balance enabled
        // 8-bit pixel format selected
        // Black level clamping enabled (if applicable)
        // Black level set to 0
        //
        // Retrieve command node from nodemap
        CCommandPtr ptrLensShadingCorrectionCalibrationSetup = nodeMap.GetNode("LensShadingCorrectionCalibrationSetup");
        if (!IsWritable(ptrLensShadingCorrectionCalibrationSetup))
        {
            cout << "Unable to execute Shading correction calibration setup (enum retrieval). Aborting..." << endl
                 << endl;
            return false;
        }

        // Execute the command
        ptrLensShadingCorrectionCalibrationSetup->Execute();

        PrintDebugMessage("Finish configuring camera for calibration");

        // Start Acquisition
        //
        // *** NOTES ***
        // We acquire an image here for the camera to perform shading correction calibration.
        // The last image in the camera buffer is used for the calibration process.
        // Before starting acquisition, ensure:
        // the scene is a uniform, plain target without artifacts
        // an appropriate lens is attached to the camera
        pCam->BeginAcquisition();

        // Stop Acquisition
        pCam->EndAcquisition();

        // Execute lens shading correction calibration
        //
        // *** NOTES ***
        // This begins the calibration process which take approx. 10 seconds after which
        // a file is saved to the selected Coefficient Active Set
        //
        // Retrieve the Command node from nodemap
        CCommandPtr ptrLensShadingCorrectionCalibration = nodeMap.GetNode("LensShadingCorrectionCalibration");
        if (!IsWritable(ptrLensShadingCorrectionCalibration))
        {
            cout << "Unable to execute lens Shading correction calibration (enum retrieval). Aborting..." << endl
                 << endl;
            return false;
        }

        // Execute the command
        ptrLensShadingCorrectionCalibration->Execute();

        PrintDebugMessage("Finished lens shading correction calibration. Testing if the calibration file is valid...");

        // Check the lens shading calibration status
        if (!LensShadingCorrectionCalibrationStatus(nodeMap))
        {
            return false;
        }

        // Release reference to the camera
        //
        // *** NOTES ***
        // Had the CameraPtr object been created within the for-loop, it would not
        // be necessary to manually break the reference because the shared pointer
        // would have automatically cleaned itself up upon exiting the loop.
        //
        pCam->DeInit();
        pCam = nullptr;

        // Clear camera list before releasing system
        camList.Clear();

        // Release system
        system->ReleaseInstance();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what();
        return false;
    }
    return true;
}

// Download Calibration file from camera
bool DownloadCalibrationFile()
{
    try
    {
        SystemPtr system;
        CameraList camList;
        CameraPtr pCam;

        // Check if filename is valid
        if (!filename.size())
        {
            cout << "Error! Filename not provided" << endl << endl;
            PrintUsage();
            return false;
        }

        // Initialize System
        if (!InitializeSystem(system, camList, pCam))
        {
            PrintResultMessage(false);
            return false;
        }

        // Print out current library version
        const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
        cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
             << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << endl
             << endl;

        // Retrieve TL device nodemap and print device information
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();
        PrintDeviceInfo(nodeMapTLDevice);

        // Initialize camera
        pCam->Init();

        // Retrieve GenICam nodemap
        INodeMap& nodeMap = pCam->GetNodeMap();

        cout << endl << "*** DOWNLOADING File ***" << endl;

        // Check file selector support
        PrintDebugMessage("Fetching \"UserShadingCoeff1\" Entry from FileSelectorNode...");
        if (pCam->FileSelector == NULL)
        {
            cout << "File selector not supported on device!";
            return false;
        }

        NodeList_t selectorList;
        pCam->FileSelector.GetEntries(selectorList);
        for (unsigned int i = 0; i < selectorList.size(); i++)
        {
            // Get current enum entry node
            CEnumEntryPtr node = selectorList.at(i);

            PrintDebugMessage("Setting value to FileSelectorNode...");

            // Check file selector entry support
            if (!node || !IsReadable(node))
            {
                // Go to next entry node
                cout << node->GetSymbolic() << " not supported!" << endl;
                continue;
            }

            if (node->GetSymbolic().compare(_fileSelector) == 0)
            {
                // Set file selector
                pCam->FileSelector.SetIntValue((int64_t)node->GetNumericValue());
                int64_t bytesToRead = pCam->FileSize.GetValue();
                if (bytesToRead == 0)
                {
                    cout << "No data available to read!" << endl;
                    continue;
                }
                PrintDebugMessage("Total data to download : " + to_string(static_cast<long long>(bytesToRead)));

                // Open file on camera for reading
                if (OpenFileToRead(pCam) != true)
                {
                    cout << "Failed to open file!" << endl;
                    continue;
                }
                // Attempt to set FileAccessLength to FileAccessBufferNode length to speed up the write
                if (pCam->FileAccessLength.GetValue() < pCam->FileAccessBuffer.GetLength())
                {
                    try
                    {
                        pCam->FileAccessLength.SetValue(pCam->FileAccessBuffer.GetLength());
                    }
                    catch (Spinnaker::Exception& e)
                    {
                        cout << "Unable to set FileAccessLength to FileAccessBuffer length : " << e.what() << endl;
                    }
                }

                // Ensure File Access Offset is 0
                pCam->FileAccessOffset.SetValue(0);

                // Compute number of read operations required
                int64_t intermediateBufferSize = pCam->FileAccessLength.GetValue();
                int64_t iterations =
                    (bytesToRead / intermediateBufferSize) + (bytesToRead % intermediateBufferSize == 0 ? 0 : 1);
                PrintDebugMessage("Fetching file from camera.");
                int64_t index = 0;
                int64_t totalSizeRead = 0;
                std::unique_ptr<unsigned char> dataBuffer(new unsigned char[static_cast<unsigned int>(bytesToRead)]);
                uint8_t* pData = dataBuffer.get();

                for (unsigned int i = 0; i < iterations; i++)
                {
                    if (!ExecuteReadCommand(pCam))
                    {
                        cout << "Reading stream failed!" << endl;
                        break;
                    }

                    // Verify size read in bytes
                    int64_t sizeRead = pCam->FileOperationResult.GetValue();

                    // Read from buffer Node
                    pCam->FileAccessBuffer.Get(&pData[index], sizeRead);

                    // Update index for next read iteration
                    index = index + sizeRead;

                    // Keep track of total bytes read
                    totalSizeRead += sizeRead;
                    PrintDebugMessage(
                        "Bytes read: " + to_string(static_cast<long long>(totalSizeRead)) + " of " +
                        to_string(static_cast<long long>(bytesToRead)));
                    cout << "Progress : " << (i * 100 / iterations) << " %" << endl;
                }

                cout << "Reading complete" << endl;

                if (!CloseFile(pCam))
                {
                    cout << "Failed to close file!" << endl;
                }
                cout << endl;

                // save file
                ofstream ofs(filename, ios_base::binary | ios_base::trunc);

                ofs.write((char*)&pData[0], totalSizeRead);
                ofs.close();

                cout << "*** SAVING CALIBRATION FILE ***" << endl;
            }
        }
        //
        // Release reference to the camera
        //
        // *** NOTES ***
        // Had the CameraPtr object been created within the for-loop, it would not
        // be necessary to manually break the reference because the shared pointer
        // would have automatically cleaned itself up upon exiting the loop.
        //
        pCam->DeInit();
        pCam = nullptr;

        // Clear camera list before releasing system
        camList.Clear();

        // Release system
        system->ReleaseInstance();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what() << endl;
        return false;
    }
    return true;
}

// Upload Calibration file to the camera
bool UploadCalibrationFile()
{
    try
    {

        SystemPtr system;
        CameraList camList;
        CameraPtr pCam;

        // Check if filename is valid
        if (!filename.size())
        {
            cout << "Error! Filename not provided" << endl << endl;
            PrintUsage();
            return false;
        }

        // Read from the file
        std::ifstream ifs(filename, std::ios::binary);

        // Read the data:
        std::vector<unsigned char> data =
            std::vector<unsigned char>((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        // Check if data is valid
        if (!ifs && !ifs.eof())
        {
            cerr << "Error Reading from file. Aborting..." << endl;
            return false;
        }

        // Initialize System
        if (!InitializeSystem(system, camList, pCam))
        {
            PrintResultMessage(false);
            return false;
        }

        // Retrieve TL device nodemap and print device information
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();
        PrintDeviceInfo(nodeMapTLDevice);

        // Initialize camera
        pCam->Init();

        // Retrieve GenICam nodemap
        INodeMap& nodeMap = pCam->GetNodeMap();

        cout << endl << "*** UPLOADING FILE ***" << endl;
        PrintDebugMessage("Fetching \"UserShadingCoeff1\" Entry from FileSelectorNode...");

        // Check file selector support
        if (pCam->FileSelector == NULL)
        {
            cout << "File selector not supported on device!";
            return false;
        }

        NodeList_t selectorList;
        pCam->FileSelector.GetEntries(selectorList);
        for (unsigned int i = 0; i < selectorList.size(); i++)
        {
            // Get current enum entry node
            CEnumEntryPtr node = selectorList.at(i);
            PrintDebugMessage("Setting value to FileSelectorNode...");

            // Check file selector entry support
            if (!node || !IsReadable(node))
            {
                // Go to next entry node
                cout << node->GetSymbolic() << " not supported!" << endl;
                continue;
            }
            if (node->GetSymbolic().compare(_fileSelector) == 0)
            {
                cout << "The node: " << node->GetSymbolic() << endl;
                PrintDebugMessage("Setting value to FileSelectorNode...");

                // Set file selector
                pCam->FileSelector.SetIntValue((int64_t)node->GetNumericValue());

                // Delete file on camera before writing in case camera runs out of space
                if (pCam->FileSize.GetValue() > 0)
                {
                    if (ExecuteDeleteCommand(pCam) != true)
                    {
                        cout << "Failed to delete file!" << endl;
                        continue;
                    }
                }

                // Open file on camera for write
                if (OpenFileToWrite(pCam) != true)
                {
                    cout << "Failed to open file!" << endl;

                    // Error may be caused by improper file closing
                    // Close and re-open again
                    if (!CloseFile(pCam))
                    {
                        // File failed to close. Abort!
                        cout << "Problem opening file node." << endl;
                        return false;
                    }

                    // File was closed. Open again.
                    if (!OpenFileToWrite(pCam))
                    {
                        // File failed to open again. Abort!
                        cout << "Problem opening file node." << endl;
                        return false;
                    }
                }

                // Attempt to set FileAccessLength to FileAccessBufferNode length to speed up the write
                if (pCam->FileAccessLength.GetValue() < pCam->FileAccessBuffer.GetLength())
                {
                    try
                    {
                        pCam->FileAccessLength.SetValue(pCam->FileAccessBuffer.GetLength());
                    }
                    catch (Spinnaker::Exception& e)
                    {
                        cout << "Unable to set FileAccessLength to FileAccessBuffer length : " << e.what();
                    }
                }
                // Ensure File Access Offset is 0
                pCam->FileAccessOffset.SetValue(0);

                // Compute number of write operations required
                int64_t totalBytesToWrite = data.size();
                int64_t intermediateBufferSize = pCam->FileAccessLength.GetValue();
                int64_t writeIterations = (totalBytesToWrite / intermediateBufferSize) +
                                          (totalBytesToWrite % intermediateBufferSize == 0 ? 0 : 1);

                if (totalBytesToWrite == 0)
                {
                    cout << "Empty file. No data will be written to camera." << endl;
                    return false;
                }

                PrintDebugMessage("Start saving user set file on camera...");
                PrintDebugMessage("Total Bytes to write : " + to_string(static_cast<long long>(totalBytesToWrite)));
                PrintDebugMessage("FileAccessLength : " + to_string(static_cast<long long>(intermediateBufferSize)));
                PrintDebugMessage("Write Iterations : " + to_string(static_cast<long long>(writeIterations)));

                int64_t index = 0;
                int64_t bytesLeftToWrite = totalBytesToWrite;
                int64_t totalBytesWritten = 0;
                bool paddingRequired = false;
                int numPaddings = 0;

                cout << "Writing data to device" << endl;

                unsigned char* pFileData = static_cast<unsigned char*>(data.data());

                for (unsigned int i = 0; i < writeIterations; i++)
                {
                    // Check whether padding is required
                    if (intermediateBufferSize > bytesLeftToWrite)
                    {
                        // Check for multiple of 4 bytes
                        unsigned int remainder = bytesLeftToWrite % 4;
                        if (remainder != 0)
                        {
                            paddingRequired = true;
                            numPaddings = 4 - remainder;
                        }
                    }

                    // Setup data to write
                    int64_t tmpBufferSize = intermediateBufferSize <= bytesLeftToWrite
                                                ? intermediateBufferSize
                                                : (bytesLeftToWrite + numPaddings);
                    std::unique_ptr<unsigned char> tmpBuffer(
                        new unsigned char[static_cast<unsigned int>(tmpBufferSize)]);
                    memcpy(
                        tmpBuffer.get(),
                        &pFileData[index],
                        static_cast<size_t>((
                            (intermediateBufferSize <= bytesLeftToWrite) ? intermediateBufferSize : bytesLeftToWrite)));

                    if (paddingRequired)
                    {
                        // Fill padded bytes
                        for (int j = 0; j < numPaddings; j++)
                        {
                            unsigned char* pTmpBuffer = tmpBuffer.get();
                            pTmpBuffer[bytesLeftToWrite + j] = 255;
                        }
                    }

                    // Update index for next write iteration
                    index = index +
                            (intermediateBufferSize <= bytesLeftToWrite ? intermediateBufferSize : bytesLeftToWrite);

                    // Write to FileAccessBufferNode
                    pCam->FileAccessBuffer.Set(tmpBuffer.get(), tmpBufferSize);

                    if (intermediateBufferSize > bytesLeftToWrite)
                    {
                        // Update file Access Length Node.
                        // This is to prevent garbage data outside
                        // the range from being written to the device.
                        pCam->FileAccessLength.SetValue(bytesLeftToWrite);
                    }

                    // Perform Write command
                    if (!ExecuteWriteCommand(pCam))
                    {
                        cout << "Writing to stream failed!" << endl;
                        break;
                    }

                    // Verify size of bytes written
                    int64_t sizeWritten = pCam->FileOperationResult.GetValue();

                    // Log current file access offset
                    PrintDebugMessage("File Access Offset: " + to_string(pCam->FileAccessOffset.GetValue()));

                    // Keep track of total bytes written
                    totalBytesWritten += sizeWritten;
                    PrintDebugMessage(
                        "Bytes written: " + to_string(static_cast<long long>(totalBytesWritten)) + " of " +
                        to_string(static_cast<long long>(totalBytesToWrite)));

                    // Keep track of bytes left to write
                    bytesLeftToWrite = totalBytesToWrite - totalBytesWritten;
                    PrintDebugMessage("Bytes left: " + to_string(static_cast<long long>(bytesLeftToWrite)));

                    cout << "Progress : " << (i * 100 / writeIterations) << " %" << endl;
                }

                cout << "Writing complete" << endl;

                if (!CloseFile(pCam))
                {
                    cout << "Failed to close file!" << endl;
                }
            }
        }

        // Set calibration mode to off
        CEnumerationPtr ptrLensShadingCorrectionMode = nodeMap.GetNode("LensShadingCorrectionMode");
        if (!IsReadable(ptrLensShadingCorrectionMode) || !IsWritable(ptrLensShadingCorrectionMode))
        {
            cout << "Unable to set Lens Shading Correction mode to off (enum retrieval). Aborting..." << endl << endl;
            return false;
        }

        // Retrieve entry node from enumeration node
        CEnumEntryPtr ptrLensShadingCorrectionModeOff = ptrLensShadingCorrectionMode->GetEntryByName("Off");
        if (!IsReadable(ptrLensShadingCorrectionModeOff))
        {
            cout << "Unable to set Lens Shading Correction mode to off (entry retrieval). Aborting..." << endl << endl;
            return false;
        }

        // Set integer value from entry node as new value of enumeration node
        ptrLensShadingCorrectionMode->SetIntValue(ptrLensShadingCorrectionModeOff->GetValue());

        cout << "Lens Shading Coorrection mode set to off..." << endl;

        // Configure the camera and test shading correction with uploaded calibration file
        // Select Lens Shading Coefficient Active Set
        CEnumerationPtr ptrLensShadingCoefficientActiveSet = nodeMap.GetNode("LensShadingCoefficientActiveSet");
        if (!IsReadable(ptrLensShadingCoefficientActiveSet) || !IsWritable(ptrLensShadingCoefficientActiveSet))
        {
            cout << "Unable to set Lens Shading Coefficient Active Set to User Shading Coeff (enum retrieval). "
                    "Aborting..."
                 << endl
                 << endl;
            return false;
        }

        // Retrieve entry node from enumeration node
        CEnumEntryPtr ptrLensShadingCoefficientActiveSetUserShadingCoeff =
            ptrLensShadingCoefficientActiveSet->GetEntryByName(_fileSelector);
        if (!IsReadable(ptrLensShadingCoefficientActiveSetUserShadingCoeff))
        {
            cout << "Unable to set Lens Shading Coefficient Active Set to User Shading Coeff (entry retrieval). "
                    "Aborting..."
                 << endl
                 << endl;
            return false;
        }

        // Retrieve integer value from entry node
        const int64_t lensShadingCoefficientActiveSetUserShadingCoeff =
            ptrLensShadingCoefficientActiveSetUserShadingCoeff->GetValue();

        // Set integer value from entry node as new value of enumeration node
        ptrLensShadingCoefficientActiveSet->SetIntValue(lensShadingCoefficientActiveSetUserShadingCoeff);

        cout << "Lens Shading Correction mode set to "
             << ptrLensShadingCoefficientActiveSet->GetCurrentEntry()->GetDisplayName() << "..." << endl;

        //
        // Release reference to the camera
        //
        // *** NOTES ***
        // Had the CameraPtr object been created within the for-loop, it would not
        // be necessary to manually break the reference because the shared pointer
        // would have automatically cleaned itself up upon exiting the loop.
        //
        pCam->DeInit();
        pCam = nullptr;

        // Clear camera list before releasing system
        camList.Clear();

        // Release system
        system->ReleaseInstance();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what();
        return false;
    }
    return true;
}
// Example entry point; please see Enumeration example for more in-depth
// comments on preparing and cleaning up the system.
int main(int argc, char** argv)
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

    int result = 0;
    bool acquireImages = false;
    // Change arguments to string for easy read
    std::vector<std::string> args(argv, argv + argc);

    // Check verbose output flag or help message
    for (size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "-v" || args[i] == "-V")
        {
            _enableDebug = true;
        }

        if (args[i] == "--help")
        {
            PrintUsage();
            return result;
        }

        if (args[i] == "-f" || args[i] == "-F")
        {
            filename = args[i + 1];
        }
    }

    for (size_t i = 1; i < args.size(); ++i)
    {

        if (args[i] == "-c" || args[i] == "-C")
        {
            if (!CreateCalibrationFile())
            {
                PrintResultMessage(false);
                return -1;
            }
            return result;
        }

        else if (args[i] == "-d" || args[i] == "-D")
        {
            if (!DownloadCalibrationFile())
            {
                PrintResultMessage(false);
                return -1;
            }
            return result;
        }
        else if (args[i] == "-u" || args[i] == "-U")
        {
            if (!UploadCalibrationFile())
            {
                PrintResultMessage(false);
                return -1;
            }
            return result;
        }
        else if (args[i] == "-a" || args[i] == "-A")
        {
            acquireImages = true;
        }
    }

    if (acquireImages)
    {
        SystemPtr system;
        CameraList camList;
        CameraPtr pCam;

        // Initialize System
        if (!InitializeSystem(system, camList, pCam))
        {
            PrintResultMessage(false);
            return false;
        }

        result = result | SwitchShadingCorrectionMode(pCam, true);

        result = result | AcquireImages(pCam, pCam->GetNodeMap(), pCam->GetTLDeviceNodeMap());
    }
    return result;
}