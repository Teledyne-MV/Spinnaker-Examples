#!/usr/bin/env python
# coding=utf-8
# =============================================================================
# Copyright (c) 2026 FLIR Integrated Imaging Solutions, Inc. All Rights Reserved.
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

import math
import os
import time
import argparse
import threading

import cv2
import numpy as np
import torch
import PySpin

from mdm.model.v2 import MDMModel

DEVICE = "cuda" if torch.cuda.is_available() else "cpu"
SAVE_DIR = "captures_lingbot"


# ---------------- Utility to set enumerations ----------------
def set_selector_to_value(nodemap, selector, value):
    try:
        selector_node = PySpin.CEnumerationPtr(nodemap.GetNode(selector))
        if not PySpin.IsAvailable(selector_node) or not PySpin.IsWritable(selector_node):
            return False
        selector_entry = selector_node.GetEntryByName(value)
        if not PySpin.IsAvailable(selector_entry) or not PySpin.IsReadable(selector_entry):
            return False
        selector_node.SetIntValue(selector_entry.GetValue())
        return True
    except PySpin.SpinnakerException:
        return False


def get_node_value(nodemap, name):
    node = nodemap.GetNode(name)
    if not PySpin.IsAvailable(node) or not PySpin.IsReadable(node):
        raise RuntimeError(f"Node {name} not readable/available")
    try:
        return float(PySpin.CFloatPtr(node).GetValue())
    except Exception:
        return float(PySpin.CIntegerPtr(node).GetValue())


def configure_stereo_params(params, nodemap):
    """Read all stereo parameters from the camera nodemap.

    Populates the PySpin StereoCameraParameters struct including:
        baseline, focalLength, principalPointU/V,
        coordinateScale, invalidDataValue
    """
    params.baseline = get_node_value(nodemap, 'Scan3dBaseline')
    params.focalLength = get_node_value(nodemap, 'Scan3dFocalLength')
    params.principalPointU = get_node_value(nodemap, 'Scan3dPrincipalPointU')
    params.principalPointV = get_node_value(nodemap, 'Scan3dPrincipalPointV')
    params.coordinateScale = get_node_value(nodemap, 'Scan3dCoordinateScale')
    params.coordinateOffset = get_node_value(nodemap, 'Scan3dCoordinateOffset')
    params.invalidDataValue = get_node_value(nodemap, 'Scan3dInvalidDataValue')


def enable_software_trigger(cam):
    nm = cam.GetNodeMap()
    src = PySpin.CEnumerationPtr(nm.GetNode('TriggerSource'))
    src.SetIntValue(src.GetEntryByName('Software').GetValue())

    mode = PySpin.CEnumerationPtr(nm.GetNode('TriggerMode'))
    mode.SetIntValue(mode.GetEntryByName('Off').GetValue())

    sel = PySpin.CEnumerationPtr(nm.GetNode('TriggerSelector'))
    sel.SetIntValue(sel.GetEntryByName('FrameStart').GetValue())

    mode.SetIntValue(mode.GetEntryByName('On').GetValue())


def disable_software_trigger(cam):
    nm = cam.GetNodeMap()
    mode = PySpin.CEnumerationPtr(nm.GetNode('TriggerMode'))
    if not PySpin.IsAvailable(mode) or not PySpin.IsWritable(mode):
        return
    mode.SetIntValue(mode.GetEntryByName('Off').GetValue())


def convert_to_cv_bgr(pyspin_image):
    """Returns a BGR numpy array suitable for OpenCV display."""
    np_img = pyspin_image.GetNDArray()
    fmt = pyspin_image.GetPixelFormatName()
    if fmt == 'RGB8':
        np_img = cv2.cvtColor(np_img, cv2.COLOR_RGB2BGR)
    elif fmt in ('Mono8', 'Mono16'):
        if len(np_img.shape) == 2:
            np_img = cv2.cvtColor(np_img, cv2.COLOR_GRAY2BGR)
    return np_img


