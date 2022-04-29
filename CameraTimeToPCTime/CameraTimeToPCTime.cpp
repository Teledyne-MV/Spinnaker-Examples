//=============================================================================
// Copyright (c) 2001-2018 FLIR Systems, Inc. All Rights Reserved.
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
*  @example cameraTimeToPCTime.cpp
*
*  @brief cameraTimeToPCTime.cpp shows how to convert camera's timestamp to PC
*  time.

*  It may be helpful to read through article below to understand how PC time is
*  calculated from system time, timestamp latch, chunk timestamp:
*  https://www.flir.com/support-center/iis/machine-vision/application-note/synchronizing-a-blackfly-or-grasshopper3-gige-cameras-time-to-pc-time/

*  It can also be helpful to familiarize yourself with the ChunkData.cpp as it
*  is shorter and simpler, and provides a strong introduction to camera
*  customization identifiers such as frame ID, properties such as black level,
*  and more. This information can be acquired from either the nodemap or the
*  image itself.

*  It may be preferable to grab chunk data from each individual image, as it
*  can be hard to verify whether data is coming from the correct image when
*  using the nodemap. This is because chunk data retrieved from the nodemap is
*  only valid for the current image; when GetNextImage() is called, chunk data
*  will be updated to that of the new current image.
*/

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>
#include <ctime>
#include <chrono>
#include <time.h>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

// Use the following enum and global constant to select whether chunk data is
// displayed from the image or the nodemap.
enum chunkDataType
{
    IMAGE,
    NODEMAP
};

const chunkDataType chosenChunkData = IMAGE;

