#!/usr/bin/env python
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

import os
import sys
import PySpin
import platform
import cv2
import numpy as np
import time
import torch
import threading

##############################
# Global Variables
##############################
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"
REF_MODEL = None
REF_OPT = None

# Mouse coordinates for hover callback
mouse_x, mouse_y = -1, -1

# Globals for multi-threaded image sharing and FPS measurement
lock = threading.Lock()
exit_flag = False

# Shared acquisition images (SGM stream)
latest_rect_left = None          # Left image from acquisition
latest_disparity_camera = None   # SGM disparity (float32, scaled)

# Variables for refinement inference (snapshot and result)
latest_refined_disp = None   # Refined disparity (float32)
latest_refined_left = None   # Snapshot of left image used for refinement

# FPS variables
camera_fps = 0.0
refined_fps = 0.0

# Depthmap visualization (in meters)
MAX_DEPTH = 10

# Flag to indicate new refined result availability
new_refined_available = False

##############################
# Helper Functions
##############################
def mouse_callback(event, x, y, flags, param):
    global mouse_x, mouse_y
    if event == cv2.EVENT_MOUSEMOVE:
        mouse_x, mouse_y = x, y

def get_node_value(nodemap, node_name, data_type='float'):
    if data_type == 'float':
        node = PySpin.CFloatPtr(nodemap.GetNode(node_name))
    elif data_type == 'int':
        node = PySpin.CIntegerPtr(nodemap.GetNode(node_name))
    else:
        node = PySpin.CBooleanPtr(nodemap.GetNode(node_name))
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

def print_stereo_params(stereo_params):
    print('coordinateOffset:', stereo_params.coordinateOffset)
    print('baseline:', stereo_params.baseline)
    print('focalLength:', stereo_params.focalLength)
    print('principalPointU:', stereo_params.principalPointU)
    print('principalPointV:', stereo_params.principalPointV)
    print('disparityScaleFactor:', stereo_params.disparityScaleFactor)
    print('invalidDataValue:', stereo_params.invalidDataValue)
    print('invalidDataFlag:', stereo_params.invalidDataFlag)

def convert_to_cv_image(pyspin_image):
    try:
        np_image = pyspin_image.GetNDArray()
        pixel_format = pyspin_image.GetPixelFormatName()
        if pixel_format == 'RGB8':
            np_image = cv2.cvtColor(np_image, cv2.COLOR_RGB2BGR)
        elif pixel_format == 'Mono8':
            np_image = cv2.cvtColor(np_image, cv2.COLOR_GRAY2BGR)
        elif pixel_format == 'Mono16':
            np_image = np_image.astype(np.int16)
        else:
            print(f'Unhandled pixel format: {pixel_format}')
        return np_image
    except PySpin.SpinnakerException as ex:
        print('Error converting image:', ex)
        return None

##############################
# Neural Refinement Inference
##############################
def load_refinement_model():
    global REF_MODEL, REF_OPT
    from lib.evaluation_utils import predict
    from lib.model import Refiner
    from lib.options import BaseOptions
    from lib.utils import pad_img, depad_img
    opt = BaseOptions().parse()
    opt.load_checkpoint_path = "checkpoints/sceneflow/net_latest"
    opt.backbone = "vgg13"
    opt.algorithm = "rSGM"
    opt.results_path = "./output/refined"
    opt.rgb = "dummy"
    opt.disparity = "dummy"
    opt.max_disp = 256
    opt.upsampling_factor = 1
    opt.disp_scale = 1
    opt.downsampling_factor = 1
    opt.scale_factor16bit = 1
    device = torch.device("cuda:0" if torch.cuda.is_available() else "cpu")
    opt.device = device
    net = Refiner(opt).to(device=device)
    print("Using Refinement Network: ", net.name)
    if opt.load_checkpoint_path is None:
        raise ValueError("Missing path to checkpoint! Use --load_checkpoint_path flag")
    checkpoint = torch.load(opt.load_checkpoint_path, map_location=device)
    net.load_state_dict(checkpoint["state_dict"])
    net.eval()
    REF_MODEL = net
    REF_OPT = opt
    print("Refinement model loaded.")

