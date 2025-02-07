# coding=utf-8
# =============================================================================
# Copyright (c) 2024 FLIR Integrated Imaging Solutions, Inc. All Rights Reserved.
#
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
# =============================================================================
#
# StereoOpenCV.py shows how to acquire image sets from a stereo
# camera, visualize, and save them using OpenCV.
#
# Please leave us feedback at: https://www.surveymonkey.com/r/TDYMVAPI
# More source code examples at: https://github.com/Teledyne-MV/Spinnaker-Examples
# Need help? Check out our forum at: https://teledynevisionsolutions.zendesk.com/hc/en-us/community/topics

import os
import PySpin
import sys
import platform
import cv2
import numpy as np
import time

def print_stereo_params(stereo_params):
    """
    Print stereo camera parameters.

    Args:
        stereo_params (PySpin.StereoCameraParameters): The stereo camera parameters object.
    """
    print('coordinateOffset: ', stereo_params.coordinateOffset)
    print('baseline: ', stereo_params.baseline)
    print('focalLength: ', stereo_params.focalLength)
    print('principalPointU: ', stereo_params.principalPointU)
    print('principalPointV: ', stereo_params.principalPointV)
    print('disparityScaleFactor: ', stereo_params.disparityScaleFactor)
    print('invalidDataValue: ', stereo_params.invalidDataValue)
    print('invalidDataFlag: ', stereo_params.invalidDataFlag)

def get_node_value(nodemap, node_name, data_type='float'):
    """
    Retrieve a node value from the camera's nodemap.

    Args:
        nodemap (PySpin.INodeMap): Nodemap object for the camera.
        node_name (str): The name of the node to retrieve.
        data_type (str): The expected data type of the node's value.
                         Supported values are 'float', 'int', 'bool'.

    Returns:
        float|int|bool|None: The value of the node if successful, otherwise None.
    """
    node = PySpin.CFloatPtr(nodemap.GetNode(node_name)) if data_type == 'float' else (
           PySpin.CIntegerPtr(nodemap.GetNode(node_name)) if data_type == 'int' else
           PySpin.CBooleanPtr(nodemap.GetNode(node_name)))
    if not PySpin.IsAvailable(node) or not PySpin.IsReadable(node):
        print(f"Node {node_name} not available or not readable.")
        return None
    if data_type == 'float':
        return node.GetValue()
    elif data_type == 'int':
        return node.GetValue()
    elif data_type == 'bool':
        return node.GetValue()
    else:
        print(f"Unsupported data type for node {node_name}.")
        return None

def configure_stereo_params(stereo_params, nodemap):
    """
    Configure stereo camera parameters by reading corresponding nodes from the nodemap.

    Args:
        stereo_params (PySpin.StereoCameraParameters): The stereo parameters object to fill.
        nodemap (PySpin.INodeMap): Nodemap object from which to read parameters.

    Raises:
        None: Prints error messages if retrieval of parameters fails.
    """
    coord_offset = get_node_value(nodemap, 'Scan3dCoordinateOffset', 'float')
    if coord_offset is not None:
        stereo_params.coordinateOffset = coord_offset
    else:
        print("Failed to retrieve Scan3dCoordinateOffset.")

    baseline = get_node_value(nodemap, 'Scan3dBaseline', 'float')
    if baseline is not None:
        stereo_params.baseline = baseline
    else:
        print("Failed to retrieve Scan3DBaseline.")

    focal_length = get_node_value(nodemap, 'Scan3dFocalLength', 'float')
    if focal_length is not None:
        stereo_params.focalLength = focal_length
    else:
        print("Failed to retrieve Scan3DFocalLength.")

    principal_point_u = get_node_value(nodemap, 'Scan3dPrincipalPointU', 'float')
    if principal_point_u is not None:
        stereo_params.principalPointU = principal_point_u
    else:
        print("Failed to retrieve Scan3dPrincipalPointU.")

    principal_point_v = get_node_value(nodemap, 'Scan3dPrincipalPointV', 'float')
    if principal_point_v is not None:
        stereo_params.principalPointV = principal_point_v
    else:
        print("Failed to retrieve Scan3dPrincipalPointV.")

    disparity_scale_factor = get_node_value(nodemap, 'Scan3dCoordinateScale', 'float')
    if disparity_scale_factor is not None:
        stereo_params.disparityScaleFactor = disparity_scale_factor
    else:
        print("Failed to retrieve Scan3dCoordinateScale.")

    invalid_data_value = get_node_value(nodemap, 'Scan3dInvalidDataValue', 'float')
    if invalid_data_value is not None:
        stereo_params.invalidDataValue = invalid_data_value
    else:
        print("Failed to retrieve Scan3dInvalidDataValue.")

    invalid_data_flag = get_node_value(nodemap, 'Scan3dInvalidDataFlag', 'bool')
    if invalid_data_flag is not None:
        stereo_params.invalidDataFlag = invalid_data_flag
    else:
        print("Failed to retrieve Scan3dInvalidDataFlag.")

