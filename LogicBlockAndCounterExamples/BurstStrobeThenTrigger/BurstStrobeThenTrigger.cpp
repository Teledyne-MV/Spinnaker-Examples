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
*  @example BurstStrobeThenTrigger.cpp
*
*  @brief BurstStrobeThenTrigger.cpp shows how to use logic blocks and counters
*  to achieve a burst trigger with the strobe preceding exposure.
*/

/*

Create a burst trigger with strobe preceding exposure

******************** High Level Events ********************

Inputs:
Burst Trigger Event
Source: UserOutput0
Activation: Rising Edge

Outputs:
Burst Strobe Output
Logic Block 1

******************** Low Level Settings ********************

Individual Trigger Event
Source: Counter 1 Start
Activation: Rising Edge
Delay: uS_TRIGGER_DELAY

Counter 0
Description: Burst Exposure Count
Counter Event: ExposureStart
Counter Event Activation: Rising Edge
Counter Durration: BURST_COUNT
Counter Trigger Source: UserOutput0
Counter Trigger Activation: Rising Edge

Counter 1
Description: Time between exposure start events
Counter Event: MHzTick
Counter Event Activation: Rising Edge
Counter Durration: uS_BETWEEN_TRIGGER
Counter Trigger Source: Logic Block 0
Counter Trigger Activation: Level High

Logic Block 0
Description: Counter 0 Active
Input 0: Counter 0 End
Input 1: UserOutput0
Input 2: UserOutput1 (Used to Initialize LB saved Q value)
Value LUT: 0x40
Enable LUT: 0xEF
+----+----+----+--------+--------+-------+
| I2 | I1 | I0 | Output | Enable | Value |
+----+----+----+--------+--------+-------+
|  0 |  0 |  0 | 0      |      1 |     0 |
|  0 |  0 |  1 | 0      |      1 |     0 |
|  0 |  1 |  0 | 0      |      1 |     0 |
|  0 |  1 |  1 | 0      |      1 |     0 |
|  1 |  0 |  0 | Q      |      0 |     0 |
|  1 |  0 |  1 | 0      |      1 |     0 |
|  1 |  1 |  0 | 1      |      1 |     1 |
|  1 |  1 |  1 | 0      |      1 |     0 |
+----+----+----+--------+--------+-------+

Logic Block 1
Description: Strobe output, High on counter 1 start, Low on Exposure End
Input 0: Exposure End
Input 1: Counter 1 Start
Input 2: UserOutput1 (Used to Initialize LB saved Q value)
Value LUT: 0x40
Enable LUT: 0xEF

+----+----+----+--------+--------+-------+
| I2 | I1 | I0 | Output | Enable | Value |
+----+----+----+--------+--------+-------+
|  0 |  0 |  0 | 0      |      1 |     0 |
|  0 |  0 |  1 | 0      |      1 |     0 |
|  0 |  1 |  0 | 0      |      1 |     0 |
|  0 |  1 |  1 | 0      |      1 |     0 |
|  1 |  0 |  0 | Q      |      0 |     0 |
|  1 |  0 |  1 | 0      |      1 |     0 |
|  1 |  1 |  0 | 1      |      1 |     1 |
|  1 |  1 |  1 | 0      |      1 |     0 |
+----+----+----+--------+--------+-------+

*/

#include <iostream>
#include <sstream>
#include <windows.h>
#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"

// spacing between bursts in a trigger
#define uS_BETWEEN_TRIGGER    5000
#define BURST_COUNT           10
#define uS_TRIGGER_DELAY      20
// Note uS_EXPOSURE_LENGTH must be shorter than uS_BETWEEN_TRIGGER
#define uS_EXPOSURE_LENGTH    2000
#define TRIGGER_NUM           5

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

