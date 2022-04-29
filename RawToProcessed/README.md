# RawToProcessed

## Overview 

This example shows how to convert saved raw image files to spinnaker image objects, then save them in a processed image file format, as saved.  Useful for high bandwidth applications where the users has a fast hard drive, but is unable to keep up with the processing requirements for converting images to compressed image file formats like jpeg, while capturing images live.

## How to Run the Application

To successfully run this example, please double-check the following:
* Add header file "dirent.h" to the project, which contains functions for manipulating file system directories;
* Create an folder named "input" under the current directory (unless specified otherwise in RAW_INPUT_DIR), and store the .raw images in the "input" folder;
* In the #define section at the beginning of the .cpp code, change the image settings (such as HEIGHT, WIDTH, BYTE_DEPTH, RAW_IMAGE_PIXEL_TYPE, etc.), to conform to the user's requirements.