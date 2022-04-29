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
*  @example BurstStrobeThenTrigger.cs
*
*  @brief BurstStrobeThenTrigger.cs shows how to use logic blocks and counters
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
using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
using SpinnakerNET;
using SpinnakerNET.GenApi;
using System.Diagnostics;

namespace BurstStrobeThenTrigger
{
    class Program
    {

        // spacing between bursts in a trigger
        private const int uS_BETWEEN_TRIGGER = 5000;
        private const int BURST_COUNT = 10;
        private const double uS_TRIGGER_DELAY = 20;
        // Note uS_EXPOSURE_LENGTH must be shorter than uS_BETWEEN_TRIGGER
        private const int uS_EXPOSURE_LENGTH = 2000;
        private const int TRIGGER_NUM = 5;


        void Sleep(long timeInMilliSeconds)
        {
            Stopwatch stopwatch = Stopwatch.StartNew();
            while (true)
            {
                if(stopwatch.ElapsedMilliseconds >= timeInMilliSeconds)
                {
                    break;
                }
            }
        }
        void UserOutputPulse(INodeMap nodeMap, string userOutputStr, UInt32 pulseLength_milli)
        {
            UserOutputSet(nodeMap, userOutputStr, true);
            Sleep(pulseLength_milli);
            UserOutputSet(nodeMap, userOutputStr, false);
        }

        // This function prints the device information of the camera from the
        // transport layer; please see NodeMapInfo_CSharp example for more
        // in-depth comments on printing device information from the nodemap.
        private int PrintDeviceInfo(INodeMap nodeMap)
        {
            int result = 0;

            try
            {
                Console.WriteLine("\n*** DEVICE INFORMATION ***\n");

                ICategory category = nodeMap.GetNode<ICategory>("DeviceInformation");
                if (category != null && category.IsReadable)
                {
                    for (int i = 0; i < category.Children.Length; i++)
                    {
                        Console.WriteLine(
                            "{0}: {1}",
                            category.Children[i].Name,
                            (category.Children[i].IsReadable ? category.Children[i].ToString()
                             : "Node not available"));
                    }
                    Console.WriteLine();
                }
                else
                {
                    Console.WriteLine("Device control information not available.");
                }
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
                result = -1;
            }

            return result;
        }

        private int ConfigureLine2Strobe(INodeMap nodeMap)
        {
            int result = 0;

            Console.WriteLine("\n\n *** CONFIGURING STROBE *** \n\n");

            // Set Line Selector
            IEnum iLineSelector = nodeMap.GetNode<IEnum>("LineSelector");
            if (!iLineSelector.IsAvailable || !iLineSelector.IsWritable)
            {
                Console.WriteLine("Unable to set Line Selector (enum retrieval). Aborting...\n");
                return -1;
            }

            IEnumEntry iLineSelectorLine2 = iLineSelector.GetEntryByName("Line2");
            if (!iLineSelectorLine2.IsAvailable || !iLineSelectorLine2.IsReadable)
            {
                Console.WriteLine("Unable to set Line Selector (entry retrieval). Aborting...\n");
                return -1;
            }

            Int64 lineSelectorLine2 = iLineSelectorLine2.Value;

            iLineSelector.Value = lineSelectorLine2;

            // Set Line Mode to output
            IEnum iLineMode = nodeMap.GetNode<IEnum>("LineMode");
            if (!iLineMode.IsAvailable || !iLineMode.IsWritable)
            {
                Console.WriteLine("Unable to set Line Mode (enum retrieval). Aborting...\n");
                return -1;
            }

            IEnumEntry iLineModeOutput = iLineMode.GetEntryByName("Output");
            if (!iLineModeOutput.IsAvailable || !iLineModeOutput.IsReadable)
            {
                Console.WriteLine("Unable to set Line Mode (entry retrieval). Aborting...\n");
                return -1;
            }

            Int64 lineModeOutput = iLineModeOutput.Value;

            iLineMode.Value = lineModeOutput;

            // Set Line Source for Selected Line to LogicBlock1
            IEnum iLineSource = nodeMap.GetNode<IEnum>("LineSource");
            if (!iLineSource.IsAvailable || !iLineSource.IsWritable)
            {
                Console.WriteLine("Unable to set Line Source (enum retrieval). Aborting...\n");
                return -1;
            }

            IEnumEntry iLineSourceLB1 = iLineSource.GetEntryByName("LogicBlock1");
            if (!iLineSourceLB1.IsAvailable || !iLineSourceLB1.IsReadable)
            {
                Console.WriteLine("Unable to set Line Source (entry retrieval). Aborting...\n");
                return -1;
            }

            Int64 LineSourceCounterLB1 = iLineSourceLB1.Value;

            iLineSource.Value = LineSourceCounterLB1;

            return result;
        }

        private int ConfigureLine1Strobe(INodeMap nodeMap)
        {
            int result = 0;

            Console.WriteLine("\n\n *** CONFIGURING STROBE *** \n\n");

            // Set Line Selector
            IEnum iLineSelector = nodeMap.GetNode<IEnum>("LineSelector");
            if (!iLineSelector.IsAvailable || !iLineSelector.IsWritable)
            {
                Console.WriteLine("Unable to set Line Selector (enum retrieval). Aborting...\n");
                return -1;
            }

            IEnumEntry iLineSelectorLine1 = iLineSelector.GetEntryByName("Line1");
            if (!iLineSelectorLine1.IsAvailable || !iLineSelectorLine1.IsReadable)
            {
                Console.WriteLine("Unable to set Line Selector (entry retrieval). Aborting...\n");
                return -1;
            }

            Int64 lineSelectorLine1 = iLineSelectorLine1.Value;

            iLineSelector.Value = lineSelectorLine1;

            // Set Line Source for Selected Line to ExposureActive
            IEnum iLineSource = nodeMap.GetNode<IEnum>("LineSource");
            if (!iLineSource.IsAvailable || !iLineSource.IsWritable)
            {
                Console.WriteLine("Unable to set Line Source (enum retrieval). Aborting...\n");
                return -1;
            }

            IEnumEntry iLineSourceExposure = iLineSource.GetEntryByName("ExposureActive");
            if (!iLineSourceExposure.IsAvailable || !iLineSourceExposure.IsReadable)
            {
                Console.WriteLine("Unable to set Line Source (entry retrieval). Aborting...\n");
                return -1;
            }

            Int64 LineSourceCounterUserExposure = iLineSourceExposure.Value;

            iLineSource.Value = LineSourceCounterUserExposure;

            return result;
        }

