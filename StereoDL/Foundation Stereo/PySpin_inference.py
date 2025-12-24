#!/usr/bin/env python3
import PySpin
import cv2
import numpy as np
import torch
import sys
import time
import os
from omegaconf import OmegaConf

# --- Deep Learning Model Imports ---
code_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(code_dir, ".."))  # Adjust path to repo root
from core.foundation_stereo import FoundationStereo
from core.utils.utils import InputPadder

DEVICE = "cuda" if torch.cuda.is_available() else "cpu"
MAX_DEPTH = 10.0  # meters (used to cut far points in point cloud)
SAVE_DIR = "captures"  # will be created if missing
FS_CKPT = os.path.join(code_dir, "..", "fast", "model_best_bp2.pth")
FS_CFG = os.path.join(code_dir, "..", "fast", "cfg.yaml")


def load_deep_model():
    cfg = OmegaConf.load(FS_CFG)
    if "vit_size" not in cfg:
        cfg["vit_size"] = "vitl"
    cfg["ckpt_dir"] = FS_CKPT
    cfg["scale"] = 1.0
    cfg["hiera"] = 1
    cfg["valid_iters"] = 32
    cfg["z_far"] = 10.0

    args = OmegaConf.create(cfg)
    model = FoundationStereo(args)
    ckpt = torch.load(
        FS_CKPT,
        map_location=DEVICE,
        weights_only=False,
    )
    model.load_state_dict(ckpt["model"])
    model.to(DEVICE)
    model.eval()
    print("FoundationStereo model loaded.")
    return model, args


def deep_learning_inference(model, args, left_cv_img, right_cv_img):
    left_rgb = cv2.cvtColor(left_cv_img, cv2.COLOR_BGR2RGB)
    right_rgb = cv2.cvtColor(right_cv_img, cv2.COLOR_BGR2RGB)
    left_tensor = torch.from_numpy(left_rgb).permute(2, 0, 1).float()[None].to(DEVICE)
    right_tensor = torch.from_numpy(right_rgb).permute(2, 0, 1).float()[None].to(DEVICE)
    padder = InputPadder(left_tensor.shape, divis_by=32)
    left_padded, right_padded = padder.pad(left_tensor, right_tensor)
    with torch.no_grad(), torch.cuda.amp.autocast(torch.cuda.is_available()):
        if getattr(args, "hiera", 1):
            disp_tensor = model.run_hierachical(
                left_padded,
                right_padded,
                iters=args.valid_iters,
                test_mode=True,
                small_ratio=0.5,
            )
        else:
            disp_tensor = model.forward(
                left_padded,
                right_padded,
                iters=args.valid_iters,
                test_mode=True,
            )
        disp_tensor = padder.unpad(disp_tensor)
    disp = disp_tensor.cpu().numpy().squeeze().astype(np.float32)
    return disp


def convert_to_cv_image(pyspin_image):
    np_img = pyspin_image.GetNDArray()
    fmt = pyspin_image.GetPixelFormatName()
    if fmt == 'RGB8':
        np_img = cv2.cvtColor(np_img, cv2.COLOR_RGB2BGR)
    elif fmt == 'Mono8' or fmt == 'Mono16':
        # keep as-is
        if len(np_img.shape) == 2:
            np_img = cv2.cvtColor(np_img, cv2.COLOR_GRAY2BGR)
    else:
        print(f"Warning: Unhandled pixel format {fmt}")
    return np_img


# --- Utility to set enumerations ---
def set_selector_to_value(nodemap, selector, value):
    try:
        selector_node = PySpin.CEnumerationPtr(nodemap.GetNode(selector))
        selector_entry = selector_node.GetEntryByName(value)
        selector_value = selector_entry.GetValue()
        selector_node.SetIntValue(selector_value)
    except PySpin.SpinnakerException:
        print("Failed to set {} selector to {} value".format(selector, value))


# ---------------- Stereo parameter helpers (copied/minimally adapted) ----------------
def get_node_value(nodemap, name):
    # Try float first (most Scan3d nodes are float), fall back to integer if needed
    node = nodemap.GetNode(name)
    if not PySpin.IsAvailable(node) or not PySpin.IsReadable(node):
        raise RuntimeError(f"Node {name} not readable/available")

    if PySpin.CFloatPtr(node).IsValid():
        return PySpin.CFloatPtr(node).GetValue()
    elif PySpin.CIntegerPtr(node).IsValid():
        return float(PySpin.CIntegerPtr(node).GetValue())
    else:
        raise RuntimeError(f"Node {name} is neither float nor integer readable")


