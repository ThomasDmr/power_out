# power_out

## Project
This power shortage alarm is a plug-in device that has a built-in battery and is capable of detecting power losses. 
If detected, it warns its owner via SMS (the only signal still present in the event of a power failure) that the power is cut. 
The latter can then warn other people to go and check and resolve the situation.
The device alerts when power is returned and allows the current state of the power to be checked at any time.

## Custom sim800l library
This library is based on the sim800l chip and its documentation. 
This library allows to send and received pre-established messages to warn and configurate the device. 
