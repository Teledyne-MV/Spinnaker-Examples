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

/**
*  @example Synchronized.cpp
*
*  @brief Synchronized.cpp shows how to setup multiple FLIR Machine Vision cameras
*  in a primary/secondary configuration, synchronizing image capture.  It 
*  relies on users to have followed the hardware layout defined on the FLIR
*  IIS article, "Configuring Synchronized Capture with Multiple Cameras".
*
*  This examples accounts for GPIO layout and other differences between camera 
*  families, automatically setting the correct settings depending upon the camera
*  family being used.  The cameras are initially set to factory default settings,
*  and reset back to factory default settings upon example completion as well.
*
*/
#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream> 
#include <vector>
#include <iomanip>
#include <omp.h>
#include <direct.h>
#include <string>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

// Global constants and objects
const unsigned int k_numImages = 10;
const unsigned int k_grabTimeout = 800;
const unsigned int k_packetSize = 9000;

const double k_exposureTime = 50000.0; // Units in micro seconds; should not exceed 1,000,000/k_frameRate.
const double k_gain = 3.0;
const double k_frameRate = 10.0;
const char* k_pixelFormat = "Mono8";

const gcstring cameraFamilyFFY = "FFY";
const gcstring cameraFamilyBFS = "BFS";
const gcstring cameraFamilyORX = "ORX";
const gcstring cameraFamilyBFLY = "BFLY";
const gcstring cameraFamilyCM3 = "CM3";
const gcstring cameraFamilyGS3 = "GS3";
const gcstring cameraFamilyFL3 = "FL3";

int PrintBuildInfo(SystemPtr system)
{
    int result = 0;

    // Print out current library version
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: "
        << spinnakerLibraryVersion.major << "."
        << spinnakerLibraryVersion.minor << "."
        << spinnakerLibraryVersion.type << "."
        << spinnakerLibraryVersion.build << endl;

    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    return result;
}

int PrintDeviceInfo(INodeMap & nodeMap, unsigned int camIndex)
{
    int result = 0;

    cout << "*** CAMERA " << camIndex << " ***" << endl;

    FeatureList_t features;

    CCategoryPtr pCategory = nodeMap.GetNode("DeviceInformation");
    if (IsAvailable(pCategory) && IsReadable(pCategory))
    {
        pCategory->GetFeatures(features);
        FeatureList_t::const_iterator it;
        for (it = features.begin(); it != features.end(); ++it)
        {
            CNodePtr pFeatureNode = *it;
            cout << pFeatureNode->GetName() << " : ";
            CValuePtr pValue = (CValuePtr)pFeatureNode;
            cout << (IsReadable(pValue) ? pValue->ToString() : "Node not readable");
            cout << endl;
        }
    }
    else
    {
        cout << "Device control information not available!!" << endl;
    }
    cout << endl;

    return result;
}

bool SetVideoMode(INodeMap & nodeMap)
{
    CEnumerationPtr pVideoMode = nodeMap.GetNode("VideoMode");
    if (!IsAvailable(pVideoMode) || !IsWritable(pVideoMode))
    {
        cout << "[Video Mode] node not available!!" << endl;
        return false;
    }

    CEnumEntryPtr pVideoMode0 = pVideoMode->GetEntryByName("Mode0");
    if (!IsAvailable(pVideoMode0) || !IsReadable(pVideoMode0))
    {
        cout << "Unable to set [Video Mode] to [Mode0]. Aborting..." << endl;
        return false;
    }

    pVideoMode->SetIntValue(pVideoMode0->GetValue());
    cout << "Video Mode:                    " << pVideoMode->GetCurrentEntry()->GetSymbolic() << endl;

    return true;
}

bool SetPacketSize(INodeMap & nodeMap)
{
    CIntegerPtr ptrPacketSize = nodeMap.GetNode("GevSCPSPacketSize");
    if (!IsAvailable(ptrPacketSize) || !IsWritable(ptrPacketSize))
    {
        cout << "[GEV SCPS Packet Size] node not available!!" << endl;
        return false;
    }

    ptrPacketSize->SetValue(k_packetSize);
    cout << "Packet Size: " << ptrPacketSize->GetValue() << endl;

    return true;
}

