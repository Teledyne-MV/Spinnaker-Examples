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
*  @example ExtendTriggerDelay.cpp
*
*  @brief ExtendTriggerDelay.cpp shows how to use BFS/ORX Counters and Logic
*  Blocks in order to delay exposure by up to 2^32 microseconds after trigger.
*  This is accomplished by cascading both of the camera's 16 bit counters.
*  This method can be used to delay exposure for longer than the maximimum 65535
*  microsecond value of the node TriggerDelay. For trigger delay values less than
*  65535, the node TriggerDelay should be used instead of this method.
*
*  Counter0 is used as a MHzTimer. Coutner1 is used to count the number of
*  Counter0End events that occur. The extended trigger delay is controlled
*  by the duration values of Counter0 and Counter1.
*
*  LogicBlock0 is set to HIGH on the GPIO input, held, and then returned to LOW
*  on Counter1End. LogicBlock0 is used as the CounterTriggerSource source for
*  Counter0 with CounterTriggerActivation set to LevelHigh. This means that when
*  Counter0 reaches its durration it will restart if LogicBlock0 is still HIGH.
*/

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

uint64_t TRIGGER_DELAY_US = 125000;
uint64_t COUNTER_0_DURRATION_US = 1000;

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

bool ConfigureLogicBlock0(CameraPtr pCam)
{
    cout << endl << endl << "Configuring Logic Block 0" << endl << endl;

    try
    {
        pCam->LogicBlockSelector.SetValue(LogicBlockSelector_LogicBlock0);

        pCam->LogicBlockLUTSelector.SetValue(LogicBlockLUTSelector_Enable);
        pCam->LogicBlockLUTOutputValueAll.SetValue(0xEE);

        pCam->LogicBlockLUTSelector.SetValue(LogicBlockLUTSelector_Value);
        pCam->LogicBlockLUTOutputValueAll.SetValue(0x22);

        pCam->LogicBlockLUTInputSelector.SetValue(LogicBlockLUTInputSelector_Input0);
        pCam->LogicBlockLUTInputSource.SetValue(LogicBlockLUTInputSource_Line0);
        pCam->LogicBlockLUTInputActivation.SetValue(LogicBlockLUTInputActivation_RisingEdge);

        pCam->LogicBlockLUTInputSelector.SetValue(LogicBlockLUTInputSelector_Input1);
        pCam->LogicBlockLUTInputSource.SetValue(LogicBlockLUTInputSource_Counter1End);
        pCam->LogicBlockLUTInputActivation.SetValue(LogicBlockLUTInputActivation_RisingEdge);

        // The logic block output does not depend on this value so we will just use Zero
        pCam->LogicBlockLUTInputSelector.SetValue(LogicBlockLUTInputSelector_Input2);
        pCam->LogicBlockLUTInputSource.SetValue(LogicBlockLUTInputSource_Zero);
        pCam->LogicBlockLUTInputActivation.SetValue(LogicBlockLUTInputActivation_LevelHigh);
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return false;
    }
    return true;
}

bool ConfigureCounter0(CameraPtr pCam, uint64_t durrationUS)
{
    cout << endl << "Configuring Counter 0" << endl;
    try
    {
        pCam->CounterSelector.SetValue(CounterSelector_Counter0);
        pCam->CounterEventSource.SetValue(CounterEventSource_MHzTick);
        pCam->CounterDuration.SetValue(durrationUS);

        if (!IsWritable(pCam->CounterTriggerSource))
        {
            pCam->CounterResetSource.SetValue(CounterResetSource_Off);
        }

        pCam->CounterTriggerSource.SetValue(CounterTriggerSource_LogicBlock0);
        pCam->CounterTriggerActivation.SetValue(CounterTriggerActivation_LevelHigh);
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return false;
    }
    return true;
}

bool ConfigureCounter1(CameraPtr pCam, uint64_t counter0Cycles)
{
    cout << endl << "Configuring Counter 1" << endl;

    try
    {
        pCam->CounterSelector.SetValue(CounterSelector_Counter1);
        pCam->CounterEventSource.SetValue(CounterEventSource_Counter0End);
        pCam->CounterEventActivation.SetValue(CounterEventActivation_RisingEdge);
        pCam->CounterDuration.SetValue(counter0Cycles);

        if (!IsWritable(pCam->CounterTriggerSource))
        {
            pCam->CounterResetSource.SetValue(CounterResetSource_Off);
        }

        pCam->CounterTriggerSource.SetValue(CounterTriggerSource_Line0);
        pCam->CounterTriggerActivation.SetValue(CounterTriggerActivation_RisingEdge);
    }
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return false;
    }
    return true;
}

// Strobe Exposure Active Output signal, for debugging (Optional)
bool ConfigureLine2(CameraPtr pCam)
{
    cout << endl << "Configuring Line 2" << endl;

    try
    {
        pCam->LineSelector.SetValue(LineSelector_Line2);
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

bool ConfigureTrigger(CameraPtr pCam)
{
    cout << endl << "Configure Trigger Settings" << endl;
    try
    {
        // Ensure trigger mode is off before configuring settings
        pCam->TriggerMode.SetValue(TriggerMode_Off);

        pCam->TriggerSource.SetValue(TriggerSource_Counter1End);
        pCam->TriggerActivation.SetValue(TriggerActivation_RisingEdge);

        // Turn trigger mode back on
        pCam->TriggerMode.SetValue(TriggerMode_On);
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

        pCam->BeginAcquisition();

        cout << "Acquiring images..." << endl;

        string deviceSerialNumber = pCam->DeviceID.GetValue();

        const int unsigned k_numImages = 10;

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

                    ImagePtr convertedImage = pResultImage->Convert(PixelFormat_Mono8, HQ_LINEAR);

                    ostringstream filename;

                    filename << "Trigger-";
                    if (deviceSerialNumber != "")
                    {
                        filename << deviceSerialNumber.c_str() << "-";
                    }
                    filename << imageCnt << ".jpg";

                    convertedImage->Save(filename.str().c_str());

                    cout << "Image saved at " << filename.str() << endl;
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
            return -1;

        uint64_t iterationsOfCounter0 = TRIGGER_DELAY_US / COUNTER_0_DURRATION_US;

        cout << "Counter 0 durration: " << COUNTER_0_DURRATION_US << "us" << endl;
        cout << "Counter 0 cycles: " << iterationsOfCounter0 << endl;
        cout << "Trigger delay: " << COUNTER_0_DURRATION_US * iterationsOfCounter0 << endl;

        if (!ConfigureCounter0(pCam, COUNTER_0_DURRATION_US))
        {
            return -1;
        }

        if (!ConfigureCounter1(pCam, iterationsOfCounter0))
        {
            return -1;
        }

        if (!ConfigureLogicBlock0(pCam))
        {
            return -1;
        }

        // Configure Line 2 Exposure active output for debugging (Optional)
        if (!ConfigureLine2(pCam))
        {
            return -1;
        }

        if (!ConfigureTrigger(pCam))
        {
            return -1;
        }

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