        private int UserOutputSet(INodeMap nodeMap, string userOutputStr, bool val)
        {
            int result = 0;

            // Configure the user output
            IEnum iUserOutputSelector = nodeMap.GetNode<IEnum>("UserOutputSelector");
            if (!iUserOutputSelector.IsAvailable || !iUserOutputSelector.IsWritable)
            {
                Console.WriteLine("Unable to set UserOutputSelector (enum retrieval). Aborting...\n");
                return -1;
            }

            IEnumEntry iUserOutput = iUserOutputSelector.GetEntryByName(userOutputStr);
            if (!iUserOutput.IsAvailable || !iUserOutput.IsReadable)
            {
                Console.WriteLine("Unable to set FrameTriggerWait (entry retrieval). Aborting...\n");
                return -1;
            }

            Int64 userInput = iUserOutput.Value;

            iUserOutputSelector.Value = userInput;

            IBool iOutputValue = nodeMap.GetNode<IBool>("UserOutputValue");
            if (!iOutputValue.IsAvailable || !iOutputValue.IsWritable)
            {
                Console.WriteLine("Unable to set User Output Enable (boolean retrieval). Aborting...\n");
                return -1;
            }

            iOutputValue.Value = val;

            return result;
        }

        private int ConfigureLogicBlock0(INodeMap nodeMap)
        {
            int result = 0;

            Console.WriteLine("\n\n *** CONFIGURING LOGIC BLOCKS *** \n\n");

            // Select Logic Block 0
            IEnum iLogicBlockSelector = nodeMap.GetNode<IEnum>("LogicBlockSelector");
            if (!iLogicBlockSelector.IsAvailable || !iLogicBlockSelector.IsWritable)
            {
                Console.WriteLine("Unable to set logic block selector to Logic Block 0 (node retrieval). Non-fatal error...\n");
                return -1;
            }

            IEnumEntry iLogicBlock0 = iLogicBlockSelector.GetEntryByName("LogicBlock0");
            if (!iLogicBlock0.IsAvailable || !iLogicBlock0.IsReadable)
            {
                Console.WriteLine("Unable to set logic block selector to Logic Block 0 (enum entry retrieval). Non-fatal error...\n");
                return -1;
            }

            iLogicBlockSelector.Value = iLogicBlock0.Value;

            Console.WriteLine("Logic Block 0 selected...\n");

            // Set Logic Block LUT to Enable
            IEnum iLBLUTSelector = nodeMap.GetNode<IEnum>("LogicBlockLUTSelector");
            if (!iLBLUTSelector.IsAvailable || !iLBLUTSelector.IsWritable)
            {
                Console.WriteLine("Unable to set LUT logic block selector to Enable (node retrieval). Non-fatal error...\n");
                return -1;
            }

            IEnumEntry iLBLUTEnable = iLBLUTSelector.GetEntryByName("Enable");
            if (!iLBLUTEnable.IsAvailable || !iLBLUTEnable.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block selector to Enable (enum entry retrieval). Non-fatal error...\n");
                return -1;
            }

            iLBLUTSelector.Value = iLBLUTEnable.Value;

            Console.WriteLine("Logic Block LUT set to Enable...\n");

            // Set Logic Block LUT Output Value All to 0xEF
            IInteger iLBLUTOutputValueAll = nodeMap.GetNode<IInteger>("LogicBlockLUTOutputValueAll");
            if (!iLBLUTOutputValueAll.IsAvailable || !iLBLUTOutputValueAll.IsWritable)
            {
                Console.WriteLine("Unable to set value to LUT logic block output value all (integer retrieval). Non-fatal error...\n");
                return -1;
            }

            iLBLUTOutputValueAll.Value = 0xEF;

            Console.WriteLine("Logic Block LUT Output Value All set to to 0xEF...\n");

            // Set Logic Block LUT Input Selector to Input 0
            IEnum iLBLUTInputSelector = nodeMap.GetNode<IEnum>("LogicBlockLUTInputSelector");
            if (!iLBLUTInputSelector.IsAvailable || !iLBLUTInputSelector.IsWritable)
            {
                Console.WriteLine("Unable to set LUT logic block input selector to Input 0 (node retrieval). Non-fatal error...\n");
                return -1;
            }

            IEnumEntry iLBLUTInput0 = iLBLUTInputSelector.GetEntryByName("Input0");
            if (!iLBLUTInput0.IsAvailable || !iLBLUTInput0.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block selector to Input 0 (enum entry retrieval). Non-fatal error...\n");
                return -1;
            }

            iLBLUTInputSelector.Value = iLBLUTInput0.Value;

            Console.WriteLine("Logic Block LUT Input Selector set to to Input 0 ...\n");

            // Set Logic Block LUT Input Source to CounterOEnd
            IEnum iLBLUTSource = nodeMap.GetNode<IEnum>("LogicBlockLUTInputSource");
            if (!iLBLUTSource.IsAvailable || !iLBLUTSource.IsWritable)
            {
                Console.WriteLine("Unable to set LUT logic block input source to Frame Trigger Wait (node retrieval). Non-fatal error...\n");
                return -1;
            }

            IEnumEntry iLBLUTSourceC0E = iLBLUTSource.GetEntryByName("Counter0End");
            if (!iLBLUTSourceC0E.IsAvailable || !iLBLUTSourceC0E.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block input source to Counter0End (enum entry retrieval). Non-fatal error...\n");
                return -1;
            }

            iLBLUTSource.Value = iLBLUTSourceC0E.Value;

            Console.WriteLine("Logic Block LUT Input Source set to to Counter0End ...\n");

            // Set Logic Block LUT Activation Type to Rising Edge
            IEnum iLBLUTActivation = nodeMap.GetNode<IEnum>("LogicBlockLUTInputActivation");
            if (!iLBLUTActivation.IsReadable || !iLBLUTActivation.IsAvailable)
            {
                Console.WriteLine("Unable to set LUT logic block input activation to level high (node retrieval). Non-fatal error...\n");
                return -1;
            }

            IEnumEntry iLBLUTActivationRE = iLBLUTActivation.GetEntryByName("RisingEdge");
            if (!iLBLUTActivationRE.IsAvailable || !iLBLUTActivationRE.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block input activation to RisingEdge (enum entry retrieval). Non-fatal error...\n");
                return -1;
            }

            iLBLUTActivation.Value = iLBLUTActivationRE.Value;

            Console.WriteLine("Logic Block LUT Input Activation set to level high...\n");

            // Set Logic Block LUT Input Selector to Input 1
            IEnumEntry iLBLUTInput1 = iLBLUTInputSelector.GetEntryByName("Input1");
            if (!iLBLUTInput1.IsAvailable || !iLBLUTInput1.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block Input selector to input 1 (enum entry retrieval). Non-fatal error...");
                return -1;
            }

            iLBLUTInputSelector.Value = iLBLUTInput1.Value;

            Console.WriteLine("Logic Block LUT Input Selector set to to Input 1 ...");

            // Set Logic Block LUT Source to UserOutput0
            IEnumEntry iLBLUTSourceUO0 = iLBLUTSource.GetEntryByName("UserOutput0");
            if (!iLBLUTSourceUO0.IsAvailable || !iLBLUTSourceUO0.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block input source to UserOutput0 (enum entry retrieval). Non-fatal error...");
                return -1;
            }

            iLBLUTSource.Value = iLBLUTSourceUO0.Value;

            Console.WriteLine("Logic Block LUT Input Source set to to Counter0Start ...");

            // Set Logic Block LUT Activation Type to Rising Edge
            iLBLUTActivation.Value = iLBLUTActivationRE.Value;
            Console.WriteLine("Logic Block LUT Input Activation set to Rising Edge...");

            // Set Logic Block LUT Input Selector to Input 2
            IEnumEntry iLBLUTInput2 = iLBLUTInputSelector.GetEntryByName("Input2");
            if (!iLBLUTInput2.IsAvailable || !iLBLUTInput2.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block selector to input 2 (enum entry retrieval). Non-fatal error...");
                return -1;
            }

            iLBLUTInputSelector.Value = iLBLUTInput2.Value;

            Console.WriteLine("Logic Block LUT Input Selector set to to Input 2 ...");

            // Set Logic Block LUT Source to UserOutput1
            IEnumEntry iLBLUTSourceUO1 = iLBLUTSource.GetEntryByName("UserOutput1");
            if (!iLBLUTSourceUO1.IsAvailable || !iLBLUTSourceUO1.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block input source to UserOutput1 (enum entry retrieval). Non-fatal error...");
                return -1;
            }

            iLBLUTSource.Value = iLBLUTSourceUO1.Value;

            Console.WriteLine("Logic Block LUT Input Source set to to UserOutput1 ...");

            // Set Logic Block LUT Activation Type to Level High
            IEnumEntry iLBLUTActivationLevelHigh = iLBLUTActivation.GetEntryByName("LevelHigh");
            if (!iLBLUTActivationLevelHigh.IsAvailable || !iLBLUTActivationLevelHigh.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block input activation to level high (enum entry retrieval). Non-fatal error...");
                return -1;
            }

            iLBLUTActivation.Value = iLBLUTActivationLevelHigh.Value;

            Console.WriteLine("Logic Block LUT Input Activation set to level high...");

            // Set Logic Block Lut Selector to Value
            IEnumEntry iLBLUTValue = iLBLUTSelector.GetEntryByName("Value");
            if (!iLBLUTValue.IsAvailable || !iLBLUTValue.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block selector to Value (enum entry retrieval). Non-fatal error...");
                return -1;
            }

            iLBLUTSelector.Value = iLBLUTValue.Value;

            Console.WriteLine("Logic Block LUT set to to Value...");

            // Set Logic Block LUT output Value All to 0x40
            iLBLUTOutputValueAll.Value = (0x40);
            Console.WriteLine("Logic Block LUT Output Value All set to to 0x40...");

            return result;
        }

