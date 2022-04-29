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
*  @example AcquisitionOpenCV.cs
*
*  @brief AcquisitionOpenCV.cs demonstrates how to convert an image to an
*  OpenCV Mat object and display the result. Tested with Emgu.CV.runtime.windows 
*  version 4.4.0.4061 target framework .net 4.6.1 and spinnkar version 2.0.0.147
*/

using System;
using System.IO;
using SpinnakerNET;
using SpinnakerNET.GenApi;

using Emgu.CV;
using Emgu.CV.CvEnum;
using Emgu.CV.Structure;



namespace AcquisitionOpenCV
{
    class Program
    {
        const string windowName = "Current Image";

        const bool singleChannel = true;

#if DEBUG
        // Disables heartbeat on GEV cameras so debugging does not incur timeout errors
        static int DisableHeartbeat(IManagedCamera cam, INodeMap nodeMap, INodeMap nodeMapTLDevice)
        {
            Console.WriteLine("Checking device type to see if we need to disable the camera's heartbeat...\n\n");

            //
            // Write to boolean node controlling the camera's heartbeat
            //
            // *** NOTES ***
            // This applies only to GEV cameras and only applies when in DEBUG mode.
            // GEV cameras have a heartbeat built in, but when debugging applications the
            // camera may time out due to its heartbeat. Disabling the heartbeat prevents
            // this timeout from occurring, enabling us to continue with any necessary debugging.
            // This procedure does not affect other types of cameras and will prematurely exit
            // if it determines the device in question is not a GEV camera.
            //
            // *** LATER ***
            // Since we only disable the heartbeat on GEV cameras during debug mode, it is better
            // to power cycle the camera after debugging. A power cycle will reset the camera
            // to its default settings.
            //
            IEnum iDeviceType = nodeMapTLDevice.GetNode<IEnum>("DeviceType");
            IEnumEntry iDeviceTypeGEV = iDeviceType.GetEntryByName("GigEVision");
            // We first need to confirm that we're working with a GEV camera
            if (iDeviceType != null && iDeviceType.IsReadable)
            {
                if (iDeviceType.Value == iDeviceTypeGEV.Value)
                {
                    Console.WriteLine(
                        "Working with a GigE camera. Attempting to disable heartbeat before continuing...\n\n");
                    IBool iGEVHeartbeatDisable = nodeMap.GetNode<IBool>("GevGVCPHeartbeatDisable");
                    if (iGEVHeartbeatDisable == null || !iGEVHeartbeatDisable.IsWritable)
                    {
                        Console.WriteLine(
                            "Unable to disable heartbeat on camera. Continuing with execution as this may be non-fatal...");
                    }
                    else
                    {
                        iGEVHeartbeatDisable.Value = true;
                        Console.WriteLine("WARNING: Heartbeat on GigE camera disabled for the rest of Debug Mode.");
                        Console.WriteLine(
                            "         Power cycle camera when done debugging to re-enable the heartbeat...");
                    }
                }
                else
                {
                    Console.WriteLine("Camera does not use GigE interface. Resuming normal execution...\n\n");
                }
            }
            else
            {
                Console.WriteLine("Unable to access TL device nodemap. Aborting...");
                return -1;
            }

            return 0;
        }
#endif
        // This function acquires and saves 10 images from a device.
        static int AcquireImages(IManagedCamera cam, INodeMap nodeMap, INodeMap nodeMapTLDevice)
        {
            int result = 0;

            Console.WriteLine("\n*** IMAGE ACQUISITION ***\n");

            try
            {
                //
                // Set acquisition mode to continuous
                //
                // *** NOTES ***
                // Because the example acquires and saves 10 images, setting
                // acquisition mode to continuous lets the example finish. If
                // set to single frame or multiframe (at a lower number of
                // images), the example would just hang. This is because the
                // example has been written to acquire 10 images while the
                // camera would have been programmed to retrieve less than that.
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
                if (iAcquisitionMode == null || !iAcquisitionMode.IsWritable)
                {
                    Console.WriteLine("Unable to set acquisition mode to continuous (node retrieval). Aborting...\n");
                    return -1;
                }

                // Retrieve entry node from enumeration node
                IEnumEntry iAcquisitionModeContinuous = iAcquisitionMode.GetEntryByName("Continuous");
                if (iAcquisitionModeContinuous == null || !iAcquisitionMode.IsReadable)
                {
                    Console.WriteLine(
                        "Unable to set acquisition mode to continuous (enum entry retrieval). Aborting...\n");
                    return -1;
                }

                // Set symbolic from entry node as new value for enumeration node
                iAcquisitionMode.Value = iAcquisitionModeContinuous.Symbolic;

                Console.WriteLine("Acquisition mode set to continuous...");

#if DEBUG
                Console.WriteLine("\n\n*** DEBUG ***\n\n");
                // If using a GEV camera and debugging, should disable heartbeat first to prevent further issues

                if (DisableHeartbeat(cam, nodeMap, nodeMapTLDevice) != 0)
                {
                    return -1;
                }

                Console.WriteLine("\n\n*** END OF DEBUG ***\n\n");
#endif
                //
                // Begin acquiring images
                //
                // *** NOTES ***
                // What happens when the camera begins acquiring images depends
                // on which acquisition mode has been set. Single frame captures
                // only a single image, multi frame catures a set number of
                // images, and continuous captures a continuous stream of images.
                // Because the example calls for the retrieval of 10 images,
                // continuous mode has been set for the example.
                //
                // *** LATER ***
                // Image acquisition must be ended when no more images are needed.
                //
                cam.BeginAcquisition();

                Console.WriteLine("Acquiring images...");

                //
                // Retrieve device serial number for filename
                //
                // *** NOTES ***
                // The device serial number is retrieved in order to keep
                // different cameras from overwriting each other's images.
                // Grabbing image IDs and frame IDs make good alternatives for
                // this purpose.
                //
                String deviceSerialNumber = "";

                IString iDeviceSerialNumber = nodeMapTLDevice.GetNode<IString>("DeviceSerialNumber");
                if (iDeviceSerialNumber != null && iDeviceSerialNumber.IsReadable)
                {
                    deviceSerialNumber = iDeviceSerialNumber.Value;

                    Console.WriteLine("Device serial number retrieved as {0}...", deviceSerialNumber);
                }
                Console.WriteLine();

                // Create a highgui window to display incomming images
                CvInvoke.NamedWindow(windowName);

                // Retrieve, convert, and save images
                const int NumImages = 10;
                Console.WriteLine("current directly: {0}", System.IO.Directory.GetCurrentDirectory());
                for (int imageCnt = 0; imageCnt < NumImages; imageCnt++)
                {
                    try
                    {


                        //
                        // Retrieve next received image
                        //
                        // *** NOTES ***
                        // Capturing an image houses images on the camera buffer.
                        // Trying to capture an image that does not exist will
                        // hang the camera.
                        //
                        // Using-statements help ensure that images are released.
                        // If too many images remain unreleased, the buffer will
                        // fill, causing the camera to hang. Images can also be
                        // released manually by calling Release().
                        //
                        using(IManagedImage rawImage = cam.GetNextImage(1000))
                        {
                            //
                            // Ensure image completion
                            //
                            // *** NOTES ***
                            // Images can easily be checked for completion. This
                            // should be done whenever a complete image is
                            // expected or required. Alternatively, check image
                            // status for a little more insight into what
                            // happened.
                            //
                            if (rawImage.IsIncomplete)
                            {
                                Console.WriteLine("Image incomplete with image status {0}...", rawImage.ImageStatus);
                            }
                            else
                            {
                                //
                                // Print image information; width and height
                                // recorded in pixels
                                //
                                // *** NOTES ***
                                // Images have quite a bit of available metadata
                                // including CRC, image status, and offset
                                // values to name a few.
                                //
                                uint width = rawImage.Width;

                                uint height = rawImage.Height;

                                Console.WriteLine(
                                    "Grabbed image {0}, width = {1}, height = {2}", imageCnt, width, height);

                                // Convert the Image to OpenCV Mat and display the result in a window.
                                // Note that if the camera is streaming in BayerRG8 format you will need to debayer the image to
                                // convert it into a 3-channel color image. You can do this before passing the spinnaker image data
                                // to the Mat object, using rawImage.Convert(PixelFormat_RGB8, HQ_LINEAR) or you can
                                // use emgu to debayer the image after the BayerRG8 image data is passed to the Mat object.

                                int rows = (int)rawImage.Height;

                                int cols = (int)rawImage.Width;

                                int num_channels = singleChannel == true ? 1 : 3;

                                IntPtr image_data = rawImage.DataPtr;

                                int stride = (int)rawImage.Stride;                           

                                Mat current_frame = new Mat(rows,cols, DepthType.Cv8U, num_channels, image_data, stride);

                                // Resize to a quarter resolution for image display
                                Mat display_frame = new Mat();

                                CvInvoke.Resize(current_frame, display_frame,new System.Drawing.Size(), 0.5, 0.5);

                                CvInvoke.Imshow(windowName, display_frame);

                                // Call waitkey function otherwise the image will not display...
                                CvInvoke.WaitKey(1);


                                // Create a unique filename

                                String filename = "Acquisition-CSharp-";
                                if (deviceSerialNumber != "")
                                {
                                    filename = filename + deviceSerialNumber + "-";
                                }
                                filename = filename + imageCnt + ".jpg";

                                //
                                // Save image
                                //
                                // *** NOTES ***
                                // The standard practice of the examples is
                                // to use device serial numbers to keep
                                // images of one device from overwriting
                                // those of another.
                                //

                                CvInvoke.Imwrite(filename, current_frame);

                                Console.WriteLine("Image saved at {0}\n", filename);

                            }
                        }
                    }
                    catch (SpinnakerException ex)
                    {
                        Console.WriteLine("Error: {0}", ex.Message);
                        result = -1;
                    }
                }

               CvInvoke.DestroyWindow(windowName); // destroy the window
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

                // Acquire images
                result = result | AcquireImages(cam, nodeMap, nodeMapTLDevice);

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
