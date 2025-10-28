#!/usr/bin/env python3
# coding=utf-8

import cv2
import numpy as np
import PySpin
import argparse
import sys
import statistics

RATIOTEST = 0.6
NUM_CAMERA_PAIRS = 10

# ----------------- Helpers -----------------
def to_gray(img):
    if img.ndim == 3 and img.shape[2] == 3:
        return cv2.cvtColor(img, cv2.COLOR_RGB2GRAY)
    return img

def ensure_left_right_enabled(cam):
    nm = cam.GetNodeMap()

    def check_and_enable(source_name, component_name):
        try:
            # 1) Select source
            source_sel = PySpin.CEnumerationPtr(nm.GetNode("SourceSelector"))
            if not PySpin.IsAvailable(source_sel) or not PySpin.IsWritable(source_sel):
                print("[WARN] SourceSelector not available.")
                return False
            source_entry = source_sel.GetEntryByName(source_name)
            if not PySpin.IsAvailable(source_entry):
                print(f"[WARN] Source {source_name} not available.")
                return False
            source_sel.SetIntValue(source_entry.GetValue())

            # 2) Select component
            comp_sel = PySpin.CEnumerationPtr(nm.GetNode("ComponentSelector"))
            if not PySpin.IsAvailable(comp_sel) or not PySpin.IsWritable(comp_sel):
                print("[WARN] ComponentSelector not available.")
                return False
            comp_entry = comp_sel.GetEntryByName(component_name)
            if not PySpin.IsAvailable(comp_entry):
                print(f"[WARN] Component {component_name} not available for {source_name}.")
                return False
            comp_sel.SetIntValue(comp_entry.GetValue())

            # 3) Check if component is already enabled
            comp_enable = PySpin.CBooleanPtr(nm.GetNode("ComponentEnable"))
            if not PySpin.IsAvailable(comp_enable) or not PySpin.IsReadable(comp_enable):
                print("[WARN] ComponentEnable not readable/writable.")
                return False

            current_state = comp_enable.GetValue()
            if current_state:
                # Already enabled; do nothing
                return True

            # 4) Enable only if currently disabled
            if PySpin.IsWritable(comp_enable):
                comp_enable.SetValue(True)
                print(f"[INFO] Enabled {component_name} for {source_name}.")
                return True
            else:
                print(f"[WARN] Cannot enable {component_name} for {source_name}.")
                return False

        except PySpin.SpinnakerException as e:
            print(f"[ERROR] Failed to enable {component_name} for {source_name}: {e}")
            return False

    ok_left  = check_and_enable("Sensor1", "Rectified")
    ok_right = check_and_enable("Sensor2", "Rectified")

    if not (ok_left and ok_right):
        print("[WARN] Some components might not have been enabled properly.")

    return ok_left and ok_right


def get_rectified_pair_from_images(left_path, right_path):
    left_gray  = cv2.imread(left_path,  cv2.IMREAD_GRAYSCALE)
    right_gray = cv2.imread(right_path, cv2.IMREAD_GRAYSCALE)
    if left_gray is None or right_gray is None:
        raise RuntimeError("Failed to read --left/--right images.")
    return left_gray, right_gray

def get_rectified_pair_from_spinnaker(imgset):
    left_im  = imgset.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1)
    right_im = imgset.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR2)
    if left_im is None or right_im is None:
        raise RuntimeError("Left/Right rectified payload(s) not found in the image set.")
    if left_im.IsIncomplete() or right_im.IsIncomplete():
        raise RuntimeError("Incomplete left/right rectified image.")
    left_np  = left_im.GetNDArray()
    right_np = right_im.GetNDArray()
    return to_gray(left_np), to_gray(right_np)

