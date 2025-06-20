# coding=utf-8
# =============================================================================
# Copyright (c) 2025 FLIR Integrated Imaging Solutions, Inc. All Rights Reserved.
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
# ImageUtilityStereo.py shows how to acquire image sets from the BumbleBee stereo
# camera, compute 3D information using  ImageUtilityStereo functions,
# and display a depth image (created via CreateDepthImage) along with interactive
# measurements.
#
# Please leave us feedback at: https://www.surveymonkey.com/r/TDYMVAPI
# More source code examples at: https://github.com/Teledyne-MV/Spinnaker-Examples

import os
import PySpin
import sys
import platform
import cv2
import numpy as np
import time

# -----------------------------------------------------------------------------
# Global variables for mouse callback and image display
# -----------------------------------------------------------------------------
global_rect_left_image_resized = None   # Current displayed (resized) rectified left image
global_depth_image_resized = None       # Current displayed (resized) depth image (from CreateDepthImage)
global_disp_image_ptr = None            # Original PySpin disparity image pointer (for API calls)
global_stereo_params = None             # Current stereo camera parameters
global_fail_counter = 0
text_distance = None
text_point = None
clicked_points = []                     # List to store clicked points as tuples: ((x_disp, y_disp), imagePixel, stereo3DPoint)
global_mouse_x = 0
global_mouse_y = 0
global_measurement = None
MAX_DEPTH = 20000                       # max depth for the depth image  in mm. Anything more than the MAX_DEPTH is set to invalid
# -----------------------------------------------------------------------------
# Mouse Callback Function
# -----------------------------------------------------------------------------
def mouse_callback(event, x, y, flags, param):
    """
    Mouse callback for the 'Rectified Left Image' window.
    Uses ImageUtilityStereo functions to:
      - Compute the 3D point for the pixel under the mouse (using Compute3DPointFromPixel)
      - Compute the distance from the camera to that pixel (using ComputeDistanceToPoint)
      - When two points are clicked, compute the distance between them (using ComputeDistanceBetweenPoints)
    """
    global global_rect_left_image_resized, global_disp_image_ptr, global_stereo_params, clicked_points, text_distance, text_point, global_mouse_x, global_mouse_y, global_measurement

    # Since the displayed left image is resized (e.g., half-size), convert display coordinates back to original coordinates.
    scale = 2  # Adjust if your resize factor is different.
    global_mouse_x = int(x)
    global_mouse_y = int(y)
    x_orig = int(x * scale)
    y_orig = int(y * scale)

    if global_rect_left_image_resized is None or global_disp_image_ptr is None:
        return

    # Create a copy for overlaying text and graphics.
    img_display = global_rect_left_image_resized.copy()

    # Retrieve the raw disparity image as a NumPy array.
    try:
        disp_array = global_disp_image_ptr.GetNDArray() #.astype(np.uint16)
    except Exception as e:
        cv2.imshow('Rectified Left Image', img_display)
        return

    # Ensure the coordinates are within bounds.
    if y_orig >= disp_array.shape[0] or x_orig >= disp_array.shape[1]:
        cv2.imshow('Rectified Left Image', img_display)
        return

    # Get the disparity value (uint16_t) at the original coordinate.
    disp_value = disp_array[y_orig, x_orig]

    # Compute the 3D point from the disparity value.
    # Create an instance of Stereo3DPoint
    stereo3DPoint = PySpin.Stereo3DPoint()
    imagePixel = PySpin.ImagePixel()
    imagePixel.u = x_orig
    imagePixel.v = y_orig
    stereo3DPoint.pixel = imagePixel
    success_pt = True 
    PySpin.ImageUtilityStereo.Compute3DPointFromPixel(int(disp_value), global_stereo_params, stereo3DPoint)

    # Prepare to compute distance from camera to this pixel.
    success_dist = True
    distance = PySpin.ImageUtilityStereo.ComputeDistanceToPoint(global_disp_image_ptr, global_stereo_params, imagePixel)
    
    # On mouse movement, overlay the computed 3D coordinates and distance.
    if event == cv2.EVENT_MOUSEMOVE:
        text_point = f"(X: {stereo3DPoint.x:.2f}, Y: {stereo3DPoint.y:.2f}, Z: {stereo3DPoint.z:.2f})"
        text_distance = f"Dist: {distance: .2f} m"
    

    # On left mouse button click, record the point for later distance measurement between two points.
    if event == cv2.EVENT_LBUTTONDOWN:
        clicked_points.append(((x, y), imagePixel, stereo3DPoint if success_pt else None))
        if len(clicked_points) == 2:
            pt1_disp, imgPixel1, pt1_3d = clicked_points[0]
            pt2_disp, imgPixel2, pt2_3d = clicked_points[1]
            if pt1_3d is not None and pt2_3d is not None:
                dist_between = PySpin.ImageUtilityStereo.ComputeDistanceBetweenPoints(global_disp_image_ptr, global_stereo_params, imgPixel1, imgPixel2)
                if dist_between:
                    print(f"Distance between points {pt1_disp} and {pt2_disp}: {dist_between:.2f}")
                    global_measurement = (((pt1_disp[0], pt1_disp[1]), (pt2_disp[0], pt2_disp[1])), dist_between)
                    # Draw a line between the two clicked points.
                    cv2.line(img_display, pt1_disp, pt2_disp, (255, 0, 0), 2)
                    # Annotate the line with the computed distance at its midpoint.
                    mid_x = (pt1_disp[0] + pt2_disp[0]) // 2
                    mid_y = (pt1_disp[1] + pt2_disp[1]) // 2
                    cv2.putText(img_display, f"{dist_between:.2f}", (mid_x, mid_y), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 2)
            clicked_points = []
    