        private int ConfigureLogicBlock1(INodeMap nodeMap)
        {
            int result = 0;

            Console.WriteLine("\n\n *** CONFIGURING LOGIC BLOCKS *** \n\n");

            // Select Logic Block 0
            IEnum iLogicBlockSelector = nodeMap.GetNode<IEnum>("LogicBlockSelector");
            if (!iLogicBlockSelector.IsAvailable || !iLogicBlockSelector.IsReadable)
            {
                Console.WriteLine("Unable to set logic block selector to Logic Block 0 (node retrieval). Non-fatal error...\n");
                return -1;
            }

            IEnumEntry iLogicBlock1 = iLogicBlockSelector.GetEntryByName("LogicBlock1");
            if (!iLogicBlock1.IsAvailable || !iLogicBlock1.IsReadable)
            {
                Console.WriteLine("Unable to set logic block selector to Logic Block 0 (enum entry retrieval). Non-fatal error...\n");
                return -1;
            }

            iLogicBlockSelector.Value = iLogicBlock1.Value;

            Console.WriteLine("Logic Block 1 selected...\n");

            // Set Logic Block LUT to Enable
            IEnum iLBLUTSelector = nodeMap.GetNode<IEnum>("LogicBlockLUTSelector");
            if (!iLBLUTSelector.IsAvailable || !iLBLUTSelector.IsWritable)
            {
                Console.WriteLine("Unable to set LUT logic block selector to Enable (node retrieval). Non-fatal error...\n");
                return -1;
            }

            IEnumEntry iLBLUTEnable = iLBLUTSelector.GetEntryByName("Enable");
            if (!iLBLUTEnable.IsAvailable || !iLBLUTEnable.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block selector to Enable (enum entry retrieval). Non-fatal error...\n");
                return -1;
            }

            iLBLUTSelector.Value = iLBLUTEnable.Value;

            Console.WriteLine("Logic Block LUT set to Enable...\n");

            // Set Logic Block LUT Output Value All to 0xEF
            IInteger iLBLUTOutputValueAll = nodeMap.GetNode<IInteger>("LogicBlockLUTOutputValueAll");
            if (!iLBLUTOutputValueAll.IsAvailable || !iLBLUTOutputValueAll.IsReadable)
            {
                Console.WriteLine("Unable to set value to LUT logic block output value all (integer retrieval). Non-fatal error...\n");
                return -1;
            }

            iLBLUTOutputValueAll.Value = 0xEF;

            Console.WriteLine("Logic Block LUT Output Value All set to to 0xEF...\n");

            // Set Logic Block LUT Input Selector to Input 0
            IEnum iLBLUTInputSelector = nodeMap.GetNode<IEnum>("LogicBlockLUTInputSelector");
            if (!iLBLUTInputSelector.IsAvailable || !iLBLUTInputSelector.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block input selector to Input 0 (node retrieval). Non-fatal error...\n");
                return -1;
            }

            IEnumEntry iLBLUTInput0 = iLBLUTInputSelector.GetEntryByName("Input0");
            if (!iLBLUTInput0.IsAvailable || !iLBLUTInput0.IsAvailable)
            {
                Console.WriteLine("Unable to set LUT logic block selector to Input 0 (enum entry retrieval). Non-fatal error...\n");
                return -1;
            }

            iLBLUTInputSelector.Value = iLBLUTInput0.Value;

            Console.WriteLine("Logic Block LUT Input Selector set to Input 0 ...\n");

            // Set Logic Block LUT Input Source to ExposureEnd
            IEnum iLBLUTSource = nodeMap.GetNode<IEnum>("LogicBlockLUTInputSource");
            if (!iLBLUTSource.IsAvailable || !iLBLUTSource.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block input source to Frame Trigger Wait (node retrieval). Non-fatal error...\n");
                return -1;
            }

            IEnumEntry iLBLUTSourceEE = iLBLUTSource.GetEntryByName("ExposureEnd");
            if (!iLBLUTSourceEE.IsAvailable || !iLBLUTSourceEE.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block input source to Counter0End (enum entry retrieval). Non-fatal error...\n");
                return -1;
            }

            iLBLUTSource.Value = iLBLUTSourceEE.Value;

            Console.WriteLine("Logic Block LUT Input Source set to ExposureEnd ...\n");

            // Set Logic Block LUT Activation Type to Rising Edge
            IEnum iLBLUTActivation = nodeMap.GetNode<IEnum>("LogicBlockLUTInputActivation");
            if (!iLBLUTActivation.IsReadable || !iLBLUTActivation.IsAvailable)
            {
                Console.WriteLine("Unable to set LUT logic block input activation to level high (node retrieval). Non-fatal error...\n");
                return -1;
            }

            IEnumEntry iLBLUTActivationRE = iLBLUTActivation.GetEntryByName("RisingEdge");
            if (!iLBLUTActivationRE.IsAvailable || !iLBLUTActivationRE.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block input activation to RisingEdge (enum entry retrieval). Non-fatal error...\n");
                return -1;
            }

            iLBLUTActivation.Value = iLBLUTActivationRE.Value;

            Console.WriteLine("Logic Block LUT Input Activation set to level high...\n");

            // Set Logic Block LUT Input Selector to Input 1
            IEnumEntry iLBLUTInput1 = iLBLUTInputSelector.GetEntryByName("Input1");
            if (!iLBLUTInput1.IsAvailable || !iLBLUTInput1.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block Input selector to input 1 (enum entry retrieval). Non-fatal error...");
                return -1;
            }

            iLBLUTInputSelector.Value = iLBLUTInput1.Value;

            Console.WriteLine("Logic Block LUT Input Selector set to to Input 1 ...");

            // Set Logic Block LUT Source to Counter1Start
            IEnumEntry iLBLUTSourceCS1 = iLBLUTSource.GetEntryByName("Counter1Start");
            if (!iLBLUTSourceCS1.IsAvailable || !iLBLUTSourceCS1.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block input source to iLBLUTSourceCS1 (enum entry retrieval). Non-fatal error...");
                return -1;
            }

            iLBLUTSource.Value = iLBLUTSourceCS1.Value;

            Console.WriteLine("Logic Block LUT Input Source set to to iLBLUTSourceCS1 ...");

            // Set Logic Block LUT Activation Type to Rising Edge
            iLBLUTActivation.Value = iLBLUTActivationRE.Value;
            Console.WriteLine("Logic Block LUT Input Activation set to Rising Edge...");

            // Set Logic Block LUT Input Selector to Input 2
            IEnumEntry iLBLUTInput2 = iLBLUTInputSelector.GetEntryByName("Input2");
            if (!iLBLUTInput2.IsAvailable || !iLBLUTInput2.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block selector to input 2 (enum entry retrieval). Non-fatal error...");
                return -1;
            }

            iLBLUTInputSelector.Value = iLBLUTInput2.Value;

            Console.WriteLine("Logic Block LUT Input Selector set to to Input 2 ...");

            // Set Logic Block LUT Source to UserOutput1
            IEnumEntry iLBLUTSourceUO1 = iLBLUTSource.GetEntryByName("UserOutput1");
            if (!iLBLUTSourceUO1.IsAvailable || !iLBLUTSourceUO1.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block input source to UserOutput1 (enum entry retrieval). Non-fatal error...");
                return -1;
            }

            iLBLUTSource.Value = iLBLUTSourceUO1.Value;

            Console.WriteLine("Logic Block LUT Input Source set to to UserOutput1 ...");

            // Set Logic Block LUT Activation Type to Level High
            IEnumEntry iLBLUTActivationLevelHigh = iLBLUTActivation.GetEntryByName("LevelHigh");
            if (!iLBLUTActivationLevelHigh.IsAvailable || !iLBLUTActivationLevelHigh.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block input activation to level high (enum entry retrieval). Non-fatal error...");
                return -1;
            }

            iLBLUTActivation.Value = iLBLUTActivationLevelHigh.Value;

            Console.WriteLine("Logic Block LUT Input Activation set to level high...");

            // Set Logic Block Lut Selector to Value
            IEnumEntry iLBLUTValue = iLBLUTSelector.GetEntryByName("Value");
            if (!iLBLUTValue.IsAvailable || !iLBLUTValue.IsReadable)
            {
                Console.WriteLine("Unable to set LUT logic block selector to Value (enum entry retrieval). Non-fatal error...");
                return -1;
            }

            iLBLUTSelector.Value = iLBLUTValue.Value;

            Console.WriteLine("Logic Block LUT set to to Value...");

            // Set Logic Block LUT output Value All to 0x40
            iLBLUTOutputValueAll.Value = (0x40);
            Console.WriteLine("Logic Block LUT Output Value All set to to 0x40...");

            return result;
        }