def validate_image_list(stream_transmit_flags, image_list):
    """
    Validate that all images in the list are complete according to the stream transmit flags.

    Args:
        stream_transmit_flags (dict): Dictionary indicating which image streams are enabled.
        image_list (PySpin.ImageList): List of images acquired from the camera.

    Returns:
        bool: True if image list is valid (complete), False otherwise.
    """
    is_image_list_incomplete = False

    if stream_transmit_flags['rectSensor1TransmitEnabled']:
        image = image_list.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1)
        is_image_list_incomplete |= ((image is None) or image.IsIncomplete())

    if stream_transmit_flags['disparityTransmitEnabled']:
        image = image_list.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1)
        is_image_list_incomplete |= ((image is None) or image.IsIncomplete())

    if is_image_list_incomplete:
        print('Image List is incomplete.')

    return not is_image_list_incomplete

def print_device_info(nodemap):
    """
    Print device information from the transport layer device nodemap.

    Args:
        nodemap (PySpin.INodeMap): The device nodemap containing device information.

    Returns:
        bool: True if device information was retrieved and printed successfully, False otherwise.
    """
    print('*** DEVICE INFORMATION ***\n')

    try:
        result = True
        node_device_information = PySpin.CCategoryPtr(nodemap.GetNode('DeviceInformation'))

        if PySpin.IsReadable(node_device_information):
            features = node_device_information.GetFeatures()
            for feature in features:
                node_feature = PySpin.CValuePtr(feature)
                print('%s: %s' % (node_feature.GetName(),
                                  node_feature.ToString() if PySpin.IsReadable(node_feature) else 'Node not readable'))

        else:
            print('Device control information not readable.')
            result = False

    except PySpin.SpinnakerException as ex:
        print('Error: %s' % ex)
        result = False

    return result

def convert_to_cv_image(pyspin_image):
    """
    Convert a PySpin image to an OpenCV BGR or grayscale image for further processing.

    Args:
        pyspin_image (PySpin.Image): The image acquired from the camera.

    Returns:
        numpy.ndarray or None: The converted OpenCV image if successful, None otherwise.
    """
    try:
        np_image = pyspin_image.GetNDArray()
        pixel_format = pyspin_image.GetPixelFormatName()

        if pixel_format == 'RGB8':
            # The image is in RGB format; convert to BGR for OpenCV
            np_image = cv2.cvtColor(np_image, cv2.COLOR_RGB2BGR)
        elif pixel_format == 'Mono16':
            # The image is in Mono16 format; convert to 8-bit for display
            np_image = (np_image).astype(np.uint16)
        else:
            # Handle other pixel formats if necessary
            print(f'Unhandled pixel format: {pixel_format}')

        return np_image
    except PySpin.SpinnakerException as ex:
        print('Error converting image: %s' % ex)
        return None
    