def configure_stereo_params(params, nodemap):
    # read stereo related parameters from the camera
    params.baseline = get_node_value(nodemap, 'Scan3dBaseline')
    params.focalLength = get_node_value(nodemap, 'Scan3dFocalLength')
    params.principalPointU = get_node_value(nodemap, 'Scan3dPrincipalPointU')
    params.principalPointV = get_node_value(nodemap, 'Scan3dPrincipalPointV')
    
    
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

    # Just turn TriggerMode OFF
    mode = PySpin.CEnumerationPtr(nm.GetNode('TriggerMode'))
    if not PySpin.IsAvailable(mode) or not PySpin.IsWritable(mode):
        print("TriggerMode not available/writable")
        return

    mode_off = mode.GetEntryByName('Off')
    mode.SetIntValue(mode_off.GetValue())
    print("Software trigger disabled (TriggerMode=Off)")



# ---------------- Disparity -> point cloud + PLY writer ----------------
def disparity_to_pointcloud(disp, left_img_bgr, stereo_params, decimation=2):
    """
    Returns (points Nx3 float32, colors Nx3 float32 in 0..1)
    """
    h, w = disp.shape

    # Cull disparities that imply depth > MAX_DEPTH
    MIN_DISP = max(1e-6, (stereo_params.focalLength * stereo_params.baseline) / MAX_DEPTH)
    mask_valid = disp > MIN_DISP

    # Build Q like in the sample script
    Q = np.float32([
        [1, 0, 0, -stereo_params.principalPointU],
        [0, 1, 0, -stereo_params.principalPointV],
        [0, 0, 0, stereo_params.focalLength],
        [0, 0, 1.0 / stereo_params.baseline, 0]
    ])

    pts3d = cv2.reprojectImageTo3D(disp, Q)
    pts = pts3d[mask_valid]

    # Colors from left image (convert BGR 0..255 -> RGB 0..1)
    colors = (left_img_bgr.astype(np.float32)[:, :, ::-1] / 255.0)[mask_valid]

    # Flip to a more conventional RHS camera coords (like sample)
    pts[:, 1] *= -1.0
    pts[:, 2] *= -1.0

    if decimation > 1:
        pts = pts[::decimation]
        colors = colors[::decimation]

    return pts.astype(np.float32), colors.astype(np.float32)


def write_ply(filename, points_xyz, colors_rgb):
    """
    Minimal PLY writer (ASCII).
    points_xyz: Nx3 float32
    colors_rgb: Nx3 float32 in [0,1]
    """
    n = points_xyz.shape[0]
    if n == 0:
        print(f"[PLY] No points to save: {filename}")
        return

    colors_u8 = np.clip(colors_rgb * 255.0, 0, 255).astype(np.uint8)

    with open(filename, 'w') as f:
        f.write("ply\n")
        f.write("format ascii 1.0\n")
        f.write(f"element vertex {n}\n")
        f.write("property float x\nproperty float y\nproperty float z\n")
        f.write("property uchar red\nproperty uchar green\nproperty uchar blue\n")
        f.write("end_header\n")
        for (x, y, z), (r, g, b) in zip(points_xyz, colors_u8):
            f.write(f"{x:.6f} {y:.6f} {z:.6f} {int(r)} {int(g)} {int(b)}\n")

    print(f"[PLY] Saved: {filename}")


def save_disparity_outputs(basepath, disp_f32):
    """
    Saves:
      - {basepath}_disp16.png (16U, disparity*64)
    """
    # 16-bit PNG
    disp16 = np.clip(disp_f32 * 64.0, 0, 65535).astype(np.uint16)
    png16_path = f"{basepath}_disp16.png"
    cv2.imwrite(png16_path, disp16)
    print(f"[SAVE] Disparity (16U PNG): {png16_path}")


