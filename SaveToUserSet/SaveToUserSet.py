# ============================================================================
# Copyright (c) 2001-2020 FLIR Systems, Inc. All Rights Reserved.

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
# ============================================================================
#
# This examle demonstrates how to save settings to a user set.
# It changes the camera exposure to 2000us and then saves the new settings
# to user set 0. The reason we change the exposure is to be able to see
# the differences between the default user set and user set 0.
# This example uses the QuickSpin syntax for simplicity.

import PySpin


def set_exposure(cam: PySpin.CameraPtr, exposure: int) -> bool:
    """
    Sets the camera exposure to the given value.
    """
    try:
        cam.ExposureAuto.SetValue(PySpin.ExposureAuto_Off)
        cam.ExposureTime.SetValue(exposure)
        print('Set exposure time to %sus' % exposure)

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return False

    return True


def save_to_user_set_zero(cam: PySpin.CameraPtr, set_as_default=False) -> bool:
    """
    Saves the given camera's current settings to UserSet0. If `set_as_default`
    is True, sets this user set as the default user set that will be active
    when the camera is next powercycled.
    """
    try:
        cam.UserSetSelector.SetValue(PySpin.UserSetSelector_UserSet0)
        cam.UserSetSave.Execute()  # To load a User Set, use cam.UserSetLoad
        print('Saved to UserSet0')
        if set_as_default:
            cam.UserSetDefault.SetValue(PySpin.UserSetDefault_UserSet0)
            print('Set UserSet0 as the Default User Set')

    except PySpin.SpinnakerException as ex:
        print('ERROR:', ex)
        return False

    return True


def main() -> bool:
    """
    The main function of the example. Sets up a system and
    runs the example for all connected cameras.
    """
    # Setup the system and camera
    system = PySpin.System.GetInstance()
    cam_list = system.GetCameras()
    result = True

    # Run the example
    for cam in cam_list:
        cam.Init()
        result &= set_exposure(cam, 2000)
        result &= save_to_user_set_zero(cam)

    # Clean up
    del cam
    cam_list.Clear()
    system.ReleaseInstance()

    if result:
        print('Example completed successfully')
    else:
        print('Example completed with some errors')
    input('Done! Press Enter to exit...')


if __name__ == "__main__":
    main()
