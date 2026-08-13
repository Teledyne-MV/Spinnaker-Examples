//=============================================================================
// Copyright (c) 2025 FLIR Integrated Imaging Solutions, Inc. All Rights Reserved.
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
 *  @example ActionCommand_Counters.cpp
 *
 *  @brief ActionCommand_Counters.cpp shows how to use Camera Generated action
 *  commands to synchronize image capture between 2 or more GigE cameras,
 *  using Counters to create a very precise framerate; this example measures and
 *  output both the accuracy of the synchronization, as well as the framerates
 *  of two of the connected cameras. It relies on information provided in the
 *  CounterAndTimer example, as well as the ChunkData example.
 *
 *  This example touches on the preparation and cleanup of a camera just before
 *  and just after the acquisition of images. Image Capture/retrieval, ChunkData
 *  enable/retrieval is all covered here.
 *
 *  Please leave us feedback at: https://www.surveymonkey.com/r/TDYMVAPI
 *  More source code examples at: https://github.com/Teledyne-MV/Spinnaker-Examples
 *  Need help? Check out our forum at: https://teledynevisionsolutions.zendesk.com/hc/en-us/community/topics
 */


#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

float framerate = 2.0f; // Framerate each camera runs at
const int k_numImages = 60; // Number of sets of images the cameras will capture
const int maxbandwidth = 60; // Max bandwidth per camera (MB/s). Megapixels × Framerate = bandwidth; combined total must stay ≤1240 MB/s on 10GigE or ≤120 MB/s on 1GigE.


