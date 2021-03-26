// Uncomment the line below to enable Serial Printing
// #define DEBUG

// Last modified 2021.3.26
// First version completed 2019.10.14

/*
 * Light meter for alternative processes, including cyanotype, gum dichromate, and other UV sensitive processes
 * This meter uses an ML8511 UV Sensor with an Arduino UNO 
 * 
 * Made at 23AF Studio in Kyoto by Tomas Svab.
 * With contributed code:
 * For the UV sensor by Nathan Seidle
 * For the LCD keypad by Joe (grugly at sdf dot org)
 */

// Timing library
#include <avdweb_VirtualDelay.h>

// EEPROM Library for read/write to permanent memory
#include <EEPROM.h>

// Include LCD library
#include "LiquidCrystal.h"

// Pin assignment
#define LCD_PIN_1  8
#define LCD_PIN_2 9
#define LCD_PIN_3 4
#define LCD_PIN_4 5
#define LCD_PIN_5 6
#define LCD_PIN_6 7
#define BACKLIGHT_PIN 10
#define BUTTON_PIN  A0

// Button reference voltage
// This might vary with each keypad version
#define BUTTON_NULL 1023
#define BUTTON_SELECT 639
#define BUTTON_LEFT 408
#define BUTTON_DOWN 255
#define BUTTON_UP 98
#define BUTTON_RIGHT

// Init LCD
LiquidCrystal lcd(LCD_PIN_1, LCD_PIN_2, LCD_PIN_3, LCD_PIN_4, LCD_PIN_5, LCD_PIN_6);

// UV Sensor pin definitions
float UVOUT = A3; //Output from the sensor
float REF_3V3 = A2; //3.3V power on the Arduino board
const byte buzzer = 12; //buzzer to arduino pin 9

// UV Sensor Values
float totalTimeOfExposure;
float accumulatedUV;
float uvIntensity;
// For Calibrate Sequence
float uvLevel;
float refLevel;
float zeroLevel = 0.9905;

int standardExposure = 1000; // Units are mJ/cm2
float outputVoltage;

// Defaults for screen and menus
byte backlight = 0; // Holds backlight status
byte brightness = 128; // Brightnes of backlight
int sleep_time = 11000;  // Time before screen turns off (11000 is always awake)
int refresh_delay = 500;// Info display refresh delay
int cycle_delay = 500;  // Work cycle refresh delay

// Timers
unsigned long currentMillis, onMillis, resetMillis, wake_time, wake_duration, last_refresh, last_cycle = 0;
unsigned long loopInterval; // the loop time for keeping track of the clock

// Buzzer and notifications
bool executed = false;
float notifyMultiple = 50.00;
long nextNotification = notifyMultiple;
VirtualDelay singleDelay; // default = millis

// Displayed text for each menu item (16 char max)
String menu_list[] = {
  "Brightness",
  "Standby Delay",
  "Raw Values",
  "Reset Timer?",
  "Calibration",
  "Print Dose"
};

// Count and store number of menu items
byte items = round((sizeof(menu_list) / sizeof(String) - 1));
byte item = 3;   // Menu position
byte menu_displayed = 0; // Menu displayed if 1

/*
 * EEPROM Read and Write
*/
void EEPROMWriteInt(int address, int value)
{
  byte two = (value & 0xFF);
  byte one = ((value >> 8) & 0xFF);

  EEPROM.update(address, two);
  EEPROM.update(address + 1, one);
}

int EEPROMReadInt(int address)
{
  long two = EEPROM.read(address);
  long one = EEPROM.read(address + 1);

  return ((two << 0) & 0xFFFFFF) + ((one << 8) & 0xFFFFFFFF);
}
/*
 * End of EEPROM section
*/

