#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

#define CAM_LIST_REFRESH_TIME 1000

// This function prints the device information of the camera from the transport
// layer; please see NodeMapInfo example for more in-depth comments on printing
// device information from the nodemap.
int PrintDeviceInfo(INodeMap & nodeMap)
{
    cout << endl << "*** DEVICE INFORMATION ***" << endl << endl;

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
    catch (Spinnaker::Exception &e)
    {
        cout << "Error: " << e.what() << endl;
        return -1;
    }

    return 0;
}

// This function acts as the body of the example; please see NodeMapInfo example 
// for more in-depth comments on setting up cameras.
int RunSingleCamera(CameraPtr pCam, CameraList & camList, SystemPtr pSystem)
{
    int result;

    try
    {
        result = PrintDeviceInfo(pCam->GetTLDeviceNodeMap());

        // Initialize camera
        pCam->Init();

        string deviceSerialNumber = "";

        CStringPtr ptrStringSerial = pCam->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");
        if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
        {
            deviceSerialNumber = ptrStringSerial->GetValue();

            cout << "Device serial number retrieved as " << deviceSerialNumber << "..." << endl;
        }
        cout << endl;

        CCommandPtr ptrResetCommand = pCam->GetNodeMap().GetNode("DeviceReset");
        if (!IsAvailable(ptrResetCommand) || !IsWritable(ptrResetCommand))
        {
            cout << "Unable to execute reset. Aborting..." << endl;
            return -1;
        }

        ptrResetCommand->Execute();

        // remove camrea from list by serial number
        camList.RemoveBySerial(deviceSerialNumber);

        pCam->DeInit();

        pCam = nullptr;

        cout << "Camera: " << deviceSerialNumber << " has been reset" << endl;

        int waitTime_s = 0;

        while (pCam == nullptr) {
#ifdef _WIN32
            Sleep(CAM_LIST_REFRESH_TIME);
#else
            usleep(CAM_LIST_REFRESH_TIME * 1000);
#endif
            cout << "Wait time is " << ++waitTime_s << "seconds..." << endl;

            camList = pSystem->GetCameras();

            cout << " CameraList size = " << camList.GetSize() << endl;

            pCam = camList.GetBySerial(deviceSerialNumber);
        }

        // Camera is back
        cout << "Camera with serial number: " << deviceSerialNumber << " has been re-enumerated" << endl;

        result = PrintDeviceInfo(pCam->GetTLDeviceNodeMap());

        // Deinitialize camera
        pCam->DeInit();
    }
    catch (Spinnaker::Exception &e)
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

    vector<string> camSerialList;

    for (int i = 0; i < camList.GetSize(); i++) {
        CStringPtr ptrStringSerial = camList.GetByIndex(i)->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");
        camSerialList.push_back(string(ptrStringSerial->GetValue()));
    }

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

    //
    // Create shared pointer to camera
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
    CameraPtr pCam = nullptr;

    int result = 0;

    // Run example on each camera
    for (unsigned int i = 0; i < camSerialList.size(); i++)
    {
        // Select camera
        pCam = camList.GetBySerial(camSerialList[i]);

        cout << endl << "Running example for camera: " << camSerialList[i] << "..." << endl;

        // Run example
        result = result | RunSingleCamera(pCam, camList, system);

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
    pCam = nullptr;

    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return result;
}
