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
 *  @example TriggerReadyWithCounter.cpp
 *
 *  @brief TriggerReadyWithCounter.cpp shows alternative approach to check when camera 
 *  is ready to accept the next trigger using FrameTriggerWait together with camera's
 *  Counter and Timer Control features.
 * 
 *  Counter Event Source is set to FrameTriggerWait. An increase in the Counter Value node  
 *  after each trigger indicates that camera has entered a state where it is ready to accept 
 *  the next trigger.
 *
 *  Please leave us feedback at: https://www.surveymonkey.com/r/TDYMVAPI
 *  More source code examples at: https://github.com/Teledyne-MV/Spinnaker-Examples
 *  Need help? Check out our forum at: https://teledynevisionsolutions.zendesk.com/hc/en-us/community/topics
 */

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;

// ============================================================
// Configuration
// ============================================================

constexpr unsigned int MAX_IMAGES = 10;
constexpr unsigned int READY_TIMEOUT_MS = 5000;

// ============================================================
// GenICam helper functions
// ============================================================

void SetEnumerationNode(
    INodeMap& nodeMap,
    const char* nodeName,
    const char* entryName)
{
    CEnumerationPtr node = nodeMap.GetNode(nodeName);

    if (!IsAvailable(node) || !IsWritable(node))
    {
        throw std::runtime_error(
            std::string("Node unavailable or not writable: ") +
            nodeName);
    }

    CEnumEntryPtr entry = node->GetEntryByName(entryName);

    if (!IsAvailable(entry) || !IsReadable(entry))
    {
        throw std::runtime_error(
            std::string("Entry unavailable: ") +
            nodeName + " = " + entryName);
    }

    node->SetIntValue(entry->GetValue());

    std::cout << nodeName << " = " << entryName << '\n';
}

int64_t ReadIntegerNode(
    INodeMap& nodeMap,
    const char* nodeName)
{
    CIntegerPtr node = nodeMap.GetNode(nodeName);

    if (!IsAvailable(node) || !IsReadable(node))
    {
        throw std::runtime_error(
            std::string("Node unavailable or not readable: ") +
            nodeName);
    }

    // Bypass cached counter values.
    return node->GetValue(false, true);
}

int64_t SetIntegerNodeToMaximum(
    INodeMap& nodeMap,
    const char* nodeName)
{
    CIntegerPtr node = nodeMap.GetNode(nodeName);

    if (!IsAvailable(node) || !IsWritable(node))
    {
        throw std::runtime_error(
            std::string("Node unavailable or not writable: ") +
            nodeName);
    }

    const int64_t minimum = node->GetMin();
    const int64_t maximum = node->GetMax();
    const int64_t increment = node->GetInc();

    //int64_t value = maximum;
    int64_t value = 5;

    if (increment > 1)
    {
        value = minimum +
            ((maximum - minimum) / increment) * increment;
    }

    node->SetValue(value);

    std::cout << nodeName << " = " << value << '\n';

    return value;
}

void ExecuteCommandNode(
    INodeMap& nodeMap,
    const char* nodeName)
{
    CCommandPtr node = nodeMap.GetNode(nodeName);

    if (!IsAvailable(node) || !IsWritable(node))
    {
        throw std::runtime_error(
            std::string("Command unavailable: ") +
            nodeName);
    }

    node->Execute();

    while (!node->IsDone())
    {
    }
}

// ============================================================
// Configure software trigger
// ============================================================

void ConfigureSoftwareTrigger(INodeMap& nodeMap)
{
    std::cout << "\nConfiguring software trigger...\n";

    SetEnumerationNode(
        nodeMap, "TriggerMode", "Off");

    SetEnumerationNode(
        nodeMap, "TriggerSelector", "FrameStart");

    SetEnumerationNode(
        nodeMap, "TriggerSource", "Software");

    SetEnumerationNode(
        nodeMap, "TriggerMode", "On");

    std::cout << "Software trigger configured.\n";
}

// ============================================================
// Configure Counter0
// ============================================================