bool EnableV3_3(INodeMap & nodeMap, gcstring cameraModel)
{
    // Set Line Selector to Line2 before enable 3.3V output, this step is necessary for BFS
    if (cameraModel.find(cameraFamilyBFS) != std::string::npos)
    {
        CEnumerationPtr ptrLineSelector = nodeMap.GetNode("LineSelector");
        if (!IsAvailable(ptrLineSelector) || !IsWritable(ptrLineSelector))
        {
            cout << "[Line Selector] node not available!!" << endl;
            return false;
        }

        CEnumEntryPtr ptrLineSelectorLine2 = ptrLineSelector->GetEntryByName("Line2");
        if (!IsAvailable(ptrLineSelectorLine2) || !IsReadable(ptrLineSelectorLine2))
        {
            cout << "Unable to set [Line Selector] to [Line2]. Aborting..." << endl;
        }
        else
        {
            ptrLineSelector->SetIntValue(ptrLineSelectorLine2->GetValue());
            cout << "Line Selector: " << ptrLineSelector->GetCurrentEntry()->GetSymbolic() << endl;
        }
    }

    // Enable 3.3V output
    CBooleanPtr ptrV3_3Enable = nodeMap.GetNode("V3_3Enable");
    if (!IsAvailable(ptrV3_3Enable) || !IsWritable(ptrV3_3Enable))
    {
        cout << "[3.3V Enable] node not available!!" << endl;
        return false;
    }

    ptrV3_3Enable->SetValue(true);
    cout << "3.3V Enable: " << ptrV3_3Enable->GetValue() << endl;

    return true;
}