// This function configures the camera to add chunk data to each image. It does
// this by enabling each type of chunk data before enabling chunk data mode.
// When chunk data is turned on, the data is made available in both the nodemap
// and each image.
int ConfigureChunkData(INodeMap & nodeMap)
{
    int result = 0;

    cout << endl << endl << "*** CONFIGURING CHUNK DATA ***" << endl << endl;

    try
    {
        //
        // Activate chunk mode
        //
        // *** NOTES ***
        // Once enabled, chunk data will be available at the end of the payload
        // of every image captured until it is disabled. Chunk data can also be
        // retrieved from the nodemap.
        //
        CBooleanPtr ptrChunkModeActive = nodeMap.GetNode("ChunkModeActive");

        if (!IsAvailable(ptrChunkModeActive) || !IsWritable(ptrChunkModeActive))
        {
            cout << "Unable to activate chunk mode. Aborting..." << endl << endl;
            return -1;
        }

        ptrChunkModeActive->SetValue(true);

        cout << "Chunk mode activated..." << endl;

        //
        // Enable all types of chunk data
        //
        // *** NOTES ***
        // Enabling chunk data requires working with nodes: "ChunkSelector"
        // is an enumeration selector node and "ChunkEnable" is a boolean. It
        // requires retrieving the selector node (which is of enumeration node
        // type), selecting the entry of the chunk data to be enabled, retrieving
        // the corresponding boolean, and setting it to true.
        //
        // In this example, all chunk data is enabled, so these steps are
        // performed in a loop. Once this is complete, chunk mode still needs to
        // be activated.
        //
        NodeList_t entries;

        // Retrieve the selector node
        CEnumerationPtr ptrChunkSelector = nodeMap.GetNode("ChunkSelector");

        if (!IsAvailable(ptrChunkSelector) || !IsReadable(ptrChunkSelector))
        {
            cout << "Unable to retrieve chunk selector. Aborting..." << endl << endl;
            return -1;
        }

        // Retrieve entries
        ptrChunkSelector->GetEntries(entries);

        cout << "Enabling entries..." << endl;

        for (size_t i = 0; i < entries.size(); i++)
        {
            // Select entry to be enabled
            CEnumEntryPtr ptrChunkSelectorEntry = entries.at(i);

            // Go to next node if problem occurs
            if (!IsAvailable(ptrChunkSelectorEntry) || !IsReadable(ptrChunkSelectorEntry))
            {
                continue;
            }

            ptrChunkSelector->SetIntValue(ptrChunkSelectorEntry->GetValue());

            cout << "\t" << ptrChunkSelectorEntry->GetSymbolic() << ": ";

            // Retrieve corresponding boolean
            CBooleanPtr ptrChunkEnable = nodeMap.GetNode("ChunkEnable");

            // Enable the boolean, thus enabling the corresponding chunk data
            if (!IsAvailable(ptrChunkEnable))
            {
                cout << "not available" << endl;
                result = -1;
            }
            else if (ptrChunkEnable->GetValue())
            {
                cout << "enabled" << endl;
            }
            else if (IsWritable(ptrChunkEnable))
            {
                ptrChunkEnable->SetValue(true);
                cout << "enabled" << endl;
            }
            else
            {
                cout << "not writable" << endl;
                result = -1;
            }
        }
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

// This function prints the device information of the camera from the transport
// layer; please see NodeMapInfo example for more in-depth comments on printing
// device information from the nodemap.
int PrintDeviceInfo(INodeMap & nodeMap)
{
    int result = 0;

    cout << endl << "*** DEVICE INFORMATION ***" << endl << endl;

    try
    {
        FeatureList_t features;
        CCategoryPtr category = nodeMap.GetNode("DeviceInformation");
        if (IsAvailable(category) && IsReadable(category))
        {
            category->GetFeatures(features);

            FeatureList_t::const_iterator it;
            for (it = features.begin(); it != features.end(); ++it)
            {
                CNodePtr pfeatureNode = *it;
                cout << pfeatureNode->GetName() << " : ";
                CValuePtr pValue = (CValuePtr)pfeatureNode;
                cout << (IsReadable(pValue) ? pValue->ToString() : "Node not readable");
                cout << endl;
            }
        }
        else
        {
            cout << "Device control information not available." << endl;
        }
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

// Calculate offset for BFS / Oryx and some other cameras
int64_t calculateOffset(CameraPtr pCam)
{
    const std::time_t now = std::time(nullptr);
    const std::tm calendar_time = *std::localtime(std::addressof(now));

    // Convert current calendar time into seconds
    int64_t hour_in_seconds = calendar_time.tm_hour * 3600;
    int64_t minute_in_seconds = calendar_time.tm_min * 60;
    int64_t current_time_in_seconds = hour_in_seconds + minute_in_seconds + calendar_time.tm_sec;
    cout << "Current time in seconds: " << current_time_in_seconds << endl;

    // Get current camera time using TimestampLatch
    CCommandPtr ptrTimestampLatch = pCam->GetNodeMap().GetNode("TimestampLatch");
    // Execute command
    ptrTimestampLatch->Execute();
    CIntegerPtr ptrTimestampLatchValue = pCam->GetNodeMap().GetNode("TimestampLatchValue");

    if (!IsAvailable(ptrTimestampLatchValue) || !IsReadable(ptrTimestampLatchValue)) {
        cout << " Unable to read Time Stamp Latch Value (node retrieval). Aborting.." << endl; return -1;
    }

    // Print out timestamp value
    int64_t timestampLatchValue = ptrTimestampLatchValue->GetValue();
    cout << "Timestamp Latch Value in ns: " << timestampLatchValue << endl;

    // Calculate offset = PC_TIME (s) - CAMERA_TIME(s)
    int64_t offset = current_time_in_seconds - (timestampLatchValue / 1000000000);
    cout << "Offset (current PC time(s) - camera timestamp(s)): " << offset << endl;

    return offset;
}

// Calculate offset for BFLY-U3, FL3-U3 and GS3-U3 cameras
int64_t calculateOffset_gen2USB(CameraPtr pCam)
{
    const std::time_t now = std::time(nullptr);
    const std::tm calendar_time = *std::localtime(std::addressof(now));

    int64_t hour_in_seconds = calendar_time.tm_hour * 3600;
    int64_t minute_in_seconds = calendar_time.tm_min * 60;
    int64_t current_time_in_seconds = hour_in_seconds + minute_in_seconds + calendar_time.tm_sec;

    // Convert current calendar time into seconds
    cout << "Current time in seconds: " << current_time_in_seconds << endl;

    // Get current camera time using TimestampLatch
    CCommandPtr ptrTimestampLatch = pCam->GetNodeMap().GetNode("TimestampLatch");
    // Execute command
    ptrTimestampLatch->Execute();
    CIntegerPtr ptrTimestampLatchValue = pCam->GetNodeMap().GetNode("Timestamp");

    if (!IsAvailable(ptrTimestampLatchValue) || !IsReadable(ptrTimestampLatchValue)) {
        cout << " Unable to read Time Stamp Latch Value (node retrieval). Aborting.." << endl; return -1;
    }

    // Print out timestamp value
    int64_t timestampLatchValue = ptrTimestampLatchValue->GetValue();
    cout << "Timestamp Latch Value in ns: " << timestampLatchValue << endl;

    // Calculate offset = PC_TIME (s) - CAMERA_TIME(s)
    int64_t offset = current_time_in_seconds - (timestampLatchValue / 1000000000);
    cout << "Offset (current PC time(s) - camera timestamp(s)): " << offset << endl;

    return offset;
}

// Calculate offset for BFLY-PGE and FL3-GE cameras
int64_t calculateOffset_GEV(CameraPtr pCam)
{
    const std::time_t now = std::time(nullptr);
    const std::tm calendar_time = *std::localtime(std::addressof(now));

    int64_t hour_in_seconds = calendar_time.tm_hour * 3600;
    int64_t minute_in_seconds = calendar_time.tm_min * 60;
    int64_t current_time_in_seconds = hour_in_seconds + minute_in_seconds + calendar_time.tm_sec;

    // Convert current calendar time into seconds
    cout << "Current time in seconds: " << current_time_in_seconds << endl;

    // Get current camera time using TimestampControlLatch
    CCommandPtr ptrTimestampLatch = pCam->GetNodeMap().GetNode("GevTimestampControlLatch");
    // Execute command
    ptrTimestampLatch->Execute();
    CIntegerPtr ptrTimestampLatchValue = pCam->GetNodeMap().GetNode("GevTimestampValue");

    if (!IsAvailable(ptrTimestampLatchValue) || !IsReadable(ptrTimestampLatchValue)) {
        cout << " Unable to read Time Stamp Latch Value (node retrieval). Aborting.." << endl; return -1;
    }

    // Print out timestamp value
    int64_t timestampLatchValue = ptrTimestampLatchValue->GetValue();
    cout << "Timestamp Latch Value in ns: " << timestampLatchValue << endl;

    // Calculate offset = PC_TIME (s) - CAMERA_TIME(s)
    int64_t offset = current_time_in_seconds - (timestampLatchValue / 1000000000);
    cout << "Offset (current PC time(s) - camera timestamp(s)): " << offset << endl;

    return offset;
}

// This function acquires and saves 10 images from a device; please see
// Acquisition example for more in-depth comments on acquiring images.
int AcquireImages(CameraPtr pCam, INodeMap & nodeMap, INodeMap & nodeMapTLDevice)
{
    int result = 0;

    cout << endl << endl << "*** IMAGE ACQUISITION ***" << endl << endl;

    try
    {
        // Set acquisition mode to continuous
        CEnumerationPtr ptrAcquisitionMode = nodeMap.GetNode("AcquisitionMode");
        if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
        {
            cout << "Unable to set acquisition mode to continuous (node retrieval). Aborting..." << endl << endl;
            return -1;
        }

        CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
        if (!IsAvailable(ptrAcquisitionModeContinuous) || !IsReadable(ptrAcquisitionModeContinuous))
        {
            cout << "Unable to set acquisition mode to continuous (entry 'continuous' retrieval). Aborting..." << endl << endl;
            return -1;
        }

        int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();

        ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

        cout << "Acquisition mode set to continuous..." << endl;

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

        CEnumerationPtr ptrDeviceType = nodeMapTLDevice.GetNode("DeviceType");
        if (!IsAvailable(ptrDeviceType) || !IsReadable(ptrDeviceType))
        {
            cout << "Error with reading the device's type. Aborting..." << endl << endl;
            return -1;
        }

        CStringPtr ptrDeviceModelName = nodeMap.GetNode("DeviceModelName");
        gcstring cameraModel = ptrDeviceModelName->ToString();
        cout << "Camera model is: " << cameraModel << endl;

        if (!IsAvailable(ptrDeviceModelName) || !IsReadable(ptrDeviceModelName))
        {
            cout << "Unable to determine camera family. Aborting..." << endl << endl;
            return -1;
        }

        int64_t hours, minutes;
        int64_t imageTimestamp;
        int64_t imageTimestamp_converted;

        // Retrieve, convert, and save images
        const unsigned int k_numImages = 10;

        for (unsigned int imageCnt = 0; imageCnt < k_numImages; imageCnt++)
        {
            try
            {
                // Retrieve next received image and ensure image completion

                ImagePtr pResultImage = pCam->GetNextImage();

                ChunkData chunkData = pResultImage->GetChunkData();

                if (pResultImage->IsIncomplete())
                {
                    cout << "Image incomplete with image status " << pResultImage->GetImageStatus() << "..." << endl;
                }
                else
                {
                    // Print image information
                    cout << "\nGrabbed image " << imageCnt << ", width = " << pResultImage->GetWidth() << ", height = "
                        << pResultImage->GetHeight() << endl;

                    // Retrieve timestamp from chunk data
                    uint64_t timestamp = chunkData.GetTimestamp();
                    cout << "Chunk Timestamp in ns: " << timestamp << endl;

                    // Get DeviceType
                    CEnumerationPtr ptrDeviceType = nodeMapTLDevice.GetNode("DeviceType");
                    if (!IsAvailable(ptrDeviceType) || !IsReadable(ptrDeviceType))
                    {
                        cout << "Error with reading the device's type. Aborting..." << endl << endl;
                        return -1;
                    }

                    // How to calcualte offset for GigE vision cameras
                    if (ptrDeviceType->GetIntValue() == DeviceType_GEV)
                    {
                        if (cameraModel.find("Blackfly S") == 0 || cameraModel.find("Oryx") == 0)
                        {
                            imageTimestamp_converted = (timestamp / 1000000000) + calculateOffset(pCam);
                        }
                        else
                        {
                            imageTimestamp_converted = (timestamp / 1000000000) + calculateOffset_GEV(pCam);
                        }
                    }

                    // How to calcualte offset for BFLY-U3, GS3-U3, FL3-U3 or CM3-U3
                    else if (cameraModel.find("Blackfly BFLY-U3") == 0 || cameraModel.find("Grasshopper3 GS3-U3") == 0
                        || cameraModel.find("Flea3 FL3-U3") == 0 || cameraModel.find("Chameleon3 CM3-U3") == 0)
                    {
                        imageTimestamp_converted = (timestamp / 1000000000) + calculateOffset_gen2USB(pCam);
                    }

                    // How to calculate offset for the rest of the cameras
                    else
                    {
                        imageTimestamp_converted = (timestamp / 1000000000) + calculateOffset(pCam);
                    }
                    cout << "PC timestamp in seconds (chunk timestamp + offset): " << imageTimestamp_converted << endl;

                    // Convert imageTimestamp to clock time
                    minutes = imageTimestamp_converted / 60;
                    hours = minutes / 60;

                    uint16_t minutes_converted = minutes % 60;
                    uint16_t seconds_converted = imageTimestamp_converted % 60;

                    cout << "ImageTimestamp " << imageTimestamp_converted << " (seconds) is equivalent to " << endl
                        << hours << " hours " << minutes_converted << " minutes " << seconds_converted << " seconds."
                        << endl;

                    // Convert image to mono 8
                    ImagePtr convertedImage = pResultImage->Convert(PixelFormat_Mono8, HQ_LINEAR);

                    // Create a unique filename
                    ostringstream filename;

                    filename << "ChunkData-";
                    if (deviceSerialNumber != "")
                    {
                        filename << deviceSerialNumber.c_str() << "-";
                        filename << hours << "-" << minutes_converted << "-" << seconds_converted << "-";
                    }
                    filename << imageCnt << ".jpg";

                    // Save image
                    convertedImage->Save(filename.str().c_str());

                    cout << "Image saved at " << filename.str() << endl;
                }

                // Release image
                pResultImage->Release();

                cout << endl;
            }
            catch (Spinnaker::Exception &e)
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

// This function disables each type of chunk data before disabling chunk data mode.
int DisableChunkData(INodeMap & nodeMap)
{
    int result = 0;

    try
    {
        NodeList_t entries;

        // Retrieve the selector node
        CEnumerationPtr ptrChunkSelector = nodeMap.GetNode("ChunkSelector");

        if (!IsAvailable(ptrChunkSelector) || !IsReadable(ptrChunkSelector))
        {
            cout << "Unable to retrieve chunk selector. Aborting..." << endl << endl;
            return -1;
        }

        // Retrieve entries
        ptrChunkSelector->GetEntries(entries);

        cout << "Disabling entries..." << endl;

        for (size_t i = 0; i < entries.size(); i++)
        {
            // Select entry to be disabled
            CEnumEntryPtr ptrChunkSelectorEntry = entries.at(i);

            // Go to next node if problem occurs
            if (!IsAvailable(ptrChunkSelectorEntry) || !IsReadable(ptrChunkSelectorEntry))
            {
                continue;
            }

            ptrChunkSelector->SetIntValue(ptrChunkSelectorEntry->GetValue());

            cout << "\t" << ptrChunkSelectorEntry->GetSymbolic() << ": ";

            // Retrieve corresponding boolean
            CBooleanPtr ptrChunkEnable = nodeMap.GetNode("ChunkEnable");

            // Disable the boolean, thus disabling the corresponding chunk data
            if (!IsAvailable(ptrChunkEnable))
            {
                cout << "not available" << endl;
                result = -1;
            }
            else if (!ptrChunkEnable->GetValue())
            {
                cout << "disabled" << endl;
            }
            else if (IsWritable(ptrChunkEnable))
            {
                ptrChunkEnable->SetValue(false);
                cout << "disabled" << endl;
            }
            else
            {
                cout << "not writable" << endl;
            }
        }
        cout << endl;

        // Deactivate ChunkMode
        CBooleanPtr ptrChunkModeActive = nodeMap.GetNode("ChunkModeActive");

        if (!IsAvailable(ptrChunkModeActive) || !IsWritable(ptrChunkModeActive))
        {
            cout << "Unable to deactivate chunk mode. Aborting..." << endl << endl;
            return -1;
        }

        ptrChunkModeActive->SetValue(false);

        cout << "Chunk mode deactivated..." << endl;
    }
    catch (Spinnaker::Exception &e)
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
    int err = 0;

    try
    {
        // Retrieve TL device nodemap and print device information
        INodeMap & nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

        result = PrintDeviceInfo(nodeMapTLDevice);

        // Initialize camera
        pCam->Init();

        // Retrieve GenICam nodemap
        INodeMap & nodeMap = pCam->GetNodeMap();

        // Configure chunk data
        err = ConfigureChunkData(nodeMap);
        if (err < 0)
        {
            return err;
        }

        // Acquire images and display chunk data
        result = result | AcquireImages(pCam, nodeMap, nodeMapTLDevice);

        // Disable chunk data
        err = DisableChunkData(nodeMap);
        if (err < 0)
        {
            return err;
        }

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
    FILE *tempFile = fopen("test.txt", "w+");
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

    int result = 0;

    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    // Print out current library version
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: "
        << spinnakerLibraryVersion.major << "."
        << spinnakerLibraryVersion.minor << "."
        << spinnakerLibraryVersion.type << "."
        << spinnakerLibraryVersion.build << endl << endl;

    // Retrieve list of cameras from the system
    CameraList camList = system->GetCameras();

    unsigned int numCameras = camList.GetSize();

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

    // Run example on each camera
    for (unsigned int i = 0; i < numCameras; i++)
    {
        cout << endl << "Running example for camera " << i << "..." << endl;

        result = result | RunSingleCamera(camList.GetByIndex(i));

        cout << "Camera " << i << " example complete..." << endl << endl;
    }

    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return result;
}