void displayInfo() {
  if ((currentMillis - last_refresh) >= refresh_delay) {
 
    // Debug Serial output
      Serial.print(" CURR ");
      Serial.print(currentMillis);
      Serial.print(" LASTR ");
      Serial.print(last_refresh);
      Serial.print(" REFDEL ");
      Serial.println(refresh_delay);
 
    // The next Block displays default screen data
    // Show time and UV values
    lcd.home(); // go to start without clearing
    sensorUVGet(); // Display UV Readings

    // Reset timer
    if ((currentMillis - last_cycle) >= cycle_delay) {
      lcd.setCursor(5, 0);
      lcd.print(":");
      float progress = mapfloat(accumulatedUV, 0.00, standardExposure, 0.00, 100.00);
 
      // Debug Serial output
      Serial.print(" progress ");
      Serial.print(progress);
      
      lcd_percentage(progress, 8, 11, 0); // Progress bar
    } else if ((currentMillis - last_cycle) >= (cycle_delay / 2)) {
     // Debug Serial output     
        Serial.print(uvLevel);
        Serial.print(" ");
        Serial.print(accumulatedUV);
        Serial.print(" ");
        Serial.println(standardExposure);
      
      if (uvLevel > 205) { // Flash clock dots only when there is light
        lcd.setCursor(5, 0);
        lcd.print(" ");
      }
    }
    last_refresh = millis();
  }
}

// Read sensors
void doWork() {
  if ((currentMillis - last_cycle) >= cycle_delay) {

// Buzz alarm whenever neccessary and then once each notification period
// Each buzzer size and type will have a resonant frequency that is much louder than others.
// Adjust the next section to suit your buzzer or preferences.

       if ( accumulatedUV > standardExposure && executed == false) {
        Serial.println("Buzzing");
        buzzerTone(3700);
        buzzerTone(4500);
        buzzerTone(3700);
        buzzerTone(4500);
      } else {
     // Debug Serial output  
        Serial.print("Exec? ");
        Serial.print(executed);
        Serial.print(" Next Notif ");
        Serial.println(nextNotification);
      }

    if (accumulatedUV > nextNotification && executed == false)
    {
      buzzerTone(3700);
      if (accumulatedUV > nextNotification && (nextNotification % 100) == 0 ) {
        buzzerTone(3300);
      }
      nextNotification += notifyMultiple;
      executed = true;
    // Debug Serial output
      Serial.print(nextNotification);
      Serial.print(" bool ");
      Serial.println(executed);
     
    }
    if (accumulatedUV > nextNotification && executed == true)
    {
      executed = false;
     // Debug Serial output 
  Serial.println(executed);
    }

  } else if ((currentMillis - last_cycle) >= (cycle_delay / 2)) {

  }
}

// Read a button press and return number of button pressed
int readButton() {
  int button_voltage = analogRead(BUTTON_PIN);
  int button_pressed = 0;
  Serial.println(button_voltage);

  if (button_voltage <= (BUTTON_NULL + 10)
      && button_voltage >= (BUTTON_NULL - 10)) {
    button_pressed = 0;
  }
  if (button_voltage <= (BUTTON_SELECT + 10)
      && button_voltage >= (BUTTON_SELECT - 10)) {
    button_pressed = 1;
  }
  if (button_voltage <= (BUTTON_LEFT + 10)
      && button_voltage >= (BUTTON_LEFT - 10)) {
    button_pressed = 2;
  }
  if (button_voltage <= (BUTTON_DOWN + 10)
      && button_voltage >= (BUTTON_DOWN - 10)) {
    button_pressed = 3;
  }
  if (button_voltage <= (BUTTON_UP + 10)
      && button_voltage >= (BUTTON_UP - 10)) {
    button_pressed = 4;
  }
  if (button_voltage <= (BUTTON_RIGHT + 10)
      && button_voltage >= (BUTTON_RIGHT - 10)) {
    button_pressed = 5;
  }

  // Wait until button is released before we return
  while (button_voltage <= (BUTTON_NULL - 10)) {
    button_voltage = analogRead(BUTTON_PIN);
    delay(50);
  }

  if (button_pressed > 0) {
    wake_time = millis(); // Reset the backlight timer
  }

  return button_pressed;
}