def compute_row_offset(img_right_gray, img_left_gray):
    """Returns (avg_row_offset, inlier_matches, matched_vis_image or None)."""
    akaze = cv2.AKAZE_create()
    akaze.setThreshold(3e-4)

    k1, d1 = akaze.detectAndCompute(img_right_gray, None)  # right
    k2, d2 = akaze.detectAndCompute(img_left_gray,  None)  # left

    if d1 is None or d2 is None or len(k1) < 8 or len(k2) < 8:
        return None, 0, None

    bf = cv2.BFMatcher()
    matches = bf.knnMatch(d1, d2, k=2)
    good = [m[0] for m in matches if len(m) == 2 and m[0].distance < RATIOTEST * m[1].distance]
    if len(good) < 8:
        return None, 0, None

    pts1 = np.float32([k1[m.queryIdx].pt for m in good])
    pts2 = np.float32([k2[m.trainIdx].pt for m in good])

    F, mask = cv2.findFundamentalMat(pts1, pts2, cv2.FM_RANSAC, 1.0, 0.99)
    if mask is None:
        return None, 0, None

    inliers = [good[i] for i in range(len(good)) if mask[i]]
    if len(inliers) == 0:
        return None, 0, None

    row_offsets = [k2[m.trainIdx].pt[1] - k1[m.queryIdx].pt[1] for m in inliers]
    avg_row_offset = float(np.mean(row_offsets))

    # Optional visualization (build but only return for the last pair in camera mode)
    vis = cv2.drawMatches(img_right_gray, k1, img_left_gray, k2, inliers, None,
                          flags=cv2.DrawMatchesFlags_NOT_DRAW_SINGLE_POINTS)
    return avg_row_offset, len(inliers), vis

# ----------------- CLI -----------------
def parse_args():
    ap = argparse.ArgumentParser(description="AKAZE-based epipolar row offset on Bumblebee rectified pairs or provided images.")
    ap.add_argument("--left",  type=str, help="Path to LEFT image (grayscale or color).")
    ap.add_argument("--right", type=str, help="Path to RIGHT image (grayscale or color).")
    ap.add_argument("--show", action="store_true", help="Show match visualization (last pair).")
    return ap.parse_args()

# ----------------- Main -----------------
def main():
    args = parse_args()

    # --- File mode: one computation on provided images ---
    if args.left and args.right:
        left_gray, right_gray = get_rectified_pair_from_images(args.left, args.right)
        avg_off, inl, vis = compute_row_offset(right_gray, left_gray)
        if avg_off is None:
            print("Not enough matches/inliers to compute row offset.")
            sys.exit(0)

        print(f"Average row offset (single pair): {avg_off:.6f}")
        verdict = "Camera might have stereo calibration issue" if abs(avg_off) > 0.5 else "Camera appears well calibrated."
        print(f"Verdict: {verdict}")

        if args.show and vis is not None:
            vis = cv2.resize(vis, (0,0), fx=0.5, fy=0.5)
            cv2.imshow("AKAZE Feature Matching (files)", vis)
            cv2.waitKey(0)
            cv2.destroyAllWindows()
        return

    # --- Camera mode: grab 10 pairs, average offsets ---
    system = PySpin.System.GetInstance()
    cams = system.GetCameras()
    if cams.GetSize() == 0:
        system.ReleaseInstance()
        print("No cameras found.")
        sys.exit(1)

    cam = cams.GetByIndex(0)
    offsets = []
    last_vis = None

    try:
        cam.Init()
        ensure_left_right_enabled(cam)

        cam.BeginAcquisition()
        try:
            print(f"Getting average row offset across {NUM_CAMERA_PAIRS} frames")
            for i in range(NUM_CAMERA_PAIRS):
                imgset = cam.GetNextImageSync(5000)
                try:
                    left_gray, right_gray = get_rectified_pair_from_spinnaker(imgset)
                finally:
                    imgset.Release()

                off, inl, vis = compute_row_offset(right_gray, left_gray)
                if off is not None:
                    offsets.append(off)
                    last_vis = vis  # keep last good vis
                    print(f"[{i+1}/{NUM_CAMERA_PAIRS}] offset={off:.6f} (inliers={inl})")
                else:
                    print(f"[{i+1}/{NUM_CAMERA_PAIRS}] Insufficient matches/inliers; skipping.")
        finally:
            cam.EndAcquisition()

    except Exception as e:
        print(f"[Camera ERROR] {e}")
        try:
            cam.DeInit()
        except:
            pass
        del cam
        cams.Clear()
        system.ReleaseInstance()
        sys.exit(1)

    cam.DeInit()
    del cam
    cams.Clear()
    system.ReleaseInstance()

    if len(offsets) == 0:
        print("No valid offsets computed across the 10 pairs.")
        sys.exit(0)

    avg_offset = statistics.mean(offsets)
    print(f"\nAverage row offset across {len(offsets)} valid pairs: {avg_offset:.6f}")
    verdict = "Camera might have stereo calibration issue" if abs(avg_offset) > 0.5 else "Camera appears well calibrated."
    print(f"Verdict: {verdict}")

    if args.show and last_vis is not None:
        last_vis = cv2.resize(last_vis, (0,0), fx=0.5, fy=0.5)
        cv2.imshow("AKAZE Feature Matching (last camera pair)", last_vis)
        cv2.waitKey(0)
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()
