/***************************************************
*/

#include "Adafruit_FONA.h"
#include <avr/sleep.h>
#include <EEPROM.h>
const int EEPROM_MIN_ADDR = 0;
const int EEPROM_MAX_ADDR = 511;

// pin definitions
#define FONA_RST 4
#define FOB_LOCK 12
#define FOB_UNLOCK 13
#define FOB_START 14
#define FOB_ALARM 15
#define FOB_TRUNK 16
#define RI 6

// debug prints
#ifdef DEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif
// this is a large buffer for replies
char replybuffer[255];

// Hardware serial is also possible!
HardwareSerial *fonaSerial = &Serial1;

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

volatile bool gotSMS = false;

void setup() {

#ifdef DEBUG
  Serial.begin(115200);
#endif
  DEBUG_PRINTLN(F("GSM Start Module"));
  DEBUG_PRINTLN(F("Initializing FONA....(May take 3 seconds)"));

  // pin config for Key Fob
  digitalWrite(FOB_LOCK, HIGH);
  pinMode(FOB_LOCK, OUTPUT);
  digitalWrite(FOB_UNLOCK, HIGH);
  pinMode(FOB_UNLOCK, OUTPUT);
  digitalWrite(FOB_START, HIGH);
  pinMode(FOB_START, OUTPUT);
  digitalWrite(FOB_ALARM, HIGH);
  pinMode(FOB_ALARM, OUTPUT);

  // make it slow so its easy to read!
  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    DEBUG_PRINTLN(F("Couldn't find FONA"));
    while (1);
  }
  DEBUG_PRINTLN(F("FONA is OK"));

  // Print SIM card IMEI number.
  char imei[15] = {0}; // MUST use a 16 character buffer for IMEI!
  uint8_t imeiLen = fona.getIMEI(imei);
  if (imeiLen > 0) {
    DEBUG_PRINT("SIM card IMEI: "); DEBUG_PRINTLN(imei);
  }

  fona.setSMSInterrupt(1);
  attachInterrupt(digitalPinToInterrupt(RI), message, LOW);
  DEBUG_PRINTLN("FONA Ready, Arduino Ready");

}

void message() {
  gotSMS = 1;
}

void handleSMS() {
  gotSMS = 0;
  int charCount = 0;
  uint16_t smslen;
  char password[20];
  int8_t smsnum = fona.getNumSMS();
  char callerIDbuffer[32]; //we'll store the SMS sender number here
  DEBUG_PRINT("Num SMS: ");
  DEBUG_PRINTLN(smsnum);
  for (int i = 1; i < smsnum + 1; i++) {
    // Retrieve SMS sender address/phone number.
    if (! fona.getSMSSender(i, callerIDbuffer, 250)) {
      DEBUG_PRINTLN("Didn't find SMS message in slot!");
    }
    DEBUG_PRINT(F("FROM: ")); DEBUG_PRINTLN(callerIDbuffer);

    if (! fona.readSMS(i, replybuffer, 250, &smslen)) {
      DEBUG_PRINTLN("Failed to read SMS!");
    }
    char responseBuffer[255]; //we'll store the response here

    eepromReadPass(0, password, sizeof(password));
    char* recText = strtok(replybuffer, ":");
    if ((recText != NULL) && !strcmp(recText, password)) {
      recText = strtok(NULL, "");
      if ((recText != NULL) && !strcmp(recText, "start")) {
        // successful command
        if (startCar()) {
          strcpy(responseBuffer, "Start Success");
        } else {
          strcpy(responseBuffer, "Start Failed");
        }
      } else if ((recText != NULL) && !strcmp(recText, "unlock")) {
        if (unlockCar()) {
          strcpy(responseBuffer, "Unlock Success");
        } else {
          strcpy(responseBuffer, "Unlock Failed");
        }
      } else if ((recText != NULL) && !strcmp(recText, "lock")) {
        if (lockCar()) {
          strcpy(responseBuffer, "Lock Success");
        } else {
          strcpy(responseBuffer, "Lock Failed");
        }
      } else if ((recText != NULL) && !strcmp(recText, "alarm")) {
        if (alarmCar()) {
          strcpy(responseBuffer, "Alarm Success");
        } else {
          strcpy(responseBuffer, "Alarm Failed");
        }
      } else if ((recText != NULL) && !strcmp(recText, "batt")) {
        char bat[5];
        uint16_t vbat;
        if (fona.getBattVoltage(&vbat)) {
          sprintf(bat, "%d", vbat);
          strcpy(responseBuffer, bat);
        } else {
          strcpy(responseBuffer, "Battery Read Failed");
        }
      } else {
        strcpy(responseBuffer, "Bad Command - Failed");
      }
    } else {
      strcpy(responseBuffer, "Bad Password or spam\n");
      strcat(responseBuffer, replybuffer);
    }

    //Send back an automatic response
    DEBUG_PRINTLN("Sending reponse...");
    if (! fona.sendSMS(callerIDbuffer, responseBuffer)) {
      DEBUG_PRINTLN(F("Failed"));
    } else {
      DEBUG_PRINTLN(F("Sent!"));
    }

    // delete the original msg after it is processed
    //   otherwise, we will fill up all the slots
    //   and then we won't be able to receive SMS anymore
    if (fona.deleteSMS(i)) {
      DEBUG_PRINTLN(F("OK!"));
    } else {
      DEBUG_PRINTLN(F("Couldn't delete"));
    }
  }
}

void enterSleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  sleep_enable();

  sleep_mode();

  sleep_disable();
}

void loop() {
  if (gotSMS) {
    delay(5000);
    handleSMS();
  } else {
    enterSleep();
  }
}

boolean eepromAddrOk(int addr) {
  return ((addr >= EEPROM_MIN_ADDR) && (addr <= EEPROM_MAX_ADDR));
}

boolean eepromWriteBytes(int startAddr, const byte* array, int numBytes) {
  // counter
  int i;

  // both first byte and last byte addresses must fall within
  // the allowed range
  if (!eepromAddrOk(startAddr) || !eepromAddrOk(startAddr + numBytes)) {
    return false;
  }

  for (i = 0; i < numBytes; i++) {
    EEPROM.write(startAddr + i, array[i]);
  }

  return true;
}

boolean eepromWritePass(int addr, const char* string) {

  int numBytes; // actual number of bytes to be written

  //write the string contents plus the string terminator byte (0x00)
  numBytes = strlen(string) + 1;

  return eepromWriteBytes(addr, (const byte*)string, numBytes);
}

boolean eepromReadPass(int addr, char* buffer, int bufSize) {
  byte ch; // byte read from eeprom
  int bytesRead; // number of bytes read so far

  if (!eepromAddrOk(addr)) { // check start address
    return false;
  }

  if (bufSize == 0) { // how can we store bytes in an empty buffer ?
    return false;
  }

  // is there is room for the string terminator only, no reason to go further
  if (bufSize == 1) {
    buffer[0] = 0;
    return true;
  }

  bytesRead = 0; // initialize byte counter
  ch = EEPROM.read(addr + bytesRead); // read next byte from eeprom
  buffer[bytesRead] = ch; // store it into the user buffer
  bytesRead++; // increment byte counter

  // stop conditions:
  // - the character just read is the string terminator one (0x00)
  // - we have filled the user buffer
  // - we have reached the last eeprom address
  while ( (ch != 0x00) && (bytesRead < bufSize) && ((addr + bytesRead) <= EEPROM_MAX_ADDR) ) {
    // if no stop condition is met, read the next byte from eeprom
    ch = EEPROM.read(addr + bytesRead);
    buffer[bytesRead] = ch; // store it into the user buffer
    bytesRead++; // increment byte counter
  }

  // make sure the user buffer has a string terminator, (0x00) as its last byte
  if ((ch != 0x00) && (bytesRead >= 1)) {
    buffer[bytesRead - 1] = 0;
  }

  return true;
}

void pressFobButton(int pin, int onTime, int offTime) {
  digitalWrite(pin, LOW);
  delay(onTime);
  digitalWrite(pin, HIGH);
  delay(offTime);
}

boolean startCar() {
  pressFobButton(FOB_LOCK, 200, 200);
  pressFobButton(FOB_LOCK, 200, 200);
  pressFobButton(FOB_START, 2000, 0);
  //check some pin
  return true;
}

boolean unlockCar() {
  pressFobButton(FOB_UNLOCK, 200, 200);
  pressFobButton(FOB_UNLOCK, 200, 0);
  //check some pin
  return true;
}

boolean lockCar() {
  pressFobButton(FOB_LOCK, 200, 200);
  pressFobButton(FOB_LOCK, 200, 0);
  //check some pin
  return true;
}

boolean alarmCar() {
  pressFobButton(FOB_ALARM, 200, 200);
  //check some pin
  return true;
}