int UserOutputSet(INodeMap & nodeMap, char * userOutputStr, bool val)
{
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
        cout << "Unable to set FrameTriggerWait (entry retrieval). Aborting..." << endl << endl;
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

void UserOutputPulse(INodeMap & nodeMap, char *  userOutputStr, uint32_t pulseLength_milli) {
    UserOutputSet(nodeMap, userOutputStr, true);
    Sleep(pulseLength_milli);
    UserOutputSet(nodeMap, userOutputStr, false);
}

// Configure counter 0
int SetupCounter0(INodeMap & nodeMap, int burstCount)
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
            cout << endl << "Camera does not support Counter and Timer Functionality. Aborting..." << endl;
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

        ptrCounterDuration->SetValue(burstCount);

        // Set Counter Event Source to ExposureStart
        CEnumerationPtr ptrCounterEventSource = nodeMap.GetNode("CounterEventSource");
        if (!IsAvailable(ptrCounterEventSource) || !IsWritable(ptrCounterEventSource))
        {
            cout << "Unable to set Counter Event Source (enum retrieval). Aborting..." << endl << endl;
            return -1;
        }

        CEnumEntryPtr ptrCounterEventSourceExposureStart = ptrCounterEventSource->GetEntryByName("ExposureStart");
        if (!IsAvailable(ptrCounterEventSourceExposureStart) || !IsReadable(ptrCounterEventSourceExposureStart))
        {
            cout << "Unable to set Counter Event Source (entry retrieval). Aborting..." << endl << endl;
            return -1;
        }

        int64_t counterEventSourceExposureStart = ptrCounterEventSourceExposureStart->GetValue();

        ptrCounterEventSource->SetIntValue(counterEventSourceExposureStart);

        // Set Counter 0 Trigger Source to UserOutput0
        CEnumerationPtr ptrCounterTriggerSource = nodeMap.GetNode("CounterTriggerSource");
        if (!IsAvailable(ptrCounterTriggerSource) || !IsWritable(ptrCounterTriggerSource))
        {
            cout << "Unable to set Counter Trigger Source (enum retrieval). Aborting..." << endl << endl;
            return -1;
        }

        CEnumEntryPtr ptrCounterTriggerSourceUserOutput0 = ptrCounterTriggerSource->GetEntryByName("UserOutput0");
        if (!IsAvailable(ptrCounterTriggerSourceUserOutput0) || !IsReadable(ptrCounterTriggerSourceUserOutput0))
        {
            cout << "Unable to set Counter Trigger Source (entry retrieval). Aborting..." << endl << endl;
            return -1;
        }

        int64_t counterTriggerSourceUserOutput0 = ptrCounterTriggerSourceUserOutput0->GetValue();

        ptrCounterTriggerSource->SetIntValue(counterTriggerSourceUserOutput0);

        // Set Counter Trigger Activation to Rising Edge
        CEnumerationPtr ptrCounterTriggerActivation = nodeMap.GetNode("CounterTriggerActivation");
        if (!IsAvailable(ptrCounterTriggerActivation) || !IsWritable(ptrCounterTriggerActivation))
        {
            cout << "Unable to set Counter Trigger Activation (enum retrieval). Aborting..." << endl << endl;
            return -1;
        }

        CEnumEntryPtr ptrCounterTriggerActivationRE = ptrCounterTriggerActivation->GetEntryByName("RisingEdge");
        if (!IsAvailable(ptrCounterTriggerActivationRE) || !IsReadable(ptrCounterTriggerActivationRE))
        {
            cout << "Unable to set Counter Trigger Activation (entry retrieval). Aborting..." << endl << endl;
            return -1;
        }

        int64_t counterTriggerActivationRE = ptrCounterTriggerActivationRE->GetValue();

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
int SetupCounter1(INodeMap & nodeMap, int microSecondsBetweenTriggers)
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

        ptrCounterDuration->SetValue(microSecondsBetweenTriggers);

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

        // Set Counter 0 Trigger Source to LogicBlock1
        CEnumerationPtr ptrCounterTriggerSource = nodeMap.GetNode("CounterTriggerSource");
        if (!IsAvailable(ptrCounterTriggerSource) || !IsWritable(ptrCounterTriggerSource))
        {
            cout << "Unable to set Counter Trigger Source (enum retrieval). Aborting..." << endl << endl;
            return -1;
        }

        CEnumEntryPtr ptrCounterTriggerSourceLB0 = ptrCounterTriggerSource->GetEntryByName("LogicBlock0");
        if (!IsAvailable(ptrCounterTriggerSourceLB0) || !IsReadable(ptrCounterTriggerSourceLB0))
        {
            cout << "Unable to set Counter Trigger Source (entry retrieval). Aborting..." << endl << endl;
            return -1;
        }

        int64_t counterTriggerSourceLB0 = ptrCounterTriggerSourceLB0->GetValue();

        ptrCounterTriggerSource->SetIntValue(counterTriggerSourceLB0);

        // Set Counter Trigger Activation to Level High
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

        int64_t counterTriggerActivationLH = ptrCounterTriggerActivationLH->GetValue();

        ptrCounterTriggerActivation->SetIntValue(counterTriggerActivationLH);
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

    // Set Logic Block LUT to Enable
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

    // Set Logic Block LUT Output Value All to 0xEF
    CIntegerPtr ptrLBLUTOutputValueAll = nodeMap.GetNode("LogicBlockLUTOutputValueAll");
    if (!IsAvailable(ptrLBLUTOutputValueAll) || !IsReadable(ptrLBLUTOutputValueAll))
    {
        cout << "Unable to set value to LUT logic block output value all (integer retrieval). Non-fatal error..." << endl;
        return -1;
    }

    ptrLBLUTOutputValueAll->SetValue(0xEF);

    cout << "Logic Block LUT Output Value All set to to 0xEF..." << endl;

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

    // Set Logic Block LUT Input Source to CounterOEnd
    CEnumerationPtr ptrLBLUTSource = nodeMap.GetNode("LogicBlockLUTInputSource");
    if (!IsAvailable(ptrLBLUTSource) || !IsReadable(ptrLBLUTSource))
    {
        cout << "Unable to set LUT logic block input source to Frame Trigger Wait (node retrieval). Non-fatal error..." << endl;
        return -1;
    }

    CEnumEntryPtr ptrLBLUTSourceC0E = ptrLBLUTSource->GetEntryByName("Counter0End");
    if (!IsAvailable(ptrLBLUTSourceC0E) || !IsReadable(ptrLBLUTSourceC0E))
    {
        cout << "Unable to set LUT logic block input source to Counter0End (enum entry retrieval). Non-fatal error..." << endl;
        return -1;
    }

    ptrLBLUTSource->SetIntValue(ptrLBLUTSourceC0E->GetValue());

    cout << "Logic Block LUT Input Source set to to Counter0End ..." << endl;

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

    // Set Logic Block LUT Source to UserOutput0
    CEnumEntryPtr ptrLBLUTSourceUO0 = ptrLBLUTSource->GetEntryByName("UserOutput0");
    if (!IsAvailable(ptrLBLUTSourceUO0) || !IsReadable(ptrLBLUTSourceUO0))
    {
        cout << "Unable to set LUT logic block input source to UserOutput0 (enum entry retrieval). Non-fatal error..." << endl;
        return -1;
    }

    ptrLBLUTSource->SetIntValue(ptrLBLUTSourceUO0->GetValue());

    cout << "Logic Block LUT Input Source set to to Counter0Start ..." << endl;

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

    // Set Logic Block LUT Source to UserOutput1
    CEnumEntryPtr ptrLBLUTSourceUO1 = ptrLBLUTSource->GetEntryByName("UserOutput1");
    if (!IsAvailable(ptrLBLUTSourceUO1) || !IsReadable(ptrLBLUTSourceUO1))
    {
        cout << "Unable to set LUT logic block input source to UserOutput1 (enum entry retrieval). Non-fatal error..." << endl;
        return -1;
    }

    ptrLBLUTSource->SetIntValue(ptrLBLUTSourceUO1->GetValue());

    cout << "Logic Block LUT Input Source set to to UserOutput1 ..." << endl;

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

    // Set Logic Block LUT output Value All to 0x40
    ptrLBLUTOutputValueAll->SetValue(0x40);
    cout << "Logic Block LUT Output Value All set to to 0x40..." << endl << endl;

    return result;
}