// Display the menu item on the LCD
void displayItem(int item) {
  // Clear the screen and print the item title
  lcd.clear();
  lcd.print(String(menu_list[item]));

  // Display the current value in a readable form
  // Compiler says these need defining outside the case statement!
  int percent, sec;
  unsigned long d, h, m, s;
  switch (item) {
    case 0: // Brightness is displayed as a percentage
      // We don't want to turn the screen completely off
      // and a value of 256 will overflow it so we trim
      // the values. I like the result.
      percent = round(((brightness - 16) / 224.0) * 100);
      lcd.setCursor(0, 1);
      lcd.print(String(percent) + "%");
      break;
    case 1: // Stand-by delay is shown in seconds
      if (sleep_time < 11000) {
        sec = sleep_time / 1000;
        lcd.setCursor(0, 1);
        lcd.print(String(sec) + "sec");
      }
      else {
        lcd.setCursor(0, 1);
        lcd.print("OFF");
      }
      break;
    case 2: // Raw Values
   
      printRawData();
        // Debug Serial output
            Serial.print(outputVoltage);
            Serial.print("V ");
            Serial.print(zeroLevel);
            Serial.print("Z ");
            Serial.print(refLevel);
            Serial.print("R ");
            Serial.print(uvLevel);
            Serial.print("U ");
            Serial.print(standardExposure);
    
      break;
    case 3: // ResetTimer reset timer
      resetTimer();
      break;
    case 4: // UV Sensor Calibration
      calibrateSensor();
      lcd.noBlink(); // Turn off blinking cursor before going back to default display
      break;
    case 5: // Print Dose

      lcd.clear();
      lcd.print("Print Dose ->");
      lcd.setCursor(0, 1);
      lcd.print(standardExposure);
      lcd.print("mJ/cm2");
    // Debug Serial output
       Serial.print("SAVE");
       Serial.println(standardExposure);
      break;
  }
}

// Change values of menu items
// Action is 0 for decreasing (left button) and
// 1 for increasing (right button)

void updateItem(int item, int action) {
  switch (item) {
    case 0: // Brightness
      // don't dim the screen to off
      if (action == 0 && brightness != 16) {
        brightness = brightness - 16;
        analogWrite(BACKLIGHT_PIN, brightness);
      }
      // 256 overflows and the screen is quite bright anyway
      else if (action == 1 && brightness != 240) {
        brightness = brightness + 16;
        analogWrite(BACKLIGHT_PIN, brightness);
      }
      break;
    case 1: // Standby Delay
      // Minimum delay is 2 seconds
      if (action == 0 && sleep_time > 2000) {
        sleep_time -= 1000;
      }
      // Max is 10 seconds (11 is off)
      else if (action == 1 && sleep_time < 11000) {
        sleep_time += 1000;
      }
      break;
    // To display info, we don't need to modify the value
    // so for item[2] (Uptime) we don't take any action.
    case 3: // Reset timer menu action
      if (action == 1) {
        // offset the already passed time so our exposure starts at zero seconds
        lcd.clear();
        lcd.print("00:00:00");
        lcd.setCursor(0, 1);
        lcd.print("Timer reset");
        delay(1000);
        wake_time -= sleep_time; // Clear the menu right away
        resetMillis = millis(); // reset the timer
        accumulatedUV = 0; // reset the UV dose
      }
      break;
    case 4: // Calibrate UV sensor
      if (action == 1) {
        // offset the already passed time so our exposure starts at zero seconds
        lcd.clear();
        lcd.print("Cover UV sensor");
        lcd.setCursor(0, 1);
        lcd.print("(Reading black)");
        delay(1000);

        byte numberOfReadings = 1000;
        float runningUVValue = 0;
        float runningREFValue = 0;

        for (int x = 0 ; x < numberOfReadings ; x++) {
          runningUVValue += analogRead(UVOUT);
          runningREFValue += analogRead(REF_3V3);

          delay(5);
          //Serial.println(x);
        }
        
        runningUVValue /= numberOfReadings;
        runningREFValue /= numberOfReadings;
        uvLevel = runningUVValue;
        refLevel = runningREFValue;
        zeroLevel = (3.30 / refLevel * uvLevel); // Base level from calibration cycle

        lcd.clear();
        lcd.print("UV ");
        lcd.print(uvLevel,4);
        lcd.print(" Ref ");
        lcd.print(refLevel,4);
        lcd.setCursor(0, 1);
        lcd.print("Zero ");
        lcd.print(zeroLevel,4);
        delay(3000);
        
        wake_time -= sleep_time; // Clear the menu right away
        resetMillis = millis(); // reset the timer
        accumulatedUV = 0; // reset the UV dose
       
      }
      break;
    case 5: // Standard Dose Adjustment (TODO add EEPROM memory for last set)
      // don't dim the screen to off
      if (action == 0 && standardExposure != 50) {
        standardExposure = standardExposure - 50;
      }
      // 256 overflows and the screen is quite bright anyway
      else if (action == 1 && standardExposure != 10000) {
        standardExposure = standardExposure + 50;
      }
    // Debug Serial output      
       Serial.print("SAVE");
       Serial.println(standardExposure);
      break;
  }
  displayItem(item);
}

