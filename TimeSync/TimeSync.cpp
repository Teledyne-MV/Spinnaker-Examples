//=============================================================================
// Copyright (c) 2001-2019 FLIR Systems, Inc. All Rights Reserved.
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

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

// This helper function allows the example to sleep in both Windows and Linux
// systems. Note that Windows sleep takes milliseconds as a parameter while
// Linux systems take microseconds as a parameter.
void SleepyWrapper(int milliseconds)
{
#if defined WIN32 || defined _WIN32 || defined WIN64 || defined _WIN64
    Sleep(milliseconds);
#else
    usleep(1000 * milliseconds);
#endif
}

// This function prints the device information of the camera from the transport
// layer; please see NodeMapInfo example for more in-depth comments on printing
// device information from the nodemap.
int PrintDeviceInfo(INodeMap& nodeMap, const unsigned int camNum)
{
    int result = 0;

    cout << "Printing device information for camera " << camNum << "..." << endl << endl;

    FeatureList_t features;
    const CCategoryPtr category = nodeMap.GetNode("DeviceInformation");
    if (IsAvailable(category) && IsReadable(category))
    {
        category->GetFeatures(features);

        for (FeatureList_t::const_iterator it = features.begin(); it != features.end(); ++it)
        {
            const CNodePtr pfeatureNode = *it;
            cout << pfeatureNode->GetName() << " : ";
            CValuePtr pValue = (CValuePtr)pfeatureNode;
            cout << (IsReadable(pValue) ? pValue->ToString() : "Node not readable");
            cout << endl;
        }
    }
    else
    {
        cout << "Device control information not available." << endl;
        result = -1;
    }
    cout << endl;

    return result;
}

