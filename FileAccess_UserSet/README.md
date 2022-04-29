### FileAccessUserset

This example sets the exposure to a desired value, then save the custom settings to the camera's "UserSet0" or "UserSet1" userset, then saving those custom settings to the current folder.  The example can also be used to upload the saved file's settings back to the camera.

## How to Run the Application:

Build and run the executable file.  Interact with the program with specified options:
	
* /d : Prompt the user to enter exposure time in microseconds, store the user set file on the current folder with giving file name 
* /u : Read the user set file with giving file name, upload it to camera and print the current exposure value.
* /v : Enable verbose output. 
* /? : Print usage informaion. 

Hint: After saving the user set file with a giving file name in a specified location, one can decide to change the camera exposure time via GUI before loading the file back to the camera. More details on usersets can be found in this article: https://www.flir.com/support-center/iis/machine-vision/application-note/saving-custom-settings-on-flir-machine-vision-cameras/.