void timeElapsed(int offTime) { // clock and timer display

  unsigned long allSeconds = (currentMillis - resetMillis) / 1000;
  int runHours = allSeconds / 3600;
  int secsRemaining = allSeconds % 3600;
  int runMinutes = secsRemaining / 60;
  int runSeconds = secsRemaining % 60;

     // Debug Serial output
    Serial.print("OFFT ");
    Serial.print(offTime/1000);
    Serial.print(" NOW ");
    Serial.print(currentMillis/1000);
    Serial.print(" RES ");
    Serial.print(resetMillis/1000);
    Serial.print(" DISP ");
    Serial.print(allSeconds);

  char buf[21];
  sprintf(buf, "%02d:%02d:%02d", runHours, runMinutes, runSeconds);
    // Debug Serial output
   Serial.print(buf);
  lcd.print(buf);
}

void resetTimer() {
  // Reset the timer and the accumulated exposure
  // See Case 4 in the menus for the button action
  lcd.clear();
  lcd.blink();
  lcd.print("Timer Reset?");
  lcd.setCursor(0, 1);
  lcd.print("Press (RIGHT)");
  lcd.noBlink();
}

void calibrateSensor() { // Calibrate the Sensor

    // Debug Serial output
  Serial.print("Calibrate Black Voltage");
  lcd.home();
  lcd.clear();
  lcd.blink();
  lcd.print("Calibrate Black");
  lcd.setCursor(0, 1);
  lcd.print("Press (RIGHT)");

}

