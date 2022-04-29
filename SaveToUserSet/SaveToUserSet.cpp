//=============================================================================
// Copyright © 2019 FLIR Integrated Imaging Solutions, Inc. All Rights Reserved.
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
*	@example SavetoUserSet.cpp
*
*	@brief SavetoUserSet.cpp shows how to save custom settings to User Set. By default,
*   it modifies the exposure time to 2000 microseconds, then saves this change to User Set 1.
*   It can also be configured to reset the camera to factory default settings.
*/

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream> 

//
// Uncomment to have the camera restored to factory default settings (please note this overrides any settings changes made)
//
//#define RESTORE_FACTORY_DEFAULT

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

// This function sets the exposure time to 2000 microseconds and saves that to User Set 1
int SaveCustomSettings(CameraPtr pCam, INodeMap& nodeMap, INodeMap& nodeMapTLDevice)
{
    int result = 0;

    cout << endl << endl << "Saving Custom Settings" << endl << endl;

    try
    {
        // Turn off auto exposure
        CEnumerationPtr ptrExposureAuto = pCam->GetNodeMap().GetNode("ExposureAuto");
        if (!IsAvailable(ptrExposureAuto) || !IsWritable(ptrExposureAuto))
        {
            cout << "Unable to set exposure auto (enumeration retrieval). Aborting..." << endl << endl;
            return -1;
        }

        CEnumEntryPtr ptrExposureAutoOff = ptrExposureAuto->GetEntryByName("Off");
        if (!IsAvailable(ptrExposureAutoOff) || !IsReadable(ptrExposureAutoOff))
        {
            cout << "Unable to set exposure auto (enum entry retrieval). Aborting..." << endl << endl;
            return -1;
        }

        ptrExposureAuto->SetIntValue(ptrExposureAutoOff->GetValue());

        cout << "Exposure auto turned off" << endl;

        // Set exposure time to 2000 milliseconds
        CFloatPtr ptrExposureTime = pCam->GetNodeMap().GetNode("ExposureTime");
        if (!IsAvailable(ptrExposureTime) || !IsWritable(ptrExposureTime))
        {
            cout << "Unable to set exposure time (Float retrieval). Aborting..." << endl << endl;
            return -1;
        }

        ptrExposureTime->SetValue(2000);

        cout << "Exposure Time set to " << ptrExposureTime->GetValue() << endl;

        // Change User Set to 1
        CEnumerationPtr ptrUserSetSelector = nodeMap.GetNode("UserSetSelector");
        if (!IsAvailable(ptrUserSetSelector) || !IsWritable(ptrUserSetSelector))
        {
            cout << "Unable to access User Set Selector (enum retrieval). Aborting..." << endl << endl;
            return -1;
        }

        CEnumEntryPtr ptrUserSet1 = ptrUserSetSelector->GetEntryByName("UserSet1");
        if (!IsAvailable(ptrUserSet1) || !IsReadable(ptrUserSet1))
        {
            cout << "Unable to access User Set (entry retrieval). Aborting..." << endl << endl;
            return -1;
        }

        ptrUserSetSelector->SetIntValue(ptrUserSet1->GetValue());

        // Save custom settings to User Set 1
        CCommandPtr ptrUserSetSave = pCam->GetNodeMap().GetNode("UserSetSave");
        if (!IsAvailable(ptrUserSetSave) || !IsWritable(ptrUserSetSave))
        {
            cout << "Unable to save to User Set. Aborting..." << endl << endl;
            return -1;
        }

        ptrUserSetSave->Execute();

        cout << "Saved Custom Settings to " << ptrUserSetSelector->GetCurrentEntry()->GetSymbolic() << endl;

        // Change default User Set to User Set 1
        CEnumerationPtr ptrUserSetDefault = nodeMap.GetNode("UserSetDefault");
        if (!IsAvailable(ptrUserSetDefault) || !IsWritable(ptrUserSetDefault))
        {
            cout << "Unable to access User Set Default(enum retrieval). Aborting..." << endl << endl;
            return -1;
        }

        ptrUserSetDefault->SetIntValue(ptrUserSet1->GetValue());

        cout << "Default User Set now set to UserSet1; camera will start up with custom settings " << endl;

#ifdef RESTORE_FACTORY_DEFAULT

        // Set User Set to Default
        CEnumEntryPtr ptrDefault = ptrUserSetSelector->GetEntryByName("Default");
        if (!IsAvailable(ptrDefault) || !IsReadable(ptrDefault))
        {
            cout << "Unable to access User Set (entry retrieval). Aborting..." << endl << endl;
            return -1;
        }

        ptrUserSetSelector->SetIntValue(ptrDefault->GetValue());

        // Load Default User Set to camera
        CCommandPtr ptrUserSetLoad = pCam->GetNodeMap().GetNode("UserSetLoad");
        if (!IsAvailable(ptrUserSetLoad) || !IsReadable(ptrUserSetLoad))
        {
            cout << "Unable to access User Set load command. Aborting..." << endl << endl;
            return -1;
        }

        ptrUserSetLoad->Execute();

        cout << "Camera set to default settings" << endl;

        // Set Default User Set to Default
        ptrUserSetDefault->SetIntValue(ptrDefault->GetValue());

        cout << "Default User Set now set to Default; camera will start up with  default settings" << endl;
#endif

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

        // Acquire images
        result = result | SaveCustomSettings(pCam, nodeMap, nodeMapTLDevice);

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
    int result = 0;

    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

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

    //
    // Create shared pointer to camera
    CameraPtr pCam = NULL;

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

    //
    // Release reference to the camera
    //
    // *** NOTES ***
    // Had the CameraPtr object been created within the for-loop, it would not
    // be necessary to manually break the reference because the shared pointer
    // would have automatically cleaned itself up upon exiting the loop.
    //
    pCam = NULL;

    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return result;
}
