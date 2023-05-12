using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Media;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using SpinnakerNET;
using SpinnakerNET.GenApi;
using System.Threading;

namespace SpinPictureBox
{
    public partial class Form1 : Form
    {
        ManagedSystem spinnakerSystem;
        ManagedCameraList camList;
        IManagedCamera cam;

        // Configure image event handlers
        ImageEventListener myImageEventListener = null;
        SemaphoreSlim sema = new SemaphoreSlim(1);

        private void InitializeSpinnaker()
        {
            spinnakerSystem = new ManagedSystem();
            // Print out current library version
            LibraryVersion spinVersion = spinnakerSystem.GetLibraryVersion();
            Console.WriteLine(
                "Spinnaker library version: {0}.{1}.{2}.{3}",
                spinVersion.major,
                spinVersion.minor,
                spinVersion.type,
                spinVersion.build);
        }

        class ImageEventListener : ManagedImageEventHandler
        {
            private string deviceSerialNumber;
            public int imageCnt;
            List<IManagedImage> ConvertList;
            IManagedImageProcessor processor;

            PictureBox imageEventPictureBox;
            SemaphoreSlim displayMutex;

            // The constructor retrieves the serial number and initializes the
            // image counter to 0.
            public ImageEventListener(IManagedCamera cam, ref PictureBox pictureBoxInput, ref SemaphoreSlim displayMutexInput)
            {
                // Double buffer
                ConvertList = new List<IManagedImage>();
                ConvertList.Add(new ManagedImage());
                ConvertList.Add(new ManagedImage());

                // Initialize image counter to 0
                imageCnt = 0;
                imageEventPictureBox = pictureBoxInput;
                displayMutex = displayMutexInput;
                deviceSerialNumber = "";

                // Retrieve device serial number
                INodeMap nodeMap = cam.GetTLDeviceNodeMap();
                IString iDeviceSerialNumber = nodeMap.GetNode<IString>("DeviceSerialNumber");
                if (iDeviceSerialNumber != null && iDeviceSerialNumber.IsReadable)
                {
                    deviceSerialNumber = iDeviceSerialNumber.Value;
                }
                Console.WriteLine("ImageEvent initialized for camera serial: {0}", deviceSerialNumber);
                processor = new ManagedImageProcessor();
            }

            ~ImageEventListener()
            {
                //Cleanup double buffer
                if (ConvertList != null)
                {
                    foreach (var item in ConvertList)
                    {
                        item.Dispose();
                    }
                }
            }
            // This method defines an image event. In it, the image that
            // triggered the event is converted and saved before incrementing
            // the count. Please see Acquisition_CSharp example for more
            // in-depth comments on the acquisition of images.
            override protected void OnImageEvent(ManagedImage image)
            {
                // Example console print
                // Print Only every x to not overwhelme the console log and cause slowdown
                if (image.FrameID % 100 == 0)
                {
                    Console.WriteLine("Image event! (We are only printing every 100 counts..) FrameID:{0}, ImageStatus:{1}",
                       image.FrameID,
                       image.ImageStatus.ToString()
                       );
                }

                // Alternate between the two images we've pre-allocated (double buffering or ping-pong buffering); this is to make sure that the 
                // bitmap is not overwritten mid-rendering on the picturebox, which causes exceptions either on some painting
                // events or when picturebox is resized 
                IManagedImage doubleBufferImage = ConvertList[(int)image.FrameID % 2];

                // The mutex could potentially alleviate clashes if image events arrive faster 
                // than the image can be processed
                if (displayMutex.Wait(TimeSpan.Zero))
                {
                    try
                    {
                        using (IManagedImage convertedImage = processor.Convert(image, PixelFormatEnums.BGR8))
                        {
                            doubleBufferImage.DeepCopy(convertedImage);
                            imageEventPictureBox.Image = doubleBufferImage.bitmap;
                        } // ConvertedImage is disposed here and freed by falling out of 'using' scope
                    }
                    catch (SpinnakerException ex)
                    {
                        Console.WriteLine("Exception: {0} ", ex);
                    }
                    finally
                    {
                        image.Release();
                        displayMutex.Release();
                    }
                }
                else
                {
                    // If this line is being printed then our bitmap/rendering path is not fast enough
                    Console.WriteLine("Not processing FrameID: {0} as previous one is still being processed", image.FrameID);
                    // Make sure to release the image so it can be requeued
                    image.Release();
                }
            }             
        }

        // This function configures the example to execute image events by
        // preparing and registering an image event.
        int ConfigureImageEvents(IManagedCamera cam, ref ImageEventListener eventListenerToConfig)
        {
            int result = 0;
            try
            {
                eventListenerToConfig = new ImageEventListener(cam, ref pictureBox1, ref sema);
                cam.RegisterEventHandler(eventListenerToConfig);
                Console.WriteLine("***Image Event Handler Registered***");
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Error: {0}", ex.Message);
                result = -1;
            }
            return result;
        }