// Get UV Sensor Data and Average it
void sensorUVGet() {
  unsigned long startSensor = millis(); // for elapsed time
  byte numberOfReadings = 430;
  float runningUVValue = 0;
  float runningREFValue = 0;

  for (int x = 0 ; x < numberOfReadings ; x++) {

    runningUVValue += analogRead(UVOUT);
    runningREFValue += analogRead(REF_3V3);

    // Debug Serial output
    Serial.println(x);
  }
  runningUVValue /= numberOfReadings;
  runningREFValue /= numberOfReadings;

  uvLevel = runningUVValue; // Offset for the backlight level
  refLevel = runningREFValue;  // Offset for the backlight level

  //Use the 3.3V power pin as a reference to get a 1% accurate output value from sensor
  outputVoltage = 3.295 / refLevel * uvLevel;

  // Map Function
  float uvIntensity = mapfloat(outputVoltage, zeroLevel, 2.80, 0.00, 15.00); //Convert the voltage to a UV intensity level

  unsigned long endSensor = millis(); // for elapsed time

  if ( uvIntensity >= 0.01 ) { // Accumulated dose is multiplied by fraction of a second 392 millis to read sensor, 500 delay, 11 other
    accumulatedUV += uvIntensity * (refresh_delay + endSensor - startSensor + 22) / 1000 ; // only add accumulated UV dose on intensities greater than zero
  } else {
    uvIntensity = 0; // make sure zero values don't show
  }

  totalTimeOfExposure = standardExposure / uvIntensity / 60.00 ; // a dose of 2000 mJ/cm2 is about the standard cyanotype exposure
  if (totalTimeOfExposure > 600 ) { // Don't estimate extremely long exposure times
    totalTimeOfExposure = INFINITY;
  }
 
  timeElapsed(0); // Exposure timer
    // Debug Serial output
     Serial.print("  ##  ");
     Serial.println(resetMillis);

    // Debug Serial output
    Serial.print(" Output V ");
    Serial.print(outputVoltage);
    Serial.print(" Zero ");
    Serial.print(zeroLevel);
    Serial.print(" Ref ");
    Serial.print(refLevel);
    Serial.print(" Ref RAW ");
    Serial.print(runningREFValue);
    Serial.print(" UV Lev ");
    Serial.print(uvLevel);
    Serial.print(" UV Int ");
    Serial.print(uvIntensity);
    Serial.print(" ");
    Serial.print(standardExposure);
    Serial.println();

  // Output various info to the screen
  lcd.setCursor(12, 0); // After Exposure Tme
  lcd.print(" ");
  lcd.print(totalTimeOfExposure); // Estimated Time on the First line
  lcd.setCursor(0, 1); // Second line
  lcd.print(uvIntensity);
  lcd.print("mW "); // mW/cm2 units for momentary intensity
  lcd.setCursor(8, 1); // After intensity Tme
  if (accumulatedUV > 100 && accumulatedUV <= 999) {
    lcd.setCursor(13, 1);
    lcd.print("      ");
    lcd.setCursor(8, 1); // After intensity Tme
    lcd.print(accumulatedUV, 1); // print with one decimal place
  } else if (accumulatedUV > 999) {
    lcd.setCursor(13, 1);
    lcd.print("      ");
    lcd.setCursor(8, 1); // After intensity Tme
    lcd.print(accumulatedUV, 0);
  } else {
    lcd.print(accumulatedUV);
  }
  lcd.print("mJ"); //  mJ/cm2 units for accumulated UV exposure

}

void printRawData() { // Print Raw Data to screen
  lcd.clear();
  lcd.print(outputVoltage,4);
  lcd.print("V ");
  lcd.print(zeroLevel,4);
  lcd.print("Z");
  lcd.setCursor(0, 1);
  lcd.print(refLevel,3);
  lcd.print("R ");
  lcd.print(uvLevel);
  lcd.print("U ");
}
//Takes an average of readings on a given pin
//Returns the average
float averageAnalogRead(int pinToRead)
{
  byte numberOfReadings = 50;
  unsigned int runningValue = 0;

  for (int x = 0 ; x < numberOfReadings ; x++)
    runningValue += analogRead(pinToRead);
  runningValue /= numberOfReadings;

  return (runningValue);
}

