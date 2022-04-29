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
*  @example cameraTimeToPCTime.cs
*
*  @brief cameraTimeToPCTime.cs shows how to convert camera's timestamp to PC
*  time.

*  It may be helpful to read through article below to understand how PC time is
*  calculated from system time, timestamp latch, chunk timestamp:
*  https://www.flir.com/support-center/iis/machine-vision/application-note/synchronizing-a-blackfly-or-grasshopper3-gige-cameras-time-to-pc-time/

*  It can also be helpful to familiarize yourself with the ChunkData.cpp as it
*  is shorter and simpler, and provides a strong introduction to camera
*  customization identifiers such as frame ID, properties such as black level,
*  and more. This information can be acquired from either the nodemap or the
*  image itself.

*  It may be preferable to grab chunk data from each individual image, as it
*  can be hard to verify whether data is coming from the correct image when
*  using the nodemap. This is because chunk data retrieved from the nodemap is
*  only valid for the current image; when GetNextImage() is called, chunk data
*  will be updated to that of the new current image.
 */

using System;
using System.IO;
using System.Collections.Generic;
using SpinnakerNET.GenApi;
using SpinnakerNET;

namespace cameraTimeToPCTime
{
    class Program
    {
        // Use the following enum and global static variable to select whether
        // chunk data is displayed from the image or the nodemap.
        enum chunkDataType
        {
            Image,
            Nodemap
        }

        static chunkDataType chosenChunkData = chunkDataType.Image;

        // This function configures the camera to add chunk data to each image.
        // It does this by enabling each type of chunk data before enabling
        // chunk data mode. When chunk data is turned on, the data is made
        // available in both the nodemap and each image.
        int ConfigureChunkData(INodeMap nodeMap)
        {
            int result = 0;

            Console.WriteLine("\n*** CONFIGURING CHUNK DATA ***\n");

            try
            {
                //
                // Activate chunk mode
                //
                // *** NOTES ***
                // Once enabled, chunk data will be available at the end of the
                // payload of every image captured until it is disabled. Chunk
                // data can also be retrieved from the nodemap.
                //
                IBool iChunkModeActive = nodeMap.GetNode<IBool>("ChunkModeActive");
                if (iChunkModeActive == null || !iChunkModeActive.IsWritable)
                {
                    Console.WriteLine("Cannot active chunk mode. Aborting...");
                    return -1;
                }

                iChunkModeActive.Value = true;

                Console.WriteLine("Chunk mode activated...");

                //
                // Enable all types of chunk data
                //
                // *** NOTES ***
                // Enabling chunk data requires working with nodes:
                // "ChunkSelector" is an enumeration selector node and
                // "ChunkEnable" is a boolean. It requires retrieving the
                // selector node (which is of enumeration node type), selecting
                // the entry of the chunk data to be enabled, retrieving the
                // corresponding boolean, and setting it to true.
                //
                // In this example, all chunk data is enabled, so these steps
                // are performed in a loop. Once this is complete, chunk mode
                // still needs to be activated.
                //
                // Retrieve selector node
                IEnum iChunkSelector = nodeMap.GetNode<IEnum>("ChunkSelector");
                if (iChunkSelector == null || !iChunkSelector.IsReadable)
                {
                    Console.WriteLine("Chunk selector not available. Aborting...");
                    return -1;
                }

                // Retrieve entries
                EnumEntry[] entries = iChunkSelector.Entries;

                Console.WriteLine("Enabling entries...");

                for (int i = 0; i < entries.Length; i++)
                {
                    // Select entry to be enabled
                    IEnumEntry iChunkSelectorEntry = entries[i];

                    // Go to next node if problem occurs
                    if (!iChunkSelectorEntry.IsAvailable || !iChunkSelectorEntry.IsReadable)
                    {
                        continue;
                    }

                    iChunkSelector.Value = iChunkSelectorEntry.Value;

                    Console.Write("\t{0}: ", iChunkSelectorEntry.Symbolic);

                    // Retrieve corresponding boolean
                    IBool iChunkEnable = nodeMap.GetNode<IBool>("ChunkEnable");

                    // Enable the boolean, thus enabling the corresponding chunk
                    // data
                    if (iChunkEnable == null)
                    {
                        Console.WriteLine("not available");
                        result = -1;
                    }
                    else if (iChunkEnable.Value)
                    {
                        Console.WriteLine("enabled");
                    }
                    else if (iChunkEnable.IsWritable)
                    {
                        iChunkEnable.Value = true;
                        Console.WriteLine("enabled");
                    }
                    else
                    {
                        Console.WriteLine("not writable");
                        result = -1;
                    }
                }
                Console.WriteLine();
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
                result = -1;
            }

            return result;
        }