// This function prints the device information of the camera from the transport
// layer; please see NodeMapInfo example for more in-depth comments on printing
// device information from the nodemap.
int PrintDeviceInfo(INodeMap& nodeMap)
{
    int result = 0;

    cout << endl << "*** DEVICE INFORMATION ***" << endl << endl;

    try
    {
        FeatureList_t features;
        CCategoryPtr category = nodeMap.GetNode("DeviceInformation");
        if (IsReadable(category))
        {
            category->GetFeatures(features);

            FeatureList_t::const_iterator it;
            for (it = features.begin(); it != features.end(); ++it)
            {
                CNodePtr pfeatureNode = *it;
                cout << pfeatureNode->GetName() << " : ";
                CValuePtr pValue = static_cast<CValuePtr>(pfeatureNode);
                cout << (IsReadable(pValue) ? pValue->ToString() : "Node not readable");
                cout << endl;
            }
        }
        else
        {
            cout << "Device control information not readable." << endl;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

// This function configure each of the cameras including the acquisition mode
bool ConfigureCameras(CameraList& camList, unsigned int numCameras)
{
    bool result = true;

    cout << endl << endl << "*** CONFIGURING CAMERAS... ***" << endl << endl;

    try
    {
        for (unsigned int cameraCnt = 0; cameraCnt < numCameras; cameraCnt++)
        {
            // Get the camera node Map
            INodeMap& nodeMap = camList.GetByIndex(cameraCnt)->GetNodeMap();

            CameraPtr cam = camList.GetByIndex(cameraCnt);

            // Disable auto exposure
            cam->ExposureAuto.SetValue(ExposureAuto_Off);

            // Query the minimum exposure time available
            double minExp = cam->ExposureTime.GetMin();

            // Set exposure time to minimum
            cam->ExposureTime.SetValue(minExp);

            cout << "Camera " << cameraCnt << " exposure set to " << minExp << " microseconds" << endl;

            // Initialize Counter Duration values
            int Counter1Duration = 0;
            int Counter0Duration = 0;

            // We use the megahertz tick as the counter source (operates at 1,000,000 ticks per second).
            // 1,0000,000 divided by framerate determines the Counter duration value
            // When the tick counts to the Counter duration value, the counter end event occurs
            // When Counter0end occurs, a frame is captured; this repeats at the desired framerate.
            Counter0Duration = static_cast<int>(1000000 / framerate);

            // All Counter functionality will be handled by the first camera in index
            if (cameraCnt == 0)
            {
                // For slower framerates (less than 15.26), two counters are needed, due to each counter having a max limit of 65520
                // The following logic is used to determine optimal values for both counters, to get the exact requested framerate              
                if (Counter0Duration > 65520)
                {
                    int best_remainder = Counter0Duration;
                    for (int d = 2; d <= 65520; ++d)
                    {
                        if (Counter0Duration / d > 65520)
                        {
                            continue;
                        }
                        int remainder = Counter0Duration % d;

                        if (remainder < best_remainder)
                        {
                            best_remainder = remainder;
                            Counter1Duration = d;
                            if (best_remainder == 0) // perfect divisor found
                            {
                                break;
                            }
                        }
                    }

                    Counter0Duration = Counter0Duration / Counter1Duration;

                    // Use Counter 1 as our second counter
                    cam->CounterSelector.SetValue(CounterSelector_Counter1);

                    // Have the end of Counter 0 be set as our counter source (this occurs every 65 milliseconds) 
                    CEnumerationPtr ptrCounterEventSource = cam->GetNodeMap().GetNode("CounterEventSource");
                    CEnumEntryPtr ptrCounterEventSourceCounter0End = ptrCounterEventSource->GetEntryByName("Counter0End");
                    ptrCounterEventSource->SetIntValue(ptrCounterEventSourceCounter0End->GetValue());

                    // Set the duration of the second counter (counter 1) to calculated value to maintain desired framerate
                    cam->CounterEventActivation.SetValue(CounterEventActivation_RisingEdge);
                    cam->CounterDuration.SetValue(Counter1Duration);

                    // Set UserOutput0 to be the Trigger Source
                    CEnumerationPtr ptrCounterTriggerSource = cam->GetNodeMap().GetNode("CounterTriggerSource");
                    CEnumEntryPtr ptrCounterTriggerSourcUserOutput1 = ptrCounterTriggerSource->GetEntryByName("UserOutput0");
                    ptrCounterTriggerSource->SetIntValue(ptrCounterTriggerSourcUserOutput1->GetValue());

                    // Start Counter when Trigger Source is high
                    cam->CounterTriggerActivation.SetValue(CounterTriggerActivation_LevelHigh);

                }

                // Use Counter0 as our primary Counter
                cam->CounterSelector.SetValue(CounterSelector_Counter0);

                // Set our primary counter source to Megahertz Tick (ticks every microsecond, up to a maximum of 65520)
                CEnumerationPtr ptrCounterEventSource = cam->GetNodeMap().GetNode("CounterEventSource");
                CEnumEntryPtr ptrCounterEventSourceMHzTick = ptrCounterEventSource->GetEntryByName("MHzTick");
                ptrCounterEventSource->SetIntValue(ptrCounterEventSourceMHzTick->GetValue());

                // Set duration to calculated value, to set primary counter to end whenever a frame is needed (desired framerate)
                cam->CounterDuration.SetValue(Counter0Duration);

                // Set UserOutput0 to be the Trigger Source
                CEnumerationPtr ptrCounterTriggerSource = cam->GetNodeMap().GetNode("CounterTriggerSource");
                CEnumEntryPtr ptrCounterTriggerSourceUserOutput1 = ptrCounterTriggerSource->GetEntryByName("UserOutput0");
                ptrCounterTriggerSource->SetIntValue(ptrCounterTriggerSourceUserOutput1->GetValue());

                // Start Counter when Trigger Source is high
                cam->CounterTriggerActivation.SetValue(CounterTriggerActivation_LevelHigh);

                // Make sure that UserOutput0 is set to low before we start acquiring images
                CEnumerationPtr ptrUserOutputSelector = cam->GetNodeMap().GetNode("UserOutputSelector");
                CEnumEntryPtr ptrUserOutputSelectorUserOutput0 = ptrUserOutputSelector->GetEntryByName("UserOutput0");
                ptrUserOutputSelector->SetIntValue(ptrUserOutputSelectorUserOutput0->GetValue());

                CBooleanPtr ptrUserOutputValue = cam->GetNodeMap().GetNode("UserOutputValue");
                ptrUserOutputValue->SetValue(false);
            }

            // Enable Timestamp and Frame ID on each camera
            cam->ChunkModeActive.SetValue(true);
            cam->ChunkSelector.SetValue(ChunkSelector_Timestamp);
            cam->ChunkEnable.SetValue(true);
            cam->ChunkSelector.SetValue(ChunkSelector_FrameID);
            cam->ChunkEnable.SetValue(true);

            cout << "Camera " << cameraCnt << " Chunk Data Timestamp and Frame ID enabled " << endl;

            // Set bandwidth limit of each camera (in bytes)
            cam->DeviceLinkThroughputLimit.SetValue(maxbandwidth * 1000000);

            cout << "Camera " << cameraCnt << " throughputlimit set to " << maxbandwidth << " MB / s" << endl;

            // Enable IEEE1588/PTP on both cameras
            if (cameraCnt == 0)
            {
                cam->GevIEEE1588Mode.SetValue(GevIEEE1588Mode_Auto);
                cam->GevIEEE1588.SetValue(true);
            }
            else
            {
                cam->GevIEEE1588Mode.SetValue(GevIEEE1588Mode_SlaveOnly);
                cam->GevIEEE1588.SetValue(true);
            }

            // Set Camera 0 (primary camera) to generate Action Commands
            if (cameraCnt == 0)
            {
                CEnumerationPtr ptrActionSelector = cam->GetNodeMap().GetNode("ActionSelector");
                CEnumEntryPtr ptrActionSelectorActionGenerator = ptrActionSelector->GetEntryByName("ActionGenerator");
                ptrActionSelector->SetIntValue(ptrActionSelectorActionGenerator->GetValue());

                CIntegerPtr ptrActionGroupMask = cam->GetNodeMap().GetNode("ActionGroupMask");
                ptrActionGroupMask->SetValue(1);

                CIntegerPtr ptrActionGroupKey = cam->GetNodeMap().GetNode("ActionGroupKey");
                ptrActionGroupKey->SetValue(0);

                // Set Counter 0 End event as source of Action Command
                CEnumerationPtr ptrActionCMDSource = cam->GetNodeMap().GetNode("ActionCMDSource");
                CEnumEntryPtr ptrActionCMDSourceCounterEnd = ptrActionCMDSource->GetEntryByName("Counter0End");

                // If the framerate is slow enough to require using two counters to accomodate, change 
                // Action Command Source to Counter 1 End event rather than Counter 0 End event
                if ((1000000 / framerate) > 65520)
                {
                    ptrActionCMDSourceCounterEnd = ptrActionCMDSource->GetEntryByName("Counter1End");
                }

                ptrActionCMDSource->SetIntValue(ptrActionCMDSourceCounterEnd->GetValue());

                CEnumerationPtr ptrActionCMDActivation = cam->GetNodeMap().GetNode("ActionCMDActivation");
                CEnumEntryPtr ptrActionCMDActivationRisingEdge = ptrActionCMDActivation->GetEntryByName("RisingEdge");
                ptrActionCMDActivation->SetIntValue(ptrActionCMDActivationRisingEdge->GetValue());
            }

            // Set up all other cameras to receive Action Commands (applies only to cameras on the same network/bus as primary camera)
            else
            {
                CEnumerationPtr ptrActionSelector = cam->GetNodeMap().GetNode("ActionSelector");
                CEnumEntryPtr ptrActionSelectorAction0 = ptrActionSelector->GetEntryByName("Action0");
                ptrActionSelector->SetIntValue(ptrActionSelectorAction0->GetValue());

                CIntegerPtr ptrActionGroupMask = cam->GetNodeMap().GetNode("ActionGroupMask");
                ptrActionGroupMask->SetValue(1);

                CIntegerPtr ptrActionGroupKey = cam->GetNodeMap().GetNode("ActionGroupKey");
                ptrActionGroupKey->SetValue(0);
            }

            cout << "Camera " << cameraCnt << " Camera Generated Action Command settings set " << endl;

            // Set cameras to enable trigger mode, set trigger source as action signal
            cam->TriggerMode.SetValue(TriggerMode_Off);
            cam->TriggerSelector.SetValue(TriggerSelector_FrameStart);
            cam->TriggerSource.SetValue(TriggerSource_Action0);
            cam->TriggerOverlap.SetValue(TriggerOverlap_ReadOut);
            cam->TriggerMode.SetValue(TriggerMode_On);

            cout << "Camera " << cameraCnt << " Trigger Settings set " << endl;
        }

        cout << "Wait for 10 seconds to enable ptp on all cameras" << endl;
        std::this_thread::sleep_for(std::chrono::seconds(10)); //need time to properly enable ptp/matching timestamps.  
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = false;
    }

    return result;
}

bool AcquireImages(CameraList& camList, unsigned int numCameras)
{
    bool result = true;

    uint64_t missedImageCnts = 0;

    cout << endl << endl << "*** ACQUIRING SYNCHRONIZED IMAGES ***" << endl << endl;

    try
    {
        // Set all cameras to be ready to acquire images
        for (unsigned int cameraCnt = 0; cameraCnt < numCameras; cameraCnt++)
        {
            camList.GetByIndex(cameraCnt)->BeginAcquisition();
            cout << "Camera[" << cameraCnt << "]: Started acquiring images" << endl;
        }
       

        //Initializing variables to track image‑set accuracy; how closely each image aligns with its paired image from the other camera.
        // Accuracy measured via timestamp values
        // Only the first two cameras (based on index) will be measured, to simplify calculations
        int64_t prevDifference = -1;  // sentinel value
        int64_t minDifference = 10000000000; // Best measured accuracy; initialized to a large default value so real measurement data can replace it.
        int64_t maxDifference = 0; // Worst accuracy measured
        double totalDelta1 = 0.0; // Total 
        double totalDelta2 = 0.0;
        int frameCount1 = 0;
        int frameCount2 = 0;
        int64_t prevTS1 = -1; // Previous Timestamp for first camera
        int64_t prevTS2 = -1; // Previous Timestamp for second camera
        int64_t TS1 = -1; // Current Timestamp for first camera
        int64_t FID1 = -1; // Frame ID for first camera
        int64_t TS2 = -1; // Current Timestamp for second camera
        int64_t FID2 = -1; // Frame ID for second camera
        double totalDifference = 0.0; // The combined total of Difference between all image sets
        int DifferenceCount = 0; // Assuming no skipped frames, the number of images each camera captures

        // Set camera 0's (primary camera) User0utput0 value to high/true, starting the counter source/synchronized image capture
        CBooleanPtr ptrUserOutputValue = camList.GetByIndex(0)->GetNodeMap().GetNode("UserOutputValue");
        ptrUserOutputValue->SetValue(true);

        for (unsigned int imageCnt = 0; imageCnt < k_numImages; imageCnt++)
        {
            // Loop through each of the cameras
            for (unsigned int cameraCnt = 0; cameraCnt < numCameras; cameraCnt++)
            {
                try
                {
                    ImagePtr pResultImage = camList.GetByIndex(cameraCnt)->GetNextImage();
                    cout << "Image retrieved from camera " << cameraCnt << "." << endl;

                    if (pResultImage->IsIncomplete())
                    {
                        cout << "Incomplete image detected." << endl;
                        continue;
                    }

                    // Retrieve and display Chunk Timestamp/FrameID for primary camera 
                    if (cameraCnt == 0)
                    {
                        TS1 = pResultImage->GetChunkData().GetTimestamp();
                        FID1 = pResultImage->GetChunkData().GetFrameID();
                        cout << "  Camera 1 Timestamp: " << TS1 << "\n";
                        cout << "  Camera 1 Frame ID: " << FID1 << "\n";

                        if (prevTS1 != -1)
                        {
                            // Used to calculate accuracy of primary camera framerate
                            double delta1 = (TS1 - prevTS1) / 1e9; // seconds
                            totalDelta1 += delta1;
                            frameCount1++;
                        }
                        
                        // Current Timestamp is set as previous timestamp for next capture loop for primary
                        prevTS1 = TS1;
                    }

                    // Retrieve and display Chunk Timestamp/FrameID for secondary camera 
                    if (cameraCnt == 1)
                    {
                        TS2 = pResultImage->GetChunkData().GetTimestamp();
                        FID2 = pResultImage->GetChunkData().GetFrameID();
                        cout << "  Camera 2 Timestamp: " << TS2 << "\n";
                        cout << "  Camera 2 Frame ID: " << FID2 << "\n";

                        // Determine the difference between Timestamps for current set of images from primary and secondary camera
                        int64_t Difference = abs(TS2 - TS1);

                        // Update overall minimum and maximum Difference values (best and worse accuracy) should previous values be exceeded
                        if (Difference < minDifference) minDifference = Difference;
                        if (Difference > maxDifference) maxDifference = Difference;

                        // accumulate for average
                        totalDifference += Difference;
                        DifferenceCount++;
                        cout << "  Timestamp Difference: " << fixed << setprecision(0) << Difference << " nanoseconds" << endl;

                        if (prevTS2 != -1)
                        {
                            // Used to calculate accuracy of secondary camera framerate
                            double delta2 = (TS2 - prevTS2) / 1e9; // seconds
                            totalDelta2 += delta2;
                            frameCount2++;
                        }
                        
                        // Current Timestamp is set as previous timestamp for next capture loop for secondary camera
                        prevTS2 = TS2;
                    }



                    pResultImage->Release();
                }
                catch (Spinnaker::Exception& e)
                {
                    cout << "Error: " << e.what() << endl;
                    result = false;
                }
            }

        }
        // End acquisition for all cameras
        for (unsigned int cameraCnt = 0; cameraCnt < numCameras; cameraCnt++)
        {
            // Check for any dropped images
            CIntegerPtr pDroppedImages =
                camList.GetByIndex(cameraCnt)->GetTLStreamNodeMap().GetNode("StreamDroppedFrameCount");
            if (IsReadable(pDroppedImages))
            {
                if (pDroppedImages->GetValue() > 0)
                {
                    missedImageCnts += pDroppedImages->GetValue();
                    cout << pDroppedImages->GetValue() << " images "
                        << " missed at camera " << cameraCnt << endl;
                }
            }
            else
            {
                cout << "Unable to determine the dropped frame count from the nodemap at camera " << cameraCnt << endl
                    << endl;
            }
            camList.GetByIndex(cameraCnt)->EndAcquisition();
            cout << "Camera[" << cameraCnt << "]: Stop acquiring images " << endl;
        }
        cout << endl;

        cout << "We missed a combined total of " << missedImageCnts << " images from all cameras!" << endl;

        // The average accuracy of this synchronization capture sequence
        double avgDifference = DifferenceCount > 0 ? (totalDifference / DifferenceCount) : 0.0;

        // Outputting accuracy results
        cout << "\nSummary across all sets" << endl;
        cout << "  Smallest Timestamp Difference (best synchronization accuracy value measured): " << minDifference << " ns" << endl;
        cout << "  Largest Timestamp Difference (accurate up to):" << maxDifference << " ns)" << endl;
        cout << "  Average Timestamp Difference: " << fixed << setprecision(0) << avgDifference << " ns" << endl;
        cout << "  Accuracy values were measured over a period of " << k_numImages << " image sets" << endl;
   

        double avgFPS1 = frameCount1 > 0 ? (frameCount1 / totalDelta1) : 0.0;
        double avgFPS2 = frameCount2 > 0 ? (frameCount2 / totalDelta2) : 0.0;

        cout << "\nAverage FPS Summary" << endl;
        cout << fixed << setprecision(8); // 8 decimal places
        cout << "  Camera 1 Average FPS: " << avgFPS1 << endl;
        cout << "  Camera 2 Average FPS: " << avgFPS2 << endl;
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = false;
    }

    return result;
}
int RunCameras(CameraList& camList, unsigned int numCameras)
{
    int result = 0;

    try
    {
        // Retrieve TL device nodemap, print device information, and initialize camera
        for (unsigned int i = 0; i < numCameras; i++)
        {
            cout << endl << "Printing device info for camera " << i << "..." << endl;

            INodeMap& nodeMapTLDevice = camList.GetByIndex(i)->GetTLDeviceNodeMap();

            result = PrintDeviceInfo(nodeMapTLDevice);

            cout << endl << "Initializing camera " << i << "..." << endl;

            camList.GetByIndex(i)->Init();
        }
        // Configure each of the cameras before starting the acquisition
        if (!ConfigureCameras(camList, numCameras))
        {
            return -1;
        }

        // Acquire Imageas from each camera and save to a file
        if (!AcquireImages(camList, numCameras))
        {
            return -1;
        }
        // Deinitialize camera
        for (unsigned int i = 0; i < numCameras; i++)
        {
            cout << endl << "Deinitializing camera " << i << "..." << endl;

            camList.GetByIndex(i)->DeInit();
        }

    }

    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }

    return result;
}

int main(int /*argc*/, char** /*argv*/)
{
    int result = 0;

    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    // Print out current library version
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
        << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << endl
        << endl;

    // Retrieve list of cameras from the system
    CameraList camList = system->GetCameras();
    if (camList.GetSize() < 2)
    {
        cout << "Need at least two cameras." << endl;
        camList.Clear();
        system->ReleaseInstance();

        cout << "Done! Press Enter to exit..." << endl;
        getchar();
        return -1;
    }

    unsigned int numCameras = camList.GetSize();

    cout << "Number of cameras detected: " << numCameras << endl << endl;

    // Run example on all cameras
    result = result | RunCameras(camList, numCameras);

    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return result;
}