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

using SpinnakerNET;
using SpinnakerNET.GenApi;

/**
*  @example FileAccess_UserSet_CSharp.cs
*
*  @brief FileAccess_UserSet_CSharp.cs shows how to read and write the user set
*  using camera File Access function.
*  This example uploads User Set 1 to the camera File Access storage and also
*  download the User Set 1 from the camera File Access storage and saves it to
*  the disk.
*  It also provides debug message when debug mode is turned on giving more
*  detail status of the progress and error messages to the users.
*
*  It relies on information provided in the
*  Enumeration, Acquisition, and NodeMapInfo examples.
*/

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace FileAccessUtility
{
    internal class Program
    {
        private const string _fileSelectorNodeName = "FileSelector";
        private const string _fileOpenModeNodeName = "FileOpenMode";
        private const string _fileAccessOffsetNodeName = "FileAccessOffset";
        private const string _fileAccessLengthNodeName = "FileAccessLength";
        private const string _fileOperationStatusNodeName = "FileOperationStatus";
        private const string _fileOperationResultNodeName = "FileOperationResult";
        private const string _fileSizeNodeName = "FileSize";
        private const string _fileOperationSelectorNodeName = "FileOperationSelector";
        private const string _fileOperationExecuteNodeName = "FileOperationExecute";
        private const string _fileAccessBufferNodeName = "FileAccessBuffer";

        // Spinnaker System
        private ManagedSystem _system;

        // Spinnaker Camera
        private IManagedCamera _camera;

        // Camera list
        private IList<IManagedCamera> _camList;

        // Nodemap
        private INodeMap _nodemap;

        // Spinnaker GenICam nodes
        private Enumeration _fileSelectorNode;

        private Enumeration _fileOpenModeNode;
        private Integer _fileAccessOffsetNode;
        private Integer _fileAccessLengthNode;
        private Enumeration _fileOperationStatusNode;
        private Integer _fileOperationResultNode;
        private Integer _fileSizeNode;
        private Enumeration _fileOperationSelectorNode;
        private Command _fileOperationExecuteNode;
        private Register _fileAccessBufferNode;

        private static bool _enableDebug = false;

        private static void Main(string[] args)
        {
            Program program = new Program();

            Console.WriteLine("\n*** BEGIN APPLICATION ***\n");

            // Check verbose output flag first
            if (args.Contains("-v") || args.Contains("/v"))
            {
                _enableDebug = true;
            }

            if (args.Contains("?"))
            {
                PrintUsage();
            }
            else if (args.Contains("/u") || args.Contains("-u"))
            {
                // Perform upload

                // Initialize System
                if (!program.InitializeSystem())
                {
                    PrintResultMessage(false);
                    return;
                }

                // Initialize all file nodes
                if (!program.InitializeFileNodes())
                {
                    PrintResultMessage(false);
                    return;
                }

                // Upload User Set 1 and upload it to device
                if (!program.Upload())
                {
                    PrintResultMessage(false);
                    return;
                }

                PrintResultMessage(true);
            }
            else if (args.Contains("/d") || args.Contains("-d"))
            {
                // Perform download and file to disk

                // Initialize System
                if (!program.InitializeSystem())
                {
                    PrintResultMessage(false);
                    return;
                }

                // Initialize all file nodes
                if (!program.InitializeFileNodes())
                {
                    PrintResultMessage(false);
                    return;
                }

                // Grab User Set 1 and upload it to device
                try
                {
                    program.PerformDownloadOperation();
                }
                catch (Exception ex)
                {
                    Console.Out.WriteLine(ex.Message);
                    PrintResultMessage(false);
                    return;
                }

                PrintResultMessage(true);
            }
            else
            {
                PrintUsage();
            }
        }

        private static void PrintDebugMessage(string msg)
        {
#if DEBUG
            _enableDebug = true;
#endif
            if (_enableDebug)
            {
                Console.Out.WriteLine(msg);
            }
        }

        private static void PrintResultMessage(bool result)
        {
            if (result)
            {
                Console.WriteLine("\n*** OPERATION COMPLETE ***\n");
            }
            else
            {
                Console.WriteLine("\n*** OPERATION FAILED ***\n");
            }
        }

        private static void PrintUsage()
        {
            string appName = System.AppDomain.CurrentDomain.FriendlyName;

            Console.WriteLine(string.Format("\nUsage: {0} </u | /d>", appName));
            Console.WriteLine("\nOptions:");
            Console.WriteLine("/u : Upload UserSet1 file from the current directory and store it on camera.");
            Console.WriteLine("/d : Download User Set 1 from camera and store it in the current directory.");
            Console.WriteLine("/v : Enable verbose output.");
            Console.WriteLine("/? : Print usage informaion.");
            Console.WriteLine("\n\n");
        }

        private bool InitializeSystem()
        {
            // Retrieve singleton reference to system object
            _system = new ManagedSystem();

            // Retrieve list of cameras from the system
            _camList = _system.GetCameras();

            PrintDebugMessage(string.Format("Number of cameras detected: {0}\n", _camList.Count));

            // Finish if there are no cameras
            if (_camList.Count == 0)
            {
                // Clear camera list before releasing system
                _camList.Clear();

                // Release system
                _system.Dispose();

                Console.WriteLine("No camera found!");

                return false;
            }
            else
            {
                return true;
            }
        }

        private bool InitializeFileNodes()
        {
            //Check CameraList
            if (_camList == null || _camList.Count == 0)
            {
                Console.WriteLine("Not enough cameras!");
                Console.WriteLine("Done! Press Enter to exit...");
                Console.ReadLine();
                return false;
            }

            // Fetch first connected device
            _camera = _camList[0];

            if (_camera == null)
            {
                Console.WriteLine("Camera is not initialized!");
                Console.WriteLine("Done! Press Enter to exit...");
                Console.ReadLine();
                return false;
            }

            try
            {
                // Initialize camera
                _nodemap = _camera.Init();

                // Fetch all nodes
                _fileSelectorNode = _nodemap.GetNode<Enumeration>(_fileSelectorNodeName);
                _fileOpenModeNode = _nodemap.GetNode<Enumeration>(_fileOpenModeNodeName);
                _fileAccessOffsetNode = _nodemap.GetNode<Integer>(_fileAccessOffsetNodeName);
                _fileAccessLengthNode = _nodemap.GetNode<Integer>(_fileAccessLengthNodeName);
                _fileOperationStatusNode = _nodemap.GetNode<Enumeration>(_fileOperationStatusNodeName);
                _fileOperationResultNode = _nodemap.GetNode<Integer>(_fileOperationResultNodeName);
                _fileSizeNode = _nodemap.GetNode<Integer>(_fileSizeNodeName);
                _fileOperationSelectorNode = _nodemap.GetNode<Enumeration>(_fileOperationSelectorNodeName);
                _fileOperationExecuteNode = _nodemap.GetNode<Command>(_fileOperationExecuteNodeName);
                _fileAccessBufferNode = _nodemap.GetNode<Register>(_fileAccessBufferNodeName);

                // Check whether FileAccess is supported
                if (_fileSelectorNode == null || _fileOpenModeNode == null || _fileOperationStatusNode == null ||
                    _fileOperationSelectorNode == null || _fileOperationExecuteNode == null || _fileAccessBufferNode == null)
                {
                    // File access is not supported on this device
                    Console.WriteLine("FileAccess is not supported by current device!");
                    Console.WriteLine("Done! Press Enter to exit...");
                    Console.ReadLine();
                    return false;
                }
            }
            catch (Exception ex)
            {
                Console.Out.WriteLine(ex.Message);
                return false;
            }

            return true;
        }

        private bool Upload()
        {
            // Check Camera is valid
            if (_camera == null)
            {
                Console.WriteLine("Camera is not initialized!");
                Console.WriteLine("Done! Press Enter to exit...");
                Console.ReadLine();
                return false;
            }

            // Check Nodemap valid
            if (_nodemap == null)
            {
                Console.WriteLine("NodeMap is not initialized!");
                Console.WriteLine("Done! Press Enter to exit...");
                Console.ReadLine();
                return false;
            }

            try
            {
                PerformUploadOperation();
            }
            catch (Exception ex)
            {
                Console.Out.WriteLine(ex.Message);

                return false;
            }

            try
            {
                _camera.DeInit();
            }
            catch
            { }

            return true;
        }

        private void PerformUploadOperation()
        {
            try
            {
                // 1. Load bytes from User Set 1
                byte[] data = File.ReadAllBytes("UserSet1File");

                // Check data length
                if (data.Length == 0)
                {
                    throw new Exception("Empty File. No data will be written to camera.");
                }

                Console.WriteLine("\n*** UPLOADING USER SET 1 ***\n");

                PrintDebugMessage("Fetching \"UserSet1\" Entry from FileSelectorNode...");

                // Retrieve entry node from enumeration node
                IEnumEntry iUserSet = _fileSelectorNode.GetEntryByName("UserSet1");
                if (iUserSet == null || !iUserSet.IsReadable)
                {
                    throw new Exception("Unable to select User File from File Selector. Aborting...\n");
                }

                PrintDebugMessage("Setting value to FileSelectorNode...");

                // Set symbolic from entry node as new value for enumeration node
                _fileSelectorNode.Value = iUserSet.Symbolic;

                // Open file on camera for write
                if (!OpenFileToWrite())
                {
                    // Maybe file was not closed properly last time.
                    // Try closing it and opening it again
                    if (!CloseFile())
                    {
                        // Failed to close the file. Abort!
                        throw new Exception(string.Format("Problem opening file node. Status = {0}", _fileOperationStatusNode.Value.String));
                    }

                    // File was closed. Try opening it again.
                    if (!OpenFileToWrite())
                    {
                        // Failed again. Abort!
                        throw new Exception(string.Format("Problem opening file node. Status = {0}", _fileOperationStatusNode.Value.String));
                    }
                }

                // Get total number of bytes we need to write
                long totalBytesToWrite = data.Length;

                // Get internal buffer size
                long interBufferSize = _fileAccessLengthNode.Value;

                long bufferLength = 256;

                try
                {
                    bufferLength = _fileAccessBufferNode.Length;
                }
                catch (Exception ex)
                {
                    PrintDebugMessage(string.Format("Problem reading from FileAccessLengthNode. {0}", ex.Message));
                }

                if (interBufferSize < bufferLength && _fileAccessLengthNode.IsWritable)
                {
                    try
                    {
                        // Try setting FileAccessLength to FileAccessBufferNode.Length to speed up the write
                        _fileAccessLengthNode.Value = bufferLength;
                        interBufferSize = _fileAccessLengthNode.Value;
                    }
                    catch (Exception ex)
                    {
                        PrintDebugMessage(string.Format("Problem setting value to FileAccessLengthNode. {0}", ex.Message));
                    }
                }

                // Set File Access Offset to zero if its not
                if (_fileAccessOffsetNode.Value != 0 && _fileAccessOffsetNode.IsWritable)
                {
                    _fileAccessOffsetNode.Value = 0;
                }

                // Compute number of write operations requried
                long iterations = (totalBytesToWrite / interBufferSize) + (totalBytesToWrite % interBufferSize == 0 ? 0 : 1);

                int index = 0;
                int bytesLeftToWrite = (int)totalBytesToWrite;
                int totalBytesWritten = 0;
                bool paddingRequired = false;
                int numberOfPaddings = 0;

                PrintDebugMessage("Start saving User Set 1 on camera...");

                // Entering loop for writing data to device
                for (long i = 0; i < iterations; i++)
                {
                    if (interBufferSize > bytesLeftToWrite)
                    {
                        // Check for multiple of 4 bytes
                        int remainder = bytesLeftToWrite % 4;
                        if (remainder != 0)
                        {
                            paddingRequired = true;
                            numberOfPaddings = 4 - remainder;
                        }
                    }

                    // Extract bytes from data array
                    byte[] tempData = new byte[interBufferSize <= bytesLeftToWrite ? (int)interBufferSize : (bytesLeftToWrite + numberOfPaddings)];

                    Array.Copy(data, index, tempData, 0, interBufferSize <= bytesLeftToWrite ? (int)interBufferSize : bytesLeftToWrite);

                    if (paddingRequired)
                    {
                        // Fill padded bytes
                        for (int j = 0; j < numberOfPaddings; j++)
                        {
                            tempData[bytesLeftToWrite + j] = 255;
                        }
                    }

                    // Update index for next write iteration
                    index = index + (interBufferSize <= bytesLeftToWrite ? (int)interBufferSize : bytesLeftToWrite);

                    // Write to AccessBufferNode
                    _fileAccessBufferNode.Write(tempData);

                    if (interBufferSize > bytesLeftToWrite)
                    {
                        // Update file Access Length Node;
                        // otherwise, garbage data outside the range
                        // would be written to device.
                        _fileAccessLengthNode.Value = bytesLeftToWrite;
                    }

                    // Do Write command
                    ExecuteWriteCommand();

                    // Verify size of bytes written
                    long sizeWritten = _fileOperationResultNode.Value;

                    // Log current file access offset
                    PrintDebugMessage((string.Format("File Access Offset: {0}", _fileAccessOffsetNode.Value)));

                    // Keep track of total bytes written
                    totalBytesWritten += (int)sizeWritten;
                    PrintDebugMessage((string.Format("Bytes written: {0} of {1}", totalBytesWritten, totalBytesToWrite)));

                    // Keep track of bytes left to write
                    bytesLeftToWrite = data.Length - totalBytesWritten;
                    PrintDebugMessage((string.Format("Bytes left: {0}", bytesLeftToWrite)));

                    Console.Write("\rProgress = {0}%   ", (i * 100 / iterations));
                }

                // Clear progress message from console
                Console.Write("\r");

                // Close the file
                if (!CloseFile())
                {
                    throw new Exception("Problem closing file.");
                }
            }
            catch (Exception ex)
            {
                throw new Exception((string.Format("There was a problem uploading file to device. {0}", ex.Message)));
            }
        }

        private void PerformDownloadOperation()
        {
            // Code for downloading file from device and save to disk
            try
            {
                List<byte> allData = new List<byte>();

                // Retrieve entry node from enumeration node
                IEnumEntry iUserSet = _fileSelectorNode.GetEntryByName("UserSet1");
                if (iUserSet == null || !iUserSet.IsReadable)
                {
                    throw new Exception("Unable to select User File from File Selector. Aborting...\n");
                }

                // Set symbolic from entry node as new value for enumeration node
                _fileSelectorNode.Value = iUserSet.Symbolic;

                // File node should be opened for read at this stage
                // Open the file for reading
                if (!OpenFileToRead())
                {
                    // May be file was not closed properly last time.
                    // Close it and try  openning again.

                    if (!CloseFile())
                    {
                        // Failed to close the file. Abort
                        throw new SpinnakerException(string.Format("Problem opening file node. Status = {0}", _fileOperationStatusNode.Value.String));
                    }

                    // File was closed. Try opening again.
                    if (!OpenFileToRead())
                    {
                        // Failed again. Abort
                        throw new SpinnakerException(string.Format("Problem opening file node. Status = {0}", _fileOperationStatusNode.Value.String));
                    }
                }

                // Get file size
                long bytesToRead = _fileSizeNode.Value;

                PrintDebugMessage(string.Format("Total data to download = {0}", bytesToRead));

                if (bytesToRead == 0)
                {
                    throw new Exception("There is no user set stored on camera...");
                }

                Console.WriteLine("\n*** DOWNLOADING USER SET 1 ***\n");

                // Get internal buffer size
                long interBufferSize = _fileAccessLengthNode.Value;

                long bufferLength = 256;

                try
                {
                    bufferLength = _fileAccessBufferNode.Length;
                }
                catch (Exception ex)
                {
                    PrintDebugMessage(string.Format("Problem reading from FileAccessLengthNode. {0}", ex.Message));
                }

                if (interBufferSize < bufferLength && _fileAccessLengthNode.IsWritable)
                {
                    try
                    {
                        // Try setting FileAccessLength to _fileAccessBufferNode.Length
                        // to speed up the read
                        _fileAccessLengthNode.Value = bufferLength;
                        interBufferSize = _fileAccessLengthNode.Value;
                    }
                    catch (Exception ex)
                    {
                        PrintDebugMessage(string.Format("Problem setting value to FileAccessLengthNode. {0}", ex.Message));
                    }
                }

                // Set File Access Offset to zero if its not
                if (_fileAccessOffsetNode.Value != 0 && _fileAccessOffsetNode.IsWritable)
                {
                    _fileAccessOffsetNode.Value = 0;
                }

                // Compute number of read operations requried
                long iterations = (bytesToRead / interBufferSize) + (bytesToRead % interBufferSize == 0 ? 0 : 1);

                byte[] output;

                PrintDebugMessage("Fetching User Set 1 from camera.");

                // Entering loop for reading data from device
                for (long i = 0; i < iterations; i++)
                {
                    // Do Read command
                    ExecuteReadCommand();

                    // Verify size of bytes read
                    long sizeRead = _fileOperationResultNode.Value;

                    // Read from buffer node
                    _fileAccessBufferNode.Read(out output);

                    // Convert byte array to list
                    List<byte> tempBytes = new List<Byte>(output);

                    // Retrieve valid collection size
                    if (tempBytes.Count > sizeRead)
                    {
                        tempBytes.RemoveRange((int)sizeRead, (int)(tempBytes.Count - sizeRead));
                    }

                    // Add temp collection to container
                    allData.AddRange(tempBytes);

                    Console.Write("\rProgress = {0}%   ", (i * 100 / iterations));
                }

                // Clear progress message from console
                Console.Write("\r");

                // Save the user set 1 file into the current directory
                try
                {
                    File.WriteAllBytes("UserSet1File", allData.ToArray());
                }
                catch (Exception ex)
                {
                    Console.Out.Write(ex.Message);
                }

                PrintDebugMessage(string.Format("Total bytes read = {0}", allData.Count));

                // Close the file
                CloseFile();
            }
            catch (Exception ex)
            {
                throw new Exception(string.Format("There was a problem downloading file from device. {0}", ex));
            }
        }

        #region FILE ACCESS HELPERS

        private void ToggleEnumerationNode(IEnum node, string targetSelection)
        {
            foreach (var entry in node.Entries)
            {
                if (entry.DisplayName.ToLower() == targetSelection.ToLower())
                {
                    node.Value = entry.Value;
                    break;
                }
            }
        }

        private bool CheckNodeStatus(IEnum node, string expected)
        {
            if (node.Value.String.ToLower() != expected.ToLower())
            {
                return false;
            }

            return true;
        }

        private bool CloseFile()
        {
            // Set FileOperationSelector to "Close"
            ToggleEnumerationNode(_fileOperationSelectorNode, "close");

            // Close the file
            _fileOperationExecuteNode.Execute();

            return CheckNodeStatus(_fileOperationStatusNode, "success");
        }

        private bool OpenFileToRead()
        {
            // Set FileOperationSelector to "Open"
            ToggleEnumerationNode(_fileOperationSelectorNode, "open");

            // Set FileOpenMode to "Read"
            ToggleEnumerationNode(_fileOpenModeNode, "read");

            // Open the file for reading
            _fileOperationExecuteNode.Execute();

            return CheckNodeStatus(_fileOperationStatusNode, "success");
        }

        private bool OpenFileToWrite()
        {
            // Set FileOperationSelector to "Open"
            ToggleEnumerationNode(_fileOperationSelectorNode, "open");

            // Set FileOpenMode to "Read"
            ToggleEnumerationNode(_fileOpenModeNode, "write");

            // Open the file for reading
            _fileOperationExecuteNode.Execute();

            return CheckNodeStatus(_fileOperationStatusNode, "success");
        }

        private bool ExecuteReadCommand()
        {
            // Set FileOperationSelector to "Read"
            ToggleEnumerationNode(_fileOperationSelectorNode, "read");

            // Read file
            _fileOperationExecuteNode.Execute();

            return CheckNodeStatus(_fileOperationStatusNode, "success");
        }

        private bool ExecuteWriteCommand()
        {
            // Set FileOperationSelector to "Read"
            ToggleEnumerationNode(_fileOperationSelectorNode, "write");

            // Read file
            _fileOperationExecuteNode.Execute();

            return CheckNodeStatus(_fileOperationStatusNode, "success");
        }

        #endregion FILE ACCESS HELPERS
    }
}