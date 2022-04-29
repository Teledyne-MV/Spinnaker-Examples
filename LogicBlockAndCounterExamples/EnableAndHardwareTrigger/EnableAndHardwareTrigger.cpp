#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

// This function configures the camera to use a trigger.
int ConfigureTrigger(INodeMap & nodeMap)
{
	int result = 0;

	cout << endl << endl << "*** CONFIGURING TRIGGER ***" << endl << endl;

	try
	{
		//
		// Ensure trigger mode off
		//
		// *** NOTES ***
		// The trigger must be disabled in order to configure whether the source
		// is software or hardware.
		//
		CEnumerationPtr ptrTriggerMode = nodeMap.GetNode("TriggerMode");
		if (!IsAvailable(ptrTriggerMode) || !IsReadable(ptrTriggerMode))
		{
			cout << "Unable to disable trigger mode (node retrieval). Aborting..." << endl;
			return -1;
		}

		CEnumEntryPtr ptrTriggerModeOff = ptrTriggerMode->GetEntryByName("Off");
		if (!IsAvailable(ptrTriggerModeOff) || !IsReadable(ptrTriggerModeOff))
		{
			cout << "Unable to disable trigger mode (enum entry retrieval). Aborting..." << endl;
			return -1;
		}

		ptrTriggerMode->SetIntValue(ptrTriggerModeOff->GetValue());

		cout << "Trigger mode disabled..." << endl;

		//
		// Select trigger source
		//
		// *** NOTES ***
		// The trigger source must be set to hardware or software while trigger 
		// mode is off.
		//
		CEnumerationPtr ptrTriggerSource = nodeMap.GetNode("TriggerSource");
		if (!IsAvailable(ptrTriggerSource) || !IsWritable(ptrTriggerSource))
		{
			cout << "Unable to set trigger mode (node retrieval). Aborting..." << endl;
			return -1;
		}
		// Set trigger mode to logic block 0
		CEnumEntryPtr ptrTriggerSourceLogicBlock0 = ptrTriggerSource->GetEntryByName("LogicBlock0");
		if (!IsAvailable(ptrTriggerSourceLogicBlock0) || !IsReadable(ptrTriggerSourceLogicBlock0))
		{
			cout << "Unable to set trigger mode (enum entry retrieval). Aborting..." << endl;
			return -1;
		}

		ptrTriggerSource->SetIntValue(ptrTriggerSourceLogicBlock0->GetValue());

		cout << "Trigger source set to logic block 1..." << endl;


		//
		// Turn trigger mode on
		//
		// *** LATER ***
		// Once the appropriate trigger source has been set, turn trigger mode 
		// on in order to retrieve images using the trigger.
		//

		CEnumEntryPtr ptrTriggerModeOn = ptrTriggerMode->GetEntryByName("On");
		if (!IsAvailable(ptrTriggerModeOn) || !IsReadable(ptrTriggerModeOn))
		{
			cout << "Unable to enable trigger mode (enum entry retrieval). Aborting..." << endl;
			return -1;
		}

		ptrTriggerMode->SetIntValue(ptrTriggerModeOn->GetValue());

		cout << "Trigger mode turned back on..." << endl << endl;
	}
	catch (Spinnaker::Exception &e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}

	return result;
}


