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
*  @example AlternatingStrobe.cs
*  
*  @brief AlternatingStrobe.cs Shows how to use BFS/ORX Counters and Logic
*  Blocks in order to apply a custom strobe delay to every second frame.
*
*  LogicBlock1 is used like a T-flipflop in order to hold a state logic value 
*  that will be toggled on each ExposureEnd event.
*
*  Counter0 is used as a  MHz timer to time the delay that should be added to 
*  every second strobe signal.
*
*  Counter1 is used as a MHz timer to time the strobe durration. The ourtput
*  line will be set to Counter1Active
*
*  LocicBlock0 is used a the reset source to start Counter1. It takes as input
*  LogicBlock1 to determine if the offset is to be applied. 
*  If LogicBlock1=0, LogicBlock0 has a rising edge on ExposureStart.
*  If LogicBlock1=1, LogicBlock0 has a rising edge on Counter0End.
*/
using System;
using System.IO;
using System.Collections.Generic;
using SpinnakerNET;
using SpinnakerNET.GenApi;

namespace AlternatingStrobe
{
    class Program
    {
        public const int ALTERNATING_TRIGGER_DELAY_US = 10000;
        public const int EXPOSURE_LENGTH_US = 20000;
        public const int STROBE_DURATION_US = 10000;
        public const int FRAME_RATE = 10;


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

        private bool LoadDefaultUserSet(IManagedCamera cam)
        {
            Console.WriteLine("Loading the dedault user set \n");
            try
            {
                cam.UserSetSelector.Value = UserSetSelectorEnums.Default.ToString();
                cam.UserSetLoad.Execute();

            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0} \n", ex.Message);
                return false;
            }
            return true;
        }