// Set acquisition mode, exposure time and framerate
int ConfigureCameraSettings(INodeMap & nodeMap, gcstring cameraModel)
{
    int result = 0;

    try
    {
        // Set Acquisition Mode to Continuous
        CEnumerationPtr ptrAcquisitionMode = nodeMap.GetNode("AcquisitionMode");
        if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
        {
            cout << "[Acquisition Mode] node not available!!" << endl;
            return -1;
        }

        CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
        if (!IsAvailable(ptrAcquisitionModeContinuous) || !IsReadable(ptrAcquisitionModeContinuous))
        {
            cout << "Unable to set [Acquisition Mode] to [Continuous]. Aborting..." << endl;
            return -1;
        }

        ptrAcquisitionMode->SetIntValue(ptrAcquisitionModeContinuous->GetValue());
        cout << "Acquisition Mode: " << ptrAcquisitionMode->GetCurrentEntry()->GetSymbolic() << endl;

        // Turn off Exposure Auto
        CEnumerationPtr ptrExposureAuto = nodeMap.GetNode("ExposureAuto");
        if (!IsAvailable(ptrExposureAuto) || !IsWritable(ptrExposureAuto))
        {
            cout << "[Exposure Auto] node not available!!" << endl;
            return -1;
        }

        CEnumEntryPtr ptrExposureAutoOff = ptrExposureAuto->GetEntryByName("Off");
        if (!IsAvailable(ptrExposureAutoOff) || !IsReadable(ptrExposureAutoOff))
        {
            cout << "Unable to set [Exposure Auto] to [Off]. Aborting..." << endl;
            return -1;
        }

        ptrExposureAuto->SetIntValue(ptrExposureAutoOff->GetValue());
        cout << "Exposure Auto: " << ptrExposureAuto->GetCurrentEntry()->GetSymbolic() << endl;

        // Set Exposure Time
        CFloatPtr ptrExposureTime = nodeMap.GetNode("ExposureTime");
        if (!IsAvailable(ptrExposureTime) || !IsWritable(ptrExposureTime))
        {
            cout << "[Exposure Time] node not available!!" << endl;
            return -1;
        }

        ptrExposureTime->SetValue(k_exposureTime);

        cout << "Exposure Time:                 " << ptrExposureTime->GetValue() << " us" << endl;

        // Turn on Acquisition Frame Rate Enable
        CBooleanPtr ptrFrameRateEnable = nodeMap.GetNode("AcquisitionFrameRateEnable");
        if (ptrFrameRateEnable == NULL)
        {
            // Acquisition Frame Rate Enabled (with a "d" at the end) is used for Gen2 cameras
            ptrFrameRateEnable = nodeMap.GetNode("AcquisitionFrameRateEnabled");
        }

        if (IsAvailable(ptrFrameRateEnable) && IsWritable(ptrFrameRateEnable))
        {
            ptrFrameRateEnable->SetValue(true);
            cout << "Acquisition Frame Rate Enable: " << ptrFrameRateEnable->GetValue() << endl;
        }
        else
        {
            cout << "[Acquisition Frame Rate Enable] node not available!!" << endl;
            return false;
        }

        // Turn off Frame Rate Auto, this step is only necessary for Gen2 cameras
        if (cameraModel.find(cameraFamilyBFLY) != std::string::npos || cameraModel.find(cameraFamilyCM3) != std::string::npos ||
            cameraModel.find(cameraFamilyFL3) != std::string::npos || cameraModel.find(cameraFamilyGS3) != std::string::npos)
        {
            CEnumerationPtr ptrFrameRateAuto = nodeMap.GetNode("AcquisitionFrameRateAuto");
            if (!IsAvailable(ptrFrameRateAuto) || !IsWritable(ptrFrameRateAuto))
            {
                cout << "[Frame Rate Auto] node not available!!" << endl;
                return false;
            }

            CEnumEntryPtr ptrFrameRateAutoOff = ptrFrameRateAuto->GetEntryByName("Off");
            if (!IsAvailable(ptrFrameRateAutoOff) || !IsReadable(ptrFrameRateAutoOff))
            {
                cout << "Unable to set [Frame Rate Auto] to [Off]. Aborting..." << endl;
                return false;
            }

            ptrFrameRateAuto->SetIntValue(ptrFrameRateAutoOff->GetValue());
            cout << "Frame Rate Auto: " << ptrFrameRateAuto->GetCurrentEntry()->GetSymbolic() << endl;
        }

        // Set Acquisition Frame Rate
        CFloatPtr ptrAcquisitionFrameRate = nodeMap.GetNode("AcquisitionFrameRate");
        if (!IsAvailable(ptrAcquisitionFrameRate) || !IsWritable(ptrAcquisitionFrameRate))
        {
            cout << "[Acquisition Frame Rate] node not available!!" << endl;
            return -1;
        }

        const double frameRateMax = ptrAcquisitionFrameRate->GetMax();
        double frameRateToSet = k_frameRate;

        if (frameRateToSet > frameRateMax)
        {
            cout << "Error setting framerate (user value exceeds camera's current max framerate); setting framerate to camera's current max framerate" << endl;
            frameRateToSet = frameRateMax;
        }

        ptrAcquisitionFrameRate->SetValue(frameRateToSet);
        cout << "Frame Rate: " << ptrAcquisitionFrameRate->GetValue() << " Hz" << endl;

    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

// Configure Timestamp and Frame ID Chunk Data
int ConfigureChunkData(INodeMap & nodeMap)
{
    int result = 0;
    CameraPtr pCam = nullptr;

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

        CEnumerationPtr ptrChunkSelector = nodeMap.GetNode("ChunkSelector");
        if (!IsAvailable(ptrChunkSelector) || !IsWritable(ptrChunkSelector))
        {
            cout << "Chunk Selector is not writable" << endl;
            return -1;
        }

        // Enable Timestamp Chunk Data
        CEnumEntryPtr ptrChunkSelectorTimestamp = ptrChunkSelector->GetEntryByName("Timestamp");
        if (!IsAvailable(ptrChunkSelectorTimestamp) || !IsReadable(ptrChunkSelectorTimestamp))
        {
            cout << "Unable to set Chunk Selector (node retrieval). Aborting..." << endl;
            return -1;
        }

        ptrChunkSelector->SetIntValue(ptrChunkSelectorTimestamp->GetValue());

        // Retrieve corresponding boolean
        CBooleanPtr ptrChunkEnable = nodeMap.GetNode("ChunkEnable");

        // Enable the boolean, thus enabling the corresponding chunk data
        if (IsWritable(ptrChunkEnable))
        {
            ptrChunkEnable->SetValue(true);
            cout << "Chunk data enabled" << endl;
        }
        else
        {
            cout << "Chunk data node not writable" << endl;
            result = -1;
        }

        // Enable Frame ID Chunk Data
        CEnumEntryPtr ptrChunkSelectorFrameID = ptrChunkSelector->GetEntryByName("FrameID");
        if (ptrChunkSelectorFrameID == NULL)
        {
            // Frame Counter is used for Gen2 cameras
            ptrChunkSelectorFrameID = ptrChunkSelector->GetEntryByName("FrameCounter");
        }

        ptrChunkSelector->SetIntValue(ptrChunkSelectorFrameID->GetValue());

        // Enable the boolean, thus enabling the corresponding chunk data
        if (IsWritable(ptrChunkEnable))
        {
            ptrChunkEnable->SetValue(true);
            cout << "Chunk data enabled" << endl;
        }
        else
        {
            cout << "Chunk data node not writable" << endl;
            result = -1;
        }

    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }
    return result;
}

// Configure Primary camera Digital Output Control settings
int SetupPrimaryCam(INodeMap & nodeMap, gcstring cameraModel)
{
    int result = 0;

    try
    {
        // Enable 3.3V output for BFLY or BFS cameras
        if (cameraModel.find(cameraFamilyBFLY) != std::string::npos || cameraModel.find(cameraFamilyBFS) != std::string::npos)
        {
            EnableV3_3(nodeMap, cameraModel);
        }

        // Set Line Selector to appropriate line (only necessary for non-BFS/BFLy cameras)
        CEnumerationPtr ptrLineSelector = nodeMap.GetNode("LineSelector");
        if (!IsAvailable(ptrLineSelector) || !IsWritable(ptrLineSelector))
        {
            cout << "[Line Selector] node not available!!" << endl;
            return -1;
        }

        if (cameraModel.find(cameraFamilyCM3) != std::string::npos || cameraModel.find(cameraFamilyFL3) != std::string::npos ||
            cameraModel.find(cameraFamilyGS3) != std::string::npos || cameraModel.find(cameraFamilyORX) != std::string::npos ||
            cameraModel.find(cameraFamilyFFY) != std::string::npos)
        {
            CEnumEntryPtr ptrLineSelectorLine2 = ptrLineSelector->GetEntryByName("Line2");

            if (!IsAvailable(ptrLineSelectorLine2) || !IsReadable(ptrLineSelectorLine2))
            {
                cout << "Unable to set [Line Selector] to [Line2]. Aborting..." << endl;
                return -1;
            }
            ptrLineSelector->SetIntValue(ptrLineSelectorLine2->GetValue());
            cout << "Line Selector:                 " << ptrLineSelector->GetCurrentEntry()->GetSymbolic() << endl;
        }

        // Set Line Mode to Output
        CEnumerationPtr ptrLineMode = nodeMap.GetNode("LineMode");
        if (!IsAvailable(ptrLineMode) || !IsWritable(ptrLineMode))
        {
            cout << "[Line Mode] node not available!!" << endl;
            return -1;
        }

        CEnumEntryPtr ptrLineModeOutput = ptrLineMode->GetEntryByName("Output");
        if (!IsAvailable(ptrLineModeOutput) || !IsReadable(ptrLineModeOutput))
        {
            cout << "Unable to set [Line Source] to [ExposureActive]. Aborting..." << endl;
            return -1;
        }

        ptrLineMode->SetIntValue(ptrLineModeOutput->GetValue());

        // Set Line Source to ExposureActive
        CEnumerationPtr ptrLineSource = nodeMap.GetNode("LineSource");
        if (!IsAvailable(ptrLineSource) || !IsWritable(ptrLineSource))
        {
            cout << "[Line Source] node not available!!" << endl;
            return -1;
        }

        CEnumEntryPtr ptrLineSourceExposureActive = ptrLineSource->GetEntryByName("ExposureActive");
        if (!IsAvailable(ptrLineSourceExposureActive) || !IsReadable(ptrLineSourceExposureActive))
        {
            cout << "Unable to set [Line Source] to [ExposureActive]. Aborting..." << endl;
            return -1;
        }

        ptrLineSource->SetIntValue(ptrLineSourceExposureActive->GetValue());
        cout << "Line Source:                   " << ptrLineSource->GetCurrentEntry()->GetSymbolic() << endl;

    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

int SetupSecondaryCam(INodeMap & nodeMap, gcstring cameraModel)
{
    int result = 0;

    try
    {

        // Set Trigger Mode to On
        CEnumerationPtr ptrTriggerMode = nodeMap.GetNode("TriggerMode");
        if (!IsAvailable(ptrTriggerMode) || !IsWritable(ptrTriggerMode))
        {
            cout << "[Trigger Mode] node not available!!" << endl;
            return -1;
        }

        CEnumEntryPtr ptrTriggerModeOn = ptrTriggerMode->GetEntryByName("On");
        if (!IsAvailable(ptrTriggerModeOn) || !IsReadable(ptrTriggerModeOn))
        {
            cout << "Unable to set [Trigger Mode] to [On]. Aborting..." << endl;
            return -1;
        }

        ptrTriggerMode->SetIntValue(ptrTriggerModeOn->GetValue());
        cout << "Trigger Mode:                  " << ptrTriggerMode->GetCurrentEntry()->GetSymbolic() << endl;

        // Set Trigger Source to Line3
        CEnumerationPtr ptrTriggerSource = nodeMap.GetNode("TriggerSource");
        if (!IsAvailable(ptrTriggerSource) || !IsWritable(ptrTriggerSource))
        {
            cout << "[Trigger Source] node not available!!" << endl;
            return -1;
        }

        if (cameraModel.find(cameraFamilyBFS) != std::string::npos || cameraModel.find(cameraFamilyCM3) != std::string::npos ||
            cameraModel.find(cameraFamilyFL3) != std::string::npos || cameraModel.find(cameraFamilyGS3) != std::string::npos ||
            cameraModel.find(cameraFamilyFFY) != std::string::npos)
        {
            CEnumEntryPtr ptrTriggerSourceLine3 = ptrTriggerSource->GetEntryByName("Line3");
            if (!IsAvailable(ptrTriggerSourceLine3) || !IsReadable(ptrTriggerSourceLine3))
            {
                cout << "Unable to set [Trigger Source] to [Line3]. Aborting..." << endl;
                return -1;
            }

            ptrTriggerSource->SetIntValue(ptrTriggerSourceLine3->GetValue());
            cout << "Trigger Source:                " << ptrTriggerSource->GetCurrentEntry()->GetSymbolic() << endl;
        }
        else if (cameraModel.find(cameraFamilyORX) != std::string::npos)
        {
            CEnumEntryPtr ptrTriggerSourceLine5 = ptrTriggerSource->GetEntryByName("Line5");
            if (!IsAvailable(ptrTriggerSourceLine5) || !IsReadable(ptrTriggerSourceLine5))
            {
                cout << "Unable to set [Trigger Source] to [Line5]. Aborting..." << endl;
                return -1;
            }

            ptrTriggerSource->SetIntValue(ptrTriggerSourceLine5->GetValue());
            cout << "Trigger Source:                " << ptrTriggerSource->GetCurrentEntry()->GetSymbolic() << endl;
        }

        // Set Trigger Activation to Rising Edge
        CEnumerationPtr ptrTriggerActivation = nodeMap.GetNode("TriggerActivation");
        if (!IsAvailable(ptrTriggerActivation) || !IsWritable(ptrTriggerActivation))
        {
            cout << "[Trigger Activation] node not available!!" << endl;
            return -1;
        }

        CEnumEntryPtr ptrTriggerActivationRisingEdge = ptrTriggerActivation->GetEntryByName("RisingEdge");
        if (!IsAvailable(ptrTriggerActivationRisingEdge) || !IsReadable(ptrTriggerActivationRisingEdge))
        {
            cout << "Unable to set [Trigger Activation] to [RisingEdge]. Aborting..." << endl;
            return -1;
        }

        ptrTriggerActivation->SetIntValue(ptrTriggerActivationRisingEdge->GetValue());
        cout << "Trigger Activation:            " << ptrTriggerActivation->GetCurrentEntry()->GetSymbolic() << endl;

        // Set Trigger Overlap to Read Out
        CEnumerationPtr ptrTriggerOverlap = nodeMap.GetNode("TriggerOverlap");
        if (!IsAvailable(ptrTriggerOverlap) || !IsWritable(ptrTriggerOverlap))
        {
            cout << "[Trigger Overlap] node not available" << endl;
            return result;
        }

        CEnumEntryPtr ptrTriggerOverlapReadOut = ptrTriggerOverlap->GetEntryByName("ReadOut");
        if (!IsAvailable(ptrTriggerOverlapReadOut) || !IsReadable(ptrTriggerOverlapReadOut))
        {
            cout << "Unable to set [Trigger Overlap] to [ReadOut]." << endl;
        }

        ptrTriggerOverlap->SetIntValue(ptrTriggerOverlapReadOut->GetValue());
        cout << "Trigger Overlap:               " << ptrTriggerOverlap->GetCurrentEntry()->GetSymbolic() << endl;

    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

bool RestoreFactoryDefault(INodeMap & nodeMap)
{
    // Set User Set Selector to Default
    CEnumerationPtr ptrUserSetSelector = nodeMap.GetNode("UserSetSelector");
    if (!IsAvailable(ptrUserSetSelector) || !IsWritable(ptrUserSetSelector))
    {
        cout << "[User Set Selector] node not available!!" << endl;
        return false;
    }

    CEnumEntryPtr ptrUserSetSelectorDefault = ptrUserSetSelector->GetEntryByName("Default");
    if (!IsAvailable(ptrUserSetSelectorDefault) || !IsReadable(ptrUserSetSelectorDefault))
    {
        cout << "Unable to set [User Set Selector] to [Default]. Aborting..." << endl;
        return false;
    }

    ptrUserSetSelector->SetIntValue(ptrUserSetSelectorDefault->GetValue());
    cout << "User Set Selector:             " << ptrUserSetSelector->GetCurrentEntry()->GetSymbolic() << endl;

    // Execute User Set Load
    CCommandPtr ptrUserSetLoad = nodeMap.GetNode("UserSetLoad");
    if (!IsAvailable(ptrUserSetLoad) || !IsWritable(ptrUserSetLoad))
    {
        cout << "[User Set Load] node not available!!" << endl;
        return false;
    }

    ptrUserSetLoad->Execute();

    return true;
}

string CinString(void)
{
    string input = "";
    int myNumber = 0;

    while (true)
    {
        getline(std::cin, input);
        std::stringstream myStream(input);
        if (myStream >> myNumber)
        {
            break;
        }
        cout << endl << "Invalid number, please try again" << endl;
    }
    return input;
}

#ifdef _DEBUG
// Disables heartbeat on GEV cameras so debugging does not incur timeout errors
int DisableHeartbeat(CameraPtr pCam, INodeMap & nodeMap, INodeMap & nodeMapTLDevice)
{
    cout << "Checking device type to see if we need to disable the camera's heartbeat..." << endl;

    // Write to boolean node controlling the camera's heartbeat
    CEnumerationPtr ptrDeviceType = nodeMapTLDevice.GetNode("DeviceType");
    if (!IsAvailable(ptrDeviceType) && !IsReadable(ptrDeviceType))
    {
        cout << "Error with reading the device's type. Aborting..." << endl;
        return -1;
    }
    else
    {
        if (ptrDeviceType->GetIntValue() == DeviceType_GEV)
        {
            cout << "Working with a GigE camera. Attempting to disable heartbeat before continuing..." << endl;
            CBooleanPtr ptrDeviceHeartbeat = nodeMap.GetNode("GevGVCPHeartbeatDisable");
            if (!IsAvailable(ptrDeviceHeartbeat) || !IsWritable(ptrDeviceHeartbeat))
            {
                cout << "Unable to disable heartbeat on camera. Continuing with execution as this may be non-fatal..." << endl;
            }
            else
            {
                ptrDeviceHeartbeat->SetValue(true);
                cout << "WARNING: Heartbeat on GigE camera disabled for the rest of Debug Mode." << endl;
                cout << "         Power cycle camera when done debugging to re-enable the heartbeat..." << endl;
            }
        }
        else
        {
            cout << "Camera does not use GigE interface. Resuming normal execution..." << endl;
        }
    }
    return 0;
}
#endif

// This function acquires and saves images from each camera
int AcquireImages(CameraList camList, unsigned int primaryIndex)
{
    int result = 0;
    CameraPtr pCam = nullptr;

    try
    {
        // Prepare each camera to acquire images
        vector<gcstring> serialNumbers(camList.GetSize());

        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

#ifdef _DEBUG
            cout << endl << "*** DEBUG ***" << endl << endl;

            // If using a GEV camera and debugging, should disable heartbeat first to prevent further issues
            if (DisableHeartbeat(pCam, pCam->GetNodeMap(), pCam->GetTLDeviceNodeMap()) != 0)
            {
                return -1;
            }

            cout << endl << "*** END OF DEBUG ***" << endl;
#endif
            // Retrieve device serial number for filename
            serialNumbers[i] = "";

            CStringPtr ptrStringSerial = pCam->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");

            if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
            {
                serialNumbers[i] = ptrStringSerial->GetValue();
                cout << "Camera " << i << " serial number is " << serialNumbers[i] << "..." << endl;
            }
            cout << endl;

            // Begin Acquistion on all Secondary cameras first
            if (i != primaryIndex)
            {
                pCam->BeginAcquisition();
                cout << "Secondary camera " << i << " begin acquiring images..." << endl;
            }

        }

        cout << endl << "Press Enter to start retrieving and converting images...";
        cin.ignore();

        // Start capture on the Primary camera
        pCam = camList.GetByIndex(primaryIndex);
        pCam->BeginAcquisition();
        cout << "Primary camera " << primaryIndex << " begin acquiring images..." << endl << endl;


        vector<ImagePtr> pImages;

        // Create arrays for timestamp and Frame ID values
        vector<int64_t> timestampPrevious(camList.GetSize());
        vector<int64_t> timestampInterval(camList.GetSize());
        vector<int64_t> frameIDPrevious(camList.GetSize());

        for (unsigned int imageCnt = 0; imageCnt < k_numImages; imageCnt++)
        {
            for (unsigned int i = 0; i < camList.GetSize(); i++)
            {
                try
                {
                    // Select camera
                    pCam = camList.GetByIndex(i);

                    // Retrieve the image
                    ImagePtr pResultImage = pCam->GetNextImage(k_grabTimeout);

                    if (pResultImage->IsIncomplete())
                    {
                        cout << "Image incomplete with image   " << pResultImage->GetImageStatus() << "..." << endl << endl;
                    }
                    else
                    {
                        ChunkData chunkData = pResultImage->GetChunkData();

                        // Retrieve timestamp and Frame ID
                        const int64_t timestampCurrent = chunkData.GetTimestamp();
                        const int64_t frameIDCurrent = chunkData.GetFrameID();

                        cout << "Grabbing Frame ID " << frameIDCurrent << " from camera " << i << " - TimeStamp [" << timestampCurrent << "]" << endl;

                        if (imageCnt == 0)
                        {
                            timestampPrevious[i] = timestampCurrent;
                            frameIDPrevious[i] = frameIDCurrent;
                        }
                        else
                        {
                            timestampInterval[i] = timestampCurrent - timestampPrevious[i];
                            cout << "Time between consecutive images: " << (timestampCurrent - timestampPrevious[i]) / 1000 << " microseconds" << endl << endl;

                            if ((frameIDCurrent - frameIDPrevious[i]) > 1)
                            {
                                cout << "Frame skipped at " << frameIDCurrent << ", last frame ID was " << frameIDPrevious[i] << endl;
                            }

                            timestampPrevious[i] = timestampCurrent;
                            frameIDPrevious[i] = frameIDCurrent;
                        }

                        // Convert the image
                        pImages.push_back(pResultImage->Convert(PixelFormat_Mono8, HQ_LINEAR));

                    }
                    // Release image
                    pResultImage->Release();
                }
                catch (Spinnaker::Exception &e)
                {
                    cout << "Error: " << e.what() << endl;
                    result = -1;
                }
            }
        }

        // Run a separate loop for saving
        cout << "Saving images..." << endl;

#pragma omp parallel for
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
#pragma omp parallel for
            for (unsigned int imageCnt = 0; imageCnt < k_numImages; imageCnt++)
            {
                try
                {
                    int offset = imageCnt * camList.GetSize() + i;

                    // Create a unique filename
                    string Dir = "Camera " + serialNumbers[i] + " images";
                    _mkdir(Dir.c_str());
                    ostringstream filename;
                    filename << ".\\" + Dir + "\\" << serialNumbers[i] << "-" << imageCnt << ".jpg";

                    // Save the image
                    pImages[offset]->Save(filename.str().c_str());
                    cout << "Saving frame " << imageCnt << " from camera " << i << endl;
                }
                catch (Spinnaker::Exception &e)
                {
                    cout << "Error: " << e.what() << endl;
                    result = -1;
                }
            }
        }

        // End acquisition for each camera
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // End acquisition
            camList.GetByIndex(i)->EndAcquisition();
        }
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }
    return result;
}

// This function acts as the body of the example
int RunMultipleCameras(CameraList camList)
{
    int result = 0;
    CameraPtr pCam = nullptr;

    try
    {
        // Retrieve transport layer nodemaps and print device information for each camera
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Print device information
            INodeMap &nodeMapTLDevice = pCam->GetTLDeviceNodeMap();
            result = PrintDeviceInfo(nodeMapTLDevice, i);
        }

        // Get the Primary camera SN from user input
        string primarySN;
        string currentSN;
        unsigned int primaryIndex;
        bool primaryNotDetected = true;

        do
        {
            cout << "Please enter the Primary camera serial number: " << endl;
            primarySN = CinString();

            // Loop through cameras to find s/n provided by user, assign as primary
            for (unsigned int i = 0; i < camList.GetSize(); i++)
            {
                pCam = camList.GetByIndex(i);

                CStringPtr ptrStringSerial = pCam->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");

                if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
                {
                    currentSN = ptrStringSerial->GetValue();
                }

                if (currentSN == primarySN)
                {
                    primaryIndex = i;
                    cout << "Camera " << i << " is set to be the Primary!!" << endl;
                    primaryNotDetected = false;
                }
            }

            if (primaryNotDetected)
            {
                cout << endl << "Primary SN " << primarySN << " Not Found!" << endl;
            }
        } while (primaryNotDetected);

        // Initialize each camera
        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Initialize camera
            pCam->Init();

            // Restore camera settings to factory default
            INodeMap & nodeMap = pCam->GetNodeMap();
            if (RestoreFactoryDefault(nodeMap) == false)
            {
                cout << "Error restoring camera " << i << " settings to Factory default!" << endl;
                result = -1;
            }
            cout << "Camera " << i << " settings restored to factory default" << endl;

            // Set up Cameras
            cout << endl << "Setting up camera " << i << " ..." << endl << endl;
            cout << "Node name                      Value     " << endl;
            cout << "=========================================" << endl;

            // Sets the boolalpha format flag for the str stream
            std::cout.setf(std::ios::boolalpha);

            // Determine camera family
            CStringPtr ptrDeviceModelName = nodeMap.GetNode("DeviceModelName");
            if (!IsAvailable(ptrDeviceModelName) || !IsReadable(ptrDeviceModelName))
            {
                cout << "Unable to determine camera family. Aborting..." << endl << endl;
                return -1;
            }

            gcstring cameraModel = ptrDeviceModelName->GetValue();

            result = result | ConfigureCameraSettings(nodeMap, cameraModel);
            if (result < 0)
            {
                return result;
            }

            result = ConfigureChunkData(nodeMap);
            if (result < 0)
            {
                return result;
            }

            if (i == primaryIndex)
            {
                cout << endl << "======== Primary Camera Settings =========" << endl;
                result = result | SetupPrimaryCam(nodeMap, cameraModel);
                cout << endl;
            }
            else
            {
                cout << endl << "========= Secondary Camera Settings =========" << endl;
                result = result | SetupSecondaryCam(nodeMap, cameraModel);
                cout << endl;
            }
        }

        // Acquire images on all cameras
        cout << endl;
        result = result | AcquireImages(camList, primaryIndex);

        for (unsigned int i = 0; i < camList.GetSize(); i++)
        {
            // Select camera
            pCam = camList.GetByIndex(i);

            // Restore camera settings to factory default
            INodeMap &nodeMap = pCam->GetNodeMap();
            if (RestoreFactoryDefault(nodeMap) == false)
            {
                cout << "Error restoring camera " << i << " settings to Factory default!!" << endl;
                result = -1;
            }
            cout << "Camera " << i << " settings restored to factory default" << endl;

            // Deinitialize camera
            pCam->DeInit();
            cout << "Camera " << i << " deinitialize " << endl;
        }
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

// Example entry point
int main(int /*argc*/, char** /*argv*/)
{
    // Ensure that we have permission to write to this folder.
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

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    result = PrintBuildInfo(system);

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

    // Run example on all cameras
    result = RunMultipleCameras(camList);

    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return result;
}