        // Disables or enables heartbeat on GEV cameras so long waits during breakpoints do not cause cameras to drop off
        // timeout errors.
        static int ConfigureGVCPHeartbeat(IManagedCamera cam, bool enable)
        {
            // Retrieve TL device nodemap and print device information
            INodeMap nodeMapTLDevice = cam.GetTLDeviceNodeMap();

            // Retrieve GenICam nodemap
            INodeMap nodeMap = cam.GetNodeMap();

            IEnum iDeviceType = nodeMapTLDevice.GetNode<IEnum>("DeviceType");
            IEnumEntry iDeviceTypeGEV = iDeviceType.GetEntryByName("GigEVision");
            // We first need to confirm that we're working with a GEV camera
            if (iDeviceType != null && iDeviceType.IsReadable)
            {
                if (iDeviceType.Value == iDeviceTypeGEV.Value)
                {
                    if (enable)
                    {
                        Console.WriteLine("Resetting heartbeat");
                    }
                    else
                    {
                        Console.WriteLine("Disabling heartbeat");
                    }
                    IBool iGEVHeartbeatDisable = nodeMap.GetNode<IBool>("GevGVCPHeartbeatDisable");
                    if (iGEVHeartbeatDisable == null || !iGEVHeartbeatDisable.IsWritable)
                    {
                        Console.WriteLine(
                            "Unable to disable heartbeat on camera. Continuing with execution as this may be non-fatal...");
                    }
                    else
                    {
                        iGEVHeartbeatDisable.Value = enable;

                        if (!enable)
                        {
                            Console.WriteLine("         Heartbeat timeout has been disabled for this run. This allows pausing ");
                            Console.WriteLine("         and stepping through  code without camera disconnecting due to a lack ");
                            Console.WriteLine("         of a heartbeat register read.");
                        }
                        else
                        {
                            Console.WriteLine("         Heartbeat timeout has been enabled.");
                        }
                        Console.WriteLine();
                    }
                }
            }
            else
            {
                Console.WriteLine("Unable to access TL device nodemap. Aborting...");
                return -1;
            }

            return 0;
        }

        private void ConfigureCamera(IManagedCamera cam)
        {
            INodeMap snodeMap = cam.GetTLStreamNodeMap();
            IEnum iHandlingMode = snodeMap.GetNode<IEnum>("StreamBufferHandlingMode");
            if (iHandlingMode != null && iHandlingMode.IsWritable && iHandlingMode.IsReadable)
            {
                // Default is oldest first
                IEnumEntry iHandlingModeEntry = iHandlingMode.GetEntryByName("NewestOnly");
                iHandlingMode.Value = iHandlingModeEntry.Value;
                Console.WriteLine("Camera Serial: {0} buffer handling mode set to NewestOnly", cam.DeviceSerialNumber);
            }
        }
        private void ConnectCamera()
        {
            // Retrieve list of cameras from the system
            camList = spinnakerSystem.GetCameras();
            Console.WriteLine("Number of cameras detected: {0}", camList.Count);
            if (camList.Count > 0)
            {
                // Grab the input from GUI (assuming it is an int and use it as index to camList[])
                cam = camList[Int32.Parse(textBox1.Text)];
                if (!cam.IsInitialized())
                {
                    cam.Init();
                    Console.WriteLine("Initialized Camera:{0} Serial:{1} ", cam.DeviceModelName, cam.DeviceSerialNumber);
                }
                ConfigureCamera(cam);
                ConfigureImageEvents(cam, ref myImageEventListener);
                ConfigureGVCPHeartbeat(cam, false);
                cam.BeginAcquisition();
                Console.WriteLine("Acquisition Started");
            }
            else
            {
                Console.WriteLine("No cameras connected");
            }
        }

        private void CleanupSpinnaker()
        {
            try
            {
                // Clear camera list before releasing system
                if (cam.IsValid())
                {
                    cam.UnregisterEventHandler(myImageEventListener);
                    cam.EndAcquisition();
                    Console.WriteLine("Stream Stopped");

                    // This enables heartbeat again
                    ConfigureGVCPHeartbeat(cam, true);
                    cam.DeInit();
                    camList.Clear();
                }
                // Release system
                spinnakerSystem.Dispose();
            }
            catch (SpinnakerException ex)
            {
                Console.WriteLine("Exception during cleanup: {0}", ex);
            }

        }

        public Form1()
        {
            InitializeComponent();
            this.StopStreamButton.Enabled = false;
        }

        private void StreamButton_Click(object sender, EventArgs e)
        {
            // Disable the button to prevent double-click
            this.StreamButton.Enabled = false;
            InitializeSpinnaker();
            ConnectCamera();
            this.StopStreamButton.Enabled = true;
        }

        private void StopStreamButton_Click(object sender, EventArgs e)
        {
            // Disable the button to prevent double-click
            this.StopStreamButton.Enabled = false;
            CleanupSpinnaker();
            this.StreamButton.Enabled = true;
        }
    }
}