// configures action device key, group key and group mask.
int ConfigureInterface(const InterfaceList& interfaceList)
{
    int result = 0;
    InterfacePtr interfacePtr = nullptr;

    cout << endl << endl << "*** CONFIGURING INTERFACE ***" << endl << endl;

    try
    {
        const unsigned int numInterfaces = interfaceList.GetSize();
        cout << "Number of Interface : " << numInterfaces << endl;

        for (unsigned int i = 0; i < numInterfaces; i++)
        {
            interfacePtr = interfaceList.GetByIndex(i);
            INodeMap& nodeMapInterface = interfacePtr->GetTLNodeMap();
            interfacePtr->UpdateCameras();
            CameraList camList = interfacePtr->GetCameras();

            // Check if interface has any camera
            if (camList.GetSize() >= 1)
            {
                // Display interface name
                CStringPtr ptrInterfaceDisplayName = nodeMapInterface.GetNode("InterfaceDisplayName");
                const gcstring interfaceDisplayName = ptrInterfaceDisplayName->GetValue();
                cout << interfaceDisplayName << endl;

                // Apply Action device/group settings
                CIntegerPtr ptrGevActionDeviceKey = nodeMapInterface.GetNode("GevActionDeviceKey");
                if (!IsAvailable(ptrGevActionDeviceKey) || !IsWritable(ptrGevActionDeviceKey))
                {
                    cout << "Interface " << i
                        << " Unable to set Interface Action Device Key (node retrieval). Aborting..." << endl;
                    return -1;
                }

                // Set Action Device Key to 0
                ptrGevActionDeviceKey->SetValue(0);
                cout << "Interface " << i << " action device key is set" << endl;

                CIntegerPtr ptrGevActionGroupKey = nodeMapInterface.GetNode("GevActionGroupKey");
                if (!IsAvailable(ptrGevActionGroupKey) || !IsWritable(ptrGevActionGroupKey))
                {
                    cout << "Interface " << i
                        << " Unable to set Interface Action Group Key (node retrieval). Aborting..." << endl;
                    return -1;
                }

                // Set Action Group Key to 1
                ptrGevActionGroupKey->SetValue(1);
                cout << "Interface " << i << " action group key is set" << endl;

                CIntegerPtr ptrGevActionGroupMask = nodeMapInterface.GetNode("GevActionGroupMask");
                if (!IsAvailable(ptrGevActionGroupMask) || !IsWritable(ptrGevActionGroupMask))
                {
                    cout << "Interface " << i
                        << " Unable to set Interface Action Group Mask (node retrieval). Aborting..." << endl;
                    return -1;
                }

                // Set Action Group Mask to 1
                ptrGevActionGroupMask->SetValue(1);
                cout << "Interface " << i << " action group mask is set" << endl;
            }
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }
    return result;
}

// This function configures IEEE 1588 settings on each camera
// It enables IEEE 1588
int ConfigureIEEE1588(const CameraList& camList)
{
    int result = 0;
    CameraPtr pCam = nullptr;

    cout << endl << endl << "*** CONFIGURING IEEE 1588 ***" << endl << endl;

    try
    {
        // Enable IEEE 1588 settings for each camera
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Enable IEEE 1588 settings
            CBooleanPtr ptrIEEE1588 = pCam->GetNodeMap().GetNode("GevIEEE1588");
            if (!IsAvailable(ptrIEEE1588) || !IsWritable(ptrIEEE1588))
            {
                cout << "Camera " << i << " Unable to enable IEEE 1588 (node retrieval). Aborting..." << endl;
                return -1;
            }

            // Enable IEEE 1588
            ptrIEEE1588->SetValue(true);
            cout << "Camera " << i << " IEEE 1588 is enabled." << endl;
        }

        // Requires delay for at least 6 seconds to enable 1588 settings

        cout << "Waiting for 10 seconds " << endl;
        SleepyWrapper(10000);

        // Check if IEEE 1588 settings is enabled for each camera
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);
            CCommandPtr ptrGevIEEE1588DataSetLatch = pCam->GetNodeMap().GetNode("GevIEEE1588DataSetLatch");
            if (!IsAvailable(ptrGevIEEE1588DataSetLatch))
            {
                cout << "Camera " << i << " Unable to execute IEEE 1588 data set latch (node retrieval). Aborting..."
                    << endl;
                return -1;
            }
            ptrGevIEEE1588DataSetLatch->Execute();

            // Check if 1588 status is not in intialization
            CEnumerationPtr ptrGevIEEE1588StatusLatched = pCam->GetNodeMap().GetNode("GevIEEE1588StatusLatched");
            if (!IsAvailable(ptrGevIEEE1588StatusLatched) || !IsReadable(ptrGevIEEE1588StatusLatched))
            {
                cout << "Camera " << i << " Unable to read IEEE1588 status (node retrieval). Aborting..." << endl;
                return -1;
            }

            CEnumEntryPtr ptrGevIEEE1588StatusLatchedInitializing =
                ptrGevIEEE1588StatusLatched->GetEntryByName("Initializing");
            if (!IsAvailable(ptrGevIEEE1588StatusLatchedInitializing) ||
                !IsReadable(ptrGevIEEE1588StatusLatchedInitializing))
            {
                cout << "Camera " << i << " Unable to get IEEE1588 status (enum entry retrieval). Aborting..." << endl;
                return -1;
            }

            if (ptrGevIEEE1588StatusLatched->GetIntValue() == ptrGevIEEE1588StatusLatchedInitializing->GetValue())
            {
                cout << "Camera" << i << " is in Initializing mode.." << endl;
                return -1;
            }

            // Check if camera(s) is(are) synchronized to master camera
            // Verify if camera offset from master is larger than 1000ns which means camera(s) is(are) not synchronized
            CIntegerPtr ptrGevIEEE1588OffsetFromMasterLatched =
                pCam->GetNodeMap().GetNode("GevIEEE1588OffsetFromMasterLatched");
            if (!IsAvailable(ptrGevIEEE1588OffsetFromMasterLatched) ||
                !IsReadable(ptrGevIEEE1588OffsetFromMasterLatched))
            {
                cout << "Camera " << i << " Unable to read IEEE1588 offset (node retrieval). Aborting..." << endl;
                return -1;
            }

            if (ptrGevIEEE1588OffsetFromMasterLatched->GetValue() > 1000)
            {
                cout << "Camera " << i << " has offset higher than 1000ns. Camera(s) is(are) not synchronized" << endl;
                return -1;
            }
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }
    return result;
}