        // This function configures the camera to use a trigger. First, trigger mode is
        // set to off in order to select the trigger source. Once the trigger source
        // has been selected, trigger mode is then enabled, which has the camera
        // capture only a single image upon the execution of the chosen trigger.
        int ConfigureTrigger(INodeMap nodeMap, double triggerDelay_uS)
        {
            int result = 0;

            Console.WriteLine("*** CONFIGURING TRIGGER ***");

            try
            {
                //
                // Ensure trigger mode off
                //
                // *** NOTES ***
                // The trigger must be disabled in order to configure whether the source
                // is software or hardware.
                //
                IEnum iTriggerMode = nodeMap.GetNode<IEnum>("TriggerMode");
                if (!iTriggerMode.IsAvailable || !iTriggerMode.IsReadable)
                {
                   Console.WriteLine("Unable to disable trigger mode (node retrieval). Aborting...");
                    return -1;
                }

                IEnumEntry iTriggerModeOff = iTriggerMode.GetEntryByName("Off");
                if (!iTriggerModeOff.IsReadable || !iTriggerModeOff.IsAvailable)
                {
                    Console.WriteLine("Unable to disable trigger mode (enum entry retrieval). Aborting...");
                    return -1;
                }

                iTriggerMode.Value = iTriggerModeOff.Value;

                Console.WriteLine( "Trigger mode disabled...");

                //
                // Select trigger source
                //
                // *** NOTES ***
                // The trigger source must be set to hardware or software while trigger
                // mode is off.
                //
                IEnum iTriggerSource = nodeMap.GetNode<IEnum>("TriggerSource");
                if (!iTriggerSource.IsAvailable || !iTriggerSource.IsWritable)
                {
                    Console.WriteLine("Unable to set trigger mode (node retrieval). Aborting...");
                    return -1;
                }

                // Set trigger mode to Counter 1 Start
                IEnumEntry iTriggerSourceCounter1Start = iTriggerSource.GetEntryByName("Counter1Start");
                if (!iTriggerSourceCounter1Start.IsAvailable || !iTriggerSourceCounter1Start.IsReadable)
                {
                    Console.WriteLine("Unable to set trigger mode (enum entry retrieval). Aborting...");
                    return -1;
                }

                iTriggerSource.Value = iTriggerSourceCounter1Start.Value;

                Console.WriteLine("Trigger source set to Counter1Start...");

                // Set Trigger Delay
                IFloat iTriggerDelay = nodeMap.GetNode<IFloat>("TriggerDelay");
                if (!iTriggerDelay.IsAvailable || !iTriggerDelay.IsWritable)
                {
                    Console.WriteLine("Unable to set trigger delay. Aborting...");
                    return -1;
                }

                iTriggerDelay.Value = triggerDelay_uS;

                Console.WriteLine("Exposure time set to {0} us...", triggerDelay_uS);

                // Set Trigger Overlap to true
                IEnum iTriggerOverlap = nodeMap.GetNode<IEnum>("TriggerOverlap");

                if (!iTriggerOverlap.IsAvailable || !iTriggerOverlap.IsReadable)
                {
                    Console.WriteLine("Unable to set trigger overlap to true (node retrieval). Aborting...");
                    return -1;
                }

                IEnumEntry iTriggerOverlapReadout = iTriggerOverlap.GetEntryByName("ReadOut");
                if (!iTriggerOverlapReadout.IsAvailable || !iTriggerOverlapReadout.IsReadable)
                {
                    Console.WriteLine("Unable to disable trigger mode (enum entry retrieval). Aborting...");
                    return -1;
                }

                iTriggerOverlap.Value = iTriggerOverlapReadout.Value;

                Console.WriteLine("Trigger Overlap set to Read Out...");

                //
                // Turn trigger mode on
                //
                // *** LATER ***
                // Once the appropriate trigger source has been set, turn trigger mode
                // on in order to retrieve images using the trigger.
                //

                IEnumEntry iTriggerModeOn = iTriggerMode.GetEntryByName("On");
                if (!iTriggerModeOn.IsAvailable || !iTriggerModeOn.IsReadable)
                {
                    Console.WriteLine("Unable to enable trigger mode (enum entry retrieval). Aborting...");
                    return -1;
                }

                iTriggerMode.Value = iTriggerModeOn.Value;

                // TODO: Blackfly and Flea3 GEV cameras need 1 second delay after trigger mode is turned on

                Console.WriteLine("Trigger mode turned back on...");
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}",ex.Message);
                result = -1;
            }