        // Calculate offset for BFS / Oryx and some other cameras
        Int64 calculateOffset(IManagedCamera cam)
        {
            DateTime now = DateTime.Now;
 
            // Convert current calendar time into seconds
            Int64 hour_in_seconds = now.Hour * 3600;
            Int64 minute_in_seconds = now.Minute * 60;
            Int64 current_time_in_seconds = hour_in_seconds + minute_in_seconds + now.Second;
            Console.WriteLine("Current time in seconds: {0}", current_time_in_seconds);

            // Get current camera time using TimestampLatch
            ICommand iTimestampLatch = cam.GetNodeMap().GetNode<ICommand>("TimestampLatch");
            if (iTimestampLatch == null || !iTimestampLatch.IsWritable)
            {
                Console.WriteLine(" Unable to write Time Stamp Latch Value (node retrieval). Aborting..");
                return -1;
            }
            // Execute command
            iTimestampLatch.Execute();
            IInteger iTimestampLatchValue = cam.GetNodeMap().GetNode<IInteger>("TimestampLatchValue");

            if (iTimestampLatchValue == null || !iTimestampLatchValue.IsReadable)
            {
                Console.WriteLine(" Unable to read Time Stamp Latch Value (node retrieval). Aborting..");
                return -1;
            }

            // Print out timestamp value
            Int64 timestampLatchValue = iTimestampLatchValue.Value;
            Console.WriteLine("Timestamp Latch Value in ns: {0}", timestampLatchValue);

            // Calculate offset = PC_TIME (s) - CAMERA_TIME(s)
            Int64 offset = current_time_in_seconds - (timestampLatchValue / 1000000000);
            Console.WriteLine("Offset (current PC time(s) - camera timestamp(s)): {0}", offset);

            return offset;
        }

        // Calculate offset for BFLY-U3, FL3-U3 and GS3-U3 cameras
        Int64 calculateOffset_gen2USB(IManagedCamera cam)
        {
            DateTime now = DateTime.Now;

            // Convert current calendar time into seconds
            Int64 hour_in_seconds = now.Hour * 3600;
            Int64 minute_in_seconds = now.Minute * 60;
            Int64 current_time_in_seconds = hour_in_seconds + minute_in_seconds + now.Second;
            Console.WriteLine("Current time in seconds: {0}", current_time_in_seconds);

            // Get current camera time using TimestampLatch
            ICommand iTimestampLatch = cam.GetNodeMap().GetNode<ICommand>("TimestampLatch");
            if (iTimestampLatch == null || !iTimestampLatch.IsWritable)
            {
                Console.WriteLine(" Unable to write Time Stamp Latch Value (node retrieval). Aborting..");
                return -1;
            }
            // Execute command
            iTimestampLatch.Execute();
            IInteger iTimestampLatchValue = cam.GetNodeMap().GetNode<IInteger>("Timestamp");

            if (iTimestampLatchValue == null || !iTimestampLatchValue.IsReadable)
            {
                Console.WriteLine(" Unable to read Time Stamp Latch Value (node retrieval). Aborting..");
                return -1;
            }

            // Print out timestamp value
            Int64 timestampLatchValue = iTimestampLatchValue.Value;
            Console.WriteLine("Timestamp Latch Value in ns: {0}", timestampLatchValue);

            // Calculate offset = PC_TIME (s) - CAMERA_TIME(s)
            Int64 offset = current_time_in_seconds - (timestampLatchValue / 1000000000);
            Console.WriteLine("Offset (current PC time(s) - camera timestamp(s)): {0}", offset);

            return offset;
        }