        private bool ConfigureFrameRate(IManagedCamera cam, double frameRate)
        {
            Console.WriteLine("\n Configure Frame Rate Settings \n");
            try
            {
                cam.AcquisitionFrameRateEnable.Value = true;
                cam.AcquisitionFrameRate.Value = Math.Min(frameRate, cam.AcquisitionFrameRate.Max);

                Console.WriteLine("Frame rate set to: {0} fps\n", cam.AcquisitionFrameRate.Value);
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}\n", ex.Message);
                return false;
            }
            return true;
        }
        private bool ConfigureLine2(IManagedCamera cam)
        {
            Console.WriteLine("\nConfiguring Line 2 \n");
            try
            {
                cam.LineSelector.Value = LineSelectorEnums.Line2.ToString();
                cam.LineMode.Value = LineModeEnums.Output.ToString();
                cam.LineSource.Value = LineSourceEnums.Counter1Active.ToString();
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}\n", ex.Message);

                return false;
            }
            return true;
        }

        private bool ConfigureLine1(IManagedCamera cam)
        {
            Console.WriteLine("\nConfiguring Line 1 \n");
            try
            {
                cam.LineSelector.Value = LineSelectorEnums.Line1.ToString();
                cam.LineMode.Value = LineModeEnums.Output.ToString();
                cam.LineSource.Value = LineSourceEnums.ExposureActive.ToString();
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}\n", ex.Message);

                return false;
            }
            return true;
        }
        private bool ConfigureExposureTime(IManagedCamera cam, double exposreTime)
        {
            Console.WriteLine("\n Configure Exposure Time Settings \n");
            try
            {
                cam.ExposureAuto.Value = ExposureAutoEnums.Off.ToString();
                cam.ExposureTime.Value = Math.Min(exposreTime, cam.ExposureTime.Max);

                Console.WriteLine("Exposure Time set to: {0} us\n", cam.ExposureTime.Value);
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}\n", ex.Message);

                return false;
            }
            return true;
        }

        // LocicBlock0 takes LogicBlock1 as input determine if the 
        // offset is to be applied.
        // If LogicBlock1 = 0, LogicBlock0 has a rising edge on ExposureStart.
        // If LogicBlock1 = 1, LogicBlock0 has a rising edge on Counter0End.
        private bool ConfigureLogicBlock0(IManagedCamera cam)
        {
            Console.WriteLine("\n Configuring Logic Block 0 \n");

            try
            {
                cam.LogicBlockSelector.Value = LogicBlockSelectorEnums.LogicBlock0.ToString();

                cam.LogicBlockLUTSelector.Value = LogicBlockLUTSelectorEnums.Enable.ToString();
                cam.LogicBlockLUTOutputValueAll.Value = 0xFF;

                cam.LogicBlockLUTSelector.Value = LogicBlockLUTSelectorEnums.Value.ToString();
                cam.LogicBlockLUTOutputValueAll.Value = 0xCA;

                cam.LogicBlockLUTInputSelector.Value = LogicBlockLUTInputSelectorEnums.Input0.ToString();
                cam.LogicBlockLUTInputSource.Value = LogicBlockLUTInputSourceEnums.ExposureStart.ToString();
                cam.LogicBlockLUTInputActivation.Value = LogicBlockLUTInputActivationEnums.RisingEdge.ToString();

                cam.LogicBlockLUTInputSelector.Value = LogicBlockLUTInputSelectorEnums.Input1.ToString();
                cam.LogicBlockLUTInputSource.Value = LogicBlockLUTInputSourceEnums.Counter0End.ToString();
                cam.LogicBlockLUTInputActivation.Value = LogicBlockLUTInputActivationEnums.RisingEdge.ToString();

                cam.LogicBlockLUTInputSelector.Value = LogicBlockLUTInputSelectorEnums.Input2.ToString();
                cam.LogicBlockLUTInputSource.Value = LogicBlockLUTInputSourceEnums.LogicBlock1.ToString();
                cam.LogicBlockLUTInputActivation.Value = LogicBlockLUTInputActivationEnums.LevelHigh.ToString();
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}\n", ex.Message);

                return false;
            }
            return true;
        }

        // LogicBlock1 is used like a T-flipflop in order to hold a state logic value 
        // that will be toggled on each ExposureEnd event.
        private bool ConfigureLogicBlock1(IManagedCamera cam)
        {
            Console.WriteLine("\n Configuring Logic Block 1 \n");

            try
            {
                cam.LogicBlockSelector.Value = LogicBlockSelectorEnums.LogicBlock1.ToString();

                cam.LogicBlockLUTSelector.Value = LogicBlockLUTSelectorEnums.Enable.ToString();
                cam.LogicBlockLUTOutputValueAll.Value = 0xAF;

                cam.LogicBlockLUTSelector.Value = LogicBlockLUTSelectorEnums.Value.ToString();
                cam.LogicBlockLUTOutputValueAll.Value = 0x20;

                cam.LogicBlockLUTInputSelector.Value = LogicBlockLUTInputSelectorEnums.Input0.ToString();
                cam.LogicBlockLUTInputSource.Value = LogicBlockLUTInputSourceEnums.ExposureEnd.ToString();
                cam.LogicBlockLUTInputActivation.Value = LogicBlockLUTInputActivationEnums.RisingEdge.ToString();

                cam.LogicBlockLUTInputSelector.Value = LogicBlockLUTInputSelectorEnums.Input1.ToString();
                cam.LogicBlockLUTInputSource.Value = LogicBlockLUTInputSourceEnums.LogicBlock1.ToString();
                cam.LogicBlockLUTInputActivation.Value = LogicBlockLUTInputActivationEnums.LevelHigh.ToString();

                // UserOutput1 is used to activate the alternating behavior and initialize/reset
                // the LogicBlock1 value to 0. Beging with UserOutput1=0 to intialize the 
                // Locgic Block Q value to 0.
                cam.LogicBlockLUTInputSelector.Value = LogicBlockLUTInputSelectorEnums.Input2.ToString();
                cam.LogicBlockLUTInputSource.Value = LogicBlockLUTInputSourceEnums.UserOutput1.ToString();
                cam.LogicBlockLUTInputActivation.Value = LogicBlockLUTInputActivationEnums.LevelHigh.ToString();
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}\n", ex.Message);

                return false;
            }
            return true;
        }

        // Counter0 is used as a  MHz timer to count to alternatingDelayTimeUS microseconds
        // starting on Exposure Star
        private bool ConfigureCounter0(IManagedCamera cam, uint alternatingDelayTimeUS)
        {
            Console.WriteLine("\n Configuring Counter 0 \n");

            try
            {
                cam.CounterSelector.Value = CounterSelectorEnums.Counter0.ToString();
                cam.CounterEventSource.Value = CounterEventSourceEnums.MHzTick.ToString();
                cam.CounterTriggerSource.Value = CounterTriggerSourceEnums.Off.ToString();
                cam.CounterResetSource.Value = CounterResetSourceEnums.ExposureStart.ToString();
                cam.CounterResetActivation.Value = CounterResetActivationEnums.RisingEdge.ToString();
                cam.CounterDuration.Value = alternatingDelayTimeUS;

            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}\n", ex.Message);

                return false;
            }
            return true;
        }


        // Counter1 is used as a MHz timer to count to strobeDurrationUS microseconds
        // Starting on LogicBlock0 Rising Edge
        private bool ConfigureCounter1(IManagedCamera cam, uint strobeDurrationUS)
        {
            Console.WriteLine("\n Configuring Counter 1 \n");

            try
            {
                cam.CounterSelector.Value = CounterSelectorEnums.Counter1.ToString();
                cam.CounterEventSource.Value = CounterEventSourceEnums.MHzTick.ToString();
                cam.CounterTriggerSource.Value = CounterTriggerSourceEnums.Off.ToString();
                cam.CounterResetSource.Value = CounterResetSourceEnums.LogicBlock0.ToString();
                cam.CounterResetActivation.Value = CounterResetActivationEnums.RisingEdge.ToString();
                cam.CounterDuration.Value = strobeDurrationUS;

            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}\n", ex.Message);

                return false;
            }
            return true;
        }

        private bool AcquireImages(IManagedCamera cam)
        {
            Console.WriteLine("\n *** IMAGE ACQUISITION *** \n\n");

            try
            {
                cam.AcquisitionMode.Value = AcquisitionModeEnums.Continuous.ToString();

                string deviceSerialNumber = cam.DeviceID.Value;

                // enable alternating Strobe
                cam.UserOutputSelector.Value = UserOutputSelectorEnums.UserOutput1.ToString();
                cam.UserOutputValue.Value = true;

                cam.BeginAcquisition();

                Console.WriteLine("Acquiring images...\n");

                const uint k_numImages = 100;

                for (uint imageCnt = 0; imageCnt < k_numImages; imageCnt++)
                {
                    try
                    {
                        using(IManagedImage rawImage = cam.GetNextImage())
                        {
                            if (rawImage.IsIncomplete)
                            {
                                Console.WriteLine("Image incomplete with image status {0} ...\n\n", rawImage.ImageStatus);
                            }
                            else
                            {
                                Console.WriteLine("Grabbed image {0}, width = {1}, height = {2}\n",imageCnt,rawImage.Width, rawImage.Height);
                                // Omitting image conversion and saving for brevity. See Acquisition.cpp for how to convert and save images
                            }

                            rawImage.Release();

                            Console.WriteLine("\n");
                        }

              
                    }
                    catch (SpinnakerException ex)
                    {
                        Console.WriteLine("Error: {0}\n", ex.Message);
                    }
                }

                cam.EndAcquisition();

                // diable alternating Strobe
                cam.UserOutputSelector.Value = UserOutputSelectorEnums.UserOutput1.ToString();
                cam.UserOutputValue.Value = false;
            }
            catch (SpinnakerException ex)
            {
            Console.WriteLine("Error: {0}\n", ex.Message);
            return true;
            }

            return false;
        }


