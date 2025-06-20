//=============================================================================
// Copyright (c) 2025 FLIR Integrated Imaging Solutions, Inc. All Rights
// Reserved
//
// This software is the confidential and proprietary information of FLIR
// Integrated Imaging Solutions, Inc. ("Confidential Information"). You
// shall not disclose such Confidential Information and shall use it only in
// accordance with the terms of the non-disclosure or license agreement you
// entered into with FLIR Integrated Imaging Solutions, Inc. (FLIR).
//
// FLIR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. FLIR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================
//
// ImageUtilityStereo.cpp shows how to acquire image sets from the BumbleBee stereo
// camera, compute 3D information using  ImageUtilityStereo functions,
// and display a depth image (created via CreateDepthImage) along with interactive
// measurements.
//
// Please leave us feedback at: https://www.surveymonkey.com/r/TDYMVAPI
// More source code examples at: https://github.com/Teledyne-MV/Spinnaker-Examples

#include <iostream>
#include <sstream>
#include <opencv2/opencv.hpp>
#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include "ImageUtilityStereo.h"

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace std;

// Global state for mouse interaction and display
static cv::Mat g_rectResized;
static cv::Mat g_depthColor;
static ImagePtr g_dispPtr;
static StereoCameraParameters g_stereoParams;
static cv::Point g_click1;
static bool g_firstClicked = false;
static float g_lastDist = 0.0f;

// Mouse callback: compute 3D coords and distances
void onMouse(int event, int x, int y, int flags, void*) {
    if (g_rectResized.empty() || !g_dispPtr) return;

    // Reverse scaling (display is half-size)
    int origX = x * 2;
    int origY = y * 2;

    // Access disparity data pointer (uint16_t array)
    unsigned int w = g_dispPtr->GetWidth();
    unsigned int h = g_dispPtr->GetHeight();
    if (origX >= (int)w || origY >= (int)h) return;

    const uint16_t* dispData = reinterpret_cast<const uint16_t*>(g_dispPtr->GetData());
    uint16_t dispVal = dispData[origY * w + origX];

    // Prepare pixel and compute 3D point
    ImagePixel imgPx{ static_cast<uint32_t>(origX), static_cast<uint32_t>(origY) };
    Stereo3DPoint pt3d;
    pt3d.pixel = imgPx;
    bool okXYZ = ImageUtilityStereo::Compute3DPointFromPixel(dispVal, g_stereoParams, pt3d);

    // Compute distance to this point
    float dist = 0.0f;
    bool okDist = ImageUtilityStereo::ComputeDistanceToPoint(g_dispPtr, g_stereoParams, imgPx, dist);

    // Handle clicks for between-points distance
    if (event == cv::EVENT_LBUTTONDOWN && okDist) {
        if (!g_firstClicked) {
            g_click1 = cv::Point(x, y);
            g_firstClicked = true;
        }
        else {
            cv::Point click2(x, y);
            float distPts = 0.0f;
            bool okPts = ImageUtilityStereo::ComputeDistanceBetweenPoints(
                g_dispPtr, g_stereoParams,
                ImagePixel{ static_cast<uint32_t>(g_click1.x * 2), static_cast<uint32_t>(g_click1.y * 2) },
                ImagePixel{ static_cast<uint32_t>(click2.x * 2), static_cast<uint32_t>(click2.y * 2) },
                distPts
            );

            if (okPts) {
                g_lastDist = distPts;
                // Draw line & text on display
                cv::line(g_rectResized, g_click1, click2, cv::Scalar(255, 0, 0), 2);
                cv::putText(g_rectResized,
                    cv::format("%.2f mm", distPts),
                    (g_click1 + click2) * 0.5,
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 0, 0), 2);
            }
            g_firstClicked = false;
        }
    }

    // Update overlay of instant 3D coords and distance
    cv::Mat overlay = g_rectResized.clone();
    if (okXYZ && okDist) {
        cv::putText(overlay,
            cv::format("(%.2f, %.2f, %.2f) Dist: %.2f m", pt3d.x, pt3d.y, pt3d.z, dist),
            cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
    }

    if (g_lastDist > 0.0f) {
        // retained from last click-based measurement
    }

    cv::imshow("Rectified Left Image", overlay);
}

// Verify both rectified and disparity images are present
bool validateImageList(ImageList& imgs) {
    auto rect = imgs.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1);
    auto disp = imgs.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1);
    return rect && !rect->IsIncomplete() && disp && !disp->IsIncomplete();
}