        // Calculate offset for BFLY-PGE and FL3-GE cameras
        Int64 calculateOffset_GEV(IManagedCamera cam)
        {
            DateTime now = DateTime.Now;

            // Convert current calendar time into seconds
            Int64 hour_in_seconds = now.Hour * 3600;
            Int64 minute_in_seconds = now.Minute * 60;
            Int64 current_time_in_seconds = hour_in_seconds + minute_in_seconds + now.Second;
            Console.WriteLine("Current time in seconds: {0}", current_time_in_seconds);

            // Get current camera time using TimestampLatch
            ICommand iTimestampLatch = cam.GetNodeMap().GetNode<ICommand>("GevTimestampControlLatch");
            if (iTimestampLatch == null || !iTimestampLatch.IsWritable)
            {
                Console.WriteLine(" Unable to write Time Stamp Latch Value (node retrieval). Aborting..");
                return -1;
            }
            // Execute command
            iTimestampLatch.Execute();
            IInteger iTimestampLatchValue = cam.GetNodeMap().GetNode<IInteger>("GevTimestampValue");

            if (iTimestampLatchValue == null || !iTimestampLatchValue.IsReadable)
            {
                Console.WriteLine(" Unable to read Time Stamp Latch Value (node retrieval). Aborting..");
                return -1;
            }

            // Print out timestamp value
            Int64 timestampLatchValue = iTimestampLatchValue.Value;
            Console.WriteLine("Timestamp Latch Value in ns: {0}", timestampLatchValue);

            // Calculate offset = PC_TIME (s) - CAMERA_TIME(s)
            Int64 offset = current_time_in_seconds - (timestampLatchValue / 1000000000);
            Console.WriteLine("Offset (current PC time(s) - camera timestamp(s)): {0}", offset);

            return offset;
        }



        // This function displays a select amount of chunk data from the image.
        // Unlike accessing chunk data via the nodemap, there is no way to loop
        // through all available data.
        int DisplayChunkData(IManagedImage managedImage)
        {
            int result = 0;

            Console.WriteLine("Printing chunk data from image...");

            try
            {
                //
                // Retrieve chunk data from image
                //
                // *** NOTES ***
                // When retrieving chunk data from an image, the data is stored
                // in a a ChunkData object and accessed with getter functions.
                //
                ManagedChunkData managedChunkData = managedImage.ChunkData;

                //
                // Retrieve exposure time; exposure time recorded in microseconds
                //
                // *** NOTES ***
                // In C#, Floating point numbers are returned from chunk data
                // objects as a double.
                //
                double exposureTime = managedChunkData.ExposureTime;
                Console.WriteLine("\tExposure time: {0}", exposureTime);

                //
                // Retrieve frame ID
                //
                // *** NOTES ***
                // In C#, Integers are returned as a long.
                //
                long frameID = managedChunkData.FrameID;
                Console.WriteLine("\tFrame ID: {0}", frameID);

                // Retrieve gain; gain recorded in decibels
                double gain = managedChunkData.Gain;
                Console.WriteLine("\tGain: {0}", gain);

                // Retrieve height; height recorded in pixels
                long height = managedChunkData.Height;
                Console.WriteLine("\tHeight: {0}", height);

                // Retrieve offset X; offset X recorded in pixels
                long offsetX = managedChunkData.OffsetX;
                Console.WriteLine("\tOffset X: {0}", offsetX);

                // Retrieve offset Y; offset Y recorded in pixels
                long offsetY = managedChunkData.OffsetY;
                Console.WriteLine("\tOffset Y: {0}", offsetY);

                // Retrieve sequencer set active
                long sequencerSetActive = managedChunkData.SequencerSetActive;
                Console.WriteLine("\tSequencer set active: {0}", sequencerSetActive);

                // Retrieve timestamp
                long timestamp = managedChunkData.Timestamp;
                Console.WriteLine("\tTimestamp: {0}", timestamp);

                // Retrieve width; width recorded in pixels
                long width = managedChunkData.Width;
                Console.WriteLine("\tWidth: {0}", width);
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
                result = -1;
            }

            return result;
        }

