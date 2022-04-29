# ============================================================================
# Copyright (c) 2001-2020 FLIR Systems, Inc. All Rights Reserved.

# This software is the confidential and proprietary information of FLIR
# Integrated Imaging Solutions, Inc. ("Confidential Information"). You
# shall not disclose such Confidential Information and shall use it only in
# accordance with the terms of the license agreement you entered into
# with FLIR Integrated Imaging Solutions, Inc. (FLIR).
#
# FLIR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
# SOFTWARE, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE, OR NON-INFRINGEMENT. FLIR SHALL NOT BE LIABLE FOR ANY DAMAGES
# SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
# THIS SOFTWARE OR ITS DERIVATIVES.
# ============================================================================
#
# This example shows how to converts the camera's timestamp to system time.
# It may be helpful to read through the atricle below to understand how PC
# time is calculated from camera time:
# https://www.flir.com/support-center/iis/machine-vision/application-note/synchronizing-a-blackfly-or-grasshopper3-gige-cameras-time-to-pc-time/
#
# It might also me helpful to familiarize yourself with the ChunkData.py
# example first as it provides a good introduction to chunk data and
# how it works.
#
# The process of getting the camera timestamp is different for different
# cameras. For this reason, this example defines 3 functions for calculating
# timestamp offsets. Newer cameras (BFS, Oryx, and Firefly-DL) all use
# the same method of getting the timestamp. However, older camera models
# (Grasshopper, Chameleon, Blackfly, and Flea3), use different methods
# for USB and GigE cameras.


import time
import PySpin

NUM_IMAGES = 10
NEWER_CAMERAS = ['Blackfly S', 'Oryx', 'DL']
NS_PER_S = 1000000000  # The number of nanoseconds in a second


def is_newer(cam_name: str) -> bool:
    """
    This function determines whether the given `cam_name` is
    a newer camera or an older camera. The differences
    between newer and older cameras are explained in the
    description of the script.
    Returns True if the given `cam_name` belongs to a newer camera,
    False otherwise.
    """
    for cur_cam_name in NEWER_CAMERAS:
        if cur_cam_name in cam_name:
            return True

    return False


def calculate_offset_older_usb(cam: PySpin.CameraPtr) -> int:
    """
    Calculates the timestamp offset for a given older USB camera.
    Returns the offset or None if there is an error.
    """
    try:
        # Get camera time
        cam.TimestampLatch.Execute()
        camera_time = cam.Timestamp.GetValue()
        print('Current camera time:', camera_time)

        # Get system time
        system_time = time.time()  # TODO: test windows
        print('Current system time:', system_time)

        # Calculate offset in seconds
        offset = system_time - camera_time / NS_PER_S
        return offset

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return None


def calculate_offset_older_gev(cam: PySpin.CameraPtr) -> int:
    """
    Calculates the timestamp offset for a given older GigE camera.
    Returns the offset or None if there is an error.
    This function does not use the QuickSpin syntax as it is not
    supported for this scenario.
    TODO: test
    """
    try:
        nodemap = cam.GetNodeMap()
        # Get camera time
        nodemap.GetNode('GevTimestampControlLatch').Execute()
        camera_time = nodemap.GetNode('GevTimestampValue').GetValue()
        print('Current camera time:', camera_time)

        # Get system time
        system_time = time.time()  # TODO: test windows
        print('Current system time:', system_time)

        # Calculate offset in seconds
        offset = system_time - camera_time / NS_PER_S
        return offset

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return None


def calculate_offset_newer(cam: PySpin.CameraPtr) -> int:
    """
    Calculates the timestamp offset for a given newer camera.
    Returns the offset or None if there is an error.
    """
    try:
        # Get camera time
        cam.TimestampLatch.Execute()
        camera_time = cam.TimestampLatchValue.GetValue()
        print('Current camera time:', camera_time)

        # Get system time
        system_time = time.time()  # TODO: test windows
        print('Current system time:', system_time)

        # Calculate offset in seconds
        offset = system_time - camera_time / NS_PER_S
        return offset

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return None


def acquire_images(cam: PySpin.CameraPtr) -> bool:
    """
    The main function that acquires images.
    Determines the type of camera and uses the appropriate
    calculate offset function to convert timestamps to PC time.
    Returns whether the operation was successful or not.
    """
    try:
        # Get device info to know how to convert timestamps
        # DeviceType quickspin wasn't working so:
        nodemap_tldevice = cam.GetTLDeviceNodeMap()
        device_type = PySpin.CEnumerationPtr(nodemap_tldevice.GetNode('DeviceType')).GetIntValue()
        device_name = cam.DeviceModelName()

        # Determine which timestamp function to use
        print('Device name:', device_name)
        if is_newer(device_name):
            calculate_offset = calculate_offset_newer
            print('This is a newer camera')
        elif device_type == PySpin.DeviceType_GEV:
            calculate_offset = calculate_offset_older_gev
            print('This is an older GEV camera')
        elif device_type == PySpin.DeviceType_U3V:
            calculate_offset = calculate_offset_older_usb
            print('This is an older U3V camera')

        # Start acquisition
        cam.BeginAcquisition()
        for i in range(NUM_IMAGES):
            print('-' * 64)
            image = cam.GetNextImage()
            if image.IsIncomplete():
                print('Warning: image {} incomplete'.format(image.GetFrameID()))
                continue

            chunk_data = image.GetChunkData()
            timestamp = chunk_data.GetTimestamp()
            print('Chunk timestamp:', timestamp)

            # Convert timestamp
            converted_timestamp = timestamp / NS_PER_S + calculate_offset(cam)

            print('PC timestamp in seconds:', converted_timestamp)
            timestamp_full = '{:4}/{:02}/{:02} {:02}:{:02}:{:02}'.format(*time.localtime(converted_timestamp))
            print('PC timestamp:', timestamp_full)

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return False

    return True


def setup_chunk_data(cam: PySpin.CameraPtr) -> bool:
    """
    Sets up chunk data to include the image timestamp
    for the given camera.
    Returns whether the operation was successful or not.
    """
    try:
        cam.ChunkModeActive.SetValue(True)
        cam.ChunkSelector.SetValue(PySpin.ChunkSelector_Timestamp)
        cam.ChunkEnable.SetValue(True)

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return False

    return True


def run_single_camera(cam: PySpin.CameraPtr) -> bool:
    """
    Sets up the camera settings to work for this example, then runs the example.
    Returns whether the operation was successful or not.
    """
    result = True
    result &= setup_chunk_data(cam)
    result &= acquire_images(cam)
    return result


def main():
    """
    The main function of the example. Sets up a system and
    runs the example for all connected cameras.
    """
    # Setup the system and camera
    system = PySpin.System.GetInstance()
    cam_list = system.GetCameras()
    result = True

    # Run the example
    for cam in cam_list:
        cam.Init()
        result &= run_single_camera(cam)

    # Clean up
    del cam
    cam_list.Clear()
    system.ReleaseInstance()

    if result:
        print('Example completed successfully')
    else:
        print('Example completed with some errors')
    input('Done! Press Enter to exit...')


if __name__ == '__main__':
    main()
