//=============================================================================
// Copyright (c) 2001-2020 FLIR Systems, Inc. All Rights Reserved.
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
*  @example AlternatingStrobe.cpp
*
*  @brief AlternatingStrobe.cpp Shows how to use BFS/ORX Counters and Logic
*  Blocks in order to apply a custom strobe delay to every second frame.
*
*  LogicBlock1 is used like a T-flipflop in order to hold a state logic value
*  that will be toggled on each ExposureEnd event.
*
*  Counter0 is used as a  MHz timer to time the delay that should be added to
*  every second strobe signal.
*
*  Counter1 is used as a MHz timer to time the strobe durration. The output
*  line will be set to Counter1Active.
*
*  LocicBlock0 is used a the reset source to start Counter1. It takes as input
*  LogicBlock1 to determine if the offset is to be applied.
*  If LogicBlock1=0, LogicBlock0 has a rising edge on ExposureStart.
*  If LogicBlock1=1, LogicBlock0 has a rising edge on Counter0End.
*/

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

#define ALTERNATING_TRIGGER_DELAY_US 10000
#define EXPOSURE_LENGTH_US 20000
#define STROBE_DURATION_US 10000
#define FRAME_RATE 10

bool LoadDefaultUserSet(CameraPtr pCam)
{
    cout << "Loading the default user set" << endl;
    try
    {
        pCam->UserSetSelector.SetValue(UserSetSelector_Default);
        pCam->UserSetLoad();
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return false;
    }
    return true;
}

// LocicBlock0 takes LogicBlock1 as input determine if the
// offset is to be applied.
// If LogicBlock1 = 0, LogicBlock0 has a rising edge on ExposureStart.
// If LogicBlock1 = 1, LogicBlock0 has a rising edge on Counter0End.
bool ConfigureLogicBlock0(CameraPtr pCam)
{
    cout << endl << endl << "Configuring Logic Block 0" << endl << endl;
    try
    {
        pCam->LogicBlockSelector.SetValue(LogicBlockSelector_LogicBlock0);

        pCam->LogicBlockLUTSelector.SetValue(LogicBlockLUTSelector_Enable);
        pCam->LogicBlockLUTOutputValueAll.SetValue(0xFF);

        pCam->LogicBlockLUTSelector.SetValue(LogicBlockLUTSelector_Value);
        pCam->LogicBlockLUTOutputValueAll.SetValue(0xCA);

        pCam->LogicBlockLUTInputSelector.SetValue(LogicBlockLUTInputSelector_Input0);
        pCam->LogicBlockLUTInputSource.SetValue(LogicBlockLUTInputSource_ExposureStart);
        pCam->LogicBlockLUTInputActivation.SetValue(LogicBlockLUTInputActivation_RisingEdge);

        pCam->LogicBlockLUTInputSelector.SetValue(LogicBlockLUTInputSelector_Input1);
        pCam->LogicBlockLUTInputSource.SetValue(LogicBlockLUTInputSource_Counter0End);
        pCam->LogicBlockLUTInputActivation.SetValue(LogicBlockLUTInputActivation_RisingEdge);

        pCam->LogicBlockLUTInputSelector.SetValue(LogicBlockLUTInputSelector_Input2);
        pCam->LogicBlockLUTInputSource.SetValue(LogicBlockLUTInputSource_LogicBlock1);
        pCam->LogicBlockLUTInputActivation.SetValue(LogicBlockLUTInputActivation_LevelHigh);
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return false;
    }
    return true;
}

