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
 *  @example RawToProcessed_CSharp.cs
 *
 *  @brief RawToProcessed_CSharp.cs shows how to convert saved raw image files
 *  to spinnaker image objects, then saves them to disk.  Useful for high
 *  bandwidth applications where the users has a fast hard drive, but is unable
 *  to keep up with the processing requirements for converting images to
 *  compressed image file formats like jpeg, while capturing images live.
 */

using SpinnakerNET;
using System;
using System.Collections;
using System.IO;

namespace RawToProcessed_CSharp
{
    internal class Program
    {
        // Specify image settings
        public const int HEIGHT = 480;

        public const int WIDTH = 640;
        public const int X_OFFSET = 0;
        public const int Y_OFFSET = 0;

        // Specify the original and target file types
        public const string RAW_FILE_TYPE = "raw";

        public const string TARGET_FILE_TYPE = "jpg";

        // Specify the original and target pixel formats
        public const PixelFormatEnums RAW_IMAGE_PIXEL_TYPE = PixelFormatEnums.BayerBG16;

        public const PixelFormatEnums TARGET_IMAGE_FORMAT = PixelFormatEnums.BGR8;

        // Specify the input and output directories
        public const string RAW_INPUT_DIR = "./input";

        public const string PROCESSED_OUTPUT_DIR = "output";

        // Replace a filename extension with a new extension
        private static string ReplaceExt(string filename, string newExt)
        {
            return Path.ChangeExtension(filename, newExt);
        }

        // Get .raw files under the specified directory
        private static void GetFilesInDir(string dir, Queue files)
        {
            DirectoryInfo d = new DirectoryInfo(dir);
            FileInfo[] FileInfo = d.GetFiles("*." + RAW_FILE_TYPE);
            foreach (FileInfo f in FileInfo)
            {
                files.Enqueue(f);
            }
        }

        private static int Main(string[] args)
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

            string path = "./" + PROCESSED_OUTPUT_DIR;

            try
            {
                // Determine whether the output directory exists.
                if (Directory.Exists(path))
                {
                    Console.WriteLine("Output directory exists.");
                }

                // Try to create the directory.
                DirectoryInfo di = Directory.CreateDirectory(path);
                Console.WriteLine("The directory was created successfully at {0}.\n", Directory.GetCreationTime(path));
            }
            catch (Exception e)
            {
                Console.WriteLine("The process failed: {0}", e.ToString());
            }

            // Create a queue to store raw image filenames
            Queue fileQueue = new Queue();

            // Get .raw files under the specified directory
            Program.GetFilesInDir(RAW_INPUT_DIR, fileQueue);

            // Create an ManagedImage object to store the image data
            ManagedImage tempImage = new ManagedImage();
            tempImage.ResetImage(WIDTH, HEIGHT, X_OFFSET, Y_OFFSET, RAW_IMAGE_PIXEL_TYPE);

            foreach (FileInfo file in fileQueue)
            {
                unsafe
                {
                    // Read the image data into tempImage
                    var imageData = tempImage.NativeData;
                    byte[] imageDataPtr = File.ReadAllBytes(file.FullName);
                    for (int pixel = 0; pixel < imageDataPtr.Length; pixel++)
                    {
                        imageData[pixel] = imageDataPtr[pixel];
                    }
                }

                // Create the new filename and path with the target file type extension
                string newFilename = Program.ReplaceExt(file.Name, TARGET_FILE_TYPE);
                newFilename = "./" + PROCESSED_OUTPUT_DIR + "/" + newFilename;

                // Save the image to the target pixel format and file type
                using (IManagedImage convertedImage = tempImage.Convert(TARGET_IMAGE_FORMAT))
                {
                    Console.WriteLine("New file saved at: " + newFilename);
                    convertedImage.Save(newFilename);
                }
            }

            Console.WriteLine("\nDone! Press Enter to exit...");
            Console.ReadLine();

            return result;
        }
    }
}