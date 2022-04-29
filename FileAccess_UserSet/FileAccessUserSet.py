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
# This example shows how to upload and download user set files. It uses the
# quickspin syntax for simplicity, to make it easier to focus on the new concepts.
#
# When downloading a user set, this script changes the exposure time
# to 2000us and saves the new changes in UserSet0. This ensures that there is
# a difference between the default user set and uset set 0 allowing for easier
# testing. Then the example downloads user set 0 and saves it into a user
# provided path. This demonstrates the read loop pattern required for reading
# files from the camera.
# When uploading a user set, this script demonstrates the write loop
# pattern required for writing, including the necessary padding for
# the last chunk of data.
#
# USAGE: python FileAccessUserSet.py {upload/download} {file}
#   where {upload/download} is either upload or download
#   and {file} is the path to the file to be uploaded or
#   the path where the downloaded user set is saved to.

import sys
import numpy as np
import PySpin


def set_exposure(cam: PySpin.CameraPtr, exposure: int) -> bool:
    """
    Sets the camera exposure to the given `exposure` value.
    Returns whether the operation was successful or not.
    """
    try:
        cam.ExposureAuto.SetValue(PySpin.ExposureAuto_Off)
        cam.ExposureTime.SetValue(exposure)
        print('Set exposure time to %sus' % exposure)

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return False

    return True


def save_to_user_set_zero(cam: PySpin.CameraPtr) -> bool:
    """
    Saves the given camera's current settings to UserSet0.
    Returns whether the operation was successful or not.
    """
    try:
        cam.UserSetSelector.SetValue(PySpin.UserSetSelector_UserSet0)
        cam.UserSetSave.Execute()
        print('Saved to UserSet0')

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return False

    return True


def open_file_to_write(cam: PySpin.CameraPtr) -> bool:
    """
    Opens the currently selected camera file for writing.
    Returns whether the operation was successful or not.
    """
    try:
        cam.FileOperationSelector.SetValue(PySpin.FileOperationSelector_Open)
        cam.FileOpenMode.SetValue(PySpin.FileOpenMode_Write)
        cam.FileOperationExecute.Execute()
        # Make sure that the operation was successful
        if cam.FileOperationStatus.GetValue() != PySpin.FileOperationStatus_Success:
            print('ERROR: Failed to open file for writing')
            return False
        print('Opened file for writing')

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return False

    return True


def open_file_to_read(cam: PySpin.CameraPtr) -> bool:
    """
    Opens the currently selected camera file for reading.
    Returns whether the operation was successful or not.
    """
    try:
        cam.FileOperationSelector.SetValue(PySpin.FileOperationSelector_Open)
        cam.FileOpenMode.SetValue(PySpin.FileOpenMode_Read)
        cam.FileOperationExecute.Execute()
        # Make sure that the operation was successful
        if cam.FileOperationStatus.GetValue() != PySpin.FileOperationStatus_Success:
            print('ERROR: Failed to open file for reading')
            return False
        print('Opened file for reading')

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return False

    return True


def close_cam_file(cam: PySpin.CameraPtr) -> bool:
    """
    Closes the currently selected camera file.
    Returns whether the operation was successful or not.
    """
    try:
        cam.FileOperationSelector.SetValue(PySpin.FileOperationSelector_Close)
        cam.FileOperationExecute.Execute()
        # Make sure that the operation was successful
        if cam.FileOperationStatus.GetValue() != PySpin.FileOperationStatus_Success:
            print('ERROR: Failed to close camera file')
            return False
        print('Closed the camera file')

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return False

    return True


def download_user_set(cam: PySpin.CameraPtr, path: str) -> bool:
    """
    This is the main function for downloading a user set. It first sets
    the camera exposure to 2000us, then saves the settings to UserSet0.
    This is to ensure that there is a difference between the default
    user set and the downloaded one. This function demonstrates the
    read loop for reading a file from the camera (UserSet0) and saving it
    to the location at the given `path`.
    Returns whether the operation was successful or not.
    """
    # Change the camera exposure and save to UserSet0
    if not set_exposure(cam, 2000):
        return False
    if not save_to_user_set_zero(cam):
        return False

    # Download UserSet0
    try:
        # Open UserSet0 for reading
        cam.FileSelector.SetValue(PySpin.FileSelector_UserSet0)
        if not open_file_to_read(cam):
            return False

        # Set FileAccessLength to FileAccessBufferNode length to speed up
        if cam.FileAccessLength.GetValue() < cam.FileAccessBuffer.GetLength():
            cam.FileAccessLength.SetValue(cam.FileAccessBuffer.GetLength())
        print('FileAccessLength:', cam.FileAccessLength.GetValue())

        # Make sure we start from the beginning of the file
        cam.FileAccessOffset.SetValue(0)

        # Find the number of required iterations
        intermediate_buffer_size = cam.FileAccessLength.GetValue()
        num_bytes_to_read = cam.FileSize.GetValue()
        num_iter, remainder = divmod(num_bytes_to_read, intermediate_buffer_size)
        if remainder != 0:
            num_iter += 1
        print('Number of bytes to read:', num_bytes_to_read)
        print('Intermediate buffer size:', intermediate_buffer_size)
        print('Number of required iterations:', num_iter)

        # Read loop
        data = []
        for _ in range(num_iter):
            # Read the data into camera buffer
            cam.FileOperationSelector.SetValue(PySpin.FileOperationSelector_Read)
            cam.FileOperationExecute.Execute()
            # Make sure that the operation was successful
            if cam.FileOperationStatus.GetValue() != PySpin.FileOperationStatus_Success:
                print('ERROR: Failed to read file')
                return False
            # Read the data into system buffer
            size_read = cam.FileOperationResult.GetValue()
            data.append(cam.FileAccessBuffer.Get(size_read))
            print('Finished reading', size_read, 'bytes')

        # Close the file on the camera and write the data to disk
        close_cam_file(cam)
        np.asarray(data).tofile(path)

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return False

    return True