        // This function displays all available chunk data by looping through
        // the chunk data category node on the nodemap.
        int DisplayChunkData(INodeMap nodeMap)
        {
            int result = 0;

            Console.WriteLine("Printing chunk data from nodemap...");

            try
            {
                //
                // Retrieve chunk data information nodes
                //
                // *** NOTES ***
                // As well as being written into the payload of the image, chunk
                // data is accessible on the GenICam nodemap. Insofar as chunk
                // data is enabled, it is available from both sources.
                //
                ICategory iChunkDataControl = nodeMap.GetNode<ICategory>("ChunkDataControl");
                if (iChunkDataControl == null || !iChunkDataControl.IsReadable)
                {
                    Console.WriteLine("Chunk selector not available. Aborting...");
                    return -1;
                }

                // Retrieve entries
                var features = iChunkDataControl.Features;

                // Iterate through entries
                for (int i = 0; i < features.Length; i++)
                {
                    IValue currentFeature = features[i];

                    if (currentFeature == null || !currentFeature.IsReadable)
                    {
                        Console.WriteLine("\tNode not available");
                    }
                    //
                    // Print boolean node type value
                    //
                    // *** NOTES ***
                    // Boolean information is manipulated to output the
                    // more -easily identifiable 'true' and 'false' as opposed to
                    // '1' and '0'.
                    //
                    else if (currentFeature.GetType() == typeof (IBool))
                    {
                        IBool boolFeature = (IBool) currentFeature;

                        Console.WriteLine(
                            "\t{0}: {1}",
                            currentFeature.DisplayName,
                            (boolFeature.Value ? "true"
                             : "false"));
                    }
                    //
                    // Print non-boolean node type value
                    //
                    // *** NOTES ***
                    // Features are retrieved from category nodes as value nodes.
                    // Value nodes (or 'IValue') can have their values accessed
                    // and returned as a string using the ToString() method.
                    //
                    else
                    {
                        Console.WriteLine("\t{0}: {1}", currentFeature.DisplayName, currentFeature.ToString());
                    }
                }
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
                result = -1;
            }

            return result;
        }

        // This function prints the device information of the camera from the
        // transport layer; please see NodeMapInfo_CSharp example for more
        // in-depth comments on printing device information from the nodemap.
        int PrintDeviceInfo(INodeMap nodeMap)
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