// This function does the logic block 0 configuration
int ConfigureLogicBlock0(INodeMap & nodeMap)
{
	int result = 0;

	cout << endl << endl << "*** CONFIGURING LOGIC BLOCKS ***" << endl << endl;

	// Select Logic Block 0
	CEnumerationPtr ptrLogicBlockSelector = nodeMap.GetNode("LogicBlockSelector");
	if (!IsAvailable(ptrLogicBlockSelector) || !IsReadable(ptrLogicBlockSelector))
	{
		cout << "Unable to set logic block selector to Logic Block 0 (node retrieval). Non-fatal error..." << endl;
		return -1;
	}

	CEnumEntryPtr ptrLogicBlock0 = ptrLogicBlockSelector->GetEntryByName("LogicBlock0");
	if (!IsAvailable(ptrLogicBlock0) || !IsReadable(ptrLogicBlock0))
	{
		cout << "Unable to set logic block selector to Logic Block 0 (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLogicBlockSelector->SetIntValue(ptrLogicBlock0->GetValue());

	cout << "Logic Block 0 selected...." << endl;

	// Set Logic Block Lut to Enable
	CEnumerationPtr ptrLBLUTSelector = nodeMap.GetNode("LogicBlockLUTSelector");
	if (!IsAvailable(ptrLBLUTSelector) || !IsReadable(ptrLBLUTSelector))
	{
		cout << "Unable to set LUT logic block selector to Enable (node retrieval). Non-fatal error..." << endl;
		return -1;
	}

	CEnumEntryPtr ptrLBLUTEnable = ptrLBLUTSelector->GetEntryByName("Enable");
	if (!IsAvailable(ptrLBLUTEnable) || !IsReadable(ptrLBLUTEnable))
	{
		cout << "Unable to set LUT logic block selector to Enable (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTSelector->SetIntValue(ptrLBLUTEnable->GetValue());

	cout << "Logic Block LUT set to to Enable..." << endl;

	// Set Logic Block LUT Output Value All to 0xFF
	CIntegerPtr ptrLBLUTOutputValueAll = nodeMap.GetNode("LogicBlockLUTOutputValueAll");
	if (!IsAvailable(ptrLBLUTOutputValueAll) || !IsReadable(ptrLBLUTOutputValueAll))
	{
		cout << "Unable to set value to LUT logic block output value all (integer retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTOutputValueAll->SetValue(0xFF);

	cout << "Logic Block LUT Output Value All set to to 0xFF..." << endl;

	// Set Logic Block LUT Input Selector to Input 0
	CEnumerationPtr ptrLBLUTInputSelector = nodeMap.GetNode("LogicBlockLUTInputSelector");
	if (!IsAvailable(ptrLBLUTInputSelector) || !IsReadable(ptrLBLUTInputSelector))
	{
		cout << "Unable to set LUT logic block input selector to Input 0 (node retrieval). Non-fatal error..." << endl;
		return -1;
	}

	CEnumEntryPtr ptrLBLUTInput0 = ptrLBLUTInputSelector->GetEntryByName("Input0");
	if (!IsAvailable(ptrLBLUTInput0) || !IsReadable(ptrLBLUTInput0))
	{
		cout << "Unable to set LUT logic block selector to Input 0 (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTInputSelector->SetIntValue(ptrLBLUTInput0->GetValue());

	cout << "Logic Block LUT Input Selector set to to Input 0 ..." << endl;

	// Set Logic Block LUT Input Source to Line0
	CEnumerationPtr ptrLBLUTSource = nodeMap.GetNode("LogicBlockLUTInputSource");
	if (!IsAvailable(ptrLBLUTSource) || !IsReadable(ptrLBLUTSource))
	{
		cout << "Unable to set LUT logic block input source to Frame Trigger Wait (node retrieval). Non-fatal error..." << endl;
		return -1;
	}

	CEnumEntryPtr ptrLBLUTSourceL0 = ptrLBLUTSource->GetEntryByName("Line0");
	if (!IsAvailable(ptrLBLUTSourceL0) || !IsReadable(ptrLBLUTSourceL0))
	{
		cout << "Unable to set LUT logic block input source to Line0 (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTSource->SetIntValue(ptrLBLUTSourceL0->GetValue());

	cout << "Logic Block LUT Input Source set to to Line0 ..." << endl;

	// Set Logic Block LUT Activation Type to Rising Edge
	CEnumerationPtr ptrLBLUTActivation = nodeMap.GetNode("LogicBlockLUTInputActivation");
	if (!IsAvailable(ptrLBLUTActivation) || !IsReadable(ptrLBLUTActivation))
	{
		cout << "Unable to set LUT logic block input activation to RisingEdge (node retrieval). Non-fatal error..." << endl;
		return -1;
	}

	CEnumEntryPtr ptrLBLUTActivationRE = ptrLBLUTActivation->GetEntryByName("RisingEdge");
	if (!IsAvailable(ptrLBLUTActivationRE) || !IsReadable(ptrLBLUTActivationRE))
	{
		cout << "Unable to set LUT logic block input activation to RisingEdge (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTActivation->SetIntValue(ptrLBLUTActivationRE->GetValue());

	cout << "Logic Block LUT Input Activation set to rising edge.." << endl;

	// Set Logic Block LUT Input Selector to Input 1
	CEnumEntryPtr ptrLBLUTInput1 = ptrLBLUTInputSelector->GetEntryByName("Input1");
	if (!IsAvailable(ptrLBLUTInput1) || !IsReadable(ptrLBLUTInput1))
	{
		cout << "Unable to set LUT logic block Input selector to input 1 (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTInputSelector->SetIntValue(ptrLBLUTInput1->GetValue());

	cout << "Logic Block LUT Input Selector set to to Input 1 ..." << endl;

	// Set Logic Block LUT Source to Line2
	CEnumEntryPtr ptrLBLUTSourceL2 = ptrLBLUTSource->GetEntryByName("Line2");
	if (!IsAvailable(ptrLBLUTSourceL2) || !IsReadable(ptrLBLUTSourceL2))
	{
		cout << "Unable to set LUT logic block input source to Line2 (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTSource->SetIntValue(ptrLBLUTSourceL2->GetValue());

	cout << "Logic Block LUT Input Source set to to Line2 ..." << endl;

	// Set Logic Block LUT Activation Type to Level High
	CEnumEntryPtr ptrLBLUTActivationLH = ptrLBLUTActivation->GetEntryByName("LevelHigh");
	if (!IsAvailable(ptrLBLUTActivationLH) || !IsReadable(ptrLBLUTActivationLH))
	{
		cout << "Unable to set LUT logic block input activation to Level High (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTActivation->SetIntValue(ptrLBLUTActivationLH->GetValue());

	cout << "Logic Block LUT Input Activation set to Level High..." << endl;

	// Set Logic Block LUT Input Selector to Input 2
	CEnumEntryPtr ptrLBLUTInput2 = ptrLBLUTInputSelector->GetEntryByName("Input2");
	if (!IsAvailable(ptrLBLUTInput2) || !IsReadable(ptrLBLUTInput2))
	{
		cout << "Unable to set LUT logic block selector to input 2 (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTInputSelector->SetIntValue(ptrLBLUTInput2->GetValue());

	cout << "Logic Block LUT Input Selector set to to Input 2 ..." << endl;

	ptrLBLUTActivation->SetIntValue(ptrLBLUTActivationLH->GetValue());

	cout << "Logic Block LUT Input Activation set to Level High..." << endl;

	// Set Logic Block LUT Source to Zero since the logic block only depends on Inputs 0 and 1
	CEnumEntryPtr ptrLBLUTSourceZero = ptrLBLUTSource->GetEntryByName("Zero");
	if (!IsAvailable(ptrLBLUTSourceZero) || !IsReadable(ptrLBLUTSourceZero))
	{
		cout << "Unable to set LUT logic block input source to Zero (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTSource->SetIntValue(ptrLBLUTSourceZero->GetValue());

	cout << "Logic Block LUT Input Source set to to Zero ..." << endl;


	// Set Logic Block Lut Selector to Value
	CEnumEntryPtr ptrLBLUTValue = ptrLBLUTSelector->GetEntryByName("Value");
	if (!IsAvailable(ptrLBLUTValue) || !IsReadable(ptrLBLUTValue))
	{
		cout << "Unable to set LUT logic block selector to Value (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTSelector->SetIntValue(ptrLBLUTValue->GetValue());

	cout << "Logic Block LUT set to to Value..." << endl;

	// Set Logic Block LUT output Value All to 0x88
	ptrLBLUTOutputValueAll->SetValue(0x88);

	cout << "Logic Block LUT Output Value All set to to 0x88..." << endl << endl;

	return result;
}


int GrabNextImageByTrigger(INodeMap & nodeMap, CameraPtr pCam)
{
	int result = 0;

	try
	{

		cout << " Set the enable input to high and hardware trigger to trigger image acquisition." << endl;

	}
	catch (Spinnaker::Exception &e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}

	return result;
}

// This function returns the camera to a normal state by turning off trigger 
// mode.
int ResetTrigger(INodeMap & nodeMap)
{
	int result = 0;

	try
	{
		//
		// Turn trigger mode back off
		//
		// *** NOTES ***
		// Once all images have been captured, turn trigger mode back off to
		// restore the camera to a clean state.
		//
		CEnumerationPtr ptrTriggerMode = nodeMap.GetNode("TriggerMode");
		if (!IsAvailable(ptrTriggerMode) || !IsReadable(ptrTriggerMode))
		{
			cout << "Unable to disable trigger mode (node retrieval). Non-fatal error..." << endl;
			return -1;
		}

		CEnumEntryPtr ptrTriggerModeOff = ptrTriggerMode->GetEntryByName("Off");
		if (!IsAvailable(ptrTriggerModeOff) || !IsReadable(ptrTriggerModeOff))
		{
			cout << "Unable to disable trigger mode (enum entry retrieval). Non-fatal error..." << endl;
			return -1;
		}

		ptrTriggerMode->SetIntValue(ptrTriggerModeOff->GetValue());

		cout << "Trigger mode disabled..." << endl << endl;
	}
	catch (Spinnaker::Exception &e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}

	return result;
}

// This function configures I/O
int ConfigureIO(INodeMap & nodeMap)
{
	int result = 0;

	try
	{

		// Set Line Selector
		CEnumerationPtr ptrLineSelector = nodeMap.GetNode("LineSelector");
		if (!IsAvailable(ptrLineSelector) || !IsWritable(ptrLineSelector))
		{
			cout << "Unable to set Line Selector (enum retrieval). Aborting..." << endl << endl;
			return -1;
		}

		// Change Line Selector to Line 2
		CEnumEntryPtr ptrLineSelectorLine2 = ptrLineSelector->GetEntryByName("Line2");
		if (!IsAvailable(ptrLineSelectorLine2) || !IsReadable(ptrLineSelectorLine2))
		{
			cout << "Unable to set Line Selector (entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		int64_t lineSelectorLine2 = ptrLineSelectorLine2->GetValue();

		ptrLineSelector->SetIntValue(lineSelectorLine2);

		//Set Line Mode to Input
		CEnumerationPtr ptrLineMode = nodeMap.GetNode("LineMode");
		if (!IsAvailable(ptrLineMode) || !IsWritable(ptrLineMode))
		{
			cout << "Unable to set Line Mode (enum retrieval). Aborting..." << endl << endl;
			return -1;
		}

		CEnumEntryPtr ptrLineModeInput = ptrLineMode->GetEntryByName("Input");
		if (!IsAvailable(ptrLineModeInput) || !IsReadable(ptrLineModeInput))
		{
			cout << "Unable to set Line Mode (entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		int64_t lineModeInput = ptrLineModeInput->GetValue();

		ptrLineMode->SetIntValue(lineModeInput);

		cout << "Line 2 set to input" << endl << endl;
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

// This function acquires and saves 10 images from a device; please see
// Acquisition example for more in-depth comments on acquiring images.
int AcquireImages(CameraPtr pCam, INodeMap & nodeMap, INodeMap & nodeMapTLDevice)
{
	int result = 0;

	cout << endl << "*** IMAGE ACQUISITION ***" << endl << endl;

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

		// Retrieve, convert, and save images
		const int unsigned k_numImages = 10;

		for (unsigned int imageCnt = 0; imageCnt < k_numImages; imageCnt++)
		{
			try
			{
				// Retrieve the next image from the trigger
				result = result | GrabNextImageByTrigger(nodeMap, pCam);

				// Retrieve the next received image
				ImagePtr pResultImage = pCam->GetNextImage();

				if (pResultImage->IsIncomplete())
				{
					cout << "Image incomplete with image status " << pResultImage->GetImageStatus() << "..." << endl << endl;
				}
				else
				{
					// Print image information
					cout << "Grabbed image " << imageCnt << ", width = " << pResultImage->GetWidth() << ", height = " << pResultImage->GetHeight() << endl;

					// Convert image to mono 8
					ImagePtr convertedImage = pResultImage->Convert(PixelFormat_Mono8, HQ_LINEAR);

					// Create a unique filename
					ostringstream filename;

					filename << "Trigger-";
					if (deviceSerialNumber != "")
					{
						filename << deviceSerialNumber.c_str() << "-";
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

		// Configure Line 2 as input
		err = ConfigureIO(nodeMap);
		if (err < 0)
		{
			return err;
		}

		// Cofigure Logic Block 0
		err = ConfigureLogicBlock0(nodeMap);
		if (err < 0)
		{
			return err;
		}


		// Configure trigger
		err = ConfigureTrigger(nodeMap);
		if (err < 0)
		{
			return err;
		}

		// Acquire images
		result = result | AcquireImages(pCam, nodeMap, nodeMapTLDevice);

		// Reset trigger
		result = result | ResetTrigger(nodeMap);

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