// This function configures action control settings
// For each camera, it sets action device key, group key and group mask.
int ConfigureActionControl(const CameraList& camList)
{
    int result = 0;
    CameraPtr pCam = nullptr;

    cout << endl << endl << "*** CONFIGURING ACTION CONTROL ***" << endl << endl;

    try
    {
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Apply action group setting
            CIntegerPtr ptrActionDeviceKey = pCam->GetNodeMap().GetNode("ActionDeviceKey");
            if (!IsAvailable(ptrActionDeviceKey) || !IsWritable(ptrActionDeviceKey))
            {
                cout << "Camera " << i << " Unable to set Action Device Key (node retrieval). Aborting..." << endl;
                return -1;
            }

            // Set action device key to 0
            ptrActionDeviceKey->SetValue(0);
            cout << "Camera " << i << " action device key is set" << endl;

            CIntegerPtr ptrActionGroupKey = pCam->GetNodeMap().GetNode("ActionGroupKey");
            if (!IsAvailable(ptrActionGroupKey) || !IsWritable(ptrActionGroupKey))
            {
                cout << "Camera " << i << " Unable to set Action Group Key (node retrieval). Aborting..." << endl;
                return -1;
            }

            // Set action group key to 1
            ptrActionGroupKey->SetValue(1);
            cout << "Camera " << i << " action group key is set" << endl;

            CIntegerPtr ptrActionGroupMask = pCam->GetNodeMap().GetNode("ActionGroupMask");
            if (!IsAvailable(ptrActionGroupMask) || !IsWritable(ptrActionGroupMask))
            {
                cout << "Camera " << i << " Unable to retrieve Action Group Mask (node retrieval). Aborting...e"
                    << endl;
                return -1;
            }

            // Set action group mask to 1
            ptrActionGroupMask->SetValue(1);
            cout << "Camera " << i << " action group mask is set" << endl;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }
    return result;
}

// This function configures other nodes for frame synchronization.
// It sets acquisition frame rate , exposure settings,
// acquisition Timing and image timestamp
int ConfigureOtherNodes(const CameraList& camList)
{
    int result = 0;
    CameraPtr pCam = nullptr;

    cout << endl << endl << "*** CONFIGURING OTHER NODES ***" << endl << endl;

    try
    {
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Frame rate setting
            // Turn on frame rate control
            CBooleanPtr ptrFrameRateEnable = pCam->GetNodeMap().GetNode("AcquisitionFrameRateEnable");
            if (!IsAvailable(ptrFrameRateEnable) || !IsWritable(ptrFrameRateEnable))
            {
                cout << "Camera " << i << " Unable to enable Acquisition Frame Rate (node retrieval). Aborting..."
                    << endl;
                return -1;
            }

            // Enable  Acquisition Frame Rate Enable
            ptrFrameRateEnable->SetValue(true);

            CFloatPtr ptrFrameRate = pCam->GetNodeMap().GetNode("AcquisitionFrameRate");
            if (!IsAvailable(ptrFrameRate) || !IsWritable(ptrFrameRate))
            {
                cout << "Camera " << i << " Unable to set Acquisition Frame Rate (node retrieval). Aborting..." << endl;
                return -1;
            }

            // Set 10fps for this example
            const float frameRate = 10.0f;
            ptrFrameRate->SetValue(frameRate);
            cout << "Camera " << i << " Frame rate is set to " << frameRate << endl;

            // Turn off exposure auto.
            CEnumerationPtr ptrExposureAuto = pCam->GetNodeMap().GetNode("ExposureAuto");
            if (!IsAvailable(ptrExposureAuto) || !IsWritable(ptrExposureAuto))
            {
                cout << "Camera " << i << " Unable to disable Exposure Auto (node retrieval). Aborting..." << endl;
                return -1;
            }

            CEnumEntryPtr ptrExposureAutoOff = ptrExposureAuto->GetEntryByName("Off");
            if (!IsAvailable(ptrExposureAutoOff) || !IsReadable(ptrExposureAutoOff))
            {
                cout << "Camera " << i << " Unable to set Exposure Auto (enum entry retrieval). Aborting..." << endl;
                return -1;
            }

            // Turn off Exposure Auto
            ptrExposureAuto->SetIntValue(ptrExposureAutoOff->GetValue());

            CFloatPtr ptrExposureTime = pCam->GetNodeMap().GetNode("ExposureTime");
            if (IsAvailable(ptrExposureTime) && IsWritable(ptrExposureTime))
            {
                // Set exposure time to 1000 for this example
                const float exposureTime = 1000.0f;
                ptrExposureTime->SetValue(exposureTime);
                cout << "Camera " << i << " Exposure time is set to " << exposureTime << endl;
            }
            else
            {
                cout << "Camera " << i << " Unable to set Exposure Time (node retrieval). Aborting..." << endl;
            }
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }
    return result;
}