// Function to acquire and process images from the camera
bool acquireImages(CameraPtr cam) {
    try {
        cam->BeginAcquisition();
        cout << "Starting acquisition..." << endl;

        // Read stereo params from node map
        INodeMap& nodemap = cam->GetNodeMap();

        // Retrieve and read stereo parameters from camera node map
        CFloatPtr fptr;
        CIntegerPtr iptr;
        CBooleanPtr bptr;

        fptr = nodemap.GetNode("Scan3dCoordinateOffset");
        if (IsReadable(fptr)) g_stereoParams.coordinateOffset = fptr->GetValue();

        fptr = nodemap.GetNode("Scan3dBaseline");
        if (IsReadable(fptr)) g_stereoParams.baseline = fptr->GetValue();

        fptr = nodemap.GetNode("Scan3dFocalLength");
        if (IsReadable(fptr)) g_stereoParams.focalLength = fptr->GetValue();

        fptr = nodemap.GetNode("Scan3dPrincipalPointU");
        if (IsReadable(fptr)) g_stereoParams.principalPointU = fptr->GetValue();

        fptr = nodemap.GetNode("Scan3dPrincipalPointV");
        if (IsReadable(fptr)) g_stereoParams.principalPointV = fptr->GetValue();

        fptr = nodemap.GetNode("Scan3dCoordinateScale");
        if (IsReadable(fptr)) g_stereoParams.disparityScaleFactor = fptr->GetValue();

        iptr = nodemap.GetNode("Scan3dInvalidDataValue");
        if (IsReadable(iptr)) g_stereoParams.invalidDataValue = iptr->GetValue();

        bptr = nodemap.GetNode("Scan3dInvalidDataFlag");
        if (IsReadable(bptr)) g_stereoParams.invalidDataFlag = bptr->GetValue();

        cout << "Stereo parameters loaded." << endl;

        uint64_t timeout = 2000;
        while (true) {
            ImageList imgs = cam->GetNextImageSync(timeout);
            if (!validateImageList(imgs)) { imgs.Release(); continue; }

            // Retrieve images
            ImagePtr rectImg = imgs.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1);
            ImagePtr dispImg = imgs.GetByPayloadType(SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1);

            // Speckle filter
            ImageUtilityStereo::FilterSpecklesFromImage(
                dispImg,
                1000, 4,
                g_stereoParams.disparityScaleFactor,
                g_stereoParams.invalidDataValue
            );

            // Depth image creation
            float minDepth = 0.0f, maxDepth = 0.0f;
            ImagePtr depthImg = ImageUtilityStereo::CreateDepthImage(
                dispImg, g_stereoParams,
                static_cast<uint16_t>(g_stereoParams.invalidDataValue),
                minDepth, maxDepth
            );

            // Convert for display
            cv::Mat rectMat = cv::Mat(
                rectImg->GetHeight(), rectImg->GetWidth(), CV_8UC3,
                rectImg->GetData(), rectImg->GetStride()
            );
            cv::cvtColor(rectMat, rectMat, cv::COLOR_BGR2RGB);

            unsigned int w = depthImg->GetWidth(), h = depthImg->GetHeight();
            cv::Mat depth16(h, w, CV_16UC1, depthImg->GetData());
            depth16 = depth16.clone();
            cv::Mat norm;
            depth16.convertTo(norm, CV_8U, 255.0 / 20000.0);
            cv::Mat color;
            cv::applyColorMap(norm, color, cv::COLORMAP_JET);
            color.setTo(cv::Scalar(0, 0, 0), depth16 == g_stereoParams.invalidDataValue);

            // Prepare global display mats
            cv::resize(rectMat, g_rectResized, cv::Size(), 0.5, 0.5);
            cv::resize(color, g_depthColor, cv::Size(), 0.5, 0.5);
            g_dispPtr = dispImg;

            // Show windows
            cv::namedWindow("Rectified Left Image", cv::WINDOW_NORMAL);
            cv::setMouseCallback("Rectified Left Image", onMouse);
            cv::imshow("Rectified Left Image", g_rectResized);
            cv::imshow("Depth Image", g_depthColor);

            int key = cv::waitKey(1);
            if (key == 27) break; // Esc
            if (key == 's' || key == 'S') {
                static int idx = 0;
                string base = "Capture_" + to_string(idx++);
                cv::imwrite(base + "_Rect.png", rectMat);
                cv::imwrite(base + "_Depth.png", color);
                cout << "Saved " << base << " images." << endl;
            }
            imgs.Release();
        }

        cam->EndAcquisition();
        cv::destroyAllWindows();
    }
    catch (Spinnaker::Exception& e) {
        cerr << "Spinnaker Error: " << e.what() << endl;
        return false;
    }
    return true;
}

int main() {
    // Verify write access
    FILE* test = fopen("test.txt", "w+");
    if (!test) {
        cerr << "Cannot write to current directory." << endl;
        return -1;
    }
    fclose(test);
    remove("test.txt");

    SystemPtr system = System::GetInstance();
    LibraryVersion ver = system->GetLibraryVersion();
    cout << "Spinnaker Library Version: "
        << ver.major << "." << ver.minor << "." << ver.type << "." << ver.build << endl;

    CameraList cams = system->GetCameras();
    unsigned int num = cams.GetSize();
    cout << "Number of cameras detected: " << num << endl;

    if (num == 0) {
        cams.Clear();
        system->ReleaseInstance();
        cerr << "No cameras found." << endl;
        return -1;
    }

    bool ok = true;
    for (unsigned int i = 0; i < num; ++i) {
        CameraPtr cam = cams.GetByIndex(i);
        cout << "Running camera " << i << "..." << endl;
        cam->Init();
        ok &= acquireImages(cam);
        cam->DeInit();
    }

    cams.Clear();
    system->ReleaseInstance();
    return ok ? 0 : -1;
}