# -----------------------------------------------------------------------------
# Helper Functions (unchanged from your original code)
# -----------------------------------------------------------------------------
def print_stereo_params(stereo_params):
    print('coordinateOffset: ', stereo_params.coordinateOffset)
    print('baseline: ', stereo_params.baseline)
    print('focalLength: ', stereo_params.focalLength)
    print('principalPointU: ', stereo_params.principalPointU)
    print('principalPointV: ', stereo_params.principalPointV)
    print('disparityScaleFactor: ', stereo_params.disparityScaleFactor)
    print('invalidDataValue: ', stereo_params.invalidDataValue)
    print('invalidDataFlag: ', stereo_params.invalidDataFlag)

def get_node_value(nodemap, node_name, data_type='float'):
    node = PySpin.CFloatPtr(nodemap.GetNode(node_name)) if data_type == 'float' else (
           PySpin.CIntegerPtr(nodemap.GetNode(node_name)) if data_type == 'int' else
           PySpin.CBooleanPtr(nodemap.GetNode(node_name)))
    if not PySpin.IsAvailable(node) or not PySpin.IsReadable(node):
        print(f"Node {node_name} not available or not readable.")
        return None
    return node.GetValue()

def configure_stereo_params(stereo_params, nodemap):
    coord_offset = get_node_value(nodemap, 'Scan3dCoordinateOffset', 'float')
    if coord_offset is not None:
        stereo_params.coordinateOffset = coord_offset
    else:
        print("Failed to retrieve Scan3dCoordinateOffset.")

    baseline = get_node_value(nodemap, 'Scan3dBaseline', 'float')
    if baseline is not None:
        stereo_params.baseline = baseline
    else:
        print("Failed to retrieve Scan3dBaseline.")

    focal_length = get_node_value(nodemap, 'Scan3dFocalLength', 'float')
    if focal_length is not None:
        stereo_params.focalLength = focal_length
    else:
        print("Failed to retrieve Scan3dFocalLength.")

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
    try:
        np_image = pyspin_image.GetNDArray()
        pixel_format = pyspin_image.GetPixelFormatName()
        if pixel_format == 'RGB8':
            np_image = cv2.cvtColor(np_image, cv2.COLOR_RGB2BGR)
        elif pixel_format == 'Mono16':
            np_image = np_image.astype(np.uint16)
        else:
            print(f'Unhandled pixel format: {pixel_format}')
        return np_image
    except PySpin.SpinnakerException as ex:
        print('Error converting image: %s' % ex)
        return None

