//=============================================================================
// Copyright (c) 2001-2018 FLIR Systems, Inc. All Rights Reserved.
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
*  @example FileAccess_UserSet.cpp
*
*  @brief FileAccess_UserSet.cpp shows how to read and write the user set
*  using camera File Access function.
*  This example uploads User Set 0 to the camera File Access storage and also
*  download the User Set 0 from the camera File Access storage and saves it to
*  the disk.
*  It also provides debug message when debug mode is turned on giving more
*  detail status of the progress and error messages to the users.
*
*  It relies on information provided in the
*  Enumeration, Acquisition, and NodeMapInfo examples.
*/

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>
#include <fstream>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

static bool _enableDebug = false;
static gcstring _fileSelector = "UserSet0";

// This function set the exposure time to default(i.e., Auto)
bool setExposureDefault(INodeMap& nodemap)
{
    bool result = true;
    try
    {
        // Get the enum node from the nodemap
        CEnumerationPtr ptrExposureAuto = nodemap.GetNode("ExposureAuto");

        if (!IsAvailable(ptrExposureAuto) || !IsWritable(ptrExposureAuto))
        {
            cout << "Unable to set exposure auto (enumeration retrieval). Aborting..." << endl << endl;
            return -1;
        }

        // Get the entry node from enum node
        CEnumEntryPtr ptrExposureContinuous = ptrExposureAuto->GetEntryByName("Continuous");

        if (!IsAvailable(ptrExposureContinuous) || !IsReadable(ptrExposureContinuous))
        {
            cout << "Unable to set exposure auto (enum entry retrieval). Aborting..." << endl << endl;
            return -1;
        }

        // Set the exposure to auto
        ptrExposureAuto->SetIntValue(ptrExposureContinuous->GetValue());
        cout << "Exposure auto set to continuous" << endl;
    }
    catch (Spinnaker::Exception& e) {
        cerr << "Error: " << e.what() << endl;
        result = false;
    }
    return result;
}

// This function set the exposure time of the camera to a given value
bool setExposureTime(INodeMap& nodemap, double exp)
{
    bool result = true;

    try
    {
        // Turn off auto exposure
        CEnumerationPtr ptrExposureAuto = nodemap.GetNode("ExposureAuto");
        if (!IsAvailable(ptrExposureAuto) || !IsWritable(ptrExposureAuto))
        {
            cout << "Unable to set exposure auto (enumeration retrieval). Aborting..." << endl << endl;
            return -1;
        }

        CEnumEntryPtr ptrExposureAutoOff = ptrExposureAuto->GetEntryByName("Off");
        if (!IsAvailable(ptrExposureAutoOff) || !IsReadable(ptrExposureAutoOff))
        {
            cout << "Unable to set exposure auto (enum entry retrieval). Aborting..." << endl << endl;
            return -1;
        }

        ptrExposureAuto->SetIntValue(ptrExposureAutoOff->GetValue());

        cout << endl << "Exposure auto turned off" << endl;

        // Set the exposure time to the desired value
        CFloatPtr ptrExposureTime = nodemap.GetNode("ExposureTime");

        // Check for if the node is writable
        if (!IsAvailable(ptrExposureTime) || !IsWritable(ptrExposureTime))
        {
            cout << "Unable to set exposure time (Float retrieval). Aborting..." << endl << endl;
            return -1;
        }
        else
        {
            //check if the value is within the minimum and maximum range
            if (exp > ptrExposureTime->GetMax())
            {
                exp = ptrExposureTime->GetMax();
            }

            if (exp < ptrExposureTime->GetMin())
            {
                exp = ptrExposureTime->GetMin();
            }

            // set the exposure time
            ptrExposureTime->SetValue(exp);
            cout << "Exposure time set to: " << ptrExposureTime->GetValue() << endl;
        }
    }
    catch (Spinnaker::Exception& e) {
        cerr << "Error: " << e.what() << endl;
        result = false;
    }
    return result;
}

// Print out operation result message
static void PrintResultMessage(bool result)
{
    if (result)
    {
        cout << "\n*** OPERATION COMPLETE ***\n";
    }
    else
    {
        cout << "\n*** OPERATION FAILED ***\n";
    }
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
    catch (Spinnaker::Exception& e)
    {
        cout << "Error: " << e.what() << endl;
        result = -1;
    }
    return result;
}