            return result;
            }

        private int ConfigureExposure(INodeMap nodeMap, double exposureTimeToSet)
            {
            int result = 0;

            Console.WriteLine("\n Configure Exposure Time Settings \n");
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
                 IEnum iExposureAuto = nodeMap.GetNode<IEnum>("ExposureAuto");
                if (!iExposureAuto.IsAvailable || !iExposureAuto.IsWritable)
                {
                    Console.WriteLine("Unable to disable automatic exposure (node retrieval). Aborting...\n\n");
                    return -1;
                }

                IEnumEntry iExposureAutoOff = iExposureAuto.GetEntryByName("Off");
                if (!iExposureAutoOff.IsAvailable || !iExposureAutoOff.IsReadable)
                {
                    Console.WriteLine("Unable to disable automatic exposure (enum entry retrieval). Aborting...\n\n");
                    return -1;
                }

                iExposureAuto.Value = iExposureAutoOff.Value;

                Console.WriteLine("Automatic exposure disabled...\n");

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
                IFloat iExposureTime = nodeMap.GetNode<IFloat>("ExposureTime");
                if (!iExposureTime.IsReadable || !iExposureTime.IsWritable)
                {
                    Console.WriteLine("Unable to set exposure time. Aborting...\n\n");
                    return -1;
                }

                // Ensure desired exposure time does not exceed the maximum
                double exposureTimeMax = iExposureTime.Max;

                if (exposureTimeToSet > exposureTimeMax)
                {
                    exposureTimeToSet = exposureTimeMax;
                }

                iExposureTime.Value = exposureTimeToSet;

                Console.WriteLine("Exposure time set to {0} us...\n\n", exposureTimeToSet);
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}\n", ex.Message);