def delete_cam_file(cam: PySpin.CameraPtr) -> bool:
    """
    Deletes the currently selected camera file.
    Returns whether the operation was successful or not.
    """
    try:
        cam.FileOperationSelector.SetValue(PySpin.FileOperationSelector_Delete)
        cam.FileOperationExecute.Execute()
        # Make sure that the operation was successful
        if cam.FileOperationStatus.GetValue() != PySpin.FileOperationStatus_Success:
            print('ERROR: Failed to delete camera file')
            return False
        print('Deleted existing camera file')

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return False

    return True


def upload_user_set(cam: PySpin.CameraPtr, path: str) -> bool:
    """
    This is the main function for uploading a user set. It
    demonstrates the write loop for writing a file to the
    camera by uploading the file given at `path` to UserSet0.
    Returns whether the operation was successful or not.
    """
    # Read the data into the system buffer
    data = np.fromfile(path, dtype=np.ubyte)

    try:
        cam.FileSelector.SetValue(PySpin.FileSelector_UserSet0)
        # Delete the file if it exists
        if cam.FileSize.GetValue() > 0:
            if not delete_cam_file(cam):
                return False
        # Open UserSet0 for writing
        if not open_file_to_write(cam):
            return False

        # Set FileAccessLength to FileAccessBufferNode length to speed up
        if cam.FileAccessLength.GetValue() < cam.FileAccessBuffer.GetLength():
            cam.FileAccessLength.SetValue(cam.FileAccessBuffer.GetLength())
        print('FileAccessLength:', cam.FileAccessLength.GetValue())

        # Make sure we start from the beginning of the file
        cam.FileAccessOffset.SetValue(0)

        # Split the data into chunks that can be written
        intermediate_buffer_size = cam.FileAccessLength.GetValue()
        num_remaining_bytes = data.size
        print('Number of bytes to write:', data.size)
        print('Intermediate buffer size:', intermediate_buffer_size)

        def split_data(chunk_size: int):
            """
            Returns a generator where each element is a
            list of `chunk_size` elements of `data`
            """
            for i in range(0, len(data), chunk_size):
                yield data[i:i+chunk_size]

        # Pad the data if the last chunk is not a multiple of 4
        last_chunk_size = data.size % intermediate_buffer_size
        num_paddings = last_chunk_size % 4
        pad = [255] * num_paddings
        data = np.append(data, pad).astype(np.ubyte)

        # Write loop
        for chunk in split_data(intermediate_buffer_size):
            # Write chunk to camera buffer
            cam.FileAccessBuffer.Set(chunk)
            # Make sure that we don't write garbage to the camera
            if intermediate_buffer_size > num_remaining_bytes:
                cam.FileAccessLength.SetValue(num_remaining_bytes)

            # Write to the camera file
            cam.FileOperationSelector.SetValue(PySpin.FileOperationSelector_Write)
            cam.FileOperationExecute.Execute()
            # Make sure that the operation was successful
            if cam.FileOperationStatus.GetValue() != PySpin.FileOperationStatus_Success:
                print('ERROR: Failed to write to file')
                return False

            size_written = cam.FileOperationResult.GetValue()
            num_remaining_bytes -= size_written
            print('Wrote {} bytes, remaining bytes: {}'.format(size_written, num_remaining_bytes))

        # Close the file
        close_cam_file(cam)

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return False

    return True


def parse_args(args: list) -> list:
    """
    Parses the given arguments.
    Returns None and prints the usage instructions if the arguments are incorrect.
    Otherwise returns the list of arguments.
    """
    usage = 'USAGE: python FileAccessUserSet.py {upload/download} {file}'\
        '\n\twhere {upload/download} is either upload or download '\
        'and {file} is the path to the file to be uploaded or '\
        'the path where the downloaded user set is saved to.'

    # Make sure that there are only 2 arguments
    if len(args) != 2:
        print(usage)
        return None
    # Make sure that we're either uploading or downloading
    if args[0].lower() != 'upload' and args[0].lower() != 'download':
        print(usage)
        return None
    return args


def main(args: list) -> None:
    """
    The main function of the example.
    Parses the given arguments and runs the example for
    the first camera in the camera list.
    """
    args = parse_args(args)
    if args is None:  # Args are incorrect
        return

    # Setup the system and camera
    system = PySpin.System.GetInstance()
    cam_list = system.GetCameras()
    cam = cam_list[0]
    cam.Init()
    result = True

    if args[0] == 'download':
        result &= download_user_set(cam, args[1])
    elif args[0] == 'upload':
        result &= upload_user_set(cam, args[1])

    # Clean up
    del cam
    cam_list.Clear()
    system.ReleaseInstance()

    if result:
        print('Example completed successfully!')
    else:
        print('Example completed with some errors.')
    input('Done! Press Enter to exit...')


if __name__ == "__main__":
    main(sys.argv[1:])