# ---------------- Stereo math ----------------
def disparity_u16_to_depth_m(disp_u16, fx_px, baseline_m, disp_scale,
                             invalid_value=0, min_disp=0, _out=None):
    """
    Convert uint16 disparity to depth in metres.
    disp_u16:      uint16 disparity
    disp_scale:    sub-pixel scale factor (from Scan3dCoordinateScale)
    invalid_value: disparity value the camera uses for invalid pixels
    min_disp:      coordinateOffset (already in raw/scaled disparity units).
                   The raw uint16 does NOT include this offset, so we add it here.
    _out:          optional pre-allocated float32 buffer (same shape)
    """
    if _out is None:
        _out = np.empty(disp_u16.shape, dtype=np.float32)
    np.copyto(_out, disp_u16, casting='unsafe')      # uint16 -> float32
    valid = _out != invalid_value
    _out[valid] += min_disp
    _out[valid] = (fx_px * baseline_m * disp_scale) / _out[valid]
    _out[~valid] = 0.0
    return _out


def make_normalized_intrinsics(fx, fy, cx, cy, w, h):
    """Build the 3x3 normalised intrinsics matrix expected by LingBot-Depth."""
    return np.array([
        [fx / w, 0.0,    cx / w],
        [0.0,    fy / h, cy / h],
        [0.0,    0.0,    1.0],
    ], dtype=np.float32)


def colorize_depth(depth_m, max_m, half_h=None, half_w=None):
    """Clamp to [0, max_m] and apply a TURBO colourmap. Optionally resize first."""
    if half_h is not None:
        d = cv2.resize(depth_m, (half_w, half_h), interpolation=cv2.INTER_NEAREST)
    else:
        d = depth_m.copy()
    np.nan_to_num(d, nan=0.0, posinf=0.0, neginf=0.0, copy=False)
    np.clip(d, 0.0, max_m, out=d)
    d *= (255.0 / max_m)
    return cv2.applyColorMap(d.astype(np.uint8), cv2.COLORMAP_TURBO)


def remove_speckles(depth_m, fx, baseline, disp_scale=64.0,
                    max_speckle_size=1000, max_diff=64, connectivity=8):
    """Speckle removal via disparity filtering (`cv2.filterSpeckles`)."""
    d = np.asarray(depth_m, dtype=np.float32)
    valid = (d > 0) & np.isfinite(d)
    if not np.any(valid):
        return d

    disp = np.zeros_like(d, dtype=np.float32)
    disp[valid] = (fx * baseline * disp_scale) / d[valid]
    disp_max = float(disp[valid].max())
    if disp_max <= 0.0:
        return d

    scale = max(1, math.ceil(disp_max / 32766.0))
    disp_buf = np.zeros_like(d, dtype=np.int16)
    scaled_disp = np.clip(np.round(disp[valid] / scale), 0, 32767).astype(np.int16)
    disp_buf[valid] = scaled_disp

    cv2.filterSpeckles(disp_buf, 0, int(max_speckle_size), float(max_diff))
    valid_after = disp_buf > 0
    if not np.any(valid_after):
        return np.zeros_like(d)

    filtered_disp = disp_buf.astype(np.int32) * scale
    out = np.zeros_like(d)
    out[valid_after] = (fx * baseline * disp_scale) / filtered_disp[valid_after].astype(np.float32)
    return out


# ---------------- Point cloud ----------------
def save_point_cloud_ply(path, depth_m, bgr, fx, fy, cx, cy, max_depth=20.0):
    """Back-project depth to 3D and save a coloured PLY point cloud."""
    h, w = depth_m.shape
    valid = (depth_m > 0) & (depth_m < max_depth) & np.isfinite(depth_m)
    vs, us = np.where(valid)
    zs = depth_m[vs, us]
    xs = (us - cx) * zs / fx
    ys = (vs - cy) * zs / fy

    # BGR -> RGB
    rs = bgr[vs, us, 2]
    gs = bgr[vs, us, 1]
    bs = bgr[vs, us, 0]

    n = len(xs)
    with open(path, 'wb') as f:
        header = (
            "ply\n"
            "format binary_little_endian 1.0\n"
            f"element vertex {n}\n"
            "property float x\n"
            "property float y\n"
            "property float z\n"
            "property uchar red\n"
            "property uchar green\n"
            "property uchar blue\n"
            "end_header\n"
        )
        f.write(header.encode('ascii'))
        # Pack as [x,y,z (float32), r,g,b (uint8)] per point
        xyz = np.stack([xs, ys, zs], axis=1).astype(np.float32)
        rgb = np.stack([rs, gs, bs], axis=1).astype(np.uint8)
        for i in range(n):
            f.write(xyz[i].tobytes())
            f.write(rgb[i].tobytes())
    return n