                return -1;
            }
            return result;
        }

        // Configure counter 0
        int SetupCounter0(INodeMap nodeMap, int burstCount)
        {
            int result = 0;

            Console.WriteLine("Configuring Pulse Width Modulation signal");

            try
            {
                // Set Counter Selector to Counter 0
                IEnum iCounterSelector = nodeMap.GetNode<IEnum>("CounterSelector");

                // Check to see if camera supports Counter and Timer functionality
                if (!iCounterSelector.IsAvailable)
                {
                    Console.WriteLine("Camera does not support Counter and Timer Functionality. Aborting...");
                    return -1;
                }

                if (!iCounterSelector.IsWritable)
                {
                    Console.WriteLine("Unable to set Counter Selector (enum retrieval). Aborting...");
                    return -1;
                }

                IEnumEntry iCounter0 = iCounterSelector.GetEntryByName("Counter0");
                if (!iCounter0.IsAvailable || !iCounter0.IsReadable)
                {
                    Console.WriteLine("Unable to set Counter Selector (entry retrieval). Aborting...");
                    return -1;
                }

                Int64 counter0 = iCounter0.Value;

                iCounterSelector.Value = counter0;

                // Set Counter Duration to input value
                IInteger iCounterDuration = nodeMap.GetNode<IInteger>("CounterDuration");
                if (!iCounterDuration.IsAvailable || !iCounterDuration.IsWritable)
                {
                    Console.WriteLine("Unable to set Counter Duration (integer retrieval). Aborting...");
                    return -1;
                }

                iCounterDuration.Value = burstCount;

                // Set Counter Event Source to ExposureStart
                IEnum iCounterEventSource = nodeMap.GetNode<IEnum>("CounterEventSource");
                if (!iCounterEventSource.IsAvailable|| !iCounterEventSource.IsWritable)
                {
                    Console.WriteLine("Unable to set Counter Event Source (enum retrieval). Aborting...");
                    return -1;
                }

                IEnumEntry iCounterEventSourceExposureStart = iCounterEventSource.GetEntryByName("ExposureStart");
                if (!iCounterEventSourceExposureStart.IsAvailable || !iCounterEventSourceExposureStart.IsReadable)
                {
                    Console.WriteLine("Unable to set Counter Event Source (entry retrieval). Aborting...");
                    return -1;
                }

                Int64 counterEventSourceExposureStart = iCounterEventSourceExposureStart.Value;

                iCounterEventSource.Value = counterEventSourceExposureStart;

                // Set Counter 0 Trigger Source to UserOutput0
                IEnum iCounterTriggerSource = nodeMap.GetNode<IEnum>("CounterTriggerSource");
                if (!iCounterTriggerSource.IsAvailable || !iCounterTriggerSource.IsWritable)
                {
                    Console.WriteLine("Unable to set Counter Trigger Source (enum retrieval). Aborting...");
                    return -1;
                }

                IEnumEntry iCounterTriggerSourceUserOutput0 = iCounterTriggerSource.GetEntryByName("UserOutput0");
                if (!iCounterTriggerSourceUserOutput0.IsAvailable || !iCounterTriggerSourceUserOutput0.IsReadable)
                {
                    Console.WriteLine("Unable to set Counter Trigger Source (entry retrieval). Aborting...");
                    return -1;
                }

                Int64 counterTriggerSourceUserOutput0 = iCounterTriggerSourceUserOutput0.Value;

                iCounterTriggerSource.Value = counterTriggerSourceUserOutput0;

                // Set Counter Trigger Activation to Rising Edge
                IEnum iCounterTriggerActivation = nodeMap.GetNode<IEnum>("CounterTriggerActivation");
                if (!iCounterTriggerActivation.IsAvailable || !iCounterTriggerActivation.IsWritable)
                {
                    Console.WriteLine("Unable to set Counter Trigger Activation (enum retrieval). Aborting...");
                    return -1;
                }

                IEnumEntry iCounterTriggerActivationRE = iCounterTriggerActivation.GetEntryByName("RisingEdge");
                if (!iCounterTriggerActivationRE.IsAvailable || !iCounterTriggerActivationRE.IsReadable)
                {
                    Console.WriteLine("Unable to set Counter Trigger Activation (entry retrieval). Aborting...");
                    return -1;
                }

                Int64 counterTriggerActivationRE = iCounterTriggerActivationRE.Value;

                iCounterTriggerActivation.Value = counterTriggerActivationRE;
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}",ex.Message);
                result = -1;
            }

            return result;
            }

        // Configure counter 0
        int SetupCounter1(INodeMap nodeMap, int burstCount)
        {
            int result = 0;

            Console.WriteLine("Configuring Pulse Width Modulation signal");

            try
            {
                // Set Counter Selector to Counter 1
                IEnum iCounterSelector = nodeMap.GetNode<IEnum>("CounterSelector");

                // Check to see if camera supports Counter and Timer functionality
                if (!iCounterSelector.IsAvailable)
                {
                    Console.WriteLine("Camera does not support Counter and Timer Functionality. Aborting...");
                    return -1;
                }

                if (!iCounterSelector.IsWritable)
                {
                    Console.WriteLine("Unable to set Counter Selector (enum retrieval). Aborting...");
                    return -1;
                }

                IEnumEntry iCounter1 = iCounterSelector.GetEntryByName("Counter1");
                if (!iCounter1.IsAvailable || !iCounter1.IsReadable)
                {
                    Console.WriteLine("Unable to set Counter Selector (entry retrieval). Aborting...");
                    return -1;
                }

                Int64 counter1 = iCounter1.Value;

                iCounterSelector.Value = counter1;

                // Set Counter Duration to input value
                IInteger iCounterDuration = nodeMap.GetNode<IInteger>("CounterDuration");
                if (!iCounterDuration.IsAvailable || !iCounterDuration.IsWritable)
                {
                    Console.WriteLine("Unable to set Counter Duration (integer retrieval). Aborting...");
                    return -1;
                }

                iCounterDuration.Value = burstCount;

                // Set Counter Event Source to ExposureStart
                IEnum iCounterEventSource = nodeMap.GetNode<IEnum>("CounterEventSource");
                if (!iCounterEventSource.IsAvailable || !iCounterEventSource.IsWritable)
                {
                    Console.WriteLine("Unable to set Counter Event Source (enum retrieval). Aborting...");
                    return -1;
                }

                IEnumEntry iCounterEventSourceMHzTick = iCounterEventSource.GetEntryByName("MHzTick");
                if (!iCounterEventSourceMHzTick.IsAvailable || !iCounterEventSourceMHzTick.IsReadable)
                {
                    Console.WriteLine("Unable to set Counter Event Source (entry retrieval). Aborting...");
                    return -1;
                }

                Int64 counterEventSourceMHzTick = iCounterEventSourceMHzTick.Value;

                iCounterEventSource.Value = counterEventSourceMHzTick;

                // Set Counter 0 Trigger Source to UserOutput0
                IEnum iCounterTriggerSource = nodeMap.GetNode<IEnum>("CounterTriggerSource");
                if (!iCounterTriggerSource.IsAvailable || !iCounterTriggerSource.IsWritable)
                {
                    Console.WriteLine("Unable to set Counter Trigger Source (enum retrieval). Aborting...");
                    return -1;
                }

                IEnumEntry iCounterTriggerSourceLB0 = iCounterTriggerSource.GetEntryByName("LogicBlock0");
                if (!iCounterTriggerSourceLB0.IsAvailable || !iCounterTriggerSourceLB0.IsReadable)
                {
                    Console.WriteLine("Unable to set Counter Trigger Source (entry retrieval). Aborting...");
                    return -1;
                }

                Int64 counterTriggerSourceLB0 = iCounterTriggerSourceLB0.Value;

                iCounterTriggerSource.Value = counterTriggerSourceLB0;

                // Set Counter Trigger Activation to Rising Edge
                IEnum iCounterTriggerActivation = nodeMap.GetNode<IEnum>("CounterTriggerActivation");
                if (!iCounterTriggerActivation.IsAvailable || !iCounterTriggerActivation.IsWritable)
                {
                    Console.WriteLine("Unable to set Counter Trigger Activation (enum retrieval). Aborting...");
                    return -1;
                }

                IEnumEntry iCounterTriggerActivationLH = iCounterTriggerActivation.GetEntryByName("LevelHigh");
                if (!iCounterTriggerActivationLH.IsAvailable || !iCounterTriggerActivationLH.IsReadable)
                {
                    Console.WriteLine("Unable to set Counter Trigger Activation (entry retrieval). Aborting...");
                    return -1;
                }

                Int64 counterTriggerActivationLH = iCounterTriggerActivationLH.Value;

                iCounterTriggerActivation.Value = counterTriggerActivationLH;
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
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
        int GrabNextImageByTrigger(INodeMap nodeMap, IManagedCamera cam)
        {
            int result = 0;

            try
            {
                // Get user input
                Console.WriteLine("Press the Enter key to initiate software trigger.");
                Console.ReadKey();

                // Send 1 ms user output pulse
                UserOutputPulse(nodeMap, "UserOutput0", 1);
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
                result = -1;
            }

            return result;
        }


        // This function acquires and saves 10 images from a device; please see
        // Acquisition example for more in-depth comments on acquiring images.
        int AcquireImages(IManagedCamera cam, INodeMap nodeMap, INodeMap nodeMapTLDevice)
        {
            int result = 0;

            Console.WriteLine("*** IMAGE ACQUISITION ***");

            try
            {
                // Set acquisition mode to continuous
                IEnum iAcquisitionMode = nodeMap.GetNode<IEnum>("AcquisitionMode");
                if (!iAcquisitionMode.IsAvailable || !iAcquisitionMode.IsWritable)
                {
                    Console.WriteLine("Unable to set acquisition mode to continuous (node retrieval). Aborting...");
                    return -1;
                }

                IEnumEntry iAcquisitionModeContinuous = iAcquisitionMode.GetEntryByName("Continuous");
                if (!iAcquisitionModeContinuous.IsAvailable || !iAcquisitionModeContinuous.IsReadable)
                {
                    Console.WriteLine("Unable to set acquisition mode to continuous (entry 'continuous' retrieval). Aborting...");
                    return -1;
                }

                Int64 acquisitionModeContinuous = iAcquisitionModeContinuous.Value;

                iAcquisitionMode.Value = acquisitionModeContinuous;

                Console.WriteLine("Acquisition mode set to continuous...");

                // Begin acquiring images
                cam.BeginAcquisition();

                Console.WriteLine("Acquiring images...");

                // Retrieve device serial number for filename
                string deviceSerialNumber = "";

                IString iStringSerial = nodeMapTLDevice.GetNode<IString>("DeviceSerialNumber");
                if (iStringSerial.IsAvailable && iStringSerial.IsReadable)
                {
                    deviceSerialNumber = iStringSerial.Value;

                    Console.WriteLine("Device serial number retrieved as {0}...", deviceSerialNumber);
                }
                Console.WriteLine();

                // Retrieve, convert, and save images
                const uint k_numImages = 10;

                for (uint triggerCnt = 0; triggerCnt < TRIGGER_NUM; triggerCnt++)
                {
                    // Retrieve the next images from the trigger
                    result = result | GrabNextImageByTrigger(nodeMap, cam);

                    for (uint imageCnt = 0; imageCnt < BURST_COUNT; imageCnt++)
                    {
                        try
                        {
                            // Retrieve the next received image
                            IManagedImage iResultImage = cam.GetNextImage();

                            if (iResultImage.IsIncomplete)
                            {
                                Console.WriteLine("Image incomplete with image status {0}...", iResultImage.ImageStatus);
                            }
                            else
                            {
                                // Print image information
                                Console.WriteLine("Grabbed image {0}, width = {1}, height = {2}", imageCnt,iResultImage.Width,iResultImage.Height);

                                // Convert image to mono 8
                                IManagedImage convertedImage = iResultImage.Convert(PixelFormatEnums.Mono8, ColorProcessingAlgorithm.HQ_LINEAR);

                                // Create a unique filename
                                StringBuilder filename = new StringBuilder();

                                filename.Append("Trigger-");
                                if (deviceSerialNumber != "")
                                {
                                    filename.Append(deviceSerialNumber +  "-");
                                }
                                filename.Append(triggerCnt.ToString() + "-" + imageCnt.ToString() + ".jpg");

                                // Save image
                                convertedImage.Save(filename.ToString());

                                Console.WriteLine("Image saved at {0}",filename.ToString());
                            }

                            // Release image
                            iResultImage.Release();

                            Console.WriteLine();
                        }
                        catch (SpinnakerException ex)
                        {
                            Console.WriteLine("Error: {0}", ex.Message);
                            result = -1;
                        }
                    }
                }

                // End acquisition
                cam.EndAcquisition();
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
                result = -1;
            }

            return result;
        }

        // This function acts as the body of the example; please see
        // NodeMapInfo_CSharp example for more in-depth comments on setting up
        // cameras.
        int RunSingleCamera(IManagedCamera cam)
        {
            int result = 0;
            int err = 0;

            try
            {
                // Retrieve TL device nodemap and print device information
                INodeMap nodeMapTLDevice = cam.GetTLDeviceNodeMap();

                result = PrintDeviceInfo(nodeMapTLDevice);

                // Initialize camera
                cam.Init();

                // Retrieve GenICam nodemap
                INodeMap nodeMap = cam.GetNodeMap();

                err = ConfigureExposure(nodeMap, uS_EXPOSURE_LENGTH);
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

                // Configure trigger
                err = ConfigureTrigger(nodeMap, uS_TRIGGER_DELAY);
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
                result = result | AcquireImages(cam, nodeMap, nodeMapTLDevice);

                // Reset trigger
                result = result | ResetTrigger(nodeMap);

                // Reset Exposure
                result = result | ResetExposure(nodeMap);

                // Deinitialize camera
                cam.DeInit();
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
                result = -1;
            }

            return result;
        }

        // This function returns the camera to a normal state by turning off trigger
        // mode.
        int ResetTrigger(INodeMap nodeMap)
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
                IEnum iTriggerMode = nodeMap.GetNode<IEnum>("TriggerMode");
                if (!iTriggerMode.IsAvailable || !iTriggerMode.IsWritable)
                {
                    Console.WriteLine("Unable to disable trigger mode (node retrieval). Non-fatal error...");
                    return -1;
                }

                IEnumEntry iTriggerModeOff = iTriggerMode.GetEntryByName("Off");
                if (!iTriggerModeOff.IsAvailable || !iTriggerModeOff.IsReadable)
                {
                    Console.WriteLine("Unable to disable trigger mode (enum entry retrieval). Non-fatal error...");
                    return -1;
                }

                iTriggerMode.Value = iTriggerModeOff.Value;

                Console.WriteLine("Trigger mode disabled...");
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
                result = -1;
            }

            return result;
        }

        int ResetExposure(INodeMap nodeMap)
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
                IEnum iExposureAuto = nodeMap.GetNode<IEnum>("ExposureAuto");
                if (!iExposureAuto.IsAvailable || !iExposureAuto.IsWritable)
                {
                    Console.WriteLine("Unable to disable trigger mode (node retrieval). Non-fatal error...");
                    return -1;
                }

                IEnumEntry iExposureAutoContinuous = iExposureAuto.GetEntryByName("Off");
                if (!iExposureAutoContinuous.IsAvailable || !iExposureAutoContinuous.IsReadable)
                {
                    Console.WriteLine("Unable to disable trigger mode (enum entry retrieval). Non-fatal error...");
                    return -1;
                }

                iExposureAuto.Value = iExposureAutoContinuous.Value;

                Console.WriteLine("Trigger mode disabled...");
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
                result = -1;
            }

            return result;
        }
        // Example entry point; please see Enumeration_CSharp example for more
        // in-depth comments on preparing and cleaning up the system.
        static int Main(string[] args)
        {
            int result = 0;

            Program program = new Program();

            // Since this application saves images in the current folder
            // we must ensure that we have permission to write to this folder.
            // If we do not have permission, fail right away.
            FileStream fileStream;
            try
            {
                fileStream = new FileStream(@"test.txt", FileMode.Create);
                fileStream.Close();
                File.Delete("test.txt");
            }
            catch
            {
                Console.WriteLine("Failed to create file in current folder. Please check permissions.");
                Console.WriteLine("Press enter to exit...");
                Console.ReadLine();
                return -1;
            }

            // Retrieve singleton reference to system object
            ManagedSystem system = new ManagedSystem();

            // Print out current library version
            LibraryVersion spinVersion = system.GetLibraryVersion();
            Console.WriteLine(
                "Spinnaker library version: {0}.{1}.{2}.{3}\n\n",
                spinVersion.major,
                spinVersion.minor,
                spinVersion.type,
                spinVersion.build);

            // Retrieve list of cameras from the system
            ManagedCameraList camList = system.GetCameras();

            Console.WriteLine("Number of cameras detected: {0}\n\n", camList.Count);

            // Finish if there are no cameras
            if (camList.Count == 0)
            {
                // Clear camera list before releasing system
                camList.Clear();

                // Release system
                system.Dispose();

                Console.WriteLine("Not enough cameras!");
                Console.WriteLine("Done! Press Enter to exit...");
                Console.ReadLine();

                return -1;
            }

            //
            // Run example on each camera
            //
            // *** NOTES ***
            // Cameras can either be retrieved as their own IManagedCamera
            // objects or from camera lists using the [] operator and an index.
            //
            // Using-statements help ensure that cameras are disposed of when
            // they are no longer needed; otherwise, cameras can be disposed of
            // manually by calling Dispose(). In C#, if cameras are not disposed
            // of before the system is released, the system will do so
            // automatically.
            //
            int index = 0;

            foreach (IManagedCamera managedCamera in camList) using (managedCamera)
                {
                    Console.WriteLine("Running example for camera {0}...", index);

                    try
                    {
                        // Run example
                        result = result | program.RunSingleCamera(managedCamera);
                    }
                    catch (SpinnakerException ex)
                    {
                        Console.WriteLine("Error: {0}", ex.Message);
                        result = -1;
                    }

                    Console.WriteLine("Camera {0} example complete...\n", index++);
                }

            // Clear camera list before releasing system
            camList.Clear();

            // Release system
            system.Dispose();

            Console.WriteLine("\nDone! Press Enter to exit...");
            Console.ReadLine();

            return result;
        }
    }
}
