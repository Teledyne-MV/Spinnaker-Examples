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
 *  @example AcquisitionGenDC.cpp
 *
 *  @brief AcquisitionGenDC.cpp demonstrates how to acquire multi-component images using
 *  the GenDC (Generic Data Container) payload format. This example is specifically for
 *  cameras that support GenDC.
 *
 *  GenDC is a standardized container format that allows cameras to transmit multiple
 *  data components (images, metadata, etc.) in a single synchronized payload. This is
 *  useful for multi-sensor cameras, stereo cameras, or cameras with multiple data streams
 *  such as intensity + depth.
 *
 *  This example relies on information provided in the Enumeration example. Also check out
 *  the ExceptionHandling and NodeMapInfo examples if you haven't already. For standard
 *  single-image acquisition, see the Acquisition example.
 *
 *  Please leave us feedback at: https://www.surveymonkey.com/r/TDYMVAPI
 *  More source code examples at: https://github.com/Teledyne-MV/Spinnaker-Examples
 *  Need help? Check out our forum at: https://teledynevisionsolutions.zendesk.com/hc/en-us/community/topics
 */

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>
#include <vector>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

// Component selection toggles for GenDC acquisition
// These flags control which components are enabled during GenDC acquisition
bool enableComponent0 = true; // Typically intensity/main image component
bool enableComponent1 = true; // Additional component such as depth or range image (if available)

// Helper function to convert ImagePayloadType enum to string
string ImagePayloadTypeToString(ImagePayloadType payloadType)
{
    switch (payloadType)
    {
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_UNKNOWN:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_UNKNOWN";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_IMAGE:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_IMAGE";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_EXTENDED_CHUNK:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_EXTENDED_CHUNK";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_JPEG:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_JPEG";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_LOSSLESS_COMPRESSED:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_LOSSLESS_COMPRESSED";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_LOSSY_COMPRESSED:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_LOSSY_COMPRESSED";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_JPEG_LOSSLESS_COMPRESSED:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_JPEG_LOSSLESS_COMPRESSED";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR1:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR1";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR2:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR2";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR2:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR2";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_GENDC_GENERIC:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_GENDC_GENERIC";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_RANGE_SENSOR1:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_RANGE_SENSOR1";
    case SPINNAKER_IMAGE_PAYLOAD_TYPE_RANGE_SENSOR2:
        return "SPINNAKER_IMAGE_PAYLOAD_TYPE_RANGE_SENSOR2";
    default:
    {
        ostringstream ss;
        ss << "UNKNOWN (" << static_cast<int>(payloadType) << ")";
        return ss.str();
    }
    }
}

// Disables or enables heartbeat on GEV cameras so debugging does not incur timeout errors
int ConfigureGVCPHeartbeat(CameraPtr pCam, bool enableHeartbeat)
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

    // Retrieve TL device nodemap
    INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

    // Retrieve GenICam nodemap
    INodeMap& nodeMap = pCam->GetNodeMap();

    CEnumerationPtr ptrDeviceType = nodeMapTLDevice.GetNode("DeviceType");
    if (!IsReadable(ptrDeviceType))
    {
        return -1;
    }

    if (ptrDeviceType->GetIntValue() != DeviceType_GigEVision)
    {
        return 0;
    }

    if (enableHeartbeat)
    {
        cout << endl << "Resetting heartbeat..." << endl << endl;
    }
    else
    {
        cout << endl << "Disabling heartbeat..." << endl << endl;
    }

    CBooleanPtr ptrDeviceHeartbeat = nodeMap.GetNode("GevGVCPHeartbeatDisable");
    if (!IsWritable(ptrDeviceHeartbeat))
    {
        cout << "Unable to configure heartbeat. Continuing with execution as this may be non-fatal..." << endl << endl;
    }
    else
    {
        ptrDeviceHeartbeat->SetValue(!enableHeartbeat);

        if (!enableHeartbeat)
        {
            cout << "WARNING: Heartbeat has been disabled for the rest of this example run." << endl;
            cout << "         Heartbeat will be reset upon the completion of this run.  If the " << endl;
            cout << "         example is aborted unexpectedly before the heartbeat is reset, the" << endl;
            cout << "         camera may need to be power cycled to reset the heartbeat." << endl << endl;
        }
        else
        {
            cout << "Heartbeat has been reset." << endl;
        }
    }

    return 0;
}