// Intializes the system
bool InitializeSystem(SystemPtr& system, CameraList& camList, CameraPtr& pCam)
{
    // Retrieve singleton reference to system object
    system = System::GetInstance();

    // Retrieve list of cameras from the system
    camList = system->GetCameras();

    const unsigned int numCameras = camList.GetSize();

    cout << "Number of cameras detected: " << numCameras << endl << endl;

    // Stop if there are no cameras
    if (numCameras == 0)
    {
        // Clear camera list before releasing system
        camList.Clear();

        // Release system
        system->ReleaseInstance();

        cout << "Not enough cameras!" << endl;
        cout << "Done! Press Enter to exit..." << endl;
        getchar();

        return false;
    }

    //
    // It creates shared pointer to camera
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
    pCam = nullptr;

    // Run example on 1st camera
    pCam = camList.GetByIndex(0);

    return true;
}

// Print out debug message
static void PrintDebugMessage(string msg)
{
#if DEBUG
    _enableDebug = true;
#endif
    if (_enableDebug)
    {
        cout << msg << endl;
    }
}

// Execute delete operation
bool ExecuteDeleteCommand(CameraPtr pCam)
{
    PrintDebugMessage("Deleting file...");
    try
    {
        pCam->FileOperationSelector.SetValue(FileOperationSelector_Delete);
        pCam->FileOperationExecute.Execute();
        if (pCam->FileOperationStatus.GetValue() != FileOperationStatus_Success)
        {
            cout << "Failed to delete file!" << endl;
            return false;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what() << endl;
        return false;
    }
    return true;
}

// Open camera file for writing
bool OpenFileToWrite(CameraPtr pCam)
{
    PrintDebugMessage("Opening file for writing...");
    try
    {
        pCam->FileOperationSelector.SetValue(FileOperationSelector_Open);
        pCam->FileOpenMode.SetValue(FileOpenMode_Write);
        pCam->FileOperationExecute.Execute();
        if (pCam->FileOperationStatus.GetValue() != FileOperationStatus_Success)
        {
            cout << "Failed to open file for writing!" << endl;
            return false;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what();
        return false;
    }
    return true;
}

// Execute write operation
bool ExecuteWriteCommand(CameraPtr pCam)
{
    try
    {
        pCam->FileOperationSelector.SetValue(FileOperationSelector_Write);
        pCam->FileOperationExecute.Execute();
        if (pCam->FileOperationStatus.GetValue() != FileOperationStatus_Success)
        {
            cout << "Failed to write to file!" << endl;
            return false;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what();
        return false;
    }
    return true;
}

// Close the file
bool CloseFile(CameraPtr pCam)
{
    PrintDebugMessage("Closing file...");
    try
    {
        pCam->FileOperationSelector.SetValue(FileOperationSelector_Close);
        pCam->FileOperationExecute.Execute();
        if (pCam->FileOperationStatus.GetValue() != FileOperationStatus_Success)
        {
            cout << "Failed to close file!" << endl;
            return false;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what();
        return false;
    }
    return true;
}

// Load user set selector operation
bool LoadUserSet(CameraPtr pCam)
{
    try
    {
        pCam->UserSetSelector.SetValue(UserSetSelectorEnums::UserSetSelector_UserSet0);
        pCam->UserSetLoad.Execute();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what();
        return false;
    }
    cout << "User set loaded\n";
    return true;
}

// Save user set selector operation
bool SaveUserSet(CameraPtr pCam)
{
    try
    {
        pCam->UserSetSelector.SetValue(UserSetSelectorEnums::UserSetSelector_UserSet0);
        pCam->UserSetSave.Execute();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what();
        return false;
    }
    cout << "User set saved\n";
    return true;
}

// Upload the user set to the camera file
bool UploadUserSet()
{
    try
    {
        // Prompt the user to enter file name for reading to camera
        cout << "Please enter the settings file name that will be loaded to the camera\n";

        string filename;
        getline(cin, filename);

        // Read from the file
        std::ifstream ifs(filename, std::ios::binary);

        // Read the data:
        std::vector<unsigned char> data = std::vector<unsigned char>((std::istreambuf_iterator<char>(ifs)),
            std::istreambuf_iterator<char>());

        // Check if it is valid
        if (!ifs && !ifs.eof()) {
            cerr << "Error Reading from file. Aborting..." << endl;
            return false;
        }

        SystemPtr system;
        CameraList camList;
        CameraPtr pCam;

        // Initialize System
        if (!InitializeSystem(system, camList, pCam))
        {
            PrintResultMessage(false);
            return false;
        }

        // Retrieve TL device nodemap and print device information
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();
        PrintDeviceInfo(nodeMapTLDevice);

        // Initialize camera
        pCam->Init();

        // Retrieve GenICam nodemap
        INodeMap& nodeMap = pCam->GetNodeMap();

        cout << endl << "*** UPLOADING FILE ***" << endl;
        PrintDebugMessage("Fetching \"UserFile1\" Entry from FileSelectorNode...");

        // Check file selector support
        if (pCam->FileSelector == NULL)
        {
            cout << "File selector not supported on device!";
            return false;
        }

        NodeList_t selectorList;
        pCam->FileSelector.GetEntries(selectorList);
        for (unsigned int i = 0; i < selectorList.size(); i++)
        {
            // Get current enum entry node
            CEnumEntryPtr node = selectorList.at(i);
            PrintDebugMessage("Setting value to FileSelectorNode...");

            // Check file selector entry support
            if (!node || !IsReadable(node))
            {
                // Go to next entry node
                cout << node->GetSymbolic() << " not supported!" << endl;
                continue;
            }
            if (node->GetSymbolic().compare(_fileSelector) == 0)
            {
                cout << "The node: " << node->GetSymbolic() << endl;
                PrintDebugMessage("Setting value to FileSelectorNode...");

                // Set file selector
                pCam->FileSelector.SetIntValue((int64_t)node->GetNumericValue());

                // Delete file on camera before writing in case camera runs out of space
                if (pCam->FileSize.GetValue() > 0)
                {
                    if (ExecuteDeleteCommand(pCam) != true)
                    {
                        cout << "Failed to delete file!" << endl;
                        continue;
                    }
                }

                // Open file on camera for write
                if (OpenFileToWrite(pCam) != true)
                {
                    cout << "Failed to open file!" << endl;

                    // Maybe file was not closed properly last time.
                    // Close and re-open again
                    if (!CloseFile(pCam))
                    {
                        // It fails to close the file. Abort!
                        cout << "Problem opening file node." << endl;
                        return false;
                    }

                    // File was closed. Open again.
                    if (!OpenFileToWrite(pCam))
                    {
                        // Fails again. Abort!
                        cout << "Problem opening file node." << endl;
                        return false;
                    }
                }

                // Attempt to set FileAccessLength to FileAccessBufferNode length to speed up the write
                if (pCam->FileAccessLength.GetValue() < pCam->FileAccessBuffer.GetLength())
                {
                    try
                    {
                        pCam->FileAccessLength.SetValue(pCam->FileAccessBuffer.GetLength());
                    }
                    catch (Spinnaker::Exception& e)
                    {
                        cout << "Unable to set FileAccessLength to FileAccessBuffer length : " << e.what();
                    }
                }
                // Set File Access Offset to zero if it is not
                pCam->FileAccessOffset.SetValue(0);

                // Compute number of write operations required
                int64_t totalBytesToWrite = data.size();
                int64_t intermediateBufferSize = pCam->FileAccessLength.GetValue();
                int64_t writeIterations = (totalBytesToWrite / intermediateBufferSize) + (totalBytesToWrite % intermediateBufferSize == 0 ? 0 : 1);

                if (totalBytesToWrite == 0)
                {
                    cout << "Empty file. No data will be written to camera." << endl;
                    return false;
                }

                PrintDebugMessage("Start saving user set file on camera...");
                PrintDebugMessage("Total Bytes to write : " + to_string(static_cast<long long>(totalBytesToWrite)));
                PrintDebugMessage("FileAccessLength : " + to_string(static_cast<long long>(intermediateBufferSize)));
                PrintDebugMessage("Write Iterations : " + to_string(static_cast<long long>(writeIterations)));

                int64_t index = 0;
                int64_t bytesLeftToWrite = totalBytesToWrite;
                int64_t totalBytesWritten = 0;
                bool paddingRequired = false;
                int numPaddings = 0;

                cout << "Writing data to device" << endl;

                unsigned char* pFileData = static_cast<unsigned char*>(data.data());

                for (unsigned int i = 0; i < writeIterations; i++)
                {
                    // Check whether padding is required
                    if (intermediateBufferSize > bytesLeftToWrite)
                    {
                        // Check for multiple of 4 bytes
                        unsigned int remainder = bytesLeftToWrite % 4;
                        if (remainder != 0)
                        {
                            paddingRequired = true;
                            numPaddings = 4 - remainder;
                        }
                    }

                    // Setup data to write
                    int64_t tmpBufferSize = intermediateBufferSize <= bytesLeftToWrite
                        ? intermediateBufferSize : (bytesLeftToWrite + numPaddings);
                    std::unique_ptr<unsigned char> tmpBuffer(new unsigned char[tmpBufferSize]);
                    memcpy(
                        tmpBuffer.get(),
                        &pFileData[index],
                        ((intermediateBufferSize <= bytesLeftToWrite) ? intermediateBufferSize : bytesLeftToWrite));

                    if (paddingRequired)
                    {
                        // Fill padded bytes
                        for (int j = 0; j < numPaddings; j++)
                        {
                            unsigned char* pTmpBuffer = tmpBuffer.get();
                            pTmpBuffer[bytesLeftToWrite + j] = 255;
                        }
                    }

                    // Update index for next write iteration
                    index = index +
                        (intermediateBufferSize <= bytesLeftToWrite ? intermediateBufferSize : bytesLeftToWrite);

                    // Write to FileAccessBufferNode
                    pCam->FileAccessBuffer.Set(tmpBuffer.get(), tmpBufferSize);

                    if (intermediateBufferSize > bytesLeftToWrite)
                    {
                        // It updates file Access Length Node;
                        // otherwise, garbage data outside the range
                        // would be written to device.
                        pCam->FileAccessLength.SetValue(bytesLeftToWrite);
                    }

                    // Perform Write command
                    if (!ExecuteWriteCommand(pCam))
                    {
                        cout << "Writing to stream failed!" << endl;
                        break;
                    }

                    // Verify size of bytes written
                    int64_t sizeWritten = pCam->FileOperationResult.GetValue();

                    // Log current file access offset
                    PrintDebugMessage("File Access Offset: " + to_string(pCam->FileAccessOffset.GetValue()));

                    // Keep track of total bytes written
                    totalBytesWritten += sizeWritten;
                    PrintDebugMessage(
                        "Bytes written: " + to_string(static_cast<long long>(totalBytesWritten)) + " of " +
                        to_string(static_cast<long long>(totalBytesToWrite)));

                    // Keep track of bytes left to write
                    bytesLeftToWrite = totalBytesToWrite - totalBytesWritten;
                    PrintDebugMessage("Bytes left: " + to_string(static_cast<long long>(bytesLeftToWrite)));

                    cout << "Progress : " << (i * 100 / writeIterations) << " %" << endl;
                }

                cout << "Writing complete" << endl;

                if (!CloseFile(pCam))
                {
                    cout << "Failed to close file!" << endl;
                }

                // Execute the user set
                LoadUserSet(pCam);
            }
        }

        //
        // Release reference to the camera
        //
        // *** NOTES ***
        // Had the CameraPtr object been created within the for-loop, it would not
        // be necessary to manually break the reference because the shared pointer
        // would have automatically cleaned itself up upon exiting the loop.
        //
        pCam->DeInit();
        pCam = nullptr;

        // Clear camera list before releasing system
        camList.Clear();

        // Release system
        system->ReleaseInstance();

        cout << endl << "Done! Press Enter to exit..." << endl;
        getchar();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what();
        return false;
    }
    return true;
}

// Open camera file to read
bool OpenFileToRead(CameraPtr pCamera)
{
    cout << "Opening file for reading..." << endl;
    try
    {
        pCamera->FileOperationSelector.SetValue(FileOperationSelector_Open);
        pCamera->FileOpenMode.SetValue(FileOpenMode_Read);
        pCamera->FileOperationExecute.Execute();

        if (pCamera->FileOperationStatus.GetValue() != FileOperationStatus_Success)
        {
            cout << "Failed to open file for reading!" << endl;
            return false;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what() << endl;
        return false;
    }
    return true;
}

// Execute read operation
bool ExecuteReadCommand(CameraPtr pCamera)
{
    try
    {
        pCamera->FileOperationSelector.SetValue(FileOperationSelector_Read);
        pCamera->FileOperationExecute.Execute();

        if (pCamera->FileOperationStatus.GetValue() != FileOperationStatus_Success)
        {
            cout << "Failed to read file!" << endl;
            return false;
        }
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what() << endl;
        return false;
    }
    return true;
}

// Download the user set to the disk from camera file
bool DownloadUserSet()
{
    try
    {
        // Prompt the user to enter file name for saving to the disc
        cout << "Please enter file name for saving the user setting file from the camera\n";
        string filename;
        getline(cin, filename);

        // Prompt the user to enter a value of exposure time to set
        cout << "Please enter the choice of exposure time in microseconds: ";
        double exp;
        (cin >> exp).get();

        // Check if cin is valid
        if (!cin) {
            cerr << "Error: Invalid input detected" << endl;

            //clear the buffer
            cin.clear();
            while (cin.get() != '\n')continue;
            return false;
        }

        SystemPtr system;
        CameraList camList;
        CameraPtr pCam;

        // Initialize System
        if (!InitializeSystem(system, camList, pCam))
        {
            PrintResultMessage(false);
            return false;
        }

        // Print out current library version
        const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
        cout << "Spinnaker library version: "
            << spinnakerLibraryVersion.major << "."
            << spinnakerLibraryVersion.minor << "."
            << spinnakerLibraryVersion.type << "."
            << spinnakerLibraryVersion.build << endl << endl;

        // Retrieve TL device nodemap and print device information
        INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();
        PrintDeviceInfo(nodeMapTLDevice);

        // Initialize camera
        pCam->Init();

        // Retrieve GenICam nodemap
        INodeMap& nodeMap = pCam->GetNodeMap();

        // Set the exposure time
        if (!setExposureTime(nodeMap, exp))
        {
            //deinitialize camera
            pCam->DeInit();

            // Clear camera list before releasing system
            camList.Clear();

            // Release system
            system->ReleaseInstance();

            return false;
        }

        // Save user seting
        SaveUserSet(pCam);

        cout << endl << "*** DOWNLOADING File ***" << endl;

        // Check file selector support
        PrintDebugMessage("Fetching \"UserFile1\" Entry from FileSelectorNode...");
        if (pCam->FileSelector == NULL)
        {
            cout << "File selector not supported on device!";
            return false;
        }

        NodeList_t selectorList;
        pCam->FileSelector.GetEntries(selectorList);
        for (unsigned int i = 0; i < selectorList.size(); i++)
        {
            // Get current enum entry node
            CEnumEntryPtr node = selectorList.at(i);

            PrintDebugMessage("Setting value to FileSelectorNode...");

            // Check file selector entry support
            if (!node || !IsReadable(node))
            {
                // Go to next entry node
                cout << node->GetSymbolic() << " not supported!" << endl;
                continue;
            }

            if (node->GetSymbolic().compare(_fileSelector) == 0)
            {
                // Set file selector
                pCam->FileSelector.SetIntValue((int64_t)node->GetNumericValue());
                int64_t bytesToRead = pCam->FileSize.GetValue();
                if (bytesToRead == 0)
                {
                    cout << "No data available to read!" << endl;
                    continue;
                }
                PrintDebugMessage("Total data to download : " + to_string(static_cast<long long>(bytesToRead)));

                // Open file on camera for reading
                if (OpenFileToRead(pCam) != true)
                {
                    cout << "Failed to open file!" << endl;
                    continue;
                }
                // Attempt to set FileAccessLength to FileAccessBufferNode length to speed up the write
                if (pCam->FileAccessLength.GetValue() < pCam->FileAccessBuffer.GetLength())
                {
                    try
                    {
                        pCam->FileAccessLength.SetValue(pCam->FileAccessBuffer.GetLength());
                    }
                    catch (Spinnaker::Exception& e)
                    {
                        cout << "Unable to set FileAccessLength to FileAccessBuffer length : " << e.what() << endl;
                    }
                }

                // Set File Access Offset to zero if its not
                pCam->FileAccessOffset.SetValue(0);

                // Compute number of read operations required
                int64_t intermediateBufferSize = pCam->FileAccessLength.GetValue();
                int64_t iterations = (bytesToRead / intermediateBufferSize) + (bytesToRead % intermediateBufferSize == 0 ? 0 : 1);
                PrintDebugMessage("Fetching file from camera.");
                int64_t index = 0;
                int64_t totalSizeRead = 0;
                std::unique_ptr<unsigned char> dataBuffer(new unsigned char[bytesToRead]);
                uint8_t* pData = dataBuffer.get();

                for (unsigned int i = 0; i < iterations; i++)
                {
                    if (!ExecuteReadCommand(pCam))
                    {
                        cout << "Reading stream failed!" << endl;
                        break;
                    }

                    // Verify size of bytes read
                    int64_t sizeRead = pCam->FileOperationResult.GetValue();

                    // Read from buffer Node
                    pCam->FileAccessBuffer.Get(&pData[index], sizeRead);

                    // Update index for next read iteration
                    index = index + sizeRead;

                    // Keep track of total bytes read
                    totalSizeRead += sizeRead;
                    PrintDebugMessage(
                        "Bytes read: " + to_string(static_cast<long long>(totalSizeRead)) + " of "
                        + to_string(static_cast<long long>(bytesToRead)));
                    cout << "Progress : " << (i * 100 / iterations) << " %" << endl;
                }

                cout << "Reading complete" << endl;

                if (!CloseFile(pCam))
                {
                    cout << "Failed to close file!" << endl;
                }
                cout << endl;

                // save file
                ofstream ofs(filename, ios_base::binary | ios_base::trunc);

                ofs.write((char*)&pData[0], totalSizeRead);
                ofs.close();

                cout << "*** SAVING USER SET FILE ***" << endl;
            }
        }
        //
        // Release reference to the camera
        //
        // *** NOTES ***
        // Had the CameraPtr object been created within the for-loop, it would not
        // be necessary to manually break the reference because the shared pointer
        // would have automatically cleaned itself up upon exiting the loop.
        //
        pCam->DeInit();
        pCam = nullptr;

        // Clear camera list before releasing system
        camList.Clear();

        // Release system
        system->ReleaseInstance();

        cout << endl << "Done! Press Enter to exit..." << endl;
        getchar();
    }
    catch (Spinnaker::Exception& e)
    {
        cout << "Unexpected exception : " << e.what() << endl;
        return false;
    }
    return true;
}
// Print out usage of the application
void PrintUsage()
{
    cout << "This program allows to test the camera for user set file download and upload\n";
    cout << "Usage: FileAcess </u | /d>" << endl;
    cout << "Options:" << endl;
    cout << "/d : Prompt the user to enter exposure time in microseconds, store the user set file on the current folder with giving file name" << endl;
    cout << "/u : Read the user set file with giving file name, upload it to camera and print the current exposure value." << endl;
    cout << "/v : Enable verbose output." << endl;
    cout << "/? : Print usage informaion." << endl;
    cout << endl << endl;
}

// Example entry point; please see Enumeration example for more in-depth
// comments on preparing and cleaning up the system.
int main(int argc, char* argv[])
{
    // Since this application saves files in the current folder
    // we must ensure that we have permission to write to this folder.
    // If we do not have permission, fail right away.
    ofstream tempFile("test.txt");
    if (!tempFile)
    {
        cout << "Failed to create file in current folder. Please check "
            "permissions."
            << endl;
        cout << "Press Enter to exit..." << endl;
        getchar();
        return -1;
    }
    tempFile.close();
    remove("test.txt");

    int result = 0;

    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    // Change arguments to string for easy read
    std::vector<std::string> args(argv, argv + argc);

    // Check verbose output flag first
    for (size_t i = 1; i < args.size(); ++i)
    {
        if (args[i] == "/v" || args[i] == "/v")
        {
            _enableDebug = true;
        }

        if (args[i] == "?")
        {
            PrintUsage();
        }
        else if (args[i] == "/u" || args[i] == "/U")
        {
            if (!UploadUserSet())
            {
                PrintResultMessage(false);
                return -1;
            }
        }

        else if (args[i] == "/d" || args[i] == "/D")
        {
            if (!DownloadUserSet())
            {
                PrintResultMessage(false);
                return -1;
            }
            return result;
        }
    }
    return result;
}