//=============================================================================
// Copyright (c) 2026 FLIR Integrated Imaging Solutions, Inc. All Rights Reserved.
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
 *  @example AcquisitionGenDC_CSharp.cs
 *
 *  @brief AcquisitionGenDC_CSharp.cs demonstrates how to acquire multi-component images using
 *  the GenDC (Generic Data Container) payload format. This example is specifically for
 *  cameras that support GenDC.
 *
 *  GenDC is a standardized container format that allows cameras to transmit multiple
 *  data components (images, metadata, etc.) in a single synchronized payload. This is
 *  useful for multi-sensor cameras, stereo cameras, or cameras with multiple data streams
 *  such as intensity + depth.
 *
 *  This example relies on information provided in the Enumeration_CSharp example. Also check out
 *  the ExceptionHandling_CSharp and NodeMapInfo_CSharp examples if you haven't already. For standard
 *  single-image acquisition, see the Acquisition_CSharp example.
 */

using System;
using System.IO;
using System.Collections.Generic;
using SpinnakerNET;
using SpinnakerNET.GenApi;

namespace AcquisitionGenDC_CSharp
{
    class Program
    {
        // Component selection toggles for GenDC acquisition
        // These flags control which components are enabled during GenDC acquisition
        static bool enableComponent0 = true; // Typically intensity/main image component
        static bool enableComponent1 = true; // Additional component such as depth or range image (if available)

        // Helper function to convert ImagePayloadType enum to string
        static string ImagePayloadTypeToString(IManagedImage image)
        {
            ImagePayloadType payloadType = image.ImagePayloadType;

            switch (payloadType)
            {
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_UNKNOWN:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_UNKNOWN";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_IMAGE:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_IMAGE";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_EXTENDED_CHUNK:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_EXTENDED_CHUNK";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_JPEG:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_JPEG";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_LOSSLESS_COMPRESSED:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_LOSSLESS_COMPRESSED";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_LOSSY_COMPRESSED:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_LOSSY_COMPRESSED";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_JPEG_LOSSLESS_COMPRESSED:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_JPEG_LOSSLESS_COMPRESSED";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_RAW_SENSOR1:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR1";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_RAW_SENSOR2:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR2";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR2:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR2";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_GENDC_GENERIC:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_GENDC_GENERIC";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_RANGE_SENSOR1:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_RANGE_SENSOR1";
                case ImagePayloadType.IMAGE_PAYLOAD_TYPE_RANGE_SENSOR2:
                    return "SPINNAKER_IMAGE_PAYLOAD_TYPE_RANGE_SENSOR2";
                default:
                    return String.Format("UNKNOWN ({0})", (int) payloadType);
            }
        }

        // Disables or enables heartbeat on GEV cameras so debugging does not incur timeout errors
        static int ConfigureGVCPHeartbeat(IManagedCamera cam, bool enableHeartbeat)
        {
            //
            // Write to boolean node controlling the camera's heartbeat
            //
            // *** NOTES ***
            // This applies only to GEV cameras.
            //
            // GEV cameras have a heartbeat built in, but when debugging applications the
            // camera may time out due to its heartbeat. Disabling the heartbeat prevents
            // this timeout from occurring, enabling us to continue with any necessary
            // debugging.
            //
            // *** LATER ***
            // Make sure that the heartbeat is reset upon completion of the debugging.
            // If the application is terminated unexpectedly, the camera may be locked
            // to Spinnaker indefinitely due to the the timeout being disabled.  When that
            // happens, a camera power cycle will reset the heartbeat to its default setting.
            //

            // Retrieve TL device nodemap and print device information
            INodeMap nodeMapTLDevice = cam.GetTLDeviceNodeMap();

            // Retrieve GenICam nodemap
            INodeMap nodeMap = cam.GetNodeMap();

            IEnum iDeviceType = nodeMapTLDevice.GetNode<IEnum>("DeviceType");
            IEnumEntry iDeviceTypeGEV = iDeviceType.GetEntryByName("GigEVision");
            // We first need to confirm that we're working with a GEV camera
            if (iDeviceType != null && iDeviceType.IsReadable)
            {
                if (iDeviceType.Value == iDeviceTypeGEV.Value)
                {
                    if (enableHeartbeat)
                    {
                        Console.WriteLine("Resetting heartbeat");
                    }
                    else
                    {
                        Console.WriteLine("Disabling heartbeat");
                    }
                    IBool iGEVHeartbeatDisable = nodeMap.GetNode<IBool>("GevGVCPHeartbeatDisable");
                    if (iGEVHeartbeatDisable == null || !iGEVHeartbeatDisable.IsWritable)
                    {
                        Console.WriteLine(
                            "Unable to disable heartbeat on camera. Continuing with execution as this may be non-fatal...");
                    }
                    else
                    {
                        iGEVHeartbeatDisable.Value = !enableHeartbeat;

                        if (!enableHeartbeat)
                        {
                            Console.WriteLine("WARNING: Heartbeat has been disabled for the rest of this example run.");
                            Console.WriteLine(
                                "         Heartbeat will be reset upon the completion of this run.  If the ");
                            Console.WriteLine(
                                "         example is aborted unexpectedly before the heartbeat is reset, the");
                            Console.WriteLine("         camera may need to be power cycled to reset the heartbeat.\n");
                        }
                        else
                        {
                            Console.WriteLine("Heartbeat has been reset.\n");
                        }
                        Console.WriteLine();
                    }
                }
            }
            else
            {
                Console.WriteLine("Unable to access TL device nodemap. Aborting...");
                return -1;
            }

            return 0;
        }