// LogicBlock1 is used like a T-flipflop in order to hold a state logic value
// that will be toggled on each ExposureEnd event.
bool ConfigureLogicBlock1(CameraPtr pCam)
{
    cout << endl << "Configuring Logic Block 1" << endl;
    try
    {
        pCam->LogicBlockSelector.SetValue(LogicBlockSelector_LogicBlock1);

        pCam->LogicBlockLUTSelector.SetValue(LogicBlockLUTSelector_Enable);
        pCam->LogicBlockLUTOutputValueAll.SetValue(0xAF);

        pCam->LogicBlockLUTSelector.SetValue(LogicBlockLUTSelector_Value);
        pCam->LogicBlockLUTOutputValueAll.SetValue(0x20);

        pCam->LogicBlockLUTInputSelector.SetValue(LogicBlockLUTInputSelector_Input0);
        pCam->LogicBlockLUTInputSource.SetValue(LogicBlockLUTInputSource_ExposureEnd);
        pCam->LogicBlockLUTInputActivation.SetValue(LogicBlockLUTInputActivation_RisingEdge);

        pCam->LogicBlockLUTInputSelector.SetValue(LogicBlockLUTInputSelector_Input1);
        pCam->LogicBlockLUTInputSource.SetValue(LogicBlockLUTInputSource_LogicBlock1);
        pCam->LogicBlockLUTInputActivation.SetValue(LogicBlockLUTInputActivation_LevelHigh);

        // UserOutput1 is used to activate the alternating behavior and initialize/reset
        // the LogicBlock1 value to 0. Begin with UserOutput1=0 to intialize the
        // Logic Block Q value to 0.
        pCam->LogicBlockLUTInputSelector.SetValue(LogicBlockLUTInputSelector_Input2);
        pCam->LogicBlockLUTInputSource.SetValue(LogicBlockLUTInputSource_UserOutput1);
        pCam->LogicBlockLUTInputActivation.SetValue(LogicBlockLUTInputActivation_LevelHigh);
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return false;
    }
    return true;
}

// Counter0 is used as a MHz timer to count to alternating DelayTimeUS microseconds
// starting on Exposure Start.
bool ConfigureCounter0(CameraPtr pCam, uint64_t alternatingDelayTimeUS)
{
    cout << endl << "Configuring Counter 0" << endl;
    try
    {
        pCam->CounterSelector.SetValue(CounterSelector_Counter0);
        pCam->CounterEventSource.SetValue(CounterEventSource_MHzTick);
        pCam->CounterTriggerSource.SetValue(CounterTriggerSource_Off);
        pCam->CounterResetSource.SetValue(CounterResetSource_ExposureStart);
        pCam->CounterResetActivation.SetValue(CounterResetActivation_RisingEdge);
        pCam->CounterDuration.SetValue(alternatingDelayTimeUS);
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return false;
    }
    return true;
}

// Counter1 is used as a MHz timer to count to strobeDurrationUS microseconds
// Starting on LogicBlock0 Rising Edge.
bool ConfigureCounter1(CameraPtr pCam, uint64_t strobeDurrationUS)
{
    cout << endl << "Configuring Counter 1" << endl;
    try
    {
        pCam->CounterSelector.SetValue(CounterSelector_Counter1);
        pCam->CounterEventSource.SetValue(CounterEventSource_MHzTick);
        pCam->CounterTriggerSource.SetValue(CounterTriggerSource_Off);
        pCam->CounterResetSource.SetValue(CounterResetSource_LogicBlock0);
        pCam->CounterResetActivation.SetValue(CounterResetActivation_RisingEdge);
        pCam->CounterDuration.SetValue(strobeDurrationUS);
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return false;
    }
    return true;
}

// Output used to output the custom strobe signal
bool ConfigureLine2(CameraPtr pCam) {
    cout << endl << "Configuring Line 2" << endl;
    try
    {
        pCam->LineSelector.SetValue(LineSelector_Line2);
        pCam->LineMode.SetValue(LineMode_Output);
        pCam->LineSource.SetValue(LineSource_Counter1Active);
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return false;
    }
    return true;
}

// Strobe Exposure Active Output signal, for debugging (Optional)
bool ConfigureLine1(CameraPtr pCam) {
    cout << endl << "Configuring Line 1" << endl;
    try
    {
        pCam->LineSelector.SetValue(LineSelector_Line1);
        pCam->LineMode.SetValue(LineMode_Output);
        pCam->LineSource.SetValue(LineSource_ExposureActive);
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return false;
    }
    return true;
}

bool ConfigureFrameRate(CameraPtr pCam, double frameRate)
{
    cout << endl << "Configure Frame Rate Settings" << endl;
    try
    {
        pCam->AcquisitionFrameRateEnable.SetValue(true);
        pCam->AcquisitionFrameRate.SetValue(fmin(frameRate, pCam->AcquisitionFrameRate.GetMax()));

        cout << "Frame rate set to: " << pCam->AcquisitionFrameRate.GetValue() << "fps" << endl;
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return false;
    }
    return true;
}

