
# Stereo Deep Learning

This example uses the PC's graphics card to implement Deep Learning, improving the disparity quality for the Disparity stream

## Installation & Setup

1. Install the PySpin SDK following FLIR's official documentation.
   [Download the Spinnaker SDK](https://www.teledynevisionsolutions.com/support/support-center/software-firmware-downloads/iis/spinnaker-sdk-download/spinnaker-sdk--download-files/?pn=Spinnaker+SDK&vn=Spinnaker+SDK)
2. Follow the instructions from one of the following links to install CUDA and cuDNN:
	(Windows): https://medium.com/analytics-vidhya/installing-cuda-and-cudnn-on-windows-d44b8e9876b5
	(Linux): https://medium.com/@milistu/how-to-install-cuda-cudnn-7e4a00ae4f44
3. Clone the Neural Disparity Refinement Repository
 
 ```bash
   git clone https://github.com/CVLAB-Unibo/neural-disparity-refinement.git
   ```

4. Install Python dependencies:

   ```bash
   pip install matplotlib
   pip install numpy
   pip install opencv-contrib-python
   pip install opencv-python
   pip install opencv-python-headless
   pip install torch
   pip install torchaudio
   pip install torchvision
   pip install tqdm 
   ```
   Note: Installing both opencv-python and opencv-python-headless in the same environment might cause conflicts; choose the one that best suits your application if you encounter issues.

5. Prepare the Refinement Script

Copy the provided file refinement.py into the cloned repository folder. Make sure it is placed in the correct location so the model and supporting scripts can locate it.

For example, if your repository is located at ~/Downloads/neural-disparity-refinement-main, then the refinement.py file should be placed directly in that folder, resulting in the path:

```bash
   ~/Downloads/neural-disparity-refinement-main/refinement.py
   ```
6. Configure SpinView Settings

Before running the model, make sure the Bumblebee X camera is connected, open the SpinView application and adjust the following camera settings:

- Uniqueness: Set this value to 5.
- Stereo Resolution: Set this to Quarter Size.

These settings ensure the camera output matches the expected input for disparity refinement.

7. Modify the Utility Code
Open the file lib/utils.py in the cloned repository and navigate to Line 202. Change the padding function call from:
```bash
img = np.lib.pad(.......)
   ```
to:

```bash
img = np.pad(.......)
   ```
This modification aligns the code with your current NumPy version’s preferred syntax.


## Usage

1. Connect the camera 
Make sure that the settings for the camera match what is mentioned in step 6 of the Installation & Setup section

2. Run the Refinement Script

Open a new terminal and navigate to the folder containing the repository. Start the refinement process by running:
```bash
python3 refinement.py
   ```
Upon running the script, three windows will appear:

- Left Image: The live camera feed.
- Disparity Image: The raw disparity output from the camera.
- Refined Disparity: The disparity refined by the deep learning model.

Additionally, when you hover your mouse over the left camera image, the distance to that point will be displayed.


