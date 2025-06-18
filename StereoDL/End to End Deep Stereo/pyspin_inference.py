import PySpin
import cv2
import numpy as np
import torch
import sys

# --- Deep Learning Model Imports ---
sys.path.append("core")  # Adjust path to your DL model core folder
from core.igev_stereo import IGEVStereo
from core.utils.utils import InputPadder

DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

def load_deep_model():
    class Args:
        pass
    args = Args()
    args.restore_ckpt = "./pretrained_models/middlebury/middlebury_finetune.pth"
    args.max_disp = 32
    args.precision_dtype = "float16"
    args.valid_iters = 32
    args.hidden_dims = [128, 128, 128]
    args.corr_implementation = "reg"
    args.shared_backbone = False
    args.corr_levels = 2
    args.corr_radius = 4
    args.n_downsample = 2
    args.slow_fast_gru = False
    args.n_gru_layers = 3
    args.mixed_precision = False

    model = torch.nn.DataParallel(IGEVStereo(args), device_ids=[0])
    model.load_state_dict(torch.load(args.restore_ckpt, map_location=DEVICE))
    model = model.module
    model.to(DEVICE)
    model.eval()
    print("Deep learning model loaded.")
    return model, args

def deep_learning_inference(model, args, left_cv_img, right_cv_img):
    left_rgb = cv2.cvtColor(left_cv_img, cv2.COLOR_BGR2RGB)
    right_rgb = cv2.cvtColor(right_cv_img, cv2.COLOR_BGR2RGB)
    left_tensor = torch.from_numpy(left_rgb).permute(2,0,1).float()[None].to(DEVICE)
    right_tensor = torch.from_numpy(right_rgb).permute(2,0,1).float()[None].to(DEVICE)
    padder = InputPadder(left_tensor.shape, divis_by=32)
    left_padded, right_padded = padder.pad(left_tensor, right_tensor)
    with torch.no_grad():
        disp_tensor = model(left_padded, right_padded, iters=args.valid_iters, test_mode=True)
        disp_tensor = padder.unpad(disp_tensor)
    disp = disp_tensor.cpu().numpy().squeeze().astype(np.float32)
    print(np.max(disp))
    return disp

def convert_to_cv_image(pyspin_image):
    np_img = pyspin_image.GetNDArray()
    fmt = pyspin_image.GetPixelFormatName()
    print(fmt)
    print(DEVICE)
    if fmt == 'RGB8':
        np_img = cv2.cvtColor(np_img, cv2.COLOR_RGB2BGR)
    elif fmt == 'Mono8' or fmt == 'Mono16':
        pass  # Use as is
    else:
        print(f"Warning: Unhandled pixel format {fmt}")
    return np_img
    
# selects an value in a node drop down selector
def set_selector_to_value(nodemap, selector, value):
    try:
        selector_node = PySpin.CEnumerationPtr(nodemap.GetNode(selector))
        selector_entry = selector_node.GetEntryByName(value)
        selector_value = selector_entry.GetValue()
        selector_node.SetIntValue(selector_value)
    except PySpin.SpinnakerException:
        print("Failed to set {} selector to {} value".format(selector, value))

def main():
    # Initialize system and camera
    system = PySpin.System.GetInstance()
    cam_list = system.GetCameras()
    if cam_list.GetSize() == 0:
        print("No cameras detected.")
        return
    cam = cam_list.GetByIndex(0)
    cam.Init()
    
    nodemap_tldevice = cam.GetTLDeviceNodeMap()
    set_selector_to_value(nodemap_tldevice, "StreamBufferHandlingMode", "NewestOnly")
    
    model, args = load_deep_model()
    
    cam.BeginAcquisition()
    try:
        while True:
            image_list = cam.GetNextImageSync(2000)
            
            try:
                left_img = image_list.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR1)
            except PySpin.SpinnakerException as ex:
                print("Left rectified stream not available, skipping frame:", ex)
                continue
            
            try:
                right_img = image_list.GetByPayloadType(PySpin.SPINNAKER_IMAGE_PAYLOAD_TYPE_RECTIFIED_SENSOR2)
            except PySpin.SpinnakerException as ex:
                print("Right rectified stream not available, skipping frame:", ex)
                continue
                
            if left_img is None or right_img is None:
                print("Left or right rectified image stream not found, skipping frame...")
                continue

            if left_img.IsIncomplete() or right_img.IsIncomplete():
                print("Incomplete image(s), skipping...")
                continue
            
            left_cv = convert_to_cv_image(left_img)
            right_cv = convert_to_cv_image(right_img)

            # Run DL inference
            disp = deep_learning_inference(model, args, left_cv, right_cv)
            
            # Normalize disparity for display
            disp_norm = np.clip(disp / np.max(disp) * 255, 0, 255).astype(np.uint8)
            disp_color = cv2.applyColorMap(disp_norm, cv2.COLORMAP_JET)
            
            # Show images
            cv2.imshow('Left Image', cv2.resize(left_cv, (left_cv.shape[1]//2, left_cv.shape[0]//2)))
            cv2.imshow('DL Disparity', cv2.resize(disp_color, (disp_color.shape[1]//2, disp_color.shape[0]//2)))

            key = cv2.waitKey(1) & 0xFF
            if key == 27:  # ESC to quit
                break
    finally:
        cam.EndAcquisition()
        cam.DeInit()
        cv2.destroyAllWindows()
        cam_list.Clear()
        system.ReleaseInstance()

if __name__ == "__main__":
    main()