def main():
    os.makedirs(SAVE_DIR, exist_ok=True)

    # Initialize system and camera
    system = PySpin.System.GetInstance()
    cam_list = system.GetCameras()
    if cam_list.GetSize() == 0:
        print("No cameras detected.")
        return
    cam = cam_list.GetByIndex(0)
    cam.Init()
    enable_software_trigger(cam)

    # --- fetch nodemaps and stream mode ---
    serial_number = cam.DeviceSerialNumber.ToString()
    nodemap_tldevice = cam.GetTLDeviceNodeMap()
    nodemap = cam.GetNodeMap()
    set_selector_to_value(nodemap_tldevice, "StreamBufferHandlingMode", "NewestOnly")

    # --- read stereo params ---
    stereo_params = PySpin.StereoCameraParameters()
    configure_stereo_params(stereo_params, nodemap)
    print("[Stereo] baseline=%.6f m, f=%.3f px, cx=%.2f, cy=%.2f"
          % (stereo_params.baseline, stereo_params.focalLength,
             stereo_params.principalPointU, stereo_params.principalPointV))

    # Load DL model
    model, args = load_deep_model()

    cam.BeginAcquisition()
    try:
        while True:
            image_list = None
            try:
                cam.TriggerSoftware()
                image_list = cam.GetNextImageSync(2000)
            except PySpin.SpinnakerException as ex:
                print("GetNextImageSync timed out or failed:", ex)
                continue

            try:
                left_img = image_list.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1)
                right_img = image_list.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR2)

                if left_img is None or right_img is None:
                    print("Left or right rectified image stream not found, skipping frame...")
                    continue

                if left_img.IsIncomplete() or right_img.IsIncomplete():
                    print("Incomplete image(s), skipping...")
                    continue

                left_cv = convert_to_cv_image(left_img)
                right_cv = convert_to_cv_image(right_img)

                # Run DL inference
                t0 = time.time()
                disp = deep_learning_inference(model, args, left_cv, right_cv)
                fps = 1.0 / max(1e-6, (time.time() - t0))
                print(f"[DL] FPS: {fps:.2f}   disp_max: {np.max(disp):.3f}")
                
                # Optional speckle filtering with OpenCV expects 16S fixed-point disparity
                #disp16 = (disp * 64.0).astype(np.int16)
                #cv2.filterSpeckles(disp16, 0, 200, 4 * 64)
                #disp = disp16.astype(np.float32) / 64.0

                # Normalize disparity for display
                dmax = max(1e-6, float(np.max(disp)))
                disp_norm = np.clip(disp / dmax * 255.0, 0, 255).astype(np.uint8)
                disp_color = cv2.applyColorMap(disp_norm, cv2.COLORMAP_JET)
                disp_color[disp==0] = (0, 0, 0)

                # Show images (half-res preview)
                cv2.imshow('Left Image', cv2.resize(left_cv, (left_cv.shape[1]//2, left_cv.shape[0]//2)))
                cv2.imshow('DL Disparity', cv2.resize(disp_color, (disp_color.shape[1]//2, disp_color.shape[0]//2)))

                key = cv2.waitKey(1) & 0xFF
                if key == 27:  # ESC
                    break
                elif key == ord('s'):
                    # --- Save everything ---
                    ts = time.strftime("%Y%m%d_%H%M%S")
                    base = os.path.join(SAVE_DIR, f"{serial_number}_{ts}")

                    # Images
                    left_path = f"{base}_left.png"
                    right_path = f"{base}_right.png"
                    cv2.imwrite(left_path, left_cv)
                    cv2.imwrite(right_path, right_cv)
                    print(f"[SAVE] Left: {left_path}")
                    print(f"[SAVE] Right: {right_path}")

                    # Disparity + 16U PNG
                    save_disparity_outputs(base, disp)

                    # Point cloud (decimate to keep files small)
                    pts, cols = disparity_to_pointcloud(disp, left_cv, stereo_params, decimation=2)
                    ply_path = f"{base}.ply"
                    write_ply(ply_path, pts, cols)
                    print("[SAVE] Done.")
            except PySpin.SpinnakerException as ex:
                print("Failed to fetch rectified streams:", ex)
                continue
            finally:
                try:
                    if image_list is not None:
                        image_list.Release()
                except PySpin.SpinnakerException as ex:
                    print("Failed to release image list:", ex)

    finally:
        try:
            cam.EndAcquisition()
            disable_software_trigger(cam)
            cam.DeInit()
            del cam
        except Exception:
            pass
        cv2.destroyAllWindows()
        cam_list.Clear()
        del cam_list
        system.ReleaseInstance()


if __name__ == "__main__":
    main()