//The Arduino Map function but for floats
//From: http://forum.arduino.cc/index.php?topic=3922.0
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void lcd_percentage(int percentage, int cursor_x, int cursor_x_end, int cursor_y) {

  byte percentage_1[8] = { B11111, B10000, B10000, B10000, B10000, B10000, B10000, B11111 };
  byte percentage_2[8] = { B11111, B11000, B11000, B11000, B11000, B11000, B11000, B11111 };
  byte percentage_3[8] = { B11111, B11100, B11100, B11100, B11100, B11100, B11100, B11111 };
  byte percentage_4[8] = { B11111, B11110, B11110, B11110, B11110, B11110, B11110, B11111 };
  byte percentage_5[8] = { B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111 };

  byte percentage_6[8] = { B11111, B00000, B00000, B00000, B00000, B00000, B00000, B11111 }; // Top bottom lines

  lcd.createChar(0, percentage_1);
  lcd.createChar(1, percentage_2);
  lcd.createChar(2, percentage_3);
  lcd.createChar(3, percentage_4);
  lcd.createChar(4, percentage_5);
  lcd.createChar(5, percentage_6);

  float calc = 0;
  if (calc <= 25 ) { // Only count to 100% then stop
    calc = mapfloat(percentage, 0, 100, 0, 25); // 0%, 100%, cursor at first pos (8) , cursor at last pos (11) 25
  }

  lcd.setCursor(cursor_x, cursor_y); // First write the border for the graph
  lcd.write((byte)5);
  lcd.write((byte)5);
  lcd.write((byte)5);
  lcd.write((byte)5);
  lcd.write((byte)5);

  // Serial.print(" calc ");
  // Serial.println(calc);

  if (cursor_x <= cursor_x_end && calc <= 25) { // Only advance the cursor the defined end of the bar area
    while (calc >= 5) {
      lcd.setCursor(cursor_x, cursor_y);
      lcd.write((byte)4);
      calc -= 5;
      cursor_x++;
    }
    while (calc >= 4 && calc < 5) {
      lcd.setCursor(cursor_x, cursor_y);
      lcd.write((byte)3);
      calc -= 4;

    }
    while (calc >= 3 && calc < 4) {
      lcd.setCursor(cursor_x, cursor_y);
      lcd.write((byte)2);
      calc -= 3;
    }
    while (calc >= 2 && calc < 3) {
      lcd.setCursor(cursor_x, cursor_y);
      lcd.write((byte)1);
      calc -= 2;
    }
    while (calc >= 1 && calc < 2) {
      lcd.setCursor(cursor_x, cursor_y);
      lcd.write((byte)0);
      calc -= 1;
    }
  } else {
    lcd.setCursor(cursor_x, cursor_y); // After completing the exposure write a full 5 bars
    lcd.write((byte)4);
    lcd.write((byte)4);
    lcd.write((byte)5);
    lcd.write((byte)4);
    lcd.write((byte)4);
  }
}

void buzzerTone(int frequency) {
    singleDelay.start(400); // calls while running are ignored
    while (! singleDelay.elapsed()) {
      tone(buzzer, frequency);
    }
    noTone(buzzer);
}

/*
   SETUP AND LOOP BELOW
*/

void setup(void) {
  buzzerTone(1800);
  buzzerTone(3800);
  buzzerTone(3900);

#ifdef DEBUG
  Serial.begin(9600);
#endif

  // Setup LCD
  lcd.begin(16, 2);
  lcd.noCursor();

  // Setup button and backlight pins
  pinMode(BUTTON_PIN, INPUT);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  pinMode(buzzer, OUTPUT); // Set buzzer - pin 12 as an output

  // Turn backlight on
  analogWrite(BACKLIGHT_PIN, brightness);
  backlight = 1;
  standardExposure = EEPROMReadInt(0); // Read standardExposure from EEPROM on boot
  zeroLevel = EEPROMReadInt(2)/10000.0000;// Read Calibrated Zero Level from EEPROM on boot
  lcd.home();
  lcd.print("Dose ");
  lcd.print(standardExposure);
  lcd.print(" mJ/cm2");
  lcd.setCursor(0, 1);
  lcd.print(" Zero ");
  lcd.print(zeroLevel,4);
    delay(2500);
  resetMillis = millis(); // reset the timer
  accumulatedUV = 0; // reset the UV dose
  lcd.clear();

  // Read EEPROM and overwrite the standard variable from memory instead
  // Debug Serial output
  Serial.println("STAND");
  Serial.println(standardExposure);
  return;
}

