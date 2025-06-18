
# End to End Deep Stereo

This example directly estimates high-quality disparity maps from stereo pairs using deep learning models.

## Installation & Setup

1. Install the Spinnaker & PySpin SDK following FLIR's official documentation.
   [Download the Spinnaker & PySpin SDK](https://www.teledynevisionsolutions.com/support/support-center/software-firmware-downloads/iis/spinnaker-sdk-download/spinnaker-sdk--download-files/?pn=Spinnaker+SDK&vn=Spinnaker+SDK)
2. Follow the instructions from one of the following links to install CUDA and cuDNN:
	[Windows](https://medium.com/analytics-vidhya/installing-cuda-and-cudnn-on-windows-d44b8e9876b5)
	[Linux](https://medium.com/@milistu/how-to-install-cuda-cudnn-7e4a00ae4f44)
3. Clone the Selective Stereo Repository
 
 ```bash
   git clone https://github.com/Windsrain/Selective-Stereo.git
   ```

4. Install Required Python Libraries:

Follow the environment setup in the Selective-IGEV readme

5. Prepare the Inference Script

Copy the provided file (pyspin_inference.py) into the Selective-IGEV folder in the cloned Selective Stereo Repository. Make sure it is placed in the correct location so the model and supporting scripts can locate it.

For example, if your repository is located at ~/Downloads/Selective-Stereo/Selective-IGEV/, then the pyspin_inference.py file should be placed directly in that folder, resulting in the path:

```bash
   ~/Downloads/Selective-Stereo/Selective-IGEV/pyspin_inference.py
   ```
6. Configure SpinView Settings

Before running the model, make sure the Bumblebee X camera is connected, open the SpinView application and adjust the following camera settings:

- Stereo Resolution: Set this to Quarter Size.
- Stream setting:  Enable left and right rectified images. Optionally, disable the disparity.

7. Download the pre-trained model
Download the trained middlebury model (follow the readme from GitHub - Windsrain/Selective-Stereo: [CVPR 2024 Highlight] Selective-Stereo: Adaptive Frequency Information Selection for Stereo Matching)

Make a folder "pretrained_models/middlebury" in your folder and paste the model in this folder.

For example, the location of the model will be: ~/Downloads/Selective-Stereo/Selective-IGEV/pretrained_models/middlebury/middlebury_finetune.pth


## Usage

1. Connect the camera 
Make sure that the settings for the camera match what is mentioned in step 6 of the Installation & Setup section

2. Run the Refinement Script

Open a new terminal and navigate to the folder containing the repository. Start the inference process by running:
```bash
   python3 pyspin_inference.py
   ```
Upon running the script, two windows will appear:

- Left Image: The live camera feed.
- DL Disparity: The disparity map output by Selective-IGEV