// This function does the logic block 1 configuration.
int ConfigureLogicBlock1(INodeMap & nodeMap)
{
    int result = 0;

    cout << endl << endl << "*** CONFIGURING LOGIC BLOCKS ***" << endl << endl;

    // Select Logic Block 1
    CEnumerationPtr ptrLogicBlockSelector = nodeMap.GetNode("LogicBlockSelector");
    if (!IsAvailable(ptrLogicBlockSelector) || !IsReadable(ptrLogicBlockSelector))
    {
        cout << "Unable to set logic block selector to Logic Block 1 (node retrieval). Non-fatal error..." << endl;
        return -1;
    }

    CEnumEntryPtr ptrLogicBlock1 = ptrLogicBlockSelector->GetEntryByName("LogicBlock1");
    if (!IsAvailable(ptrLogicBlock1) || !IsReadable(ptrLogicBlock1))
    {
        cout << "Unable to set logic block selector to Logic Block 1 (enum entry retrieval). Non-fatal error..." << endl;
        return -1;
    }

    ptrLogicBlockSelector->SetIntValue(ptrLogicBlock1->GetValue());

    cout << "Logic Block 1 selected...." << endl;

    // Set Logic Block LUT to Enable
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

    // Set Logic Block LUT Output Value All to 0xEF
    CIntegerPtr ptrLBLUTOutputValueAll = nodeMap.GetNode("LogicBlockLUTOutputValueAll");
    if (!IsAvailable(ptrLBLUTOutputValueAll) || !IsReadable(ptrLBLUTOutputValueAll))
    {
        cout << "Unable to set value to LUT logic block output value all (integer retrieval). Non-fatal error..." << endl;
        return -1;
    }

    ptrLBLUTOutputValueAll->SetValue(0xEF);

    cout << "Logic Block LUT Output Value All set to to 0xEF..." << endl;

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

    // Set Logic Block LUT Input Source to ExposureEnd
    CEnumerationPtr ptrLBLUTSource = nodeMap.GetNode("LogicBlockLUTInputSource");
    if (!IsAvailable(ptrLBLUTSource) || !IsReadable(ptrLBLUTSource))
    {
        cout << "Unable to set LUT logic block input source to Frame Trigger Wait (node retrieval). Non-fatal error..." << endl;
        return -1;
    }

    CEnumEntryPtr ptrLBLUTSourceEE = ptrLBLUTSource->GetEntryByName("ExposureEnd");
    if (!IsAvailable(ptrLBLUTSourceEE) || !IsReadable(ptrLBLUTSourceEE))
    {
        cout << "Unable to set LUT logic block input source to ExposureEnd (enum entry retrieval). Non-fatal error..." << endl;
        return -1;
    }

    ptrLBLUTSource->SetIntValue(ptrLBLUTSourceEE->GetValue());

    cout << "Logic Block LUT Input Source set to ExposureEnd ..." << endl;

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

    cout << "Logic Block LUT Input Activation set to RisingEdge..." << endl;

    // Set Logic Block LUT Input Selector to Input 1
    CEnumEntryPtr ptrLBLUTInput1 = ptrLBLUTInputSelector->GetEntryByName("Input1");
    if (!IsAvailable(ptrLBLUTInput1) || !IsReadable(ptrLBLUTInput1))
    {
        cout << "Unable to set LUT logic block Input selector to input 1 (enum entry retrieval). Non-fatal error..." << endl;
        return -1;
    }

    ptrLBLUTInputSelector->SetIntValue(ptrLBLUTInput1->GetValue());

    cout << "Logic Block LUT Input Selector set to to Input 1 ..." << endl;

    // Set Logic Block LUT Source to Counter1Start
    CEnumEntryPtr ptrLBLUTSourceCS1 = ptrLBLUTSource->GetEntryByName("Counter1Start");
    if (!IsAvailable(ptrLBLUTSourceCS1) || !IsReadable(ptrLBLUTSourceCS1))
    {
        cout << "Unable to set LUT logic block input source to Counter1Start (enum entry retrieval). Non-fatal error..." << endl;
        return -1;
    }

    ptrLBLUTSource->SetIntValue(ptrLBLUTSourceCS1->GetValue());

    cout << "Logic Block LUT Input Source set to to Counter1Start ..." << endl;

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

    // Set Logic Block LUT Source to UserOutput1
    CEnumEntryPtr ptrLBLUTSourceUO1 = ptrLBLUTSource->GetEntryByName("UserOutput1");
    if (!IsAvailable(ptrLBLUTSourceUO1) || !IsReadable(ptrLBLUTSourceUO1))
    {
        cout << "Unable to set LUT logic block input source to UserOutput1 (enum entry retrieval). Non-fatal error..." << endl;
        return -1;
    }

    ptrLBLUTSource->SetIntValue(ptrLBLUTSourceUO1->GetValue());

    cout << "Logic Block LUT Input Source set to to UserOutput1 ..." << endl;

    // Set Logic Block LUT Activation Type to LevelHigh
    CEnumEntryPtr ptrLBLUTActivationLH = ptrLBLUTActivation->GetEntryByName("LevelHigh");
    if (!IsAvailable(ptrLBLUTActivationLH) || !IsReadable(ptrLBLUTActivationLH))
    {
        cout << "Unable to set LUT logic block input activation to level high (enum entry retrieval). Non-fatal error..." << endl;
        return -1;
    }

    ptrLBLUTActivation->SetIntValue(ptrLBLUTActivationLH->GetValue());

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

    // Set Logic Block LUT output Value All to 0x40
    ptrLBLUTOutputValueAll->SetValue(0x40);

    cout << "Logic Block LUT Output Value All set to to 0x40..." << endl << endl;
    return result;
}