        static int ResetGVCPHeartbeat(IManagedCamera cam)
        {
            return ConfigureGVCPHeartbeat(cam, true);
        }
        static int DisableGVCPHeartbeat(IManagedCamera cam)
        {
            return ConfigureGVCPHeartbeat(cam, false);
        }

        // This function checks if the camera supports GenDC and configures component selection
        static int ConfigureGenDC(IManagedCamera cam, INodeMap nodeMap)
        {
            int result = 0;

            Console.WriteLine("\n*** CONFIGURING GENDC ***");

            try
            {
                //
                // Check for GenDC support by looking for GenDC-specific TL nodes
                //
                // *** NOTES ***
                // GenDC cameras expose specific Transport Layer Control nodes for GenDC streaming:
                // GenDCStreamingMode and GenDCStreamingStatus. These nodes are part of the camera's
                // GenICam nodemap (obtained via GetNodeMap()) and are required for multi-component image acquisition.
                //
                Console.WriteLine("\nChecking camera GenDC support...");
                IEnum iGenDCStreamingMode = nodeMap.GetNode<IEnum>("GenDCStreamingMode");
                IEnum iGenDCStreamingStatus = nodeMap.GetNode<IEnum>("GenDCStreamingStatus");
                if (iGenDCStreamingMode == null || !iGenDCStreamingMode.IsReadable || iGenDCStreamingStatus == null ||
                    !iGenDCStreamingStatus.IsReadable)
                {
                    INodeMap nodeMapTLDevice = cam.GetTLDeviceNodeMap();
                    IString iDeviceSerialNumber = nodeMapTLDevice.GetNode<IString>("DeviceSerialNumber");
                    string serialNumber = (iDeviceSerialNumber != null && iDeviceSerialNumber.IsReadable)
                                              ? iDeviceSerialNumber.Value
                                              : "Unknown";
                    Console.WriteLine(
                        "Device serial number {0} is not a valid GenDC camera. Skipping...", serialNumber);
                    return -1;
                }

                //
                // Display GenDC streaming mode
                //
                // *** NOTES ***
                // GenDCStreamingMode indicates the current mode of GenDC streaming.
                // This could include values like "Off", "On", "Automatic", or other streaming modes
                // depending on the camera implementation.
                //
                if (iGenDCStreamingMode.IsReadable)
                {
                    Console.WriteLine("  GenDC Streaming Mode: {0}", iGenDCStreamingMode.Value);
                    if (iGenDCStreamingMode.Value == "Off")
                    {
                        Console.WriteLine(
                            "  GenDC streaming is currently off. Setting GenDC Streaming Mode to 'On'...");
                        if (!iGenDCStreamingMode.IsWritable)
                        {
                            Console.WriteLine(
                                "Unable to set GenDC Streaming Mode to 'On'. GenDCStreamingMode is not Writable. Skipping...");
                            return -1;
                        }
                        IEnumEntry iOnMode = iGenDCStreamingMode.GetEntryByName("On");
                        if (iOnMode != null && iOnMode.IsReadable)
                        {
                            iGenDCStreamingMode.Value = iOnMode.Symbolic;
                            Console.WriteLine("  GenDC Streaming Mode now set to : {0}", iGenDCStreamingMode.Value);
                        }
                        else
                        {
                            Console.WriteLine(
                                "Unable to set GenDC Streaming Mode to 'On' (entry retrieval). Skipping...");
                            return -1;
                        }
                    }
                }

                //
                // Display GenDC streaming status
                //
                // *** NOTES ***
                // GenDCStreamingStatus indicates the current status of GenDC streaming.
                // This provides information about whether GenDC streaming is active,
                // ready, or in another state.
                //
                if (iGenDCStreamingStatus.IsReadable)
                {
                    Console.WriteLine("  GenDC Streaming Status: {0}", iGenDCStreamingStatus.Value);
                    if (iGenDCStreamingStatus.Value != "On")
                    {
                        Console.WriteLine("  GenDC Streaming Status is not On. Skipping...");
                        return -1;
                    }
                }

                //
                // Configure components if ComponentSelector is available
                //
                // *** NOTES ***
                // If the camera exposes ComponentSelector and ComponentEnable nodes,
                // we can configure individual components. Otherwise, GenDC streaming
                // will use the default component configuration.
                //
                IEnum iComponentSelector = nodeMap.GetNode<IEnum>("ComponentSelector");
                IBool iComponentEnable = nodeMap.GetNode<IBool>("ComponentEnable");

                if (iComponentSelector != null && iComponentSelector.IsReadable && iComponentEnable != null &&
                    iComponentEnable.IsReadable)
                {
                    Console.WriteLine("\nConfiguring GenDC components...");

                    IEnumEntry[] componentEntries = iComponentSelector.Entries;

                    if (componentEntries.Length == 0)
                    {
                        Console.WriteLine("Warning: No components available for selection.");
                    }
                    else
                    {
                        Console.WriteLine("  Available components: {0}", componentEntries.Length);

                        for (int i = 0; i < componentEntries.Length; i++)
                        {
                            IEnumEntry iComponent = componentEntries[i];
                            if (iComponent != null && iComponent.IsReadable)
                            {
                                // Select the component
                                iComponentSelector.Value = iComponent.Value;

                                string componentName = iComponent.Symbolic;
                                Console.Write("    Component {0}: {1} - ", i, componentName);

                                // Enable/disable component based on toggle
                                if (iComponentEnable.IsWritable)
                                {
                                    bool enableComponent = true;

                                    // This example only demonstrates toggling two components;
                                    // for cameras with more components, extend this logic as needed.
                                    enableComponent = (i == 0) ? enableComponent0 : enableComponent;
                                    enableComponent = (i == 1) ? enableComponent1 : enableComponent;

                                    iComponentEnable.Value = enableComponent;
                                    Console.WriteLine(enableComponent ? "Enabled" : "Disabled");
                                }
                                else
                                {
                                    Console.WriteLine("ComponentEnable not writable for this component");
                                }
                            }
                        }
                    }
                }
                else
                {
                    Console.WriteLine("\nComponentSelector/ComponentEnable not available.");
                    Console.WriteLine("Using default component configuration.");
                }

                Console.WriteLine("\nGenDC configuration complete.");
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("ERROR: Exception during GenDC configuration: {0}", ex.Message);
                result = -1;
            }

            return result;
        }

