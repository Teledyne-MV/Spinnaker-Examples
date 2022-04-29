//=============================================================================
// Copyright © 2019 FLIR Integrated Imaging Solutions, Inc. All Rights Reserved.
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
*	@example SavetoUserSet.cs
*
*	@brief SavetoUserSet.cpp shows how to save custom settings to User Set. By default,
*   it modifies the exposure time to 2000 microseconds, then saves this change to User Set 1.
*   It can also be configured to reset the camera to factory default settings.
*/

// Uncomment to allow the camera to restore factory default
//#define RESTORE_FACTORY_DEFAULT

using System;
using System.Collections.Generic;
using System.Linq;
using SpinnakerNET;
using SpinnakerNET.GenApi;
using System.IO;

namespace SaveToUserSet
{
    class Program
    {

        // This function acquires and saves 10 images from a device.
        static int SaveCustomSettings(IManagedCamera cam, INodeMap nodeMap, INodeMap nodeMapTLDevice)
        {
            int result = 0;

            try
            {
                // Get the User Set Selector node
                IEnum iUserSetSelector = nodeMap.GetNode<IEnum>("UserSetSelector");
                if (!iUserSetSelector.IsAvailable || !iUserSetSelector.IsWritable)
                {
                    Console.WriteLine("Unable to access User Set (enum retrieval). Aborting...\n");
                    return -1;
                }

                // Get the User Set Default node
                IEnum iUserSetDefault = nodeMap.GetNode<IEnum>("UserSetDefault");
                if (!iUserSetDefault.IsAvailable || !iUserSetDefault.IsWritable)
                {
                    Console.WriteLine("Unable to access User Set Default (enum retrieval). Aborting...\n");
                    return -1;
                }

#if RESTORE_FACTORY_DEFAULT
                Console.WriteLine("\n*** Restoring Factory Default Settings ***\n");


                // Set User Set to Default
                IEnumEntry iDefault = iUserSetSelector.GetEntryByName("Default");
                if(!iDefault.IsAvailable || !iDefault.IsReadable)
                {
                    Console.WriteLine("Unable to access User Set (entry retrieval). Aborting...\n");
                    return -1;
                }

                iUserSetSelector.Value = iDefault.Value;

                // Load Default User Set to camera
                ICommand iUserSetSetLoad = cam.GetNodeMap().GetNode<ICommand>("UserSetLoad");
                if(!iUserSetSetLoad.IsAvailable || !iUserSetSetLoad.IsReadable)
                {
                    Console.WriteLine("Unable to access User Set load command. Aborting...\n");
                    return -1;
                }

                iUserSetSetLoad.Execute();

                Console.WriteLine("Camera set to default settings\n");

                // Set Default User Set to Default
                iUserSetDefault.Value = iDefault.Value;

                Console.WriteLine("Default User Set now set to Default; camera will start up with default settings\n");
#else
                Console.WriteLine("\n*** Saving Custom Settings ***\n");

                // Turn off auto exposure
                IEnum iExposureAuto = cam.GetNodeMap().GetNode<IEnum>("ExposureAuto");
                if (iExposureAuto == null || !iExposureAuto.IsWritable)
                {
                    Console.WriteLine("Unable to disable automatic exposure (enum retrieval). Aborting...\n");
                    return -1;
                }

                IEnumEntry iExposureAutoOff = iExposureAuto.GetEntryByName("Off");
                if (iExposureAutoOff == null || !iExposureAutoOff.IsReadable)
                {
                    Console.WriteLine("Unable to disable automatic exposure (entry retrieval). Aborting...\n");
                    return -1;
                }


                iExposureAuto.Value = iExposureAutoOff.Value;

                Console.WriteLine("Automatic exposure disabled...");

                // Set exposure time to 2000 microseconds
                const double exposureTimeToSet = 2000.0;

                IFloat iExposureTime = cam.GetNodeMap().GetNode<IFloat>("ExposureTime");
                if (iExposureTime == null || !iExposureTime.IsWritable)
                {
                    Console.WriteLine("Unable to set exposure time. Aborting...\n");
                    return -1;
                }

                // Ensure desired exposure time does not exceed the maximum
                iExposureTime.Value = (exposureTimeToSet > iExposureTime.Max ? iExposureTime.Max : exposureTimeToSet);

                Console.WriteLine("Exposure time set to {0} us...\n", iExposureTime.Value);

                //Change User Set to 1
                IEnumEntry iUserSet1 = iUserSetSelector.GetEntryByName("UserSet1");
                if (!iUserSet1.IsAvailable || !iUserSet1.IsReadable)
                {
                    Console.WriteLine("Unable to access User Set (entry retrieval). Aboring...\n");
                    return -1;
                }

                iUserSetSelector.Value = iUserSet1.Value;

                // Save custom settings to User Set 1
                ICommand iUserSetSave = cam.GetNodeMap().GetNode<ICommand>("UserSetSave");
                if (!iUserSetSave.IsAvailable || !iUserSetSave.IsWritable)
                {
                    Console.WriteLine("Unable to save to User Set. Aborting...\n");
                    return -1;
                }

                iUserSetSave.Execute();

                Console.WriteLine("Saved Custom Settings as {0}\n", iUserSetSelector.Symbolics);

                // Change default User Set to User Set 1
                iUserSetDefault.Value = iUserSet1.Value;

                Console.WriteLine("Default User Set now set to UserSet1; camera will start up with custom settings \n");
#endif
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
                result = result | SaveCustomSettings(cam, nodeMap, nodeMapTLDevice);

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