int ResetGVCPHeartbeat(CameraPtr pCam)
{
    return ConfigureGVCPHeartbeat(pCam, true);
}

int DisableGVCPHeartbeat(CameraPtr pCam)
{
    return ConfigureGVCPHeartbeat(pCam, false);
}

// This function checks if the camera supports GenDC and configures component selection
int ConfigureGenDC(CameraPtr pCam, INodeMap& nodeMap)
{
    int result = 0;

    cout << endl << "*** CONFIGURING GENDC ***" << endl;

    try
    {
        //
        // Check for GenDC support by looking for GenDC-specific nodes
        //
        // *** NOTES ***
        // GenDC cameras expose specific Transport Layer Control nodes for GenDC streaming:
        // GenDCStreamingMode and GenDCStreamingStatus. These nodes are part of the camera's
        // GenICam nodemap (obtained via GetNodeMap()) and are required for multi-component image acquisition.
        //
        cout << endl << "Checking camera GenDC support..." << endl;
        CEnumerationPtr ptrGenDCStreamingMode = nodeMap.GetNode("GenDCStreamingMode");
        CEnumerationPtr ptrGenDCStreamingStatus = nodeMap.GetNode("GenDCStreamingStatus");
        if (!IsReadable(ptrGenDCStreamingMode) || !IsReadable(ptrGenDCStreamingStatus))
        {
            cout << "Device serial number " << pCam->TLDevice.DeviceSerialNumber.GetValue()
                 << " is not a valid GenDC camera. Skipping..." << endl;
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
        if (IsReadable(ptrGenDCStreamingMode))
        {
            CEnumEntryPtr ptrCurrentMode = ptrGenDCStreamingMode->GetCurrentEntry();
            if (IsReadable(ptrCurrentMode))
            {
                cout << "  GenDC Streaming Mode: " << ptrCurrentMode->GetSymbolic() << endl;
                if (ptrCurrentMode->GetSymbolic() == "Off")
                {
                    cout << "  GenDC streaming is currently off. Setting GenDC Streaming Mode to 'On'..." << endl;
                    if (!IsWritable(ptrGenDCStreamingMode))
                    {
                        cout << "Unable to set GenDC Streaming Mode to 'On'. GenDCStreamingMode is not Writable. "
                                "Skipping..."
                             << endl;
                        return -1;
                    }
                    CEnumEntryPtr ptrOnMode = ptrGenDCStreamingMode->GetEntryByName("On");
                    if (ptrOnMode != nullptr && IsReadable(ptrOnMode))
                    {
                        ptrGenDCStreamingMode->SetIntValue(ptrOnMode->GetValue());
                        CEnumEntryPtr ptrUpdatedMode = ptrGenDCStreamingMode->GetCurrentEntry();
                        cout << "  GenDC Streaming Mode now set to : " << ptrUpdatedMode->GetSymbolic() << endl;
                    }
                    else
                    {
                        cout << "Unable to set GenDC Streaming Mode to 'On' (entry retrieval). Skipping..." << endl;
                        return -1;
                    }
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
        if (IsReadable(ptrGenDCStreamingStatus))
        {
            CEnumEntryPtr ptrCurrentStatus = ptrGenDCStreamingStatus->GetCurrentEntry();
            if (IsReadable(ptrCurrentStatus))
            {
                cout << "  GenDC Streaming Status: " << ptrCurrentStatus->GetSymbolic() << endl;

                if (ptrCurrentStatus->GetSymbolic() != "On")
                {
                    cout << "  GenDC Streaming Status is not On. Skipping..." << endl;
                    return -1;
                }
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
        CEnumerationPtr ptrComponentSelector = nodeMap.GetNode("ComponentSelector");
        CBooleanPtr ptrComponentEnable = nodeMap.GetNode("ComponentEnable");

        if (IsReadable(ptrComponentSelector) && IsReadable(ptrComponentEnable))
        {
            cout << endl << "Configuring GenDC components..." << endl;

            NodeList_t componentEntries;
            ptrComponentSelector->GetEntries(componentEntries);

            if (componentEntries.size() == 0)
            {
                cout << "Warning: No components available for selection." << endl;
            }
            else
            {
                cout << "  Available components: " << componentEntries.size() << endl;

                for (size_t i = 0; i < componentEntries.size(); i++)
                {
                    CEnumEntryPtr ptrComponent = componentEntries[i];
                    if (IsReadable(ptrComponent))
                    {
                        // Select the component
                        ptrComponentSelector->SetIntValue(ptrComponent->GetValue());

                        gcstring componentName = ptrComponent->GetSymbolic();
                        cout << "    Component " << i << ": " << componentName << " - ";

                        // Enable/disable component based on toggle
                        if (IsWritable(ptrComponentEnable))
                        {
                            bool enableComponent = true;

                            // This example only demonstrates toggling two components;
                            // for cameras with more components, extend this logic as needed.
                            enableComponent = (i == 0) ? enableComponent0 : enableComponent;
                            enableComponent = (i == 1) ? enableComponent1 : enableComponent;

                            ptrComponentEnable->SetValue(enableComponent);
                            cout << (enableComponent ? "Enabled" : "Disabled") << endl;
                        }
                        else
                        {
                            cout << "ComponentEnable not writable for this component" << endl;
                        }
                    }
                }
            }
        }
        else
        {
            cout << endl << "ComponentSelector/ComponentEnable not available." << endl;
            cout << "Using default component configuration." << endl;
        }

        cout << endl << "GenDC configuration complete." << endl;
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "ERROR: Exception during GenDC configuration: " << e.what() << endl;
        result = -1;
    }

    return result;
}

// This function acquires and saves 10 GenDC image sets from a device.
int AcquireImages(CameraPtr pCam, INodeMap& nodeMap, INodeMap& nodeMapTLDevice)
{
    int result = 0;

    cout << endl << endl << "*** GENDC IMAGE ACQUISITION ***" << endl << endl;

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
        if (!IsReadable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
        {
            cout << "Unable to set acquisition mode to continuous (enum retrieval). Aborting..." << endl << endl;
            return -1;
        }

        // Retrieve entry node from enumeration node
        CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
        if (!IsReadable(ptrAcquisitionModeContinuous))
        {
            cout << "Unable to get or set acquisition mode to continuous (entry retrieval). Aborting..." << endl
                 << endl;
            return -1;
        }

        // Retrieve integer value from entry node
        const int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();

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
        // retrieval of 10 image sets, continuous mode has been set.
        //
        // *** LATER ***
        // Image acquisition must be ended when no more images are needed.
        //
        pCam->BeginAcquisition();

        cout << "Acquiring GenDC image sets..." << endl;

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
        if (IsReadable(ptrStringSerial))
        {
            deviceSerialNumber = ptrStringSerial->GetValue();

            cout << "Device serial number retrieved as " << deviceSerialNumber << "..." << endl;
        }
        cout << endl;

        // Retrieve, convert, and save images
        const unsigned int k_numImageSets = 10;

        //
        // Create ImageProcessor instance for post processing images
        //
        ImageProcessor processor;

        //
        // Set default image processor color processing method
        //
        // *** NOTES ***
        // By default, if no specific color processing algorithm is set, the image
        // processor will default to NEAREST_NEIGHBOR method.
        //
        processor.SetColorProcessing(SPINNAKER_COLOR_PROCESSING_ALGORITHM_HQ_LINEAR);

        for (unsigned int imageCnt = 0; imageCnt < k_numImageSets; imageCnt++)
        {
            try
            {
                //
                // Retrieve next synchronized GenDC ImageList
                //
                // *** NOTES ***
                // GetNextImageSync() is specifically designed for multi-stream/multi-component cameras.
                // It returns an ImageList containing synchronized images from all enabled streams/components.
                // Each image in the list represents a different component from the GenDC container.
                //
                // The ImageList object is a container for one or more ImagePtr objects, where each
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
                // Once images from the ImageList are saved and/or no longer needed, the ImageList
                // must be released in order to keep the buffers from filling up.
                //
                ImageList imageList = pCam->GetNextImageSync(1000);

                unsigned int componentCount = imageList.GetSize();

                cout << "Grabbed GenDC ImageList " << imageCnt << ", components: " << componentCount << endl;

                //
                // Validate that we received components
                //
                if (componentCount == 0)
                {
                    cout << "  Warning: Received empty ImageList" << endl;
                    imageList.Release();
                    continue;
                }

                //
                // Process each component in the ImageList
                //
                // *** NOTES ***
                // Each image in the ImageList represents a component from the GenDC container.
                // Components are synchronized by frame ID and can have different dimensions,
                // pixel formats, and payload types.
                //
                // Common component types include:
                // - SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR1: Primary intensity image
                // - SPINNAKER_IMAGE_PAYLOAD_TYPE_RAW_SENSOR2: Secondary intensity image
                // - SPINNAKER_IMAGE_PAYLOAD_TYPE_RANGE_SENSOR1: Range/depth data
                // - SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1: Disparity data
                // - SPINNAKER_IMAGE_PAYLOAD_TYPE_EXTENDED_CHUNK: Extended chunk metadata
                //
                for (unsigned int componentIdx = 0; componentIdx < componentCount; componentIdx++)
                {
                    try
                    {
                        // Get the component image from the ImageList
                        ImagePtr pComponentImage = imageList.GetByIndex(componentIdx);

                        //
                        // Check component completion status
                        //
                        if (pComponentImage->IsIncomplete())
                        {
                            cout << "  Component " << componentIdx << " incomplete: "
                                 << Image::GetImageStatusDescription(pComponentImage->GetImageStatus()) << endl;
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
                        const size_t width = pComponentImage->GetWidth();
                        const size_t height = pComponentImage->GetHeight();
                        const ImagePayloadType imagePayloadType = pComponentImage->GetImagePayloadType();
                        const string imagePayloadTypeName = ImagePayloadTypeToString(imagePayloadType);
                        uint64_t frameID = pComponentImage->GetFrameID();
                        uint64_t timestamp = pComponentImage->GetTimeStamp();

                        cout << "  Component " << componentIdx << ": " << width << "x" << height << endl
                             << "    Image Payload Type: " << imagePayloadTypeName << endl
                             << "    Pixel Format: " << pComponentImage->GetPixelFormatName() << endl
                             << "    Frame ID: " << frameID << ", Timestamp: " << timestamp << endl;

                        //
                        // Convert component image to Mono8 for saving
                        //
                        // *** NOTES ***
                        // Components can be converted between pixel formats just like standard images.
                        // The converted image does not need to be released as it does not affect the
                        // camera buffer.
                        //
                        ImagePtr convertedImage = processor.Convert(pComponentImage, PixelFormat_Mono8);

                        //
                        // Create filename with component index
                        //
                        // *** NOTES ***
                        // We include the component index in the filename to distinguish between
                        // different components from the same GenDC container. This allows you to
                        // see which files correspond to which data streams.
                        //
                        ostringstream filename;
                        filename << "AcquisitionGenDC-";
                        if (!deviceSerialNumber.empty())
                        {
                            filename << deviceSerialNumber.c_str() << "-";
                        }
                        filename << "Frame" << imageCnt << "-Component" << componentIdx << ".jpg";

                        // Save the component image
                        convertedImage->Save(filename.str().c_str());
                        cout << "    Saved: " << filename.str() << endl;
                    }
                    catch (Spinnaker::Exception& e)
                    {
                        cout << "  Error processing component " << componentIdx << ": " << e.what() << endl;
                    }
                }

                //
                // Release ImageList
                //
                // *** NOTES ***
                // ImageLists retrieved from GetNextImageSync() must be released to keep
                // the buffers from filling up. Releasing the ImageList releases all images
                // contained within it.
                //
                imageList.Release();

                cout << endl;
            }
            catch (Spinnaker::Exception& e)
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

        pCam->EndAcquisition();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        return -1;
    }

    return result;
}

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
                try
                {
                    CNodePtr pfeatureNode = *it;
                    cout << pfeatureNode->GetName() << " : ";
                    CValuePtr pValue = (CValuePtr)pfeatureNode;
                    cout << (IsReadable(pValue) ? pValue->ToString() : gcstring("Node not readable"));
                    cout << endl;
                }
                catch (Spinnaker::Exception&)
                {
                    cout << "Node not readable" << endl;
                }
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

// This function acts as the body of the example; please see NodeMapInfo example
// for more in-depth comments on setting up cameras.
int RunSingleCamera(CameraPtr pCam)
{
    int result;

    try
    {
        // Retrieve TL device nodemap and print device information
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

        result = PrintDeviceInfo(nodeMapTLDevice);

        // Initialize camera
        pCam->Init();

        // Retrieve GenICam nodemap
        INodeMap& nodeMap = pCam->GetNodeMap();

        // Configure GenDC components - exit if not supported
        result = ConfigureGenDC(pCam, nodeMap);
        if (result != 0)
        {
            return result;
        }

        // Configure heartbeat for GEV camera
#ifdef _DEBUG
        result = result | DisableGVCPHeartbeat(pCam);
#else
        result = result | ResetGVCPHeartbeat(pCam);
#endif

        // Acquire images
        result = result | AcquireImages(pCam, nodeMap, nodeMapTLDevice);

#ifdef _DEBUG
        // Reset heartbeat for GEV camera
        result = result | ResetGVCPHeartbeat(pCam);
#endif

        // Deinitialize camera
        pCam->DeInit();
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
    // Since this application saves images in the current folder
    // we must ensure that we have permission to write to this folder.
    // If we do not have permission, fail right away.
    FILE* tempFile = fopen("test.txt", "w+");
    if (tempFile == nullptr)
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

    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    // Print GenDC component configuration status
    cout << "*** GENDC COMPONENT CONFIGURATION ***" << endl;
    cout << "Component 0 (Primary): " << (enableComponent0 ? "ENABLED" : "DISABLED") << endl;
    cout << "Component 1 (Secondary): " << (enableComponent1 ? "ENABLED" : "DISABLED") << endl;
    cout << endl;
    cout << "NOTE: To change component settings, modify the flags at the top of the source file:" << endl;
    cout << "  - enableComponent0: Enable/disable primary component" << endl;
    cout << "  - enableComponent1: Enable/disable secondary component" << endl;
    cout << endl;
    cout << "IMPORTANT: This example requires a camera with GenDC support." << endl;
    cout << "           For standard single-image acquisition, use the Acquisition example." << endl;
    cout << endl;

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    // Print out current library version
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
         << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << endl
         << endl;

    // Retrieve list of cameras from the system
    CameraList camList = system->GetCameras();

    const unsigned int numCameras = camList.GetSize();

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

    //
    // Create shared pointer to camera
    //
    // *** NOTES ***
    // The CameraPtr object is a shared pointer, and will generally clean itself
    // up upon exiting its scope. However, if a shared pointer is created in the
    // same scope that a system object is explicitly released (i.e. this scope),
    // the reference to the shared point must be broken manually.
    //
    // *** LATER ***
    // Shared pointers can be terminated manually by assigning them to nullptr.
    // This keeps releasing the system from throwing an exception.
    //
    CameraPtr pCam = nullptr;

    int result = 0;

    // Run example on each camera
    for (unsigned int i = 0; i < numCameras; i++)
    {
        // Select camera
        pCam = camList.GetByIndex(i);

        cout << endl << "Running example for camera " << i << "..." << endl;

        // Run example
        result = result | RunSingleCamera(pCam);

        cout << "Camera " << i << " example complete..." << endl << endl;
    }

    //
    // Release reference to the camera
    //
    // *** NOTES ***
    // Had the CameraPtr object been created within the for-loop, it would not
    // be necessary to manually break the reference because the shared pointer
    // would have automatically cleaned itself up upon exiting the loop.
    //
    pCam = nullptr;

    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return result;
}