//=============================================================================
// Copyright © 2013 Point Grey Research, Inc. All Rights Reserved.
//
// This software is the confidential and proprietary information of Point
// Grey Research, Inc. ("Confidential Information").  You shall not
// disclose such Confidential Information and shall use it only in
// accordance with the terms of the license agreement you entered into
// with PGR.
//
// PGR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. PGR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================
// RelativeToAbsoluteShutterEx 
//
// This example saves all the absolute shutter values (real values in ms)
// for all possible relative shutter values of the camera in the current mode 
// to an Excel file.
//=============================================================================


#include "stdafx.h"

#include "FlyCapture2.h"
#include "Windows.h"
#include <iostream>
#include <sstream>

using namespace FlyCapture2;
using namespace std;

void PrintBuildInfo()
{
	FC2Version fc2Version;
	Utilities::GetLibraryVersion(&fc2Version);

	ostringstream version;
	version << "FlyCapture2 library version: " << fc2Version.major << "."
		<< fc2Version.minor << "." << fc2Version.type << "."
		<< fc2Version.build;
	cout << version.str() << endl;

	ostringstream timeStamp;
	timeStamp << "Application build date: " << __DATE__ << " " << __TIME__;
	cout << timeStamp.str() << endl << endl;
}

void PrintCameraInfo( CameraInfo* pCamInfo )
{
    printf(
        "\n*** CAMERA INFORMATION ***\n"
        "Serial number - %u\n"
        "Camera model - %s\n"
        "Camera vendor - %s\n"
        "Sensor - %s\n"
        "Resolution - %s\n"
        "Firmware version - %s\n"
        "Firmware build time - %s\n\n",
        pCamInfo->serialNumber,
        pCamInfo->modelName,
        pCamInfo->vendorName,
        pCamInfo->sensorInfo,
        pCamInfo->sensorResolution,
        pCamInfo->firmwareVersion,
        pCamInfo->firmwareBuildTime );
}

void PrintError( Error error )
{
    error.PrintErrorTrace();
}

int AllShutterValues( PGRGuid guid )
{
    Error error;
    Camera cam;

    // Connect to a camera
    error = cam.Connect(&guid);
    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }

    // Get the camera information
    CameraInfo camInfo;
    error = cam.GetCameraInfo(&camInfo);
    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }

	// Print the camera info
    PrintCameraInfo(&camInfo);        

    Property prop;
	prop.type = FRAME_RATE;
	
	// Get the current property settings of the frame rate property
	error = cam.GetProperty(&prop);
	if (error != PGRERROR_OK)
	{
		PrintError(error);
		return -1;
	}

	prop.onOff = false;
	
	error = cam.SetProperty(&prop);
	if (error != PGRERROR_OK)
	{
		PrintError(error);
		return -1;
	}
	
	
	prop.type = SHUTTER;

	// Get the current property settings of the shutter property
	error = cam.GetProperty(&prop);
	if (error != PGRERROR_OK)
	{
		PrintError( error );
		return -1;
	}

    prop.autoManualMode = false;
    prop.absControl = false;

	// change shutter property to Manual mode, and turn absolute mode off.
	error = cam.SetProperty(&prop);
	if (error != PGRERROR_OK)
	{
		PrintError( error );
		return -1;
	}
	
	// Read register for relative shutter 

	const unsigned int uiShutterRelAddr = 0x81C; 
	unsigned int uiShutterRel = 0;
	unsigned int regOffset = 0;

	cam.ReadRegister(uiShutterRelAddr, &uiShutterRel); 
	regOffset = uiShutterRel & 0xFFFFF000;
	
	
	// Create a .csv file
	FILE * fp = fopen("RelativeToAbsoluteShutterEx.csv", "w"); 
	fprintf(fp, "RELATIVE SHUTTER, ABSOLUTE SHUTTER (ms) \n");

	printf("Writing shutter values... ");

	// Loop through all possible relative shutter values. Save relative and corresponding absolute shutter

	for(unsigned int shutter_count = 2; shutter_count <= 4095; shutter_count ++)
	{

		cam.WriteRegister(uiShutterRelAddr, regOffset + shutter_count);
		cam.ReadRegister(uiShutterRelAddr, &uiShutterRel);

		// ABS_Register can be read immediately, but we need to wait after the write if we want to read back 
		// the camera property due to delays in the updates.
		// Could always just read reg 918 with no sleep, but that is the floating point hex representation and
		// you need to convert to decimal yourself.
		Sleep(200); 
		
		// Get the current shutter property to read the Absolute value for writing
		cam.GetProperty(&prop);

		// Print to .csv File 
		fprintf(fp, " %d, %.3f\n", uiShutterRel & 0xFFF, prop.absValue);
				
	}

	fclose(fp);

    return 0;
}

int main(int /*argc*/, char** /*argv*/)
{   
	PrintBuildInfo();

    Error error;

	BusManager busMgr;
    unsigned int numCameras;
    error = busMgr.GetNumOfCameras(&numCameras);
    if (error != PGRERROR_OK)
    {
        PrintError( error );
        return -1;
    }

    printf( "Number of cameras detected: %u\n", numCameras );

    for (unsigned int i=0; i < numCameras; i++)
    {
        PGRGuid guid;
        error = busMgr.GetCameraFromIndex(i, &guid);
        if (error != PGRERROR_OK)
        {
            PrintError( error );
            return -1;
        }

        AllShutterValues( guid );
    }

    printf( "Done! Press Enter to exit...\n" );
    getchar();

    return 0;
}