        // This function acquires and saves 10 images from a device; please see
        // Acquisition_CSharp example for more in-depth comments on the
        // acquisition of images.
        int AcquireImages(IManagedCamera cam, INodeMap nodeMap, INodeMap nodeMapTLDevice)
        {
            int result = 0;

            Console.WriteLine("\n*** IMAGE ACQUISITION ***\n");

            try
            {
                // Set acquisition mode to continuous
                IEnum iAcquisitionMode = nodeMap.GetNode<IEnum>("AcquisitionMode");
                if (iAcquisitionMode == null || !iAcquisitionMode.IsWritable)
                {
                    Console.WriteLine("Unable to set acquisition mode to continuous (node retrieval). Aborting...\n");
                    return -1;
                }

                IEnumEntry iAcquisitionModeContinuous = iAcquisitionMode.GetEntryByName("Continuous");
                if (iAcquisitionModeContinuous == null || !iAcquisitionMode.IsReadable)
                {
                    Console.WriteLine(
                        "Unable to set acquisition mode to continuous (enum entry retrieval). Aborting...\n");
                    return -1;
                }

                iAcquisitionMode.Value = iAcquisitionModeContinuous.Symbolic;

                Console.WriteLine("Acquisition mode set to continuous...");

                // Begin acquiring images
                cam.BeginAcquisition();

                Console.WriteLine("Acquiring images...");

                // Retrieve device serial number for filename
                String deviceSerialNumber = "";

                IString iDeviceSerialNumber = nodeMapTLDevice.GetNode<IString>("DeviceSerialNumber");
                if (iDeviceSerialNumber != null && iDeviceSerialNumber.IsReadable)
                {
                    deviceSerialNumber = iDeviceSerialNumber.Value;

                    Console.WriteLine("Device serial number retrieved as {0}...", deviceSerialNumber);
                }
                Console.WriteLine();

                IEnum iDeviceType = nodeMapTLDevice.GetNode<IEnum>("DeviceType");
                if (iDeviceType == null || !iDeviceType.IsReadable)
                {
                    Console.WriteLine("Error with reading the device's type. Aborting...");
                    return -1;
                }

                IString iDeviceModelName = nodeMap.GetNode<IString>("DeviceModelName");
                if (iDeviceModelName == null || !iDeviceModelName.IsReadable)
                {
                    Console.WriteLine("Unable to determine camera family. Aborting...");
                    return -1;
                }

                string cameraModel = iDeviceModelName.ToString();
                Console.WriteLine("Camera mode is: {0}", cameraModel);

                Int64 hours, minutes;
                Int64 imageTimestamp;
                Int64 imageTimestamp_converted = 0;

                // Retrieve, convert, and save images
                const int NumImages = 10;

                for (int imageCnt = 0; imageCnt < NumImages; imageCnt++)
                {
                    try
                    {
                        // Retrieve next received image and ensure image completion

                        using(IManagedImage rawImage = cam.GetNextImage(1000))
                        {
                            ManagedChunkData chunkData = rawImage.ChunkData;

                            if (rawImage.IsIncomplete)
                            {
                                Console.WriteLine("Image incomplete with image status {0}...", rawImage.ImageStatus);
                            }
                            else
                            {
                                // Print image information
                                Console.WriteLine(
                                    "Grabbed image {0}, width = {1}, height = {1}",
                                    imageCnt,
                                    rawImage.Width,
                                    rawImage.Height);
                                // Retrieve timestamp from chunk data
                                Int64 timestamp = chunkData.Timestamp;

                                Console.WriteLine("Chunk Timestamp in ns: {0}...", timestamp);

                                // How to calcualte offset for GigE vision cameras
                                if (iDeviceType.Value.ToString() == DeviceTLTypeEnums.GigEVision.ToString())
                                {
                                    if (cameraModel.IndexOf("Blackfly S") == 0 || cameraModel.IndexOf("Oryx") == 0)
                                    {
                                        imageTimestamp_converted = (timestamp / 1000000000) + calculateOffset(cam);
                                    }
                                    else
                                    {
                                        imageTimestamp_converted = (timestamp / 1000000000) + calculateOffset_GEV(cam);
                                    }
                                }

                                // How to calcualte offset for BFLY-U3, GS3-U3, FL3-U3 or CM3-U3
                                else if (cameraModel.IndexOf("Blackfly BFLY-U3") == 0 || cameraModel.IndexOf("Grasshopper3 GS3-U3") == 0
                                    || cameraModel.IndexOf("Flea3 FL3-U3") == 0 || cameraModel.IndexOf("Chameleon3 CM3-U3") == 0)
                                {
                                    imageTimestamp_converted = (timestamp / 1000000000) + calculateOffset_gen2USB(cam);
                                }

                                // How to calculate offset for the rest of the cameras
                                else
                                {
                                    imageTimestamp_converted = (timestamp / 1000000000) + calculateOffset(cam);
                                }
                                Console.WriteLine("PC timestamp in seconds (chunk timestamp + offset): {0}", imageTimestamp_converted);

                                // Convert imageTimestamp to clock time
                                minutes = imageTimestamp_converted / 60;
                                hours = minutes / 60;

                                UInt16 minutes_converted = (UInt16)(minutes % 60);
                                UInt16 seconds_converted = (UInt16)(imageTimestamp_converted % 60);

                                Console.WriteLine("ImageTimestamp {0} (seconds) is equivalent to \n{1} " +
                                    "hours {2} minutes {3} seconds.", imageTimestamp_converted, 
                                    hours, minutes_converted, seconds_converted);

                                // Convert image to mono 8
                                using (IManagedImage convertedImage = rawImage.Convert(PixelFormatEnums.Mono8))
                                {
                                    // Create unique file name
                                    String filename = "ChunkData-CSharp-";
                                    if (deviceSerialNumber != "")
                                    {
                                        filename = filename + deviceSerialNumber + "-";
                                    }
                                    filename = filename + imageCnt + ".jpg";

                                    // Save image
                                    convertedImage.Save(filename);

                                    Console.WriteLine("Image saved at {0}", filename);

                                    // Display chunk data
                                    if (chosenChunkData == chunkDataType.Image)
                                    {
                                        result = DisplayChunkData(rawImage);
                                    }
                                    else if (chosenChunkData == chunkDataType.Nodemap)
                                    {
                                        result = DisplayChunkData(nodeMap);
                                    }
                                }
                            }
                        }
                        Console.WriteLine();
                    }
                    catch (SpinnakerException ex)
                    {
                        Console.WriteLine("Error: {0}", ex.Message);
                        result = -1;
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

        // This function disables each type of chunk data before disabling chunk data mode.
        int DisableChunkData(INodeMap nodeMap)
        {
            int result = 0;

            try
            {
                // Retrieve selector node
                IEnum iChunkSelector = nodeMap.GetNode<IEnum>("ChunkSelector");
                if (iChunkSelector == null || !iChunkSelector.IsReadable)
                {
                    Console.WriteLine("Chunk selector not available. Aborting...");
                    return -1;
                }

                // Retrieve entries
                EnumEntry[] entries = iChunkSelector.Entries;

                Console.WriteLine("Disabling entries...");

                for (int i = 0; i < entries.Length; i++)
                {
                    // Select entry to be disabled
                    IEnumEntry iChunkSelectorEntry = entries[i];

                    // Go to next node if problem occurs
                    if (!iChunkSelectorEntry.IsAvailable || !iChunkSelectorEntry.IsReadable)
                    {
                        continue;
                    }

                    iChunkSelector.Value = iChunkSelectorEntry.Value;

                    Console.Write("\t{0}: ", iChunkSelectorEntry.Symbolic);

                    // Retrieve corresponding boolean
                    IBool iChunkEnable = nodeMap.GetNode<IBool>("ChunkEnable");

                    // Disable the boolean, thus disabling the corresponding chunk
                    // data
                    if (iChunkEnable == null)
                    {
                        Console.WriteLine("not available");
                        result = -1;
                    }
                    else if (!iChunkEnable.Value)
                    {
                        Console.WriteLine("disabled");
                    }
                    else if (iChunkEnable.IsWritable)
                    {
                        iChunkEnable.Value = false;
                        Console.WriteLine("disabled");
                    }
                    else
                    {
                        Console.WriteLine("not writable");
                    }
                }
                Console.WriteLine();

                // Deactivate ChunkMode
                IBool iChunkModeActive = nodeMap.GetNode<IBool>("ChunkModeActive");
                if (iChunkModeActive == null || !iChunkModeActive.IsWritable)
                {
                    Console.WriteLine("Cannot deactive chunk mode. Aborting...");
                    result = -1;
                }

                iChunkModeActive.Value = false;

                Console.WriteLine("Chunk mode deactivated...");
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

                // Configure chunk data
                err = ConfigureChunkData(nodeMap);
                if (err < 0)
                {
                    return err;
                }

                // Acquire images and display chunk data
                result = result | AcquireImages(cam, nodeMap, nodeMapTLDevice);

                // Disable chunk data
                err = DisableChunkData(nodeMap);
                if (err < 0)
                {
                    return err;
                }

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

            // Run example on each camera
            int i = 0;

            foreach(IManagedCamera managedCamera in camList) using(managedCamera)
            {
                Console.WriteLine("Running example for camera {0}...", i);

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

                Console.WriteLine("Camera {0} example complete...\n", i++);
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