        // This function acquires and saves 10 GenDC image sets from a device.
        static int AcquireImages(IManagedCamera cam, INodeMap nodeMap, INodeMap nodeMapTLDevice)
        {
            int result = 0;

            Console.WriteLine("\n\n*** GENDC IMAGE ACQUISITION ***\n");

            try
            {
                //
                // Set acquisition mode to continuous
                //
                // *** NOTES ***
                // Because the example acquires and saves 10 image sets, setting acquisition
                // mode to continuous lets the example finish. If set to single frame
                // or multiframe (at a lower number of images), the example would just
                // hang. This would happen because the example has been written to
                // acquire 10 image sets while the camera would have been programmed to
                // retrieve less than that.
                //
                // Setting the value of an enumeration node is slightly more
                // complicated than other node types. Two nodes are required:
                // first, the enumeration node is retrieved from the nodemap and
                // second, the entry node is retrieved from the enumeration node.
                // The symbolic of the entry node is then set as the new value
                // of the enumeration node.
                //
                // Notice that both the enumeration and entry nodes are checked
                // for availability and readability/writability. Enumeration
                // nodes are generally readable and writable whereas entry
                // nodes are only ever readable.
                //
                // Retrieve enumeration node from nodemap
                IEnum iAcquisitionMode = nodeMap.GetNode<IEnum>("AcquisitionMode");
                if (iAcquisitionMode == null || !iAcquisitionMode.IsWritable || !iAcquisitionMode.IsReadable)
                {
                    Console.WriteLine("Unable to set acquisition mode to continuous (node retrieval). Aborting...\n");
                    return -1;
                }

                // Retrieve entry node from enumeration node
                IEnumEntry iAcquisitionModeContinuous = iAcquisitionMode.GetEntryByName("Continuous");
                if (iAcquisitionModeContinuous == null || !iAcquisitionModeContinuous.IsReadable)
                {
                    Console.WriteLine(
                        "Unable to set acquisition mode to continuous (enum entry retrieval). Aborting...\n");
                    return -1;
                }

                // Set symbolic from entry node as new value for enumeration node
                iAcquisitionMode.Value = iAcquisitionModeContinuous.Symbolic;

                Console.WriteLine("Acquisition mode set to continuous...");

                //
                // Begin acquiring images
                //
                // *** NOTES ***
                // What happens when the camera begins acquiring images depends on the
                // acquisition mode. Single frame captures only a single image, multi
                // frame captures a set number of images, and continuous captures a
                // continuous stream of images. Because the example calls for the
                // retrieval of 10 image sets, continuous mode has been set.
                //
                // *** LATER ***
                // Image acquisition must be ended when no more images are needed.
                //
                cam.BeginAcquisition();

                Console.WriteLine("Acquiring GenDC image sets...");

                //
                // Retrieve device serial number for filename
                //
                // *** NOTES ***
                // The device serial number is retrieved in order to keep cameras from
                // overwriting one another. Grabbing image IDs could also accomplish
                // this.
                //
                String deviceSerialNumber = "";

                IString iDeviceSerialNumber = nodeMapTLDevice.GetNode<IString>("DeviceSerialNumber");
                if (iDeviceSerialNumber != null && iDeviceSerialNumber.IsReadable)
                {
                    deviceSerialNumber = iDeviceSerialNumber.Value;

                    Console.WriteLine("Device serial number retrieved as {0}...", deviceSerialNumber);
                }
                Console.WriteLine();

                // Retrieve, convert, and save images
                const int NumImageSets = 10;

                //
                // Create ImageProcessor instance for post processing images
                //
                IManagedImageProcessor processor = new ManagedImageProcessor();

                //
                // Set default image processor color processing method
                //
                // *** NOTES ***
                // By default, if no specific color processing algorithm is set, the image
                // processor will default to NEAREST_NEIGHBOR method.
                //
                processor.SetColorProcessing(ColorProcessingAlgorithm.HQ_LINEAR);

                for (int imageCnt = 0; imageCnt < NumImageSets; imageCnt++)
                {
                    try
                    {
                        //
                        // Retrieve next synchronized GenDC ManagedImageList
                        //
                        // *** NOTES ***
                        // GetNextImageSync() is specifically designed for multi-stream/multi-component cameras.
                        // It returns a ManagedImageList containing synchronized images from all enabled
                        // streams/components. Each image in the list represents a different component from the GenDC
                        // container.
                        //
                        // The ManagedImageList object is a container for one or more IManagedImage objects, where each
                        // image corresponds to a component in the GenDC container. All components in the list
                        // are synchronized by frame ID, ensuring temporal consistency.
                        //
                        // This function will block for the specified timeout period until images from all
                        // enabled components arrive.
                        //
                        // IMPORTANT: GetNextImageSync() cannot be used together with GetNextImage(). Using
                        // both will result in indeterministic behavior and could lead to missing or lost images.
                        //
                        // *** LATER ***
                        // Once images from the ManagedImageList are saved and/or no longer needed, each image
                        // in the list must be released in order to keep the buffers from filling up.
                        //
                        IManagedImageList imageList = cam.GetNextImageSync(1000);

                        int componentCount = imageList.Count;

                        Console.WriteLine("Grabbed GenDC ImageList {0}, components: {1}", imageCnt, componentCount);

                        //
                        // Validate that we received components
                        //
                        if (componentCount == 0)
                        {
                            Console.WriteLine("  Warning: Received empty ImageList\n");
                            continue;
                        }

                        //
                        // Process each component in the ManagedImageList
                        //
                        // *** NOTES ***
                        // Each image in the list represents a component from the GenDC container.
                        // Components are synchronized by frame ID and can have different dimensions,
                        // pixel formats, and payload types.
                        //
                        // Common component types include:
                        // - IMAGE_PAYLOAD_TYPE_RAW_SENSOR1: Primary intensity image
                        // - IMAGE_PAYLOAD_TYPE_RAW_SENSOR2: Secondary intensity image
                        // - IMAGE_PAYLOAD_TYPE_RANGE_SENSOR1: Range/depth data
                        // - IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1: Disparity data
                        // - IMAGE_PAYLOAD_TYPE_EXTENDED_CHUNK: Extended chunk metadata
                        //
                        for (int componentIdx = 0; componentIdx < componentCount; componentIdx++)
                        {
                            try
                            {
                                // Get the component image from the ManagedImageList
                                using(IManagedImage pComponentImage = imageList[componentIdx])
                                {
                                    //
                                    // Check component completion status
                                    //
                                    if (pComponentImage.IsIncomplete)
                                    {
                                        continue;
                                    }

                                    //
                                    // Retrieve component information
                                    //
                                    // *** NOTES ***
                                    // Each component has its own metadata: e.g. dimensions, pixel format,
                                    // payload type, frame ID, and timestamp. This information is useful for
                                    // understanding what type of data each component contains.
                                    //
                                    uint width = pComponentImage.Width;
                                    uint height = pComponentImage.Height;
                                    string imagePayloadTypeName = ImagePayloadTypeToString(pComponentImage);
                                    string pixelFormatName = pComponentImage.PixelFormatName;
                                    ulong frameID = pComponentImage.FrameID;
                                    ulong timestamp = pComponentImage.TimeStamp;

                                    Console.WriteLine("  Component {0}: {1}x{2}", componentIdx, width, height);
                                    Console.WriteLine("    Image Payload Type: {0}", imagePayloadTypeName);
                                    Console.WriteLine("    Pixel Format: {0}", pixelFormatName);
                                    Console.WriteLine("    Frame ID: {0}, Timestamp: {1}", frameID, timestamp);

                                    //
                                    // Convert component image to Mono8 for saving
                                    //
                                    // *** NOTES ***
                                    // Components can be converted between pixel formats just like standard images.
                                    // The converted image does not need to be released as it does not affect the
                                    // camera buffer.
                                    //
                                    using(
                                        IManagedImage convertedImage =
                                            processor.Convert(pComponentImage, PixelFormatEnums.Mono8))
                                    {
                                        //
                                        // Create filename with component index
                                        //
                                        // *** NOTES ***
                                        // We include the component index in the filename to distinguish between
                                        // different components from the same GenDC container. This allows you to
                                        // see which files correspond to which data streams.
                                        //
                                        String filename = "AcquisitionGenDC-CSharp-";
                                        if (deviceSerialNumber != "")
                                        {
                                            filename = filename + deviceSerialNumber + "-";
                                        }
                                        filename = filename + "Frame" + imageCnt + "-Component" + componentIdx + ".jpg";

                                        // Save the component image
                                        convertedImage.Save(filename);
                                        Console.WriteLine("    Saved: {0}", filename);
                                    }
                                }
                            }
                            catch (SpinnakerException ex)
                            {
                                Console.WriteLine("  Error processing component {0}: {1}\n", componentIdx, ex.Message);
                            }
                        }
                        Console.WriteLine();
                    }
                    catch (SpinnakerException ex)
                    {
                        Console.WriteLine("Error: {0}\n", ex.Message);
                        result = -1;
                    }
                }

                //
                // End acquisition
                //
                // *** NOTES ***
                // Ending acquisition appropriately helps ensure that devices
                // clean up properly and do not need to be power-cycled to
                // maintain integrity.
                //
                cam.EndAcquisition();
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
        static int PrintDeviceInfo(INodeMap nodeMap)
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

                // Retrieve GenICam nodemap
                INodeMap nodeMap = cam.GetNodeMap();

                // Configure GenDC components - exit if not supported
                result = ConfigureGenDC(cam, nodeMap);
                if (result != 0)
                {
                    return result;
                }

                // Configure heartbeat for GEV camera
#if DEBUG
                result = result | DisableGVCPHeartbeat(cam);
#else
                result = result | ResetGVCPHeartbeat(cam);
#endif
                // Acquire images
                result = result | AcquireImages(cam, nodeMap, nodeMapTLDevice);

#if DEBUG
                // Reset heartbeat for GEV camera
                result = result | ResetGVCPHeartbeat(cam);
#endif

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

            // Print GenDC component configuration status
            Console.WriteLine("*** GENDC COMPONENT CONFIGURATION ***");
            Console.WriteLine("Component 0 (Primary): {0}", enableComponent0 ? "ENABLED" : "DISABLED");
            Console.WriteLine("Component 1 (Secondary): {0}", enableComponent1 ? "ENABLED" : "DISABLED");
            Console.WriteLine();
            Console.WriteLine("NOTE: To change component settings, modify the flags at the top of the source file:");
            Console.WriteLine("  - enableComponent0: Enable/disable primary component");
            Console.WriteLine("  - enableComponent1: Enable/disable secondary component");
            Console.WriteLine();
            Console.WriteLine("IMPORTANT: This example requires a camera with GenDC support.");
            Console.WriteLine("           For standard single-image acquisition, use the Acquisition_CSharp example.");
            Console.WriteLine();

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

            foreach(IManagedCamera managedCamera in camList) using(managedCamera)
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