@torch.no_grad()
def run_refinement(rgb, sgm_disp):
    from lib.evaluation_utils import predict
    from lib.utils import pad_img, depad_img
    opt = REF_OPT
    rgb_ds = cv2.resize(rgb, (rgb.shape[1] // opt.downsampling_factor, rgb.shape[0] // opt.downsampling_factor))
    height, width = rgb_ds.shape[:2]
    disp_resized = np.expand_dims(cv2.resize(sgm_disp, (rgb_ds.shape[1], rgb_ds.shape[0]), interpolation=cv2.INTER_NEAREST), -1)
    disp_resized[disp_resized > opt.max_disp] = 0
    rgb_padded, pad = pad_img(rgb_ds, height=height, width=width, divisor=32)
    disp_padded, _ = pad_img(disp_resized, height=height, width=width, divisor=32)
    rgb_tensor = torch.from_numpy(rgb_padded).float().permute(2,0,1).unsqueeze(0).to(opt.device)
    disp_tensor = torch.from_numpy(disp_padded).float().permute(2,0,1).unsqueeze(0).to(opt.device)
    o_shape = torch.tensor([height, width]).to(opt.device)
    sample = {"rgb": rgb_tensor, "disp": disp_tensor, "o_shape": o_shape, "pad": pad}
    pred, confidence = predict(REF_MODEL, opt.device, sample, upscale_factor=opt.upsampling_factor)
    pred = pred * (pred.shape[1] / disp_resized.shape[1])
    pred = pred * opt.disp_scale
    refined_disp = pred.squeeze()
    confidence = confidence.squeeze()
    refined_disp[confidence < 0.15] = 0
    return refined_disp

##############################
# Inference Thread
##############################
def refinement_inference_thread():
    global latest_refined_disp, latest_refined_left, refined_fps, exit_flag, new_refined_available
    last_time = time.time()
    while not exit_flag:
        with lock:
            if latest_rect_left is None or latest_disparity_camera is None:
                continue
            refined_left_snapshot = latest_rect_left.copy()
            sgm = latest_disparity_camera.copy()
        refined = run_refinement(refined_left_snapshot, sgm)
        refined_int16 = (refined * 64.0).astype(np.int16)
        cv2.filterSpeckles(refined_int16, 0, 1000, 64)
        refined = refined_int16.astype(np.float32) / 64.0
        with lock:
            latest_refined_disp = refined
            latest_refined_left = refined_left_snapshot.copy()
            new_refined_available = True
        current_time = time.time()
        if current_time - last_time > 0:
            refined_fps = 1.0 / (current_time - last_time)
        last_time = current_time
        time.sleep(0.001)

##############################
# Main Acquisition Loop
##############################
def acquire_images(cam, nodemap, nodemap_tldevice):
    global latest_rect_left, latest_disparity_camera
    global camera_fps, exit_flag, mouse_x, mouse_y
    try:
        cam.BeginAcquisition()
        print('Acquiring images...')
        device_serial_number = ''
        node_device_serial_number = PySpin.CStringPtr(nodemap_tldevice.GetNode('DeviceSerialNumber'))
        if PySpin.IsReadable(node_device_serial_number):
            device_serial_number = node_device_serial_number.GetValue()
            print('Device serial number retrieved as %s...' % device_serial_number)
        stream_transmit_flags = {
            'rectSensor1TransmitEnabled': True,
            'rectSensor2TransmitEnabled': True,
            'disparityTransmitEnabled': True
        }
        stereo_params = PySpin.StereoCameraParameters()
        configure_stereo_params(stereo_params, nodemap)
        print('*** STEREO PARAMETERS ***')
        print_stereo_params(stereo_params)
        print('Starting continuous image acquisition. Press "s" to save images, "Esc" to exit.')
        cv2.namedWindow('Rectified Left Image')
        cv2.setMouseCallback('Rectified Left Image', mouse_callback)
        # Start the refinement inference thread
        ref_thread = threading.Thread(target=refinement_inference_thread)
        ref_thread.start()
        prev_time = time.time()
        while True:
            try:
                image_list = cam.GetNextImageSync(2000)
                rect_left_image = image_list.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1)
                if rect_left_image is None or rect_left_image.IsIncomplete():
                    print('Rectified left image incomplete.')
                    continue
                disparity_image = image_list.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1)
                if disparity_image is None or disparity_image.IsIncomplete():
                    print('Disparity image incomplete.')
                    continue
                rect_left_cv_image = convert_to_cv_image(rect_left_image)
                disparity_cv_image = convert_to_cv_image(disparity_image)
                if rect_left_cv_image is None or disparity_cv_image is None:
                    print('Conversion to OpenCV image failed.')
                    continue
                SGM_disp_raw = disparity_cv_image.astype(np.float32) / 64.0
                cv2.filterSpeckles(disparity_cv_image, 0, 1000, 64)
                SGM_disp_cleaned = disparity_cv_image.astype(np.float32) / 64.0
                with lock:
                    latest_rect_left = rect_left_cv_image.copy()
                    latest_disparity_camera = SGM_disp_raw.copy()
                current_time = time.time()
                if current_time - prev_time > 0:
                    camera_fps = 1.0 / (current_time - prev_time)
                prev_time = current_time

                height, width = rect_left_cv_image.shape[:2]
                rect_left_resized = cv2.resize(rect_left_cv_image, (width // 2, height // 2))
                
                # Compute camera depth map
                with np.errstate(divide='ignore', invalid='ignore'):
                    SGM_disp_cleaned[SGM_disp_cleaned != stereo_params.invalidDataValue] += stereo_params.coordinateOffset
                    SGM_depth = np.where(SGM_disp_cleaned > 0,
                                          stereo_params.focalLength * stereo_params.baseline / SGM_disp_cleaned,
                                          0)
                SGM_depth_clipped = np.clip(SGM_depth, 0, MAX_DEPTH)
                SGM_depth_vis = 255 - ((SGM_depth_clipped / MAX_DEPTH) * 255).astype(np.uint8)
                SGM_depth_color = cv2.applyColorMap(SGM_depth_vis, cv2.COLORMAP_JET)
                SGM_depth_color[SGM_disp_cleaned == stereo_params.invalidDataValue] = [0, 0, 0]
                SGM_depth_color_resized = cv2.resize(SGM_depth_color, (width // 2, height // 2))
                
                # Before computing the refined depth, check if a refined disparity is available.
                with lock:
                    # Use a zeros array if latest_refined_disp is None.
                    refined_disp = latest_refined_disp if latest_refined_disp is not None else np.zeros_like(SGM_disp_raw)

                # Get refined depth map
                with np.errstate(divide='ignore', invalid='ignore'):
                    refined_disp[refined_disp != stereo_params.invalidDataValue] += stereo_params.coordinateOffset
                    refined_depth = np.where(refined_disp > 0,
                                             stereo_params.focalLength * stereo_params.baseline / refined_disp,
                                             0)
                refined_depth_clipped = np.clip(refined_depth, 0, MAX_DEPTH)
                refined_depth_vis = 255 - ((refined_depth_clipped / MAX_DEPTH) * 255).astype(np.uint8)
                refined_depth_color = cv2.applyColorMap(refined_depth_vis, cv2.COLORMAP_JET)
                refined_depth_color[latest_refined_disp == stereo_params.invalidDataValue] = [0, 0, 0]
                refined_depth_color_resized = cv2.resize(refined_depth_color, (width // 2, height // 2))
                
                # Overlay text (using computed distances from SGM and refined disparity)
                overlay_lines = []
                if mouse_x >= 0 and mouse_y >= 0:
                    x_full = mouse_x * 2
                    y_full = mouse_y * 2
                    if y_full < SGM_disp_raw.shape[0] and x_full < SGM_disp_raw.shape[1]:
                        sgm_val = SGM_disp_raw[y_full, x_full]
                        refined_val = latest_refined_disp[y_full, x_full] if latest_refined_disp is not None else 0
                        sgm_distance = stereo_params.focalLength * stereo_params.baseline / sgm_val if sgm_val > 0 else float('inf')
                        refined_distance = stereo_params.focalLength * stereo_params.baseline / refined_val if refined_val > 0 else float('inf')
                        overlay_lines.append(f"Depth: SGM {sgm_distance:.2f} m | Refined {refined_distance:.2f} m")
                    else:
                        overlay_lines.append("")
                else:
                    overlay_lines.append("")
                overlay_lines.append(f"Cam FPS: {camera_fps:.1f} | Refined FPS: {refined_fps:.1f}")
                rect_left_display = rect_left_resized.copy()
                for i, line in enumerate(overlay_lines):
                    cv2.putText(rect_left_display, line, (10, 20 + i*30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
                if mouse_x >= 0 and mouse_y >= 0:
                    cv2.circle(rect_left_display, (mouse_x, mouse_y), 5, (0, 255, 0), -1)
                    
                # Display only the rectified left image and the depth maps.
                cv2.imshow('Rectified Left Image', rect_left_display)
                cv2.imshow('Camera Depth Image', SGM_depth_color_resized)
                cv2.imshow('Refined Depth Image', refined_depth_color_resized)
                
                key = cv2.waitKey(1) & 0xFF
                if key == ord('s'):
                    filename_prefix = 'StereoAcquisition_%s' % device_serial_number
                    cv2.imwrite(f'{filename_prefix}_RectSensor1.png', rect_left_cv_image)
                    cv2.imwrite(f'{filename_prefix}_Depth.raw.png', SGM_depth_color)
                    print('Images saved.')
                elif key == 27:
                    print('Exit key pressed.')
                    exit_flag = True
                    break
            except PySpin.SpinnakerException as ex:
                print('Spinnaker Exception:', ex)
                continue
            except KeyboardInterrupt:
                print("Exiting acquisition loop...")
                exit_flag = True
                break
        cam.EndAcquisition()
        cv2.destroyAllWindows()
        ref_thread.join()
    except PySpin.SpinnakerException as ex:
        print('Error:', ex)
        return False
    return True

def run_single_camera(cam):
    try:
        nodemap_tldevice = cam.GetTLDeviceNodeMap()
        cam.Init()
        nodemap = cam.GetNodeMap()
        result = acquire_images(cam, nodemap, nodemap_tldevice)
        cam.DeInit()
    except PySpin.SpinnakerException as ex:
        print('Error:', ex)
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
    load_refinement_model()
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
    result = True
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