// This function configures chunk data settings
int ConfigureChunkData(const CameraList& camList)
{
    int result = 0;
    CameraPtr pCam = nullptr;

    cout << endl << endl << "*** CONFIGURING CHUNK DATA ***" << endl << endl;

    try
    {
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            //
            // Activate chunk mode
            //
            // *** NOTES ***
            // Once enabled, chunk data will be available at the end of the payload
            // of every image captured until it is disabled. Chunk data can also be
            // retrieved from the nodemap.
            //

            CBooleanPtr ptrChunkModeActive = pCam->GetNodeMap().GetNode("ChunkModeActive");
            if (!IsAvailable(ptrChunkModeActive) || !IsWritable(ptrChunkModeActive))
            {
                cout << "Camera " << i << " Unable to activate chunk mode. Aborting..." << endl << endl;
                return -1;
            }

            ptrChunkModeActive->SetValue(true);

            cout << "Camera " << i << " Chunk mode activated..." << endl;

            CEnumerationPtr ptrChunkSelector = pCam->GetNodeMap().GetNode("ChunkSelector");
            if (!IsAvailable(ptrChunkSelector) || !IsWritable(ptrChunkSelector))
            {
                cout << "Camera " << i << " Chunk Selector is not writable" << endl;
                return -1;
            }

            // Select Timestamp for Chunk data
            CEnumEntryPtr ptrChunkSelectorTimestamp = ptrChunkSelector->GetEntryByName("Timestamp");
            if (!IsAvailable(ptrChunkSelectorTimestamp) || !IsReadable(ptrChunkSelectorTimestamp))
            {
                cout << "Camera " << i << " Unable to set Chunk Selector (node retrieval). Aborting..." << endl;
                return -1;
            }

            ptrChunkSelector->SetIntValue(ptrChunkSelectorTimestamp->GetValue());

            // Retrieve corresponding boolean
            CBooleanPtr ptrChunkEnable = pCam->GetNodeMap().GetNode("ChunkEnable");

            // Enable the boolean, thus enabling the corresponding chunk data
            if (!IsAvailable(ptrChunkEnable))
            {
                cout << "Camera " << i << " not available" << endl;
                result = -1;
            }
            else if (ptrChunkEnable->GetValue())
            {
                cout << "Camera " << i << " enabled" << endl;
            }
            else if (IsWritable(ptrChunkEnable))
            {
                ptrChunkEnable->SetValue(true);
                cout << "Camera " << i << " enabled" << endl;
            }
            else
            {
                cout << "Camera " << i << " not writable" << endl;
                result = -1;
            }
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }
    return result;
}

