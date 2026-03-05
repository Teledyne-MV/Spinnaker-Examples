# Live Depth Refinement (LingBot-Depth + PySpin)

This script connects to a FLIR Bumblebee stereo camera, reads the on-board disparity, converts it to metric depth, and refines it in real time using the LingBot-Depth model.

## Installation & Setup

1. Install the Spinnaker & PySpin SDK following FLIR's official documentation.  
   [Download the Spinnaker & PySpin SDK](https://www.teledynevisionsolutions.com/support/support-center/software-firmware-downloads/iis/spinnaker-sdk-download/spinnaker-sdk--download-files/?pn=Spinnaker+SDK&vn=Spinnaker+SDK)
2. Follow the instructions from one of the following links to install CUDA and cuDNN:
   - Windows: see NVIDIA/official guides
   - Linux: see NVIDIA/official guides
3. Clone the LingBot-Depth repository:
   ```bash
   git clone https://github.com/robbyant/lingbot-depth
   cd lingbot-depth
   ```
4. Install required Python libraries:
   ```bash
   pip install -e .
   ```
   - Use the PySpin wheel provided by the Spinnaker SDK installer.
5. Configure SpinView settings:
   - Enable left rectified image (`RECTIFIED_SENSOR1`) and disparity output (`DISPARITY_SENSOR1`).
   - Set your desired stereo resolution. The script does not resize before inference.
6. Model weights (`robbyant/lingbot-depth-pretrain-vitl-14`) are downloaded automatically from HuggingFace on first run. No manual download needed.

## Usage

1. Connect the camera and ensure settings from step 5 are applied.
2. Run the inference script from the repo root:
   ```bash
   python PySpin_inference.py
   ```
   Upon running, three windows will appear:
   - **Left Rectified**: the live camera feed with FPS overlay (previewed at half size).
   - **Depth Input (m)**: on-board stereo depth (TURBO colourmap).
   - **Depth Refined (m)**: model-refined depth (TURBO colourmap).

Options:
```bash
# Force fp32 (disable mixed-precision)
python PySpin_inference.py --no_fp16

# Adjust depth colourmap range
python PySpin_inference.py --max_depth_vis 5.0

# Use a different model
python PySpin_inference.py --model robbyant/lingbot-depth-pretrain-vitl-14
```

| Argument | Default | Description |
|----------|---------|-------------|
| `--model` | `robbyant/lingbot-depth-pretrain-vitl-14` | HuggingFace model ID |
| `--timeout_ms` | `2000` | PySpin capture timeout (ms) |
| `--max_depth_vis` | `10.0` | Colourmap max depth (metres) |
| `--no_fp16` | off | Disable mixed-precision inference |
| `--speckle_filter` | `0` | Remove speckles smaller than N pixels (0 = disabled) |

Keys:
- `ESC`: quit.
- `s`: save left PNG, disparity NPY, depth NPY (input & refined), and coloured PLY point clouds to `captures_lingbot/`.

### Saved files (press `s`)

| File | Format | Contents |
|------|--------|----------|
| `*_left.png` | uint8 BGR | Rectified left image |
| `*_disp_u16.npy` | uint16 | Raw on-board disparity |
| `*_depth_in_m.npy` | float32 | On-board depth (metres) |
| `*_depth_refined_m.npy` | float32 | Refined depth (metres) |
| `*_cloud_in.ply` | Binary PLY | Coloured point cloud from on-board depth |
| `*_cloud_refined.ply` | Binary PLY | Coloured point cloud from refined depth |

PLY files can be opened in MeshLab, CloudCompare, or Open3D.

## Troubleshooting

- **"No cameras detected"**: Check the connection and that the camera shows up in SpinView. Make sure no other process is using the camera.
- **"GetNextImageSync timed out"**: Increase `--timeout_ms` (e.g. `5000`).
- **Low FPS**: Check that CUDA is available (`torch.cuda.is_available()`).
- **xformers / "compute capability" error**: The model falls back to PyTorch native SDPA automatically. If issues persist, try `--no_fp16`.
- **"CUDA out of memory"**: At 2048×1536 with fp16 the model needs ~4 GB VRAM. Close other GPU processes or use a lower camera resolution.