int64_t ConfigureCounter0(INodeMap& nodeMap)
{
    std::cout << "\nConfiguring Counter0...\n";

    SetEnumerationNode(
        nodeMap,
        "CounterSelector",
        "Counter0");

    SetEnumerationNode(
        nodeMap,
        "CounterEventSource",
        "FrameTriggerWait");

    SetEnumerationNode(
        nodeMap,
        "CounterEventActivation",
        "RisingEdge");

    const int64_t counterDuration =
        SetIntegerNodeToMaximum(
            nodeMap,
            "CounterDuration");

    SetEnumerationNode(
        nodeMap,
        "CounterTriggerSource",
        "ExposureStart");

    SetEnumerationNode(
        nodeMap,
        "CounterTriggerActivation",
        "RisingEdge");

    // Reset Counter0 if supported.
    CCommandPtr counterReset =
        nodeMap.GetNode("CounterReset");

    if (IsAvailable(counterReset) &&
        IsWritable(counterReset))
    {
        counterReset->Execute();

        while (!counterReset->IsDone())
        {
        }

        std::cout << "Counter0 reset executed.\n";
    }
    else
    {
        std::cout
            << "CounterReset unavailable. "
            << "Using current counter value.\n";
    }

    std::cout << "Counter0 configured.\n";

    return counterDuration;
}

// ============================================================
// Wait until camera is ready or timeout occurs
// ============================================================

bool WaitUntilCameraReady(
    INodeMap& nodeMap,
    int64_t& counterPrevious,
    int64_t counterDuration)
{
    const auto startTime =
        std::chrono::steady_clock::now();

    std::cout
        << "\nMonitoring Counter0 for camera readiness...\n";

    while (true)
    {
        const int64_t counterCurrent =
            ReadIntegerNode(
                nodeMap,
                "CounterValue");

        // Detect rising edge of FrameTriggerWait.
        if (counterCurrent > counterPrevious)
        {
            std::cout
                << "\nCamera READY!\n"
                << "Previous CounterValue: "
                << counterPrevious
                << "\nCurrent CounterValue: "
                << counterCurrent
                << '\n';

            //// Handle counter maximum.
            //if (counterCurrent == counterDuration)
            //{
            //    counterPrevious = 0;
            //}
            //else
            //{
            //    counterPrevious = counterCurrent;
            //}

            return true;
        }

        const auto elapsed =
            std::chrono::duration_cast<
            std::chrono::milliseconds>(
                std::chrono::steady_clock::now() -
                startTime)
            .count();

        if (elapsed >= READY_TIMEOUT_MS)
        {
            std::cerr
                << "\nERROR: Camera not ready within "
                << READY_TIMEOUT_MS
                << " ms.\n"
                << "Previous CounterValue: "
                << counterPrevious
                << "\nCurrent CounterValue: "
                << counterCurrent
                << '\n';

            return false;
        }
    }
}

// ============================================================
// Acquire images using ENTER-based software triggering
// ============================================================

