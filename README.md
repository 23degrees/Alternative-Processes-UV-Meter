# Alternative-Processes-UV-Meter
A simple and moderately accurate exposure meter for cyanotype and gum dichromate printing with intensity, effective energy and an alarm for completed exposures.

## Features
* UV intensity (mW/cm2)
* Effective energy or integrated intensity (mJ/cm2)
* Progress bar
* Easy to set and forget total exposure
* Black calibration
* Alarm for milestones and complete exposure
* Uses portable, easy to source, and inexpensive parts

## Parts Needed
* ML8511 UV Sensor
* Arduino UNO
* HP-AMO-LCDSH Hyperion LCD
* Piezo buzzer
* USB battery bank or similar power supply
* Four wire cable, small case and magnet for sensor

## Software & Libraries Used
* avdweb_VirtualDelay.h https://github.com/avandalen/VirtualDelay
* EEPROM.h https://www.arduino.cc/en/Reference/EEPROM
* Portions of MT8511 Sensor Read Example by Nathan Seidle https://learn.sparkfun.com/tutorials/ml8511-uv-sensor-hookup-guide/all
* Portions of Arduino 5 Button LCD Display Menu by Joe  (https://notabug.org/grugly/Arduino_5_Button_16x2_LCD_Menu)

