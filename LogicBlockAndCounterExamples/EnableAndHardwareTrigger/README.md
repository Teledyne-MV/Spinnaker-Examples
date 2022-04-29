# EnableAndHardwareTrigger

## Overview 

A logic block example in which the criteria for triggering is Hardware Trigger Input Signal (Rising edge, Line0) AND Enable Input Signal (Level High, Line2).

Trigger Exposure on the rising edge of Logic Block 0

Logic Block 0
Description: Line0 AND Line2
Input 0: Line0 
	Activation: Rising Edge
Input 1: Line2
	Activation: Level High
Input 2: Zero (output is only dependent on Input0 and Input1)
	Activation: Level High (doesn't matter)
Value LUT: 0x88
Enable LUT: 0xFF
+----+----+----+--------+--------+-------+
| I2 | I1 | I0 | Output | Enable | Value |
+----+----+----+--------+--------+-------+
|  0 |  0 |  0 | 0      |      1 |     0 |
|  0 |  0 |  1 | 0      |      1 |     0 |
|  0 |  1 |  0 | 0      |      1 |     0 |
|  0 |  1 |  1 | 1      |      1 |     1 |
|  1 |  0 |  0 | 0      |      1 |     0 |
|  1 |  0 |  1 | 0      |      1 |     0 |
|  1 |  1 |  0 | 0      |      1 |     0 |
|  1 |  1 |  1 | 1      |      1 |     1 |
+----+----+----+--------+--------+-------+
