/*

Srtobe before Exposure.
Output a strobe signal before beginning exposure.

High level description:

Counter0 is used as a microsecond timer to start the strobe at regualar intervals (1/Framerate)

Counter1 is used to time the offset between the strobe start and exposure start

LogicBlock0 is used to determine the rising and falling edges of the strobe.
	Rising edge = Counter0start
	Falling edge = ExposureEnd

UserOutput0 is used to activate Counter0 and thus start acquisition of images

Low level details:

Counter 0
Description: (1/Framerate) timer
Counter Event: MHzTick
Counter Event Activation: Rising Edge
Counter Durration: uS_FRAME_RATE_TIMER
Counter Trigger Source: UserOutput0
Counter Trigger Activation: Level High

Counter 1
Description: Exposure Offset from Strobe
Counter Event: MHzTick
Counter Event Activation: Rising Edge
Counter Durration: us_STROBE_OFFSET
Counter Reset Source: Counter0Start
Counter Reset Activation: Rising Edge

Logic Block 0
Description: Drive the strobe
HIGH on Counter0Start'event
LOW on ExposureEnd'event
Input 0: Counter 1 Start (Rising edge)
Input 1: Exposure End (Rising edge)
Input 2: 0 - Not used (Level High)
Value LUT: 0x22
Enable LUT: 0xEE
+----+----+----+--------+--------+-------+
| I2 | I1 | I0 | Output | Enable | Value |
+----+----+----+--------+--------+-------+
|  0 |  0 |  0 | Q      |      0 |     0 |
|  0 |  0 |  1 | 1      |      1 |     1 |
|  0 |  1 |  0 | 0      |      1 |     0 |
|  0 |  1 |  1 | 0      |      1 |     0 |
|  1 |  0 |  0 | Q      |      0 |     0 |
|  1 |  0 |  1 | 1      |      1 |     1 |
|  1 |  1 |  0 | 0      |      1 |     0 |
|  1 |  1 |  1 | 0      |      1 |     0 |
+----+----+----+--------+--------+-------+

*/
//==============================================================================
#include <iostream>
#include <sstream>
#include <windows.h>
#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"

// This value is 1/FPS in mircoseconds
#define uS_FRAME_RATE_TIMER 5555
#define uS_STROBE_OFFSET 500
#define uS_EXPOSURE_TIME 500

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;


int userOutputSet(INodeMap & nodeMap, char *  userOutputStr, bool val) {
	int result = 0;
	// Configure the user output 
	CEnumerationPtr ptrUserOutputSelector = nodeMap.GetNode("UserOutputSelector");

	if (!IsAvailable(ptrUserOutputSelector) || !IsWritable(ptrUserOutputSelector))
	{
		cout << "Unable to set UserOutputSelector (enum retrieval). Aborting..." << endl << endl;
		return -1;
	}

	CEnumEntryPtr ptrUserOutput = ptrUserOutputSelector->GetEntryByName(userOutputStr);
	if (!IsAvailable(ptrUserOutput) || !IsReadable(ptrUserOutput))
	{
		cout << "Unable to set UserOutputSelector (entry retrieval). Aborting..." << endl << endl;
		return -1;
	}

	int64_t userInput = ptrUserOutput->GetValue();

	ptrUserOutputSelector->SetIntValue(userInput);


	CBooleanPtr ptrOutputValue = nodeMap.GetNode("UserOutputValue");
	if (!IsAvailable(ptrOutputValue) || !IsWritable(ptrOutputValue))
	{
		cout << "Unable to set User Output Enable (boolean retrieval). Aborting..." << endl << endl;
		return -1;
	}

	ptrOutputValue->SetValue(val);

	return result;
}

void userOutputPulse(INodeMap & nodeMap, char *  userOutputStr, uint32_t pulseLength_milli) {
	userOutputSet(nodeMap, userOutputStr, true);
	Sleep(pulseLength_milli);
	userOutputSet(nodeMap, userOutputStr, false);
}

