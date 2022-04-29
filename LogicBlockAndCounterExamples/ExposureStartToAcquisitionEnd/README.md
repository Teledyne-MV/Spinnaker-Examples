# ExposureStartToAcquisitionEnd

## Overview 

This example uses logic blocks to Set the output Strobe High when the start of exposure occurs for the first image captured, and sets it to low upon Acquisition End

High level description:

LogicBlock0 is used to output the desired signal.
Rising edge = ExposureStart
Falling edge = AcquisitionEnd

UserOutput0 is used to ensure that LogicBlock0 is initialized to 0

Low level details:

Logic Block 0
Description: Output custom signal
HIGH on ExposureStart(RisingEdge)'event
LOW on AquisitionActive(FallingEdge)'event
Input 0: ExposureStart (Rising edge)
Input 1: AquisitionActive (Rising edge)
Input 2: UserOutput0 - Used to initialize the logic block to 0
Value LUT: 0x20
Enable LUT: 0xEF
+----+----+----+--------+--------+-------+
| I2 | I1 | I0 | Output | Enable | Value |
+----+----+----+--------+--------+-------+
|  0 |  0 |  0 | 0      |      1 |     0 |
|  0 |  0 |  1 | 0      |      1 |     0 |
|  0 |  1 |  0 | 0      |      1 |     0 |
|  0 |  1 |  1 | 0      |      1 |     0 |
|  1 |  0 |  0 | Q      |      0 |     0 |
|  1 |  0 |  1 | 1      |      1 |     1 |
|  1 |  1 |  0 | 0      |      1 |     0 |
|  1 |  1 |  1 | 0      |      1 |     0 |
+----+----+----+--------+--------+-------+