void loop(void) {

  //  The Arduino Map function but for floats
  //  From: http://forum.arduino.cc/index.php?topic=3922.0
  //  float reffloat(float x, float in_min, float in_max, float out_min, float out_max)  
  
      // Debug Serial output
      Serial.print("@@START  ");
  Serial.print(millis());

  // Note how long we have been running for
  currentMillis = millis();
  // and how long since the last button push
  wake_duration = currentMillis - wake_time;

  // start reading from the first byte (address 0) of the EEPROM
  int address = 0;
  int standardExposureSaved;
  float zeroLevelSaved;
  
  /*
     Save Defaults to the EEPROM
  */
  if ((currentMillis - last_cycle) >= (cycle_delay) && wake_duration >= sleep_time ) {
    zeroLevelSaved = EEPROMReadInt(2)/10000.0000;// Read Calibrated Zero Level from EEPROM
   
     // Debug Serial output
    Serial.print("First Read ");
    Serial.println(zeroLevelSaved,4);
        standardExposureSaved = EEPROMReadInt(0);
        if (zeroLevelSaved != zeroLevel) {
      // Change zerolevel with four decimal places into an unsigned integer (65535 max)
      unsigned int zeroLevelInt = zeroLevel * 10000;
      //One simple call, with the address first and the object second.
      //EEPROM.put(eeAddress, zeroLevel);
      EEPROMWriteInt(2, zeroLevelInt); // Address 2 is after 0,1 used by standardExposure
      lcd.clear();
      zeroLevelSaved = EEPROMReadInt(2) / 10000.0000;
      zeroLevel = zeroLevelSaved; // Make sure to set the current zero level to the saved level
  // Debug Serial output
      Serial.print("Saved ");
      Serial.println(zeroLevelSaved,4);
      lcd.print("ZERO ");
      lcd.print(zeroLevel,4);
      delay(1200);
        // Debug Serial output
      Serial.print("Zero Level");
         Serial.print(zeroLevel);
         Serial.print(" Saved ZLev");
         Serial.println(zeroLevelSaved);
    }
    if ( standardExposureSaved != standardExposure) { // Write the standard Exposure
      EEPROMWriteInt(0, standardExposure);
      lcd.clear();
      standardExposureSaved = EEPROMReadInt(0);
      lcd.print("Saved ");
      lcd.print(standardExposure);
      delay(1200);
  // Debug Serial output
        Serial.print(" WRITE ");
        Serial.println(standardExposure);
        delay(1000);
        Serial.print(" READING ");
        standardExposureSaved = EEPROMReadInt(0);
    }
  }

  // Hide the menu/go to sleep
  if (backlight == 1 && wake_duration >= sleep_time) {
    if (menu_displayed == 1) {
      menu_displayed = 0;
      item = 3;
      lcd.clear();
      wake_time = millis();
    } else if (sleep_time != 11000) {
      digitalWrite(BACKLIGHT_PIN, LOW);
      lcd.clear();
      backlight = 0;
      wake_time = 0;
    }
  }

  // Read a button press
  int button = readButton();
  // Wake up when the select button is pushed
  if (button == 1 && backlight == 0) {
    analogWrite(BACKLIGHT_PIN, brightness);
    backlight = 1;
  }

  // Show the menu while awake and the select button is pushed
  else if (button == 1 && menu_displayed == 0) {
    // Set menu flag
    menu_displayed = 1;

    // Display the first item
    displayItem(item);

  }

  else if (button >= 1 && menu_displayed == 1) {
    switch (button) {
      case 1: // Select button pushed
        // Exit menu
        menu_displayed = 0;
        item = 3;
        lcd.clear();
        return;
        break;
      case 2: // Left button pushed
        // Decrease value
        updateItem(item, 0);
        break;
      case 3: // Down button pushed
        // Display next menu item
        if (++item > items) {
          item = 0;
        }
        displayItem(item);
        break;
      case 4: // Up button pushed
        // Display previous menu item
        if (--item < 0) {
          item = items;
        }
        displayItem(item);
        break;
      case 5: // Right button pushed
        // Increase value
        updateItem(item, 1);
        break;
    }
  }

  // Show info while awake
  else if (backlight == 1 && menu_displayed == 0) {
    displayInfo();
  }

  // Continue processing
  doWork();
  
  // Debug Serial output
  Serial.print("@@END  ");
  Serial.println(millis());
  return;
}