// configure counter 0
int SetupCounter0(INodeMap & nodeMap, int durration)
{
	int result = 0;

	cout << endl << "Configuring Pulse Width Modulation signal" << endl;

	try
	{
		// Set Counter Selector to Counter 0
		CEnumerationPtr ptrCounterSelector = nodeMap.GetNode("CounterSelector");

		// Check to see if camera supports Counter and Timer functionality
		if (!IsAvailable(ptrCounterSelector))
		{
			cout << endl << "Camera does not support Counter and Timer Functionality.  Aborting..." << endl;
			return -1;
		}

		if (!IsAvailable(ptrCounterSelector) || !IsWritable(ptrCounterSelector))
		{
			cout << "Unable to set Counter Selector (enum retrieval). Aborting..." << endl << endl;
			return -1;
		}

		CEnumEntryPtr ptrCounter0 = ptrCounterSelector->GetEntryByName("Counter0");
		if (!IsAvailable(ptrCounter0) || !IsReadable(ptrCounter0))
		{
			cout << "Unable to set Counter Selector (entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		int64_t counter0 = ptrCounter0->GetValue();

		ptrCounterSelector->SetIntValue(counter0);

		// Set Counter Duration to input value
		CIntegerPtr ptrCounterDuration = nodeMap.GetNode("CounterDuration");
		if (!IsAvailable(ptrCounterDuration) || !IsWritable(ptrCounterDuration))
		{
			cout << "Unable to set Counter Duration (integer retrieval). Aborting..." << endl << endl;
			return -1;
		}

		ptrCounterDuration->SetValue(durration);


		// Set Counter Event Source to MHzTick
		CEnumerationPtr ptrCounterEventSource = nodeMap.GetNode("CounterEventSource");
		if (!IsAvailable(ptrCounterEventSource) || !IsWritable(ptrCounterEventSource))
		{
			cout << "Unable to set Counter Event Source (enum retrieval). Aborting..." << endl << endl;
			return -1;
		}

		CEnumEntryPtr ptrCounterEventSourceMHzTick = ptrCounterEventSource->GetEntryByName("MHzTick");
		if (!IsAvailable(ptrCounterEventSourceMHzTick) || !IsReadable(ptrCounterEventSourceMHzTick))
		{
			cout << "Unable to set Counter Event Source (entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		int64_t counterEventSourceMHzTick = ptrCounterEventSourceMHzTick->GetValue();

		ptrCounterEventSource->SetIntValue(counterEventSourceMHzTick);


		CEnumerationPtr ptrCounterTriggerSource = nodeMap.GetNode("CounterTriggerSource");
		if (!IsAvailable(ptrCounterTriggerSource) || !IsWritable(ptrCounterTriggerSource))
		{
			cout << "Unable to set Counter Trigger Source (enum retrieval). Aborting..." << endl << endl;
			return -1;
		}

		CEnumEntryPtr ptrCounterTriggerSourceVal = ptrCounterTriggerSource->GetEntryByName("UserOutput0");
		if (!IsAvailable(ptrCounterTriggerSourceVal) || !IsReadable(ptrCounterTriggerSourceVal))
		{
			cout << "Unable to set Counter Trigger Source (entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		int64_t counterTriggerSourceOutput = ptrCounterTriggerSourceVal->GetValue();

		ptrCounterTriggerSource->SetIntValue(counterTriggerSourceOutput);


		// Set Counter Reset Activation
		CEnumerationPtr ptrCounterTriggerActivation = nodeMap.GetNode("CounterTriggerActivation");
		if (!IsAvailable(ptrCounterTriggerActivation) || !IsWritable(ptrCounterTriggerActivation))
		{
			cout << "Unable to set Counter Trigger Activation (enum retrieval). Aborting..." << endl << endl;
			return -1;
		}

		CEnumEntryPtr ptrCounterTriggerActivationLH = ptrCounterTriggerActivation->GetEntryByName("LevelHigh");
		if (!IsAvailable(ptrCounterTriggerActivationLH) || !IsReadable(ptrCounterTriggerActivationLH))
		{
			cout << "Unable to set Counter Trigger Activation (entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		int64_t counterTriggerActivationRE = ptrCounterTriggerActivationLH->GetValue();

		ptrCounterTriggerActivation->SetIntValue(counterTriggerActivationRE);
	}
	catch (Spinnaker::Exception &e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}

	return result;
}


// configure Counter 1
int SetupCounter1(INodeMap & nodeMap, int durration)
{
	int result = 0;

	cout << endl << "Configuring Pulse Width Modulation signal" << endl;

	try
	{
		// Set Counter Selector to Counter 1
		CEnumerationPtr ptrCounterSelector = nodeMap.GetNode("CounterSelector");

		// Check to see if camera supports Counter and Timer functionality
		if (!IsAvailable(ptrCounterSelector))
		{
			cout << endl << "Camera does not support Counter and Timer Functionality.  Aborting..." << endl;
			return -1;
		}

		if (!IsAvailable(ptrCounterSelector) || !IsWritable(ptrCounterSelector))
		{
			cout << "Unable to set Counter Selector (enum retrieval). Aborting..." << endl << endl;
			return -1;
		}

		CEnumEntryPtr ptrCounter1 = ptrCounterSelector->GetEntryByName("Counter1");
		if (!IsAvailable(ptrCounter1) || !IsReadable(ptrCounter1))
		{
			cout << "Unable to set Counter Selector (entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		int64_t counter1 = ptrCounter1->GetValue();

		ptrCounterSelector->SetIntValue(counter1);
		// Set Counter Duration to input value
		CIntegerPtr ptrCounterDuration = nodeMap.GetNode("CounterDuration");
		if (!IsAvailable(ptrCounterDuration) || !IsWritable(ptrCounterDuration))
		{
			cout << "Unable to set Counter Duration (integer retrieval). Aborting..." << endl << endl;
			return -1;
		}

		ptrCounterDuration->SetValue(durration);


		// Set Counter Event Source to MHzTick
		CEnumerationPtr ptrCounterEventSource = nodeMap.GetNode("CounterEventSource");
		if (!IsAvailable(ptrCounterEventSource) || !IsWritable(ptrCounterEventSource))
		{
			cout << "Unable to set Counter Event Source (enum retrieval). Aborting..." << endl << endl;
			return -1;
		}

		CEnumEntryPtr ptrCounterEventSourceMHzTick = ptrCounterEventSource->GetEntryByName("MHzTick");
		if (!IsAvailable(ptrCounterEventSourceMHzTick) || !IsReadable(ptrCounterEventSourceMHzTick))
		{
			cout << "Unable to set Counter Event Source (entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		int64_t counterEventSourceMHzTick = ptrCounterEventSourceMHzTick->GetValue();

		ptrCounterEventSource->SetIntValue(counterEventSourceMHzTick);

		CEnumerationPtr ptrCounterTriggerSource = nodeMap.GetNode("CounterTriggerSource");
		if (!IsAvailable(ptrCounterTriggerSource) || !IsWritable(ptrCounterTriggerSource))
		{
			cout << "Unable to set Counter Trigger Source (enum retrieval). Aborting..." << endl << endl;
			return -1;
		}

		CEnumEntryPtr ptrCounterTriggerSourceVal = ptrCounterTriggerSource->GetEntryByName("Off");
		if (!IsAvailable(ptrCounterTriggerSourceVal) || !IsReadable(ptrCounterTriggerSourceVal))
		{
			cout << "Unable to set Counter Trigger Source (entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		int64_t counterTriggerSourceOutput = ptrCounterTriggerSourceVal->GetValue();

		ptrCounterTriggerSource->SetIntValue(counterTriggerSourceOutput);

		CEnumerationPtr ptrCounterResetSource = nodeMap.GetNode("CounterResetSource");
		if (!IsAvailable(ptrCounterResetSource) || !IsWritable(ptrCounterResetSource))
		{
			cout << "Unable to set Counter Reset Source (enum retrieval). Aborting..." << endl << endl;
			return -1;
		}

		CEnumEntryPtr ptrCounterResetSourceVal= ptrCounterResetSource->GetEntryByName("Counter0Start");
		if (!IsAvailable(ptrCounterResetSourceVal) || !IsReadable(ptrCounterResetSourceVal))
		{
			cout << "Unable to set Counter Reset Source (entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		int64_t counterResetSourceLogicBlock = ptrCounterResetSourceVal->GetValue();

		ptrCounterResetSource->SetIntValue(counterResetSourceLogicBlock);


		// Set Counter Reset Activation to Rising Edge
		CEnumerationPtr ptrCounterResetActivation = nodeMap.GetNode("CounterResetActivation");
		if (!IsAvailable(ptrCounterResetActivation) || !IsWritable(ptrCounterResetActivation))
		{
			cout << "Unable to set Counter Reset Activation (enum retrieval). Aborting..." << endl << endl;
			return -1;
		}

		CEnumEntryPtr ptrCounterResetActivationRE = ptrCounterResetActivation->GetEntryByName("RisingEdge");
		if (!IsAvailable(ptrCounterResetActivationRE) || !IsReadable(ptrCounterResetActivationRE))
		{
			cout << "Unable to set Counter Reset Activation (entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		int64_t counterResetActivationRE = ptrCounterResetActivationRE->GetValue();

		ptrCounterResetActivation->SetIntValue(counterResetActivationRE);
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


	CIntegerPtr ptrLBLUTOutputValueAll = nodeMap.GetNode("LogicBlockLUTOutputValueAll");
	if (!IsAvailable(ptrLBLUTOutputValueAll) || !IsReadable(ptrLBLUTOutputValueAll))
	{
		cout << "Unable to set value to LUT logic block output value all (integer retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTOutputValueAll->SetValue(0xEE);

	cout << "Logic Block LUT Output Value All set..." << endl;

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

	CEnumerationPtr ptrLBLUTSource = nodeMap.GetNode("LogicBlockLUTInputSource");
	if (!IsAvailable(ptrLBLUTSource) || !IsReadable(ptrLBLUTSource))
	{
		cout << "Unable to set LUT logic block input source to Frame Trigger Wait (node retrieval). Non-fatal error..." << endl;
		return -1;
	}

	CEnumEntryPtr ptrLBLUTSourceInput0 = ptrLBLUTSource->GetEntryByName("Counter0Start");
	if (!IsAvailable(ptrLBLUTSourceInput0) || !IsReadable(ptrLBLUTSourceInput0))
	{
		cout << "Unable to set LUT logic block input 0 (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTSource->SetIntValue(ptrLBLUTSourceInput0->GetValue());

	cout << "Logic Block LUT Input Source 0 set ..." << endl;

	// Set Logic Block LUT Activation Type to Rising Edge
	CEnumerationPtr ptrLBLUTActivation = nodeMap.GetNode("LogicBlockLUTInputActivation");
	if (!IsAvailable(ptrLBLUTActivation) || !IsReadable(ptrLBLUTActivation))
	{
		cout << "Unable to set LUT logic block input activation to level high (node retrieval). Non-fatal error..." << endl;
		return -1;
	}

	CEnumEntryPtr ptrLBLUTActivationRE = ptrLBLUTActivation->GetEntryByName("RisingEdge");
	if (!IsAvailable(ptrLBLUTActivationRE) || !IsReadable(ptrLBLUTActivationRE))
	{
		cout << "Unable to set LUT logic block input activation to RisingEdge (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTActivation->SetIntValue(ptrLBLUTActivationRE->GetValue());

	cout << "Logic Block LUT Input Activation set to level high..." << endl;

	// Set Logic Block LUT Input Selector to Input 1
	CEnumEntryPtr ptrLBLUTInput1 = ptrLBLUTInputSelector->GetEntryByName("Input1");
	if (!IsAvailable(ptrLBLUTInput1) || !IsReadable(ptrLBLUTInput1))
	{
		cout << "Unable to set LUT logic block Input selector to input 1 (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTInputSelector->SetIntValue(ptrLBLUTInput1->GetValue());

	cout << "Logic Block LUT Input Selector set to to Input 1 ..." << endl;

	CEnumEntryPtr ptrLBLUTSourceInput1= ptrLBLUTSource->GetEntryByName("ExposureEnd");
	if (!IsAvailable(ptrLBLUTSourceInput1) || !IsReadable(ptrLBLUTSourceInput1))
	{
		cout << "Unable to set LUT logic block input source 1 (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTSource->SetIntValue(ptrLBLUTSourceInput1->GetValue());

	cout << "Logic Block LUT Input Source 1 set ..." << endl;

	// Set Logic Block LUT Activation Type to Rising Edge

	ptrLBLUTActivation->SetIntValue(ptrLBLUTActivationRE->GetValue());

	cout << "Logic Block LUT Input Activation set to Rising Edge..." << endl;

	// Set Logic Block LUT Input Selector to Input 2
	CEnumEntryPtr ptrLBLUTInput2 = ptrLBLUTInputSelector->GetEntryByName("Input2");
	if (!IsAvailable(ptrLBLUTInput2) || !IsReadable(ptrLBLUTInput2))
	{
		cout << "Unable to set LUT logic block selector to input 2 (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTInputSelector->SetIntValue(ptrLBLUTInput2->GetValue());

	cout << "Logic Block LUT Input Selector set to to Input 2 ..." << endl;

	CEnumEntryPtr ptrLBLUTSourceInput2= ptrLBLUTSource->GetEntryByName("Zero");
	if (!IsAvailable(ptrLBLUTSourceInput2) || !IsReadable(ptrLBLUTSourceInput2))
	{
		cout << "Unable to set LUT logic block input source 2 (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTSource->SetIntValue(ptrLBLUTSourceInput2->GetValue());

	cout << "Logic Block LUT Input Source 2 set ..." << endl;

	// Set Logic Block LUT Activation Type to Level High

	CEnumEntryPtr ptrLBLUTActivationLevelHigh = ptrLBLUTActivation->GetEntryByName("LevelHigh");
	if (!IsAvailable(ptrLBLUTActivationLevelHigh) || !IsReadable(ptrLBLUTActivationLevelHigh))
	{
		cout << "Unable to set LUT logic block input activation to level high (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTActivation->SetIntValue(ptrLBLUTActivationLevelHigh->GetValue());

	cout << "Logic Block LUT Input Activation set to level high..." << endl;


	// Set Logic Block Lut Selector to Value
	CEnumEntryPtr ptrLBLUTValue = ptrLBLUTSelector->GetEntryByName("Value");
	if (!IsAvailable(ptrLBLUTValue) || !IsReadable(ptrLBLUTValue))
	{
		cout << "Unable to set LUT logic block selector to Value (enum entry retrieval). Non-fatal error..." << endl;
		return -1;
	}

	ptrLBLUTSelector->SetIntValue(ptrLBLUTValue->GetValue());

	cout << "Logic Block LUT set to to Value..." << endl;

	// Set Logic Block LUT output Value All
	ptrLBLUTOutputValueAll->SetValue(0x22);

	cout << "Logic Block LUT Output Value All set" << endl << endl;

	return result;
}


// Strobe output
int ConfigureLine2Strobe(INodeMap & nodeMap) {
	int result = 0;
	cout << endl << endl << "*** CONFIGURING STROBE ***" << endl << endl;

	//Set Line Selector
	CEnumerationPtr ptrLineSelector = nodeMap.GetNode("LineSelector");
	if (!IsAvailable(ptrLineSelector) || !IsWritable(ptrLineSelector))
	{
		cout << "Unable to set Line Selector (enum retrieval). Aborting..." << endl << endl;
		return -1;
	}

	CEnumEntryPtr ptrLineSelectorLine2 = ptrLineSelector->GetEntryByName("Line2");
	if (!IsAvailable(ptrLineSelectorLine2) || !IsReadable(ptrLineSelectorLine2))
	{
		cout << "Unable to set Line Selector (entry retrieval). Aborting..." << endl << endl;
		return -1;
	}

	int64_t lineSelectorLine2 = ptrLineSelectorLine2->GetValue();

	ptrLineSelector->SetIntValue(lineSelectorLine2);

	//Set Line Mode to output
	CEnumerationPtr ptrLineMode = nodeMap.GetNode("LineMode");
	if (!IsAvailable(ptrLineMode) || !IsWritable(ptrLineMode))
	{
		cout << "Unable to set Line Mode (enum retrieval). Aborting..." << endl << endl;
		return -1;
	}

	CEnumEntryPtr ptrLineModeOutput = ptrLineMode->GetEntryByName("Output");
	if (!IsAvailable(ptrLineModeOutput) || !IsReadable(ptrLineModeOutput))
	{
		cout << "Unable to set Line Mode (entry retrieval). Aborting..." << endl << endl;
		return -1;
	}

	int64_t lineModeOutput = ptrLineModeOutput->GetValue();

	ptrLineMode->SetIntValue(lineModeOutput);

	// Set Line Source for Selected Line
	CEnumerationPtr ptrLineSource = nodeMap.GetNode("LineSource");
	if (!IsAvailable(ptrLineSource) || !IsWritable(ptrLineSource))
	{
		cout << "Unable to set Line Source (enum retrieval). Aborting..." << endl << endl;
		return -1;
	}

	CEnumEntryPtr ptrLineSourceVal= ptrLineSource->GetEntryByName("LogicBlock0");
	if (!IsAvailable(ptrLineSourceVal) || !IsReadable(ptrLineSourceVal))
	{
		cout << "Unable to set Line Source (entry retrieval). Aborting..." << endl << endl;
		return -1;
	}

	int64_t LineSourceVal = ptrLineSourceVal->GetValue();

	ptrLineSource->SetIntValue(LineSourceVal);

	return result;
}

// Exposure active Strobe 
// Just for debugging
int ConfigureLine1Strobe(INodeMap & nodeMap) {
	int result = 0;
	cout << endl << endl << "*** CONFIGURING STROBE ***" << endl << endl;

	//Set Line Selector
	CEnumerationPtr ptrLineSelector = nodeMap.GetNode("LineSelector");
	if (!IsAvailable(ptrLineSelector) || !IsWritable(ptrLineSelector))
	{
		cout << "Unable to set Line Selector (enum retrieval). Aborting..." << endl << endl;
		return -1;
	}

	CEnumEntryPtr ptrLineSelectorLine1 = ptrLineSelector->GetEntryByName("Line1");
	if (!IsAvailable(ptrLineSelectorLine1) || !IsReadable(ptrLineSelectorLine1))
	{
		cout << "Unable to set Line Selector (entry retrieval). Aborting..." << endl << endl;
		return -1;
	}

	int64_t lineSelectorLine1 = ptrLineSelectorLine1->GetValue();

	ptrLineSelector->SetIntValue(lineSelectorLine1);


	// Set Line Source for Selected Line to ExposureActive
	CEnumerationPtr ptrLineSource = nodeMap.GetNode("LineSource");
	if (!IsAvailable(ptrLineSource) || !IsWritable(ptrLineSource))
	{
		cout << "Unable to set Line Source (enum retrieval). Aborting..." << endl << endl;
		return -1;
	}

	CEnumEntryPtr ptrLineSourceExposure = ptrLineSource->GetEntryByName("ExposureActive");
	if (!IsAvailable(ptrLineSourceExposure) || !IsReadable(ptrLineSourceExposure))
	{
		cout << "Unable to set Line Source (entry retrieval). Aborting..." << endl << endl;
		return -1;
	}

	int64_t LineSourceCounterUserExposure = ptrLineSourceExposure->GetValue();

	ptrLineSource->SetIntValue(LineSourceCounterUserExposure);

	return result;
}

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


		// Set trigger mode to Counter 1 End
		CEnumEntryPtr ptrTriggerSourceVal = ptrTriggerSource->GetEntryByName("Counter1End");
		if (!IsAvailable(ptrTriggerSourceVal) || !IsReadable(ptrTriggerSourceVal))
		{
			cout << "Unable to set trigger mode (enum entry retrieval). Aborting..." << endl;
			return -1;
		}

		ptrTriggerSource->SetIntValue(ptrTriggerSourceVal->GetValue());

		cout << "Trigger source set..." << endl;


		// Set Trigger Overlap to true
		CEnumerationPtr ptrTriggerOverlap = nodeMap.GetNode("TriggerOverlap");

		if (!IsAvailable(ptrTriggerOverlap) || !IsReadable(ptrTriggerOverlap))
		{
			cout << "Unable to set trigger overlap to true (node retrieval). Aborting..." << endl;
			return -1;
		}

		CEnumEntryPtr ptrTriggerOverlapReadout = ptrTriggerOverlap->GetEntryByName("ReadOut");
		if (!IsAvailable(ptrTriggerOverlapReadout) || !IsReadable(ptrTriggerOverlapReadout))
		{
			cout << "Unable to disable trigger mode (enum entry retrieval). Aborting..." << endl;
			return -1;
		}

		ptrTriggerOverlap->SetIntValue(ptrTriggerOverlapReadout->GetValue());

		cout << "Trigger Overlap set to Read Out..." << endl;

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

		// TODO: Blackfly and Flea3 GEV cameras need 1 second delay after trigger mode is turned on 

		cout << "Trigger mode turned back on..." << endl << endl;
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



// This function configures a custom exposure time.Automatic exposure is turned
// off in order to allow for the customization, and then the custom setting is 
// applied.
int ConfigureExposure(INodeMap & nodeMap, double exposureTimeToSet)
{
	int result = 0;

	cout << endl << endl << "*** CONFIGURING EXPOSURE ***" << endl << endl;

	try
	{

		CEnumerationPtr ptrExposureAuto = nodeMap.GetNode("ExposureAuto");
		if (!IsAvailable(ptrExposureAuto) || !IsWritable(ptrExposureAuto))
		{
			cout << "Unable to disable automatic exposure (node retrieval). Aborting..." << endl << endl;
			return -1;
		}

		CEnumEntryPtr ptrExposureAutoOff = ptrExposureAuto->GetEntryByName("Off");
		if (!IsAvailable(ptrExposureAutoOff) || !IsReadable(ptrExposureAutoOff))
		{
			cout << "Unable to disable automatic exposure (enum entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		ptrExposureAuto->SetIntValue(ptrExposureAutoOff->GetValue());

		cout << "Automatic exposure disabled..." << endl;

		CFloatPtr ptrExposureTime = nodeMap.GetNode("ExposureTime");
		if (!IsAvailable(ptrExposureTime) || !IsWritable(ptrExposureTime))
		{
			cout << "Unable to set exposure time. Aborting..." << endl << endl;
			return -1;
		}

		// Ensure desired exposure time does not exceed the maximum
		const double exposureTimeMax = ptrExposureTime->GetMax();

		if (exposureTimeToSet > exposureTimeMax)
		{
			exposureTimeToSet = exposureTimeMax;
		}

		ptrExposureTime->SetValue(exposureTimeToSet);

		cout << "Exposure time set to " << exposureTimeToSet << " us..." << endl << endl;
	}
	catch (Spinnaker::Exception &e)
	{
		cout << "Error: " << e.what() << endl;
		result = -1;
	}

	return result;
}

int ResetExposure(INodeMap & nodeMap)
{
	int result = 0;

	try
	{
		// 
		// Turn automatic exposure back on
		//
		// *** NOTES ***
		// Automatic exposure is turned on in order to return the camera to its 
		// default state.
		//
		CEnumerationPtr ptrExposureAuto = nodeMap.GetNode("ExposureAuto");
		if (!IsAvailable(ptrExposureAuto) || !IsWritable(ptrExposureAuto))
		{
			cout << "Unable to enable automatic exposure (node retrieval). Non-fatal error..." << endl << endl;
			return -1;
		}

		CEnumEntryPtr ptrExposureAutoContinuous = ptrExposureAuto->GetEntryByName("Continuous");
		if (!IsAvailable(ptrExposureAutoContinuous) || !IsReadable(ptrExposureAutoContinuous))
		{
			cout << "Unable to enable automatic exposure (enum entry retrieval). Non-fatal error..." << endl << endl;
			return -1;
		}

		ptrExposureAuto->SetIntValue(ptrExposureAutoContinuous->GetValue());

		cout << "Automatic exposure enabled..." << endl << endl;
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



// This function acquires and saves 10 images from a device.  
int AcquireImages(CameraPtr pCam, INodeMap & nodeMap, INodeMap & nodeMapTLDevice)
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
		if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
		{
			cout << "Unable to set acquisition mode to continuous (enum retrieval). Aborting..." << endl << endl;
			return -1;
		}

		// Retrieve entry node from enumeration node
		CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
		if (!IsAvailable(ptrAcquisitionModeContinuous) || !IsReadable(ptrAcquisitionModeContinuous))
		{
			cout << "Unable to set acquisition mode to continuous (entry retrieval). Aborting..." << endl << endl;
			return -1;
		}

		// Retrieve integer value from entry node
		int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();

		// Set integer value from entry node as new value of enumeration node
		ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

		cout << "Acquisition mode set to continuous..." << endl;

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
		if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
		{
			deviceSerialNumber = ptrStringSerial->GetValue();

			cout << "Device serial number retrieved as " << deviceSerialNumber << "..." << endl;
		}
		cout << endl;

		// start the altrnating strobe
		userOutputSet(nodeMap, "UserOutput0", true);
		
		// Retrieve, convert, and save images
		const unsigned int k_numImages = 1000;

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
				ImagePtr pResultImage = pCam->GetNextImage();

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
					// Retreive and print the image status description
					cout << "Image incomplete: "
						<< Image::GetImageStatusDescription(pResultImage->GetImageStatus())
						<< "..." << endl << endl;
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
					size_t width = pResultImage->GetWidth();

					size_t height = pResultImage->GetHeight();

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
					//ImagePtr convertedImage = pResultImage->Convert(PixelFormat_Mono8, HQ_LINEAR);

					//// Create a unique filename
					//ostringstream filename;

					//filename << "Acquisition-";
					//if (deviceSerialNumber != "")
					//{
					//	filename << deviceSerialNumber.c_str() << "-";
					//}
					//filename << imageCnt << ".jpg";

					//
					// Save image
					// 
					// *** NOTES ***
					// The standard practice of the examples is to use device
					// serial numbers to keep images of one device from 
					// overwriting those of another.
					//
					//convertedImage->Save(filename.str().c_str());

					//cout << "Image saved at " << filename.str() << endl;
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
			catch (Spinnaker::Exception &e)
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

		// end the altrnating strobe
		userOutputSet(nodeMap, "UserOutput0", false);

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

		// ensure that user output 0 is false initially
		userOutputSet(nodeMap, "UserOutput0", false);

		err = ConfigureExposure(nodeMap, uS_EXPOSURE_TIME);
		if (err < 0)
		{
			return err;
		}

		err = ConfigureLine2Strobe(nodeMap);

		if (err < 0)
		{
			return err;
		}

		// configure line 1 strobe
		// exposure active (for debug purposes)
		err = ConfigureLine1Strobe(nodeMap);

		if (err < 0)
		{
			return err;
		}

		// LB0 used to determine when Counter 0 is actve
		err = ConfigureLogicBlock0(nodeMap);
		if (err < 0)
		{
			return err;
		}

		// Configure counters 0
		err = SetupCounter0(nodeMap, uS_FRAME_RATE_TIMER);
		if (err < 0)
		{
			return err;
		}

		// Configure counters 1
		err = SetupCounter1(nodeMap, uS_STROBE_OFFSET);
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

		// Reset Exposure
		result = result | ResetExposure(nodeMap);

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

// Example entry point; please see Enumeration example for more in-depth 
// comments on preparing and cleaning up the system.
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