# ---------------- Payload getters ----------------
def get_payload(image_list, payload_type):
    try:
        return image_list.GetByPayloadType(payload_type)
    except PySpin.SpinnakerException:
        return None


# \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
def main():
    ap = argparse.ArgumentParser(
        description="PySpin live depth-refinement with PyTorch")
    ap.add_argument("--model", default="robbyant/lingbot-depth-pretrain-vitl-14")
    ap.add_argument("--timeout_ms", type=int, default=2000)
    ap.add_argument("--max_depth_vis", type=float, default=10.0)
    ap.add_argument("--no_fp16", action="store_true")
    ap.add_argument("--speckle_filter", type=int, default=0,
                    help="Remove speckles smaller than this many pixels (0 = disabled)")
    args = ap.parse_args()

    os.makedirs(SAVE_DIR, exist_ok=True)

    # -- Load model --
    print(f"[INFO] DEVICE: {DEVICE}")
    print("[INFO] Loading LingBot-Depth model ...")
    model = MDMModel.from_pretrained(args.model).to(DEVICE)
    model.eval()
    print("[INFO] Model loaded.")

    # -- Camera init --
    system = PySpin.System.GetInstance()
    cam_list = system.GetCameras()
    if cam_list.GetSize() == 0:
        cam_list.Clear()
        system.ReleaseInstance()
        raise RuntimeError("No cameras detected.")

    cam = cam_list.GetByIndex(0)
    cam.Init()
    enable_software_trigger(cam)

    nodemap_tldevice = cam.GetTLDeviceNodeMap()
    nodemap = cam.GetNodeMap()

    ok = set_selector_to_value(nodemap_tldevice,
                               "StreamBufferHandlingMode", "NewestOnly")
    if not ok:
        print("[WARN] Could not set StreamBufferHandlingMode=NewestOnly.")

    serial_number = cam.DeviceSerialNumber.ToString()

    # -- Stereo parameters --
    stereo_params = PySpin.StereoCameraParameters()
    configure_stereo_params(stereo_params, nodemap)
    fx = float(stereo_params.focalLength)
    fy = float(stereo_params.focalLength)
    cx = float(stereo_params.principalPointU)
    cy = float(stereo_params.principalPointV)
    baseline = float(stereo_params.baseline)
    coord_scale = float(stereo_params.coordinateScale)
    coord_offset = float(stereo_params.coordinateOffset)
    disp_scale = 1.0 / coord_scale if coord_scale != 0.0 else 64.0
    invalid_data_value = float(stereo_params.invalidDataValue)

    print("[Stereo] baseline=%.6f m, f=%.3f px, cx=%.2f, cy=%.2f"
          % (baseline, fx, cx, cy))
    print(f"[Stereo] Scan3dCoordinateScale={coord_scale:.6f} -> disp_scale={disp_scale:.2f}")
    print(f"[Stereo] Scan3dCoordinateOffset={coord_offset:.4f} (min disparity in raw units)")
    print(f"[Stereo] Scan3dInvalidDataValue={invalid_data_value:.1f}")

    cam.BeginAcquisition()
    print("[INFO] Acquisition started. ESC to quit. 's' to save frame.")

    use_fp16 = torch.cuda.is_available() and (not args.no_fp16)

    # Detect actual frame resolution from the first captured frame.
    # cam.Width / cam.Height report the full sensor resolution, but the
    # stereo payloads (rectified left + disparity) may be smaller.
    cam_width, cam_height = None, None
    print("[INFO] Grabbing a single frame to detect resolution \u2026")
    for _attempt in range(5):
        try:
            cam.TriggerSoftware()
            image_list = cam.GetNextImageSync(args.timeout_ms)
            left_img = get_payload(
                image_list, PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1)
            if left_img is not None and not left_img.IsIncomplete():
                left_bgr = convert_to_cv_bgr(left_img)
                cam_height, cam_width = left_bgr.shape[:2]
            image_list.Release()
            if cam_width is not None:
                break
        except PySpin.SpinnakerException:
            continue
    if cam_width is None:
        raise RuntimeError("Could not determine camera resolution after 5 attempts.")
    print(f"[INFO] Actual frame resolution: {cam_width} × {cam_height}")

    # Pre-compute intrinsics once (constant per camera)
    intr = make_normalized_intrinsics(fx, fy, cx, cy, cam_width, cam_height)
    intr_t = torch.from_numpy(intr).float()[None].to(DEVICE)  # [1,3,3]

    # Pre-allocate depth buffer (reused every frame)
    depth_buf = np.empty((cam_height, cam_width), dtype=np.float32)
    half_w, half_h = cam_width // 2, cam_height // 2

    # -- Threaded camera capture --
    # Camera I/O (trigger + on-board stereo + USB transfer) takes ~100-150 ms
    # and is completely CPU/USB-bound.  Running it in a background thread
    # overlaps it with GPU inference + display, nearly doubling loop FPS.
    _cap_left  = [None]          # latest captured left BGR
    _cap_disp  = [None]          # latest captured disp_u16
    _cap_ready = threading.Event()
    _cap_stop  = threading.Event()

    def _capture_loop():
        """Daemon thread: continuously triggers and grabs frames."""
        while not _cap_stop.is_set():
            image_list = None
            try:
                cam.TriggerSoftware()
                image_list = cam.GetNextImageSync(args.timeout_ms)
            except PySpin.SpinnakerException:
                continue

            try:
                left_img = get_payload(
                    image_list,
                    PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1)
                if left_img is None or left_img.IsIncomplete():
                    continue

                disp_img = get_payload(
                    image_list,
                    PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY_SENSOR1)
                if disp_img is None:
                    disp_type = getattr(
                        PySpin, "SPINNAKER_IMAGE_PAYLOAD_TYPE_DISPARITY", None)
                    if disp_type is not None:
                        disp_img = get_payload(image_list, disp_type)
                if disp_img is None or disp_img.IsIncomplete():
                    continue

                # Copy data out before releasing the PySpin image buffer
                bgr = convert_to_cv_bgr(left_img)
                du16 = disp_img.GetNDArray().copy()
                if du16.dtype != np.uint16:
                    du16 = du16.astype(np.uint16)

                _cap_left[0] = bgr
                _cap_disp[0] = du16
                _cap_ready.set()       # signal main thread
            except Exception:
                pass
            finally:
                try:
                    if image_list is not None:
                        image_list.Release()
                except Exception:
                    pass

    cap_thread = threading.Thread(target=_capture_loop, daemon=True)
    cap_thread.start()

    # -- Main inference loop --
    fps_ema = 0.0
    last_loop = time.time()

    try:
        while True:
            # Wait for the capture thread to deliver a frame
            if not _cap_ready.wait(timeout=2.0):
                print("[WARN] No frame from capture thread in 2 s")
                continue
            _cap_ready.clear()

            left_bgr = _cap_left[0]
            disp_u16 = _cap_disp[0]
            if left_bgr is None or disp_u16 is None:
                continue

            try:
                # Disparity -> depth metres (reuse buffer)
                depth_m = disparity_u16_to_depth_m(
                    disp_u16, fx_px=fx, baseline_m=baseline,
                    disp_scale=disp_scale, invalid_value=invalid_data_value,
                    min_disp=coord_offset, _out=depth_buf)

                # Build tensors: flip(0) converts BGR->RGB without a copy
                img_t = (torch.from_numpy(left_bgr).to(DEVICE)
                         .permute(2, 0, 1).float().flip(0)[None] / 255.0)
                depth_t = torch.from_numpy(depth_m).float()[None].to(DEVICE)

                # Inference
                t0 = time.time()
                with torch.no_grad():
                    if use_fp16:
                        with torch.cuda.amp.autocast(True):
                            out = model.infer(img_t, depth_in=depth_t,
                                              intrinsics=intr_t, use_fp16=True)
                    else:
                        out = model.infer(img_t, depth_in=depth_t,
                                          intrinsics=intr_t, use_fp16=False)
                depth_refined = out["depth"][0].detach().float().cpu().numpy()
                # Optionally remove speckle blobs from the refined depth
                if args.speckle_filter and args.speckle_filter > 0:
                    depth_refined = remove_speckles(
                        depth_refined, fx, baseline,
                        disp_scale=disp_scale,
                        max_speckle_size=int(args.speckle_filter),
                        max_diff=64)
                infer_fps = 1.0 / max(1e-6, time.time() - t0)

                # Loop FPS (exponential moving average)
                now = time.time()
                loop_fps = 1.0 / max(1e-6, now - last_loop)
                last_loop = now
                fps_ema = 0.9 * fps_ema + 0.1 * loop_fps

                # Visualisation: resize BEFORE colorising (4x fewer pixels)
                disp_left = cv2.resize(left_bgr, (half_w, half_h))
                cv2.putText(disp_left,
                            f"Infer: {infer_fps:.1f} | Loop: {fps_ema:.1f} FPS",
                            (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7,
                            (0, 255, 0), 2)
                disp_in = colorize_depth(depth_m, max_m=args.max_depth_vis,
                                         half_h=half_h, half_w=half_w)
                disp_ref = colorize_depth(depth_refined,
                                          max_m=args.max_depth_vis,
                                          half_h=half_h, half_w=half_w)

                cv2.imshow("Left Rectified", disp_left)
                cv2.imshow("Depth Input (m)", disp_in)
                cv2.imshow("Depth Refined (m)", disp_ref)

                key = cv2.waitKey(1) & 0xFF
                if key == 27:  # ESC
                    break
                elif key == ord('s'):
                    ts = time.strftime("%Y%m%d_%H%M%S")
                    base = os.path.join(SAVE_DIR, f"{serial_number}_{ts}")

                    cv2.imwrite(base + "_left.png", left_bgr)
                    np.save(base + "_disp_u16.npy", disp_u16)
                    np.save(base + "_depth_in_m.npy", depth_m)
                    np.save(base + "_depth_refined_m.npy", depth_refined)

                    n_in = save_point_cloud_ply(
                        base + "_cloud_in.ply",
                        depth_m, left_bgr, fx, fy, cx, cy)
                    n_ref = save_point_cloud_ply(
                        base + "_cloud_refined.ply",
                        depth_refined, left_bgr, fx, fy, cx, cy)

                    print(f"[SAVE] {base}_left.png")
                    print(f"[SAVE] {base}_disp_u16.npy  (disp_scale={disp_scale:.4f}, invalid={invalid_data_value:.0f})")
                    print(f"[SAVE] {base}_depth_in_m.npy")
                    print(f"[SAVE] {base}_depth_refined_m.npy")
                    print(f"[SAVE] {base}_cloud_in.ply  ({n_in} pts)")
                    print(f"[SAVE] {base}_cloud_refined.ply  ({n_ref} pts)")

            except Exception as ex:
                print("Frame processing error:", ex)
                continue

    finally:
        # Stop the capture thread first
        _cap_stop.set()
        cap_thread.join(timeout=3.0)

        try:
            cam.EndAcquisition()
        except Exception:
            pass
        try:
            disable_software_trigger(cam)
        except Exception:
            pass
        try:
            cam.DeInit()
        except Exception:
            pass
        # Release the camera reference BEFORE clearing the list,
        # otherwise PySpin throws "Can't clear a camera" error.
        del cam

        cv2.destroyAllWindows()
        cam_list.Clear()
        system.ReleaseInstance()
        print("[INFO] Clean shutdown.")


if __name__ == "__main__":
    main()