# -----------------------------------------------------------------------------
# Image Acquisition and Processing
# -----------------------------------------------------------------------------
def acquire_images(cam, nodemap, nodemap_tldevice):
    global global_rect_left_image_resized, global_depth_image_resized, global_disp_image_ptr, global_stereo_params, global_mouse_x, global_mouse_y, text_distance

    try:
        result = True
        cam.BeginAcquisition()
        print('Acquiring images...')
        device_serial_number = ''
        node_device_serial_number = PySpin.CStringPtr(nodemap_tldevice.GetNode('DeviceSerialNumber'))
        if PySpin.IsReadable(node_device_serial_number):
            device_serial_number = node_device_serial_number.GetValue()
            print('Device serial number retrieved as %s...' % device_serial_number)

        # Assume stereo camera for this example.
        is_stereo_camera = True
        if is_stereo_camera:
            print('Camera %s is a stereo camera' % device_serial_number)
        else:
            print('Camera %s is not a valid stereo camera. Skipping...' % device_serial_number)
            return True

        # Enable only the rectified left image and disparity image streams.
        stream_transmit_flags = {
            'rectSensor1TransmitEnabled': True,
            'disparityTransmitEnabled': True
        }

        # Get and configure stereo parameters.
        stereo_params = PySpin.StereoCameraParameters()
        configure_stereo_params(stereo_params, nodemap)
        print('*** STEREO PARAMETERS *** ')
        # print_stereo_params(stereo_params)
        global_stereo_params = stereo_params

        print('Starting continuous image acquisition. Press "s" to save images, "Esc" to exit.')
        while True:
            try:
                image_list = cam.GetNextImageSync(2000)
                if not validate_image_list(stream_transmit_flags, image_list):
                    print('Failed to get next image set.')
                    continue

                # Retrieve the rectified left image.
                rect_left_image = image_list.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1)
                if rect_left_image is None or rect_left_image.IsIncomplete():
                    print('Rectified left image is incomplete or unavailable.')
                    continue

                # Retrieve the raw disparity image.
                disparity_image = image_list.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1)
                if disparity_image is None or disparity_image.IsIncomplete():
                    print('Disparity image is incomplete or unavailable.')
                    continue

                # Filter the disparity image if desired.
                disparity_image = PySpin.ImageUtilityStereo.FilterSpeckles(
                    disparity_image,
                    1000,
                    4,
                    stereo_params.disparityScaleFactor,
                    stereo_params.invalidDataValue
                )

                # Create a depth image using the disparity image.
                depthrangemin = 0 
                depthrangemax = 0 
                depth_range = []
                depth_range.append(depthrangemin)
                depth_range.append(depthrangemax)
                
                depth_image_ptr = PySpin.ImageUtilityStereo.CreateDepthImage(disparity_image, stereo_params, int(stereo_params.invalidDataValue), depth_range).GetNDArray().copy()
                depthrangemin, depthrangemax = depth_range
                
                # Convert images to OpenCV format.
                rect_left_cv_image = convert_to_cv_image(rect_left_image)
                # depth_cv_image = convert_to_cv_image(depth_image_ptr)
                depth_cv_image = depth_image_ptr
                #depth_cv_image = convert_to_cv_image(disparity_image)
                if rect_left_cv_image is None or depth_cv_image is None:
                    print('Failed to convert images to OpenCV format.')
                    continue

                # Resize the rectified left image for display (e.g., half-size).
                height, width = rect_left_cv_image.shape[:2]
                rect_left_cv_image_resized = cv2.resize(rect_left_cv_image, (width // 2, height // 2))
                global_rect_left_image_resized = rect_left_cv_image_resized

                # Resize the depth image similarly for display.
                depth_cv_image_resized = cv2.resize(depth_cv_image, (width // 2, height // 2))
                mask_valid = (depth_cv_image_resized >= 0) & (depth_cv_image_resized <= MAX_DEPTH)
                depth_clipped = np.clip(depth_cv_image_resized, 0, MAX_DEPTH)
                depth_normalized = cv2.convertScaleAbs(depth_clipped, alpha=(255.0/MAX_DEPTH))
                depth_normalized = depth_normalized.astype(np.uint8)
                depth_colormap = cv2.applyColorMap(depth_normalized, cv2.COLORMAP_JET)
                depth_colormap[~mask_valid] = (0, 0, 0)

                # Save the original disparity image pointer for 3D computations.
                global_disp_image_ptr = disparity_image

                # Create or update the display window and set the mouse callback (only needs to be set once).
                cv2.namedWindow('Rectified Left Image')
                cv2.setMouseCallback('Rectified Left Image', mouse_callback)

                # Display images.
                cv2.putText(rect_left_cv_image_resized, text_point, (global_mouse_x, global_mouse_y), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)
                cv2.putText(rect_left_cv_image_resized, text_distance, (global_mouse_x, global_mouse_y + 20), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)
                if global_measurement is not None:
                    (pt1, pt2), d = global_measurement
                    cv2.line(rect_left_cv_image_resized, pt1, pt2, (0, 0, 255), 2)
                    mid_x = (pt1[0] + pt2[0]) // 2
                    mid_y = (pt1[1] + pt2[1]) // 2
                    cv2.putText(rect_left_cv_image_resized, f"{d:.2f}m", (mid_x, mid_y), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 2)
                cv2.imshow('Rectified Left Image', rect_left_cv_image_resized)
                cv2.imshow('Depth Image', depth_colormap)

                # Handle key events.
                key = cv2.waitKey(1) & 0xFF
                if key == ord('s'):
                    filename_prefix = 'StereoAcquisition_%s' % device_serial_number
                    save_rect_left_image(rect_left_cv_image, filename_prefix)
                    save_depth_image(depth_cv_image, filename_prefix)
                    print('Images saved.')
                elif key == 27:  # Esc key.
                    print('Exit key pressed.')
                    break

                image_list.Release()
            except PySpin.SpinnakerException as ex:
                print('Spinnaker Exception: %s' % ex)
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
    filename = '%s_RectSensor1.png' % filename_prefix
    cv2.imwrite(filename, image)
    print('Rectified left image saved to %s' % filename)

def save_depth_image(image, filename_prefix):
    filename = '%s_DepthImage.png' % filename_prefix
    cv2.imwrite(filename, image)
    print('Depth image saved to %s' % filename)

def run_single_camera(cam):
    try:
        result = True
        nodemap_tldevice = cam.GetTLDeviceNodeMap()
        cam.Init()
        nodemap = cam.GetNodeMap()
        result &= acquire_images(cam, nodemap, nodemap_tldevice)
        cam.DeInit()
    except PySpin.SpinnakerException as ex:
        print('Error: %s' % ex)
        result = False
    return result

def main():
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
        print('Camera %d example complete...\n' % i)
        del cam

    cam_list.Clear()
    system.ReleaseInstance()
    return result

if __name__ == '__main__':
    if main():
        sys.exit(0)
    else:
        sys.exit(1)
