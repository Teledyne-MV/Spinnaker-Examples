#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>
#include <thread>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

#define CAM_LIST_REFRESH_TIME 1000

// In the example, InterfaceEventHandler inherits from InterfaceEvent while
// SystemEventHandler inherits from ArrivalEvent and RemovalEvent. This is
// done for demonstration purposes and is not a constraint of the SDK. All 
// three event types - ArrivalEvent, RemovalEvent, and InterfaceEvent - can be 
// registered to interfaces, the system, or both.
class SystemEventHandler : public ArrivalEvent, public RemovalEvent
{
public:

    SystemEventHandler(SystemPtr system) : m_system(system) {};
    ~SystemEventHandler() {};

    // This method defines the arrival event on the system. It retrieves the
    // number of cameras currently connected and prints it out.
    void OnDeviceArrival(uint64_t deviceSerialNumber)
    {
        cout << "Device arrival sn: " << deviceSerialNumber << endl;

        CameraPtr pCam = m_system->GetCameras().GetBySerial(to_string(deviceSerialNumber));
        while (pCam == NULL) {
            pCam = m_system->GetCameras().GetBySerial(to_string(deviceSerialNumber));
            cout << "Cam null" << endl;
        }

        pCam->Init();

        //Configure buffer handling 
        Spinnaker::GenApi::INodeMap & sNodeMap = pCam->GetTLStreamNodeMap();
        CEnumerationPtr ptrStreamBufferCountMode = sNodeMap.GetNode("StreamBufferCountMode");
        CEnumEntryPtr ptrStreamBufferCountModeManual = ptrStreamBufferCountMode->GetEntryByName("Manual");

        ptrStreamBufferCountMode->SetIntValue(ptrStreamBufferCountModeManual->GetValue());
        //cout << "Stream Buffer Count Mode set to manual..." << endl;

        CIntegerPtr ptrBufferCount = sNodeMap.GetNode("StreamBufferCountManual");
        ptrBufferCount->SetValue(10);
        //cout << "Buffer count now set to: " << ptrBufferCount->GetValue() << endl;

        pCam->BeginAcquisition();
    }

    // This method defines the removal event on the system. It does the same
    // as the system arrival event - it retrieves the number of cameras 
    // currently connected and prints it out.
    void OnDeviceRemoval(uint64_t deviceSerialNumber)
    {
        cout << "Device removal sn: " << deviceSerialNumber << endl;
        CameraPtr pCam = m_system->GetCameras().GetBySerial(to_string(deviceSerialNumber));

        if (pCam != NULL && pCam->IsStreaming()) {
            pCam->EndAcquisition();
            cout << "Camera: " << deviceSerialNumber << " end acquisition" << endl;
        }

        if (pCam != NULL && pCam->IsInitialized()) {
            pCam->DeInit();
            cout << "Camera: " << deviceSerialNumber << " deinit" << endl;
        }
    }

private:

    SystemPtr m_system;
};


// This function acts as the body of the example; please see NodeMapInfo example 
// for more in-depth comments on setting up cameras.
int RunMultipleCamera(SystemPtr pSystem)
{
    int result = 0;

    try
    {

        vector<string> camSerialList;
        CameraList camList = pSystem->GetCameras();

        for (int i = 0; i < camList.GetSize(); i++) {

            CStringPtr ptrStringSerial = camList.GetByIndex(i)->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");
            camSerialList.push_back(string(ptrStringSerial->GetValue()));
            if (camList.GetByIndex(i)->IsStreaming())
            {
                cout << "End Acquisition for camera: " << ptrStringSerial->GetValue() << endl;
                camList.GetByIndex(i)->EndAcquisition();
            }

            camList.GetByIndex(i)->Init();
            cout << "Camera with SN: " << ptrStringSerial->GetValue() << " initialized" << endl;
        }

        // Reset the cameras
        camList = pSystem->GetCameras();

        for (int i = 0; i < camSerialList.size(); i++) {
            CCommandPtr ptrResetCommand = camList.GetBySerial(camSerialList[i])->GetNodeMap().GetNode("DeviceReset");
            if (!IsAvailable(ptrResetCommand) || !IsWritable(ptrResetCommand))
            {
                cout << "Unable to execute reset. Aborting..." << endl;
                return -1;
            }

            ptrResetCommand->Execute();

            cout << "Camera: " << camSerialList[i] << " has been reset" << endl;
        }

        int camCount = 0;


        while (camCount < camSerialList.size()) {
            camList = pSystem->GetCameras();
            cout << " CameraList size = " << camList.GetSize() << endl;
            camCount = 0;

            for (string& sn : camSerialList) {
                if (camList.GetBySerial(sn) != NULL &&
                    camList.GetBySerial(sn)->IsInitialized() &&
                    camList.GetBySerial(sn)->IsStreaming()) {
                    camCount++;
                    cout << " " << sn;
                }
            }
            cout << endl;
        }

        cout << endl;

        camList = pSystem->GetCameras();

        // Grab 10 image from each camrea
        for (int j = 0; j < 10; j++) {
            for (int i = 0; i < camList.GetSize(); i++) {
                ImagePtr pResultImage = camList.GetByIndex(i)->GetNextImage();
                CStringPtr ptrStringSerial = camList.GetByIndex(i)->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");
                cout << "SN: " << ptrStringSerial->GetValue() << " Image number: " << j << endl;
            }
        }

        //De init all
        for (int i = 0; i < camList.GetSize(); i++) {
            camList.GetByIndex(i)->EndAcquisition();
            camList.GetByIndex(i)->DeInit();
            CStringPtr ptrStringSerial = camList.GetByIndex(i)->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");
            cout << "Camera with SN: " << ptrStringSerial->GetValue() << " denitialized" << endl;
        }

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

    //
    // Create interface event for the system
    //
    // *** NOTES ***
    // The SystemEventHandler has been constructed to accept a system object in 
    // order to print the number of cameras on the system.
    //
    SystemEventHandler systemEventHandler(system);

    //
    // Register interface event for the system
    //
    // *** NOTES ***
    // Arrival, removal, and interface events can all be registered to 
    // interfaces or the system. Do not think that interface events can only be
    // registered to an interface. An interface event is merely a combination
    // of an arrival and a removal event.
    //
    // *** LATER ***
    // Arrival, removal, and interface events must all be unregistered manually.
    // This must be done prior to releasing the system and while they are still
    // in scope.
    //
    system->RegisterInterfaceEvent(systemEventHandler);

    // Print out current library version
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: "
        << spinnakerLibraryVersion.major << "."
        << spinnakerLibraryVersion.minor << "."
        << spinnakerLibraryVersion.type << "."
        << spinnakerLibraryVersion.build << endl << endl;

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

    int result = 0;

    RunMultipleCamera(system);

    //
    // Unregister system event from system object
    //
    // *** NOTES ***
    // It is important to unregister all arrival, removal, and interface events
    // registered to the system.
    //

    // Clear camera list before releasing system
    system->GetCameras().Clear();

    system->UnregisterInterfaceEvent(systemEventHandler);

    cout << "Event handler unregistered from system..." << endl;


    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return result;
}