bool ConfigureExposureTime(CameraPtr pCam, double exposreTime)
{
    cout << endl << "Configure Exposure Time Settings" << endl;
    try
    {
        pCam->ExposureAuto.SetValue(ExposureAuto_Off);
        pCam->ExposureTime.SetValue(fmin(exposreTime, pCam->ExposureTime.GetMax()));

        cout << "Exposure Time set to: " << pCam->ExposureTime.GetValue() << "us" << endl;
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return false;
    }
    return true;
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

bool AcquireImages(CameraPtr pCam)
{
    cout << endl << "*** IMAGE ACQUISITION ***" << endl << endl;
    try
    {
        pCam->AcquisitionMode.SetValue(AcquisitionMode_Continuous);

        string deviceSerialNumber = pCam->DeviceID.GetValue();

        // Enable Alternating Strobe
        pCam->UserOutputSelector.SetValue(UserOutputSelector_UserOutput1);
        pCam->UserOutputValue.SetValue(true);

        pCam->BeginAcquisition();

        cout << "Acquiring images..." << endl;

        const int unsigned k_numImages = 100;

        for (unsigned int imageCnt = 0; imageCnt < k_numImages; imageCnt++)
        {
            try
            {
                ImagePtr pResultImage = pCam->GetNextImage();

                if (pResultImage->IsIncomplete())
                {
                    cout << "Image incomplete with image status " << pResultImage->GetImageStatus() << "..." << endl << endl;
                }
                else
                {
                    cout << "Grabbed image " << imageCnt << ", width = " << pResultImage->GetWidth() << ", height = " << pResultImage->GetHeight() << endl;
                    // Omitting image conversion and saving for brevity. See Acquisition.cpp for how to convert and save images
                }

                pResultImage->Release();

                cout << endl;
            }
            catch (Spinnaker::Exception &e)
            {
                cout << "Error: " << e.what() << endl;
            }
        }

        pCam->EndAcquisition();

        // Disable Alternating Strobe
        pCam->UserOutputSelector.SetValue(UserOutputSelector_UserOutput1);
        pCam->UserOutputValue.SetValue(false);
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return true;
    }

    return false;
}

int RunSingleCamera(CameraPtr pCam)
{
    try
    {
        pCam->Init();

        PrintDeviceInfo(pCam->GetTLDeviceNodeMap());

        // If acquisition is started then stop it
        try
        {
            pCam->AcquisitionStop();
            cout << "Acquisition already started, Stopping to configure settings." << endl;
        }
        catch (Spinnaker::Exception &e)
        {
            cout << "Acquisition not started. Configuring settings. " << endl;
        }

        if (!LoadDefaultUserSet(pCam))
        {
            return -1;
        }

        if (!ConfigureFrameRate(pCam, FRAME_RATE))
        {
            return -1;
        }

        if (!ConfigureExposureTime(pCam, EXPOSURE_LENGTH_US))
        {
            return -1;
        }

        if (!ConfigureLine2(pCam))
        {
            return -1;
        }

        // Configure Line 1 Exposure active output for debugging (Optional)
        if (!ConfigureLine1(pCam))
        {
            return -1;
        }

        // Ensure that user output 1 is false initially to initialize logic block Q value
        pCam->UserOutputSelector.SetValue(UserOutputSelector_UserOutput1);
        pCam->UserOutputValue.SetValue(false);

        if (!ConfigureLogicBlock0(pCam))
        {
            return -1;
        }

        if (!ConfigureLogicBlock1(pCam))
        {
            return -1;
        }

        if (!ConfigureCounter0(pCam, ALTERNATING_TRIGGER_DELAY_US))
        {
            return -1;
        }

        if (!ConfigureCounter1(pCam, STROBE_DURATION_US))
        {
            return -1;
        }

        cout << "Strobe Durration: " << STROBE_DURATION_US << "us" << endl;
        cout << "Alternating Strobe Delay : " << ALTERNATING_TRIGGER_DELAY_US << "us" << endl;

        AcquireImages(pCam);

        // Return to default settings
        LoadDefaultUserSet(pCam);

        pCam->DeInit();
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return -1;
    }
    return 0;
}

int main(int /*argc*/, char** /*argv*/)
{
    // Since this application saves images in the current folder
    // we must ensure that we have permission to write to this folder.
    // If we do not have permission, fail right away.
    FILE *tempFile = fopen("test.txt", "w+");
    if (tempFile == NULL)
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

    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: "
        << spinnakerLibraryVersion.major << "."
        << spinnakerLibraryVersion.minor << "."
        << spinnakerLibraryVersion.type << "."
        << spinnakerLibraryVersion.build << endl << endl;

    CameraList camList = system->GetCameras();

    unsigned int numCameras = camList.GetSize();

    cout << "Number of cameras detected: " << numCameras << endl << endl;

    if (numCameras == 0)
    {
        camList.Clear();

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