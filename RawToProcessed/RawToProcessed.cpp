//=============================================================================
// Copyright (c) 2001-2020 FLIR Systems, Inc. All Rights Reserved.
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
*  @example RawToProcessed.cpp
*
*  @brief RawToProcessed.cpp shows how to convert saved raw image files to
*  spinnaker image objects, then saves them to disk.  Useful for high bandwidth
*  applications where the users has a fast hard drive, but is unable to keep up
*  with the processing requirements for converting images to compressed image
*  file formats like jpeg, while capturing images live.
*/

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>

// for windows mkdir
#ifdef _WIN32
#include <direct.h>
#endif

#include <errno.h>
#include <thread>
#include <vector>
#include <queue>
#include <string>
#include <cstring>
#include "dirent.h"

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

// Define image settings and directories.
#define HEIGHT                  480
#define WIDTH                   640
#define BYTE_DEPTH              2
#define X_OFFSET                0
#define Y_OFFSET                0
#define RAW_FILE_TYPE           "Raw"
#define TARGET_FILE_TYPE        "Tiff"
#define RAW_IMAGE_PIXEL_TYPE    PixelFormat_BayerBG16
#define TARGET_IMAGE_FORMAT     PixelFormat_BGR8
#define RAW_INPUT_DIR           "./input"
#define PROCESSED_OUTPUT_DIR    "output"

// Create a queue to store raw image filenames
queue<string> raw_image_files;

// Number of raw image files to be processed in the current queue initialized to 0
uint64_t total_files = 0;

// Case-insenstive comparison of two characters
inline bool caseInsCharCompareN(char a, char b)
{
    return(toupper(a) == toupper(b));
}

// Case-insensitive comparison of two strings
bool caseInsCompare(const string & s1, const string & s2)
{
    return((s1.size() == s2.size()) &&
        equal(s1.begin(), s1.end(), s2.begin(), caseInsCharCompareN));
}

// Check whether a string has a specified ending
bool hasEnding(string const & fullString, string const & ending)
{
    if (fullString.length() >= ending.length())
    {
        string ext = fullString.substr(fullString.length() - ending.length(), ending.length());
        return (caseInsCompare(ext, ending));
    }
    else
    {
        return false;
    }
}

// Replace a filename extension with a new extension
void replaceExt(string & s, const string & newExt)
{
    string::size_type i = s.rfind('.', s.length());

    if (i != string::npos)
    {
        s.replace(i + 1, newExt.length(), newExt);
    }
}

// Get .raw files under the specified directory
int getdir(string dir, queue<string> & files)
{
    DIR *dp;
    struct dirent *dirp;
    if ((dp = opendir(dir.c_str())) == NULL)
    {
        cout << "Error(" << errno << ") opening " << dir << endl;
        return errno;
    }

    while ((dirp = readdir(dp)) != NULL)
    {
        if (hasEnding(dirp->d_name, RAW_FILE_TYPE))
        {
            files.push(string(dirp->d_name));
        }
    }
    closedir(dp);
    return 0;
}

// Convert .raw images to the target pixel format and file type, and store them in the output directory
void processImages()
{
    // Create an ImagePtr object to get the image data
    ImagePtr tempImage = Image::Create();
    tempImage->ResetImage(WIDTH, HEIGHT, X_OFFSET, Y_OFFSET, RAW_IMAGE_PIXEL_TYPE);
    unsigned char *buffer = static_cast<unsigned char*>(tempImage->GetData());
    FILE* inFile;

    // Convert the .raw images one by one in the specified directory
    while (!raw_image_files.empty())
    {
        string fileName;
        cout << "\nFiles remaining: " << raw_image_files.size() - 1 << "/" << total_files;
        fileName = raw_image_files.front();
        raw_image_files.pop();
        cout << "\t" << " converting file: " << fileName;

        // Filepath for the current .raw image file
        string filepath = string(RAW_INPUT_DIR) + string("/") + fileName;

        // Open the current .raw image
        inFile = fopen(filepath.c_str(), "rb");

        if (inFile == NULL)
        {
            cout << "Error reading: " << filepath;
            continue;
        }

        // Read the current .raw image data in the specified format and store them in the buffer
        fread(buffer, sizeof(unsigned char), HEIGHT * WIDTH * BYTE_DEPTH, inFile);

        fclose(inFile);

        // Create the new filename and path with the target file type extension
        string newFilename = fileName;
        replaceExt(newFilename, TARGET_FILE_TYPE);
        string newFilepath = string(PROCESSED_OUTPUT_DIR) + string("/") + newFilename;

        // Save the image to the target pixel format and file type
        try
        {
            ImagePtr convertedImage = tempImage->Convert(TARGET_IMAGE_FORMAT, HQ_LINEAR);
            convertedImage->Save(newFilepath.c_str());
        }
        catch (Spinnaker::Exception& e)
        {
            cout << "Error: " << e.what() << endl;
        }
    }
}

// Example entry point; please see Enumeration example for more in-depth
// comments on preparing and cleaning up the system.
int main(int /*argc*/, char** /*argv*/)
{
    int result = 0;
    // Since this application saves images in the current folder
    // we must ensure that we have permission to write to this folder.
    // If we do not have permission, fail right away.
    FILE *tempFile = fopen("test.txt", "w+");
    if (tempFile == NULL)
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

    struct stat info;
    stat(PROCESSED_OUTPUT_DIR, &info);

    // Create an output folder if it does not exit
    if (info.st_mode & S_IFDIR)
    {
        cout << "Output directory exists:  " << PROCESSED_OUTPUT_DIR << endl;
    }
    else
    {
        cout << "Creating output directory: " << PROCESSED_OUTPUT_DIR << endl;

        int nError = 0;
#if defined(_WIN32)
        nError = _mkdir(PROCESSED_OUTPUT_DIR); // can be used on Windows
#else
        mode_t nMode = 0733; // UNIX style permissions
        nError = mkdir(PROCESSED_OUTPUT_DIR, nMode); // can be used on non-Windows
#endif
        if (nError != 0)
        {
            // Handle your error here
            cout << "Unable to create directory" << endl;
            cout << endl << endl << "Done! Press Enter to exit..." << endl;
            getchar();
            exit(1);
        }
    }

    // Directory for the .raw files
    string dir = string(RAW_INPUT_DIR) + string("/");

    // Get .raw files under the specified directory
    getdir(dir, raw_image_files);

    // Total number of .raw images to be processed
    total_files = raw_image_files.size();

    // Convert images
    processImages();

    cout << endl << endl << "Done! Press Enter to exit..." << endl;
    getchar();
}