// Strobe output
int ConfigureLine2Strobe(INodeMap & nodeMap) {
    int result = 0;
    cout << endl << endl << "*** CONFIGURING STROBE ***" << endl << endl;

    // Set Line Selector
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

    // Set Line Mode to output
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

    // Set Line Source for Selected Line to LogicBlock1
    CEnumerationPtr ptrLineSource = nodeMap.GetNode("LineSource");
    if (!IsAvailable(ptrLineSource) || !IsWritable(ptrLineSource))
    {
        cout << "Unable to set Line Source (enum retrieval). Aborting..." << endl << endl;
        return -1;
    }

    CEnumEntryPtr ptrLineSourceLB1 = ptrLineSource->GetEntryByName("LogicBlock1");
    if (!IsAvailable(ptrLineSourceLB1) || !IsReadable(ptrLineSourceLB1))
    {
        cout << "Unable to set Line Source (entry retrieval). Aborting..." << endl << endl;
        return -1;
    }

    int64_t LineSourceCounterLB1 = ptrLineSourceLB1->GetValue();

    ptrLineSource->SetIntValue(LineSourceCounterLB1);

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

// This function configures the camera to use a trigger. First, trigger mode is
// set to off in order to select the trigger source. Once the trigger source
// has been selected, trigger mode is then enabled, which has the camera
// capture only a single image upon the execution of the chosen trigger.
int ConfigureTrigger(INodeMap & nodeMap, double triggerDelay_uS)
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

        // Set trigger mode to Counter 1 Start
        CEnumEntryPtr ptrTriggerSourceCounter1Start = ptrTriggerSource->GetEntryByName("Counter1Start");
        if (!IsAvailable(ptrTriggerSourceCounter1Start) || !IsReadable(ptrTriggerSourceCounter1Start))
        {
            cout << "Unable to set trigger mode (enum entry retrieval). Aborting..." << endl;
            return -1;
        }

        ptrTriggerSource->SetIntValue(ptrTriggerSourceCounter1Start->GetValue());

        cout << "Trigger source set to Counter1Start..." << endl;

        // Set Trigger Delay
        CFloatPtr ptrTriggerDelay = nodeMap.GetNode("TriggerDelay");
        if (!IsAvailable(ptrTriggerDelay) || !IsWritable(ptrTriggerDelay))
        {
            cout << "Unable to set trigger delay. Aborting..." << endl << endl;
            return -1;
        }

        ptrTriggerDelay->SetValue(triggerDelay_uS);

        cout << "Exposure time set to " << triggerDelay_uS << " us..." << endl << endl;

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

// This function retrieves a single image using the trigger. In this example,
// only a single image is captured and made available for acquisition - as such,
// attempting to acquire two images for a single trigger execution would cause
// the example to hang. This is different from other examples, whereby a
// constant stream of images are being captured and made available for image
// acquisition.
int GrabNextImageByTrigger(INodeMap & nodeMap, CameraPtr pCam)
{
    int result = 0;

    try
    {
        // Get user input
        cout << "Press the Enter key to initiate software trigger." << endl;
        getchar();

        // Send 1 ms user output pulse
        UserOutputPulse(nodeMap, "UserOutput0", 1);
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

        for (unsigned int triggerCnt = 0; triggerCnt < TRIGGER_NUM; triggerCnt++) {
            // Retrieve the next images from the trigger
            result = result | GrabNextImageByTrigger(nodeMap, pCam);

            for (unsigned int imageCnt = 0; imageCnt < BURST_COUNT; imageCnt++)
            {
                try
                {
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
                        filename << triggerCnt << "-" << imageCnt << ".jpg";

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

// This function configures a custom exposure time. Automatic exposure is turned
// off in order to allow for the customization, and then the custom setting is
// applied.
int ConfigureExposure(INodeMap & nodeMap, double exposureTimeToSet)
{
    int result = 0;

    cout << endl << endl << "*** CONFIGURING EXPOSURE ***" << endl << endl;

    try
    {
        //
        // Turn off automatic exposure mode
        //
        // *** NOTES ***
        // Automatic exposure prevents the manual configuration of exposure
        // time and needs to be turned off.
        //
        // *** LATER ***
        // Exposure time can be set automatically or manually as needed. This
        // example turns automatic exposure off to set it manually and back
        // on in order to return the camera to its default state.
        //
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

        //
        // Set exposure time manually; exposure time recorded in microseconds
        //
        // *** NOTES ***
        // The node is checked for availability and writability prior to the
        // setting of the node. Further, it is ensured that the desired exposure
        // time does not exceed the maximum. Exposure time is counted in
        // microseconds. This information can be found out either by
        // retrieving the unit with the GetUnit() method or by checking SpinView.
        //
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

        double exposureTimeToSet = uS_EXPOSURE_LENGTH;

        err = ConfigureExposure(nodeMap, exposureTimeToSet);
        if (err < 0)
        {
            return err;
        }

        // Configure Line 2 strobe
        // Actual strobe output that preceeds exposure
        err = ConfigureLine2Strobe(nodeMap);

        if (err < 0)
        {
            return err;
        }

        // Configure line 1 strobe
        // Exposure active (for debug purposes)
        err = ConfigureLine1Strobe(nodeMap);

        if (err < 0)
        {
            return err;
        }

        // Ensure that user output 0 is false initially
        // User output 0 is used to trigger
        UserOutputSet(nodeMap, "UserOutput0", false);

        // Ensure that user output 1 is false initially
        // User output 1 is used to initialize logic blocks
        UserOutputSet(nodeMap, "UserOutput1", false);

        // LB0 used to determine when Counter 0 is actve
        err = ConfigureLogicBlock0(nodeMap);
        if (err < 0)
        {
            return err;
        }

        // LB1 used to determine when stobe is active (counter 1 start to exposure end)
        err = ConfigureLogicBlock1(nodeMap);
        if (err < 0)
        {
            return err;
        }

        double triggerDelay_uS = uS_TRIGGER_DELAY;

        // Configure trigger
        err = ConfigureTrigger(nodeMap, triggerDelay_uS);
        if (err < 0)
        {
            return err;
        }

        // Configure counters 0
        err = SetupCounter0(nodeMap, BURST_COUNT);
        if (err < 0)
        {
            return err;
        }

        // Configure counters 1
        err = SetupCounter1(nodeMap, uS_BETWEEN_TRIGGER);
        if (err < 0)
        {
            return err;
        }

        UserOutputSet(nodeMap, "UserOutput1", true);

        // Acquire images
        result = result | AcquireImages(pCam, nodeMap, nodeMapTLDevice);

        // Reset trigger
        result = result | ResetTrigger(nodeMap);

        // Reset Exposure
        result = result | ResetExposure(nodeMap);

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