def acquire_images(cam, nodemap, nodemap_tldevice):
    """
    Acquire images from the camera, process them, and display the results.
    Press 's' to save images and 'Esc' to exit.

    Args:
        cam (PySpin.Camera): The camera object from which to acquire images.
        nodemap (PySpin.INodeMap): The camera nodemap containing camera settings.
        nodemap_tldevice (PySpin.INodeMap): The transport layer device nodemap for device info.

    Returns:
        bool: True if the acquisition process completes without critical failures, False otherwise.
    """
    
    try:
        result = True

        cam.BeginAcquisition()
        print('Acquiring images...')
        device_serial_number = ''
        node_device_serial_number = PySpin.CStringPtr(nodemap_tldevice.GetNode('DeviceSerialNumber'))
        if PySpin.IsReadable(node_device_serial_number):
            device_serial_number = node_device_serial_number.GetValue()
            print('Device serial number retrieved as %s...' % device_serial_number)

        # is_stereo_camera = PySpin.ImageUtilityStereo.IsStereoCamera(cam)
        is_stereo_camera = True
        if is_stereo_camera:
            print('Camera %s is a stereo camera' % device_serial_number)
        else:
            print('Camera %s is not a valid stereo camera. Skipping...' % device_serial_number)
            return True

        # Enable only the rectified left image and disparity image
        stream_transmit_flags = {
            'rectSensor1TransmitEnabled': True,
            'disparityTransmitEnabled': True
        }

        # Get stereo parameters
        stereo_params = PySpin.StereoCameraParameters()
        configure_stereo_params(stereo_params, nodemap)
        
        print('*** STEREO PARAMETERS *** ')
        print_stereo_params(stereo_params)

        # Start continuous acquisition loop
        print('Starting continuous image acquisition. Press "s" to save images, "Esc" to exit.')

        while True:
            try:
                # Retrieve next image set
                image_list = cam.GetNextImageSync(2000)

                if not validate_image_list(stream_transmit_flags, image_list):
                    print('Failed to get next image set.')
                    continue

                # Get rectified left image
                rect_left_image = image_list.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1)
                if rect_left_image is None or rect_left_image.IsIncomplete():
                    print('Rectified left image is incomplete or unavailable.')
                    continue

                # Get disparity image
                disparity_image_raw = image_list.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1)
                if disparity_image_raw is None or disparity_image_raw.IsIncomplete():
                    print('Disparity image is incomplete or unavailable.')
                    continue
                    
                disparity_image = PySpin.ImageUtilityStereo.FilterSpeckles(disparity_image_raw,
                                                                           1000,
                                                                           4,
                                                                           stereo_params.disparityScaleFactor,
                                                                           stereo_params.invalidDataValue)

                # Convert images to OpenCV format
                rect_left_cv_image = convert_to_cv_image(rect_left_image)
                disparity_cv_image = convert_to_cv_image(disparity_image)

                if rect_left_cv_image is None or disparity_cv_image is None:
                    print('Failed to convert images to OpenCV format.')
                    continue

                # Resize left image to half size
                height, width = rect_left_cv_image.shape[:2]
                rect_left_cv_image_resized = cv2.resize(rect_left_cv_image, (width // 2, height // 2))

                # Normalize and color-map the disparity image for visualization
                disparity_normalized = cv2.normalize(disparity_cv_image, None, 0, 255, cv2.NORM_MINMAX)
                disparity_normalized = disparity_normalized.astype(np.uint8)
                disparity_color = cv2.applyColorMap(disparity_normalized, cv2.COLORMAP_JET)
                disparity_color[disparity_cv_image == 0] = [0, 0, 0]

                # Resize colored disparity image to half size
                disparity_color_resized = cv2.resize(disparity_color, (width // 2, height // 2))

                # Display images
                cv2.imshow('Rectified Left Image', rect_left_cv_image_resized)
                cv2.imshow('Disparity Image', disparity_color_resized)

                # Handle key events
                key = cv2.waitKey(1) & 0xFF
                if key == ord('s'):
                    # Save images and point cloud
                    filename_prefix = 'StereoAcquisition_%s' % device_serial_number
                    save_rect_left_image(rect_left_cv_image, filename_prefix)
                    save_disparity_image(disparity_cv_image, filename_prefix)
                    print('Images and point cloud saved.')
                elif key == 27:  # 'Esc' key
                    print('Exit key pressed.')
                    break

            except PySpin.SpinnakerException as ex:
                continue

            except KeyboardInterrupt:
                print("Exiting acquisition loop...")
                break

        cam.EndAcquisition()
        cv2.destroyAllWindows()

    except PySpin.SpinnakerException as ex:
        print('Error: %s' % ex)
        return False

    return result

def save_rect_left_image(image, filename_prefix):
    """
    Save the rectified left image to disk.

    Args:
        image (numpy.ndarray): The rectified left image in OpenCV format.
        filename_prefix (str): The file prefix for saved images.

    Returns:
        None
    """
    filename = '%s_RectSensor1.png' % filename_prefix
    cv2.imwrite(filename, image)
    print('Rectified left image saved to %s' % filename)

def save_disparity_image(image, filename_prefix):
    """
    Save the raw and color-mapped disparity images to disk.

    Args:
        image (numpy.ndarray): The disparity image in Mono16 or other supported format.
        filename_prefix (str): The file prefix for saved images.

    Returns:
        None
    """
    filename_raw = '%s_Disparity.raw.png' % filename_prefix
    cv2.imwrite(filename_raw, image)
    print('Raw disparity image saved to %s' % filename_raw)

    # Save a normalized and color-mapped version for visualization
    disparity_normalized = cv2.normalize(image, None, 0, 255, cv2.NORM_MINMAX)
    disparity_normalized = disparity_normalized.astype(np.uint8)
    disparity_color = cv2.applyColorMap(disparity_normalized, cv2.COLORMAP_JET)
    filename_color = '%s_Disparity_color.png' % filename_prefix
    cv2.imwrite(filename_color, disparity_color)
    print('Color-mapped disparity image saved to %s' % filename_color)

def run_single_camera(cam):
    """
    Runs acquisition, processing, and display routines for a single camera.

    Args:
        cam (PySpin.Camera): The camera object to process.

    Returns:
        bool: True if the camera's operations complete successfully, False otherwise.
    """
    try:
        result = True
        nodemap_tldevice = cam.GetTLDeviceNodeMap()
        # result &= print_device_info(nodemap_tldevice)
        cam.Init()
        nodemap = cam.GetNodeMap()
        result &= acquire_images(cam, nodemap, nodemap_tldevice)
        cam.DeInit()
    except PySpin.SpinnakerException as ex:
        print('Error: %s' % ex)
        result = False
    return result

def main():
    """
    Main function that initializes the system, discovers cameras, and runs the acquisition
    and processing routine for each camera detected.

    Returns:
        bool: True if all camera operations complete successfully, False otherwise.
    """
    try:
        test_file = open('test.txt', 'w+')
        test_file.close()
        os.remove(test_file.name)
    except IOError:
        print('Unable to write to current directory. Please check permissions.')
        input('Press Enter to exit...')
        return False

    result = True
    system = PySpin.System.GetInstance()
    version = system.GetLibraryVersion()
    print('Library version: %d.%d.%d.%d' % (version.major, version.minor, version.type, version.build))
    cam_list = system.GetCameras()
    num_cameras = cam_list.GetSize()
    print('Number of cameras detected: %d' % num_cameras)

    if num_cameras == 0:
        cam_list.Clear()
        system.ReleaseInstance()
        print('Not enough cameras!')
        input('Done! Press Enter to exit...')
        return False

    for i, cam in enumerate(cam_list):
        print('Running example for camera %d...' % i)
        result &= run_single_camera(cam)
        print('Camera %d example complete... \n' % i)
        del cam  # Release camera reference

    cam_list.Clear()
    system.ReleaseInstance()
    return result

if __name__ == '__main__':
    if main():
        sys.exit(0)
    else:
        sys.exit(1)