// This function acts as the body of the example; please see
// NodeMapInfo_CSharp example for more in-depth comments on setting up
// cameras.
int RunSingleCamera(IManagedCamera cam)
        {
            int result = 0;

            try
            {
                // Retrieve TL device nodemap and print device information
                INodeMap nodeMapTLDevice = cam.GetTLDeviceNodeMap();

                result = PrintDeviceInfo(nodeMapTLDevice);

                // Initialize camera
                cam.Init();

                // If acquisition is started then stop it
                try
                {
                    cam.AcquisitionStop.Execute();
                    Console.WriteLine("Acquisition already started, Stopping to configure settings.\n");
                }
                catch (SpinnakerException ex) 
                {
                    Console.WriteLine("Acquisition not started. Configuring settings.\n"); 
                }

                if (!LoadDefaultUserSet(cam))
                    return -1;

                if (!ConfigureFrameRate(cam, FRAME_RATE))
                    return -1;

                if (!ConfigureExposureTime(cam, EXPOSURE_LENGTH_US))
                    return -1;

                if (!ConfigureLine2(cam))
                    return -1;

                // Configure Line 1 Exposure active output for debugging (Optional)
                if (!ConfigureLine1(cam))
                    return -1;

                // ensure that user output 1 is false initially to initialize logic block Q value
                cam.UserOutputSelector.Value = UserOutputSelectorEnums.UserOutput1.ToString();
                cam.UserOutputValue.Value = false;

                if (!ConfigureLogicBlock0(cam))
                    return -1;

                if (!ConfigureLogicBlock1(cam))
                    return -1;

                if (!ConfigureCounter0(cam, ALTERNATING_TRIGGER_DELAY_US))
                    return -1;

                if (!ConfigureCounter1(cam, STROBE_DURATION_US))
                    return -1;

                Console.WriteLine("Strobe Durration: {0} us\n", STROBE_DURATION_US);
                Console.WriteLine("Alternating Strobe Delay : {0} us\n", ALTERNATING_TRIGGER_DELAY_US);

                AcquireImages(cam);

                // Return to default settings
                LoadDefaultUserSet(cam);

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
