# Valve Tester Firmware

Firmware for the Valve Wizard's Valve Analyser is an Arduino-based valve tester which he explains here: https://valvewizard.co.uk/analyser_mk2.html

This application is a complement to the PC Software project (https://github.com/olivergardiner/ValveTesterSoftware) and enables PC control of the Valve Analyser such that it can run tests and display the results graphically.

## Build and Upload

This firmware has been authored to work with VS Code and PlatformIO. However, the project contains scripts in PowerShell and Python to create an Arduino project which allows a device to be flashed using the Arduino IDE.