bool AcquireImages(CameraPtr camera)
{
    INodeMap& nodeMap = camera->GetNodeMap();

    // Configure software trigger.
    ConfigureSoftwareTrigger(nodeMap);

    // Configure Counter0.
    const int64_t counterDuration =
        ConfigureCounter0(nodeMap);

    // Initialize counter variables.
    int64_t counterPrevious =
        ReadIntegerNode(
            nodeMap,
            "CounterValue");

    int64_t counterCurrent = counterPrevious;

    std::cout
        << "\nInitial CounterValue: "
        << counterCurrent
        << '\n';

    unsigned int imageCount = 0;
    bool acquisitionSuccessful = true;

    camera->BeginAcquisition();

    std::cout
        << "\n========================================\n"
        << "Acquisition started\n"
        << "Maximum images: " << MAX_IMAGES << '\n'
        << "Camera ready timeout: "
        << READY_TIMEOUT_MS / 1000
        << " seconds\n"
        << "========================================\n";

    try
    {
        while (imageCount < MAX_IMAGES)
        {
            std::cout
                << "\n----------------------------------------\n"
                << "Images acquired: "
                << imageCount
                << " / "
                << MAX_IMAGES
                << '\n'
                << "Press ENTER to trigger camera "
                << "(or q to quit): ";

            std::string userInput;

            if (!std::getline(std::cin, userInput))
            {
                std::cout << "\nInput closed. Exiting.\n";
                break;
            }

            if (userInput == "q" || userInput == "Q")
            {
                std::cout
                    << "\nUser requested exit.\n";
                break;
            }

            if (!userInput.empty())
            {
                std::cout
                    << "Invalid input. "
                    << "Press ENTER or type q.\n";

                continue;
            }

            // Read Counter0 before sending trigger.
            counterCurrent =
                ReadIntegerNode(
                    nodeMap,
                    "CounterValue");
            // Handle counter maximum.
            if (counterCurrent >= counterDuration)
            {
                counterPrevious = 0;
            }
            else
            {
                counterPrevious = counterCurrent;
            }

            std::cout
                << "\nCounterValue before trigger: "
                << counterPrevious
                << '\n';

            // Send software trigger.
            std::cout
                << "Sending software trigger...\n";

            ExecuteCommandNode(
                nodeMap,
                "TriggerSoftware");

            // Retrieve triggered image.
            ImagePtr image =
                camera->GetNextImage(
                    READY_TIMEOUT_MS);

            if (image->IsIncomplete())
            {
                std::cerr
                    << "Incomplete image. Status: "
                    << image->GetImageStatus()
                    << '\n';

                image->Release();

                acquisitionSuccessful = false;
                break;
            }

            imageCount++;

            std::cout
                << "\nImage acquired successfully!\n"
                << "Image number: "
                << imageCount
                << '\n'
                << "Width: "
                << image->GetWidth()
                << '\n'
                << "Height: "
                << image->GetHeight()
                << '\n'
                << "Frame ID: "
                << image->GetFrameID()
                << '\n';

            image->Release();

            // Stop immediately after 10 images.
            if (imageCount >= MAX_IMAGES)
            {
                std::cout
                    << "\n========================================\n"
                    << "Successfully acquired 10 images.\n"
                    << "Maximum image count reached.\n"
                    << "Stopping acquisition automatically.\n"
                    << "========================================\n";

                break;
            }

            // Wait up to 5 seconds for next trigger readiness.
            const bool cameraReady =
                WaitUntilCameraReady(
                    nodeMap,
                    counterPrevious,
                    counterDuration);

            if (!cameraReady)
            {
                std::cerr
                    << "\nCamera readiness timeout.\n"
                    << "Aborting acquisition.\n";

                acquisitionSuccessful = false;
                break;
            }

            std::cout
                << "\nCamera ready for next software trigger.\n";
        }
    }
    catch (...)
    {
        camera->EndAcquisition();
        throw;
    }

    camera->EndAcquisition();

    std::cout
        << "\nAcquisition stopped.\n"
        << "Total images acquired: "
        << imageCount
        << '\n';

    return acquisitionSuccessful;
}

// ============================================================
// Main
// ============================================================

int main()
{
    SystemPtr system = System::GetInstance();

    CameraList cameraList =
        system->GetCameras();

    const unsigned int cameraCount =
        cameraList.GetSize();

    std::cout
        << "Cameras detected: "
        << cameraCount
        << '\n';

    if (cameraCount == 0)
    {
        std::cerr << "No camera detected.\n";

        cameraList.Clear();
        system->ReleaseInstance();

        return EXIT_FAILURE;
    }

    CameraPtr camera =
        cameraList.GetByIndex(0);

    bool initialized = false;
    int result = EXIT_SUCCESS;

    try
    {
        camera->Init();
        initialized = true;

        std::cout << "Camera initialized.\n";

        if (!AcquireImages(camera))
        {
            result = EXIT_FAILURE;
        }
    }
    catch (const Spinnaker::Exception& exception)
    {
        std::cerr
            << "Spinnaker exception: "
            << exception.what()
            << '\n';

        result = EXIT_FAILURE;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "Application exception: "
            << exception.what()
            << '\n';

        result = EXIT_FAILURE;
    }

    // Cleanup.
    if (initialized)
    {
        try
        {
            SetEnumerationNode(
                camera->GetNodeMap(),
                "TriggerMode",
                "Off");

            camera->DeInit();
        }
        catch (const std::exception& exception)
        {
            std::cerr
                << "Cleanup warning: "
                << exception.what()
                << '\n';
        }
    }

    camera = nullptr;

    cameraList.Clear();

    system->ReleaseInstance();

    std::cout << "\nProgram completed.\n";

    return result;
}