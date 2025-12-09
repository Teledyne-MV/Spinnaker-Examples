# End to End Deep Stereo (FoundationStereo + PySpin)

This example directly estimates high-quality disparity maps from stereo pairs using FoundationStereo.

## Installation & Setup

1. Install the Spinnaker & PySpin SDK following FLIR's official documentation.  
   [Download the Spinnaker & PySpin SDK](https://www.teledynevisionsolutions.com/support/support-center/software-firmware-downloads/iis/spinnaker-sdk-download/spinnaker-sdk--download-files/?pn=Spinnaker+SDK&vn=Spinnaker+SDK)
2. Follow the instructions from one of the following links to install CUDA and cuDNN:
   - Windows: see NVIDIA/official guides
   - Linux: see NVIDIA/official guides
3. Clone the FoundationStereo repository:
   ```bash
   git clone https://github.com/NVlabs/FoundationStereo.git
   ```
4. Install required Python libraries:
   - Follow the environment setup in the FoundationStereo README.
   - Use the PySpin wheel provided by the Spinnaker SDK installer.
5. Prepare the inference script:
   - Place `PySpin_inference.py` into the cloned FoundationStereo repository under `scripts/`.
   - For example, if your repository is at `~/Downloads/FoundationStereo/`, the path should be:
     ```bash
     ~/Downloads/FoundationStereo/scripts/PySpin_inference.py
     ```
6. Configure SpinView settings:
   - Enable left and right rectified images (`RECTIFIED_SENSOR1/2`). You can disable disparity output.
   - Set your desired stereo resolution. The script does not resize before inference.
7. Download the pretrained FoundationStereo fast model:
   - Create `fast/` in the repo root and place:
     ```
     fast/cfg.yaml
     fast/model_best_bp2.pth
     ```

## Usage

1. Connect the camera and ensure settings from step 6 are applied.
2. Run the inference script from the repo root:
   ```bash
   python3 scripts/PySpin_inference.py
   ```
   Upon running, two windows will appear:
   - Left Image: the live camera feed (previewed at half size).
   - DL Disparity: the disparity map output by FoundationStereo.

Keys:
- `ESC`: quit.
- `s`: save left/right PNGs, disparity PNG (16U) + NPY, and a decimated point cloud to `captures/`.
