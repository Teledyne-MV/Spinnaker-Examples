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
# StereoOpen3D.py shows how to acquire image sets from a bumblebee stereo camera,
# convert the disparity image to a point cloud using PySpin, and then use Open3D
# to process and display the point cloud.
#
# Please leave us feedback at: https://www.surveymonkey.com/r/TDYMVAPI
# More source code examples at: https://github.com/Teledyne-MV/Spinnaker-Examples
# Need help? Check out our forum at: https://teledynevisionsolutions.zendesk.com/hc/en-us/community/topics

import os
import PySpin
import sys
import platform
import numpy as np
import open3d as o3d
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

def configure_point_cloud_params(point_cloud_params, nodemap):
    """
    Configure point cloud parameters by reading relevant nodes from the nodemap.

    Args:
        point_cloud_params (PySpin.PointCloudParameters): The point cloud parameters object to configure.
        nodemap (PySpin.INodeMap): Nodemap object containing relevant camera settings.

    Returns:
        None
    """
    stereo_width = get_node_value(nodemap, 'StereoWidth', 'int')
    if stereo_width is None:
        print("Failed to retrieve StereoWidth. Setting to a default value of 2048.")
        stereo_width = 2048

    stereo_height = get_node_value(nodemap, 'StereoHeight', 'int')
    if stereo_height is None:
        print("Failed to retrieve StereoHeight. Setting to a default value of 1536.")
        stereo_height = 1536

    point_cloud_params.decimationFactor = 8
    point_cloud_params.ROIImageLeft = 0
    point_cloud_params.ROIImageTop = 0
    point_cloud_params.ROIImageRight = stereo_width - 1
    point_cloud_params.ROIImageBottom = stereo_height - 1
    point_cloud_params.ROIWorldCoordinatesXMin = -10
    point_cloud_params.ROIWorldCoordinatesXMax = 10
    point_cloud_params.ROIWorldCoordinatesYMin = -10
    point_cloud_params.ROIWorldCoordinatesYMax = 10
    point_cloud_params.ROIWorldCoordinatesZMin = 0
    point_cloud_params.ROIWorldCoordinatesZMax = 20

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

def get_point_cloud(disparity_image, rect_left_image, point_cloud_params, stereo_params, pcd):
    """
    Generate a 3D point cloud from disparity and rectified left images using Spinnaker utilities.

    Args:
        disparity_image (PySpin.Image): The disparity image acquired from the camera.
        rect_left_image (PySpin.Image): The rectified left image acquired from the camera.
        point_cloud_params (PySpin.PointCloudParameters): Point cloud configuration parameters.
        stereo_params (PySpin.StereoCameraParameters): Stereo camera configuration parameters.
        pcd (open3d.geometry.PointCloud): Open3D point cloud object to update.

    Returns:
        None
    """
    # Compute point cloud
    point_cloud = PySpin.ImageUtilityStereo.ComputePointCloud(disparity_image, rect_left_image, point_cloud_params, stereo_params)
    n_points = point_cloud.GetNumPoints()

    # Extract points and colors into numpy arrays
    points = np.array([[point_cloud.GetPoint(i).x, point_cloud.GetPoint(i).y, point_cloud.GetPoint(i).z] for i in range(n_points)], dtype=np.float64)
    colors = np.array([[point_cloud.GetPoint(i).r, point_cloud.GetPoint(i).g, point_cloud.GetPoint(i).b] for i in range(n_points)], dtype=np.float64)

    # Normalize colors in place (instead of iterating over them one by one)
    colors /= 255.0  # Normalize to [0, 1]

    # Update the Open3D PointCloud in-place
    pcd.points = o3d.utility.Vector3dVector(points)
    pcd.colors = o3d.utility.Vector3dVector(colors)

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

    print('*** IMAGE ACQUISITION ***\n')
    try:
        result = True

        cam.BeginAcquisition()
        print('Acquiring images...')
        device_serial_number = ''
        node_device_serial_number = PySpin.CStringPtr(nodemap_tldevice.GetNode('DeviceSerialNumber'))
        if PySpin.IsReadable(node_device_serial_number):
            device_serial_number = node_device_serial_number.GetValue()
            print('Device serial number retrieved as %s...' % device_serial_number)

        is_stereo_camera = PySpin.ImageUtilityStereo.IsStereoCamera(cam)
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
        
        point_cloud_params = PySpin.PointCloudParameters()
        configure_point_cloud_params(point_cloud_params, nodemap)

        # Start continuous acquisition loop
        print('Starting continuous image acquisition.')

        # Initialize Open3D visualization
        vis = o3d.visualization.VisualizerWithKeyCallback()
        vis.create_window(window_name='Open3D Point Cloud Viewer')
        pcd = o3d.geometry.PointCloud()
        is_first_frame = True

        def exit_callback(vis):
            print('Exit key pressed.')
            vis.destroy_window()
            raise KeyboardInterrupt

        vis.register_key_callback(27, exit_callback)  # 27 is ASCII for Esc
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

                # Compute point cloud
                get_point_cloud(disparity_image, rect_left_image, point_cloud_params, stereo_params, pcd)

                # Update the Open3D PointCloud
                if is_first_frame:
                    vis.add_geometry(pcd)
                    is_first_frame = False
                else:
                    vis.update_geometry(pcd)
                    
                vis.poll_events()
                vis.update_renderer()

            except PySpin.SpinnakerException as ex:
                continue

            except KeyboardInterrupt:
                print("Exiting acquisition loop...")
                break

        cam.EndAcquisition()
        vis.destroy_window()

    except PySpin.SpinnakerException as ex:
        print('Error: %s' % ex)
        return False

    return result

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