int AcquireImages(const SystemPtr& system, const InterfaceList& interfaceList, CameraList camList)
{
    int result = 0;
    CameraPtr pCam = nullptr;
    InterfacePtr interfacePtr = nullptr;

    cout << endl << "*** IMAGE ACQUISITION ***" << endl << endl;

    try
    {
        //
        // Prepare each camera to acquire images
        //
        // *** NOTES ***
        // For pseudo-simultaneous streaming, each camera is prepared as if it
        // were just one, but in a loop. Notice that cameras are selected with
        // an index. We demonstrate pseudo-simultaneous streaming because true
        // simultaneous streaming would require multiple process or threads,
        // which is too complex for an example.
        //
        // Serial numbers are the only persistent objects we gather in this
        // example, which is why a vector is created.
        //
        vector<gcstring> strSerialNumbers(camList.GetSize());

        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Set acquisition mode to continuous
            CEnumerationPtr ptrAcquisitionMode = pCam->GetNodeMap().GetNode("AcquisitionMode");
            if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
            {
                cout << "Unable to set acquisition mode to continuous (node retrieval; camera " << i << "). Aborting..."
                    << endl
                    << endl;
                return -1;
            }

            CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
            if (!IsAvailable(ptrAcquisitionModeContinuous) || !IsReadable(ptrAcquisitionModeContinuous))
            {
                cout << "Unable to set acquisition mode to continuous (entry 'continuous' retrieval " << i
                    << "). Aborting..." << endl
                    << endl;
                return -1;
            }

            const int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();

            ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

            cout << "Camera " << i << " acquisition mode set to continuous..." << endl;

            // Device Link Throughput Limit setting
            CIntegerPtr ptrDeviceLinkThroughputLimit = pCam->GetNodeMap().GetNode("DeviceLinkThroughputLimit");
            if (!IsAvailable(ptrDeviceLinkThroughputLimit) || !IsWritable(ptrDeviceLinkThroughputLimit))
            {
                cout << "Unable to set device link throughput limit (node retrieval; camera " << i << "). Aborting..."
                    << endl
                    << endl;
                return -1;
            }
            // Set throughput close to the maximum limit 125000000
            const int64_t deviceLinkThroughputLimit = 100000000;
            ptrDeviceLinkThroughputLimit->SetValue(deviceLinkThroughputLimit);

            // Packet size, Packet delay setting
            CIntegerPtr ptrPacketSize = pCam->GetNodeMap().GetNode("GevSCPSPacketSize");
            if (!IsAvailable(ptrPacketSize) || !IsWritable(ptrPacketSize))
            {
                cout << "Unable to set packet size (node retrieval; camera " << i << "). Aborting..." << endl << endl;
                return -1;
            }
            // Set packet size to maximum value
            const int64_t packetSize = 9000;
            ptrPacketSize->SetValue(packetSize);

            CIntegerPtr ptrPacketDelay = pCam->GetNodeMap().GetNode("GevSCPD");
            if (!IsAvailable(ptrPacketDelay) || !IsWritable(ptrPacketDelay))
            {
                cout << "Unable to set packet delay (node retrieval; camera " << i << "). Aborting..." << endl << endl;
                return -1;
            }

            // Set packet delay 9000 by using formula packet delay x 8
            // for BFS only, there is 8 bit internal tick
            // for Gen 2 cameras , just use packet delay 9000
            const int64_t packetDelay = 72000;
            ptrPacketDelay->SetValue(packetDelay);

            // Begin acquiring images
            pCam->BeginAcquisition();

            cout << "Camera " << i << " started acquiring images..." << endl;

            // Retrieve device serial number for filename
            strSerialNumbers[i] = "";

            CStringPtr ptrStringSerial = pCam->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");

            if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
            {
                strSerialNumbers[i] = ptrStringSerial->GetValue();
                cout << "Camera " << i << " serial number set to " << strSerialNumbers[i] << "..." << endl;
            }
            cout << endl;
        }

        const unsigned int numInterfaces = interfaceList.GetSize();
        for (unsigned int i = 0; i < numInterfaces; i++)
        {
            interfacePtr = interfaceList.GetByIndex(i);
            INodeMap& nodeMapInterface = interfacePtr->GetTLNodeMap();
            interfacePtr->UpdateCameras();
            camList = interfacePtr->GetCameras();
            //
            // Retrieve, convert, and save images for each camera
            //
            // *** NOTES ***
            // In order to work with simultaneous camera streams, nested loops are
            // needed. It is important that the inner loop be the one iterating
            // through the cameras; otherwise, all images will be grabbed from a
            // single camera before grabbing any images from another.
            //
            const unsigned int k_numImages = 10;
            for (unsigned int imageCnt = 0; imageCnt < k_numImages; imageCnt++)
            {
                for (unsigned int index = 0; index < camList.GetSize(); index++)
                {
                    try
                    {
                        // Select camera
                        pCam = camList.GetByIndex(index);

                        // Retrieve next received image and ensure image completion
                        ImagePtr pResultImage = pCam->GetNextImage();

                        if (pResultImage->IsIncomplete())
                        {
                            cout << "Image incomplete with image status " << pResultImage->GetImageStatus() << "..."
                                << endl
                                << endl;
                        }
                        else
                        {
                            // Print image information
                            cout << "Camera " << index << " grabbed image " << imageCnt << endl;
                        }

                        // Get timestamp
                        ChunkData chunkData = pResultImage->GetChunkData();

                        // Retrieve timestamp
                        const int64_t timestamp = chunkData.GetTimestamp();
                        cout << "\tTimestamp: " << timestamp << endl;

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
            }

            //
            // End acquisition for each camera
            //
            // *** NOTES ***
            // Notice that what is usually a one-step process is now two steps
            // because of the additional step of selecting the camera. It is worth
            // repeating that camera selection needs to be done once per loop.
            //
            // It is possible to interact with cameras through the camera list with
            // GetByIndex(); this is an alternative to retrieving cameras as
            // CameraPtr objects that can be quick and easy for small tasks.
            //

            // End acquisition
            for (unsigned int index = 0; index < camList.GetSize(); index++)
            {
                camList.GetByIndex(index)->EndAcquisition();
            }
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
int RunMultipleCameras(const SystemPtr& system, const InterfaceList& interfaceList, const CameraList& camList)
{
    int result = 0;
    CameraPtr pCam = nullptr;

    try
    {
        //
        // Retrieve transport layer nodemaps and print device information for
        // each camera
        //
        // *** NOTES ***
        // This example retrieves information from the transport layer nodemap
        // twice: once to print device information and once to grab the device
        // serial number. Rather than caching the nodemap, each nodemap is
        // retrieved both times as needed.
        //
        cout << endl << "*** DEVICE INFORMATION ***" << endl << endl;

        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Retrieve TL device nodemap
            INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

            // Print device information
            result = PrintDeviceInfo(nodeMapTLDevice, i);
        }
        //
        // Initialize each camera
        //
        // *** NOTES ***
        // You may notice that the steps in this function have more loops with
        // less steps per loop; this contrasts the AcquireImages() function
        // which has less loops but more steps per loop. This is done for
        // demonstrative purposes as both work equally well.
        //
        // *** LATER ***
        // Each camera needs to be deinitialized once all images have been
        // acquired.
        //
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Initialize camera
            pCam->Init();
        }

        // Configure Interface Settings
        result = ConfigureInterface(interfaceList);
        if (result < 0)
        {
            return result;
        }

        // Configure IEEE 1588 settings
        result = ConfigureIEEE1588(camList);
        if (result < 0)
        {
            return result;
        }

        // Configure Action control settings
        result = ConfigureActionControl(camList);
        if (result < 0)
        {
            return result;
        }
        
        // Configure other node settings for frame synchronization
        result = ConfigureOtherNodes(camList);
        if (result < 0)
        {
            return result;
        }

        // Configure chunk data
        result = ConfigureChunkData(camList);
        if (result < 0)
        {
            return result;
        }

        // Acquire images on all cameras
        result = AcquireImages(system, interfaceList, camList);
        if (result < 0)
        {
            return result;
        }

        //
        // Deinitialize each camera
        //
        // *** NOTES ***
        // Again, each camera must be deinitialized separately by first
        // selecting the camera and then deinitializing it.
        //
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Deinitialize camera
            pCam->DeInit();
        }
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
    int result = 0;

    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    // Print out current library version
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
        << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << endl
        << endl;

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

    // Get Interface
    InterfaceList interfaceList = system->GetInterfaces();

    result = RunMultipleCameras(system, interfaceList, camList);

    cout << "Example complete..." << endl << endl;

    // Clear camera list before releasing system
    camList.Clear();

    interfaceList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return result;
}