/***************************************************
*/

#include <Adafruit_FONA.h>
#include <Adafruit_SleepyDog.h>
#include <avr/sleep.h>
#include <EEPROM.h>

//#define SLEEP_ENABLED // controls whether we have a watchdog or not

// enables all debug prints. this gets screwy with USB Serial so tied
// directly to power savings enterSleep() command
#define DEBUG
// debug prints
#ifdef DEBUG
  #undef DEBUG_PRINT // fix shadowing DEFs
  #undef DEBUG_PRINTLN // fix shadowing DEFs
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #undef DEBUG_PRINT // fix shadowing DEFs
  #undef DEBUG_PRINTLN // fix shadowing DEFs
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// key fob pin definitions
#define FOB_LOCK 12
#define FOB_UNLOCK 13
#define FOB_START 14
#define FOB_ALARM 15
#define FOB_TRUNK 16

// gsm module pin definitions
#define FONA_RST 3
#define FONA_PS 4
#define FONA_KEY 5
#define FONA_RI 6
#define FONA_TX 7
#define FONA_RX 8
#define FONA_NS 9

// unused pins array - used to save some power
unsigned int unused[] = {0, 1, 2, 10, 11, 17, 18, 19, 20, 21, 22, 23};

// eeprom constants
const int EEPROM_MIN_ADDR = 0;
const int EEPROM_MAX_ADDR = 511;

// this is a large buffer for replies
char replybuffer[255];

// Hardware serial is also possible!
HardwareSerial *fonaSerial = &Serial1;

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

volatile bool gotSMS = false;

void setup() {

  // determine last reset source
  boolean resetByWatchdog = bit_is_set(MCUSR, WDRF);

#ifndef SLEEP_ENABLED
  Watchdog.enable(8000);
#endif

#ifdef DEBUG
  Serial.begin(115200);
#endif
  DEBUG_PRINT(F("Reset by Watchdog?: ")); DEBUG_PRINTLN(resetByWatchdog);
  DEBUG_PRINTLN(F("GSM Start Module"));
  DEBUG_PRINTLN(F("Initializing FONA....(May take 3 seconds)"));
#ifndef SLEEP_ENABLED
  Watchdog.reset();
#endif
  // pin config for unused pins
  for(unsigned int i = 0; i < sizeof(unused)/sizeof(unsigned int); i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
#ifndef SLEEP_ENABLED
  Watchdog.reset();
#endif
  // pin config for Key Fob
  digitalWrite(FOB_LOCK, HIGH);
  pinMode(FOB_LOCK, OUTPUT);
  digitalWrite(FOB_UNLOCK, HIGH);
  pinMode(FOB_UNLOCK, OUTPUT);
  digitalWrite(FOB_START, HIGH);
  pinMode(FOB_START, OUTPUT);
  digitalWrite(FOB_ALARM, HIGH);
  pinMode(FOB_ALARM, OUTPUT);
  digitalWrite(FOB_TRUNK, HIGH);
  pinMode(FOB_TRUNK, OUTPUT);
#ifndef SLEEP_ENABLED
  Watchdog.reset();
#endif
  // pin config for fona
  // FONA_RST handled by Fona library
  pinMode(FONA_PS, INPUT);
  digitalWrite(FONA_KEY, HIGH);
  pinMode(FONA_KEY, OUTPUT);
  pinMode(FONA_RI, INPUT);
  // FONA_TX -> Hardware Serial RX
  // FONA_RX -> Hardware Serial TX
  pinMode(FONA_NS, INPUT);
#ifndef SLEEP_ENABLED
  Watchdog.reset();
#endif
  // power off the Fona if we took a hit from Watchdog
  // will get powered on by next block
  if (resetByWatchdog) {
    DEBUG_PRINTLN(F("Powering off Fona"));
    digitalWrite(FONA_KEY, LOW);
    delay(2000);
    digitalWrite(FONA_KEY, HIGH);
  }
#ifndef SLEEP_ENABLED
  Watchdog.reset();
#endif
  // power on the Fona if it is off
  if (!digitalRead(FONA_PS)) {
    DEBUG_PRINTLN(F("Powering on Fona"));
    digitalWrite(FONA_KEY, LOW);
    delay(2000);
    digitalWrite(FONA_KEY, HIGH);
  }
#ifndef SLEEP_ENABLED
  Watchdog.reset();
#endif
  // make it slow so its easy to read!
  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    DEBUG_PRINTLN(F("Couldn't find FONA"));
    while (1);
  }
  DEBUG_PRINTLN(F("FONA is OK"));
#ifndef SLEEP_ENABLED
  Watchdog.reset();
#endif
  // Print SIM card IMEI number.
  char imei[15] = {0}; // MUST use a 16 character buffer for IMEI!
  uint8_t imeiLen = fona.getIMEI(imei);
  if (imeiLen > 0) {
    DEBUG_PRINT("SIM card IMEI: "); DEBUG_PRINTLN(imei);
  }
#ifndef SLEEP_ENABLED
  Watchdog.reset();
#endif
  fona.setSMSInterrupt(1);
  attachInterrupt(digitalPinToInterrupt(FONA_RI), message, LOW);
  DEBUG_PRINTLN(F("FONA Ready, Arduino Ready"));
#ifndef SLEEP_ENABLED
  Watchdog.reset();
#endif
}

void loop() {
  if (gotSMS) {
    delay(5000);
    handleSMS();
  } else {
    #ifdef SLEEP_ENABLED
      #ifndef DEBUG
      enterSleep();
      #endif
    #else
      Watchdog.reset();
    #endif
  }
}

void message() {
  gotSMS = 1;
}

void handleSMS() {
  gotSMS = 0;
  uint16_t smslen;
  char password[20];
  int8_t smsnum = fona.getNumSMS();
  char callerIDbuffer[32]; //we'll store the SMS sender number here
  DEBUG_PRINT(F("Num SMS: "));
  DEBUG_PRINTLN(smsnum);
#ifndef SLEEP_ENABLED
  Watchdog.reset();
#endif
  for (int i = 1; i < smsnum + 1; i++) {
  #ifndef SLEEP_ENABLED
    Watchdog.reset();
  #endif
    // Retrieve SMS sender address/phone number.
    if (! fona.getSMSSender(i, callerIDbuffer, 250)) {
      DEBUG_PRINTLN(F("Didn't find SMS message in slot!"));
    }
    DEBUG_PRINT(F("FROM: ")); DEBUG_PRINTLN(callerIDbuffer);
  #ifndef SLEEP_ENABLED
    Watchdog.reset();
  #endif
    if (! fona.readSMS(i, replybuffer, 250, &smslen)) {
      DEBUG_PRINTLN(F("Failed to read SMS!"));
    }
  #ifndef SLEEP_ENABLED
    Watchdog.reset();
  #endif
    eepromReadPass(0, password, sizeof(password));
    char* recText = strtok(replybuffer, ":");
    if ((recText != NULL) && !strcmp(recText, password)) {
      recText = strtok(NULL, "");
      if ((recText != NULL) && !strcmp(recText, "start")) {
        // successful command
        DEBUG_PRINTLN(F("Starting Car..."));
        startCar();
      } else if ((recText != NULL) && !strcmp(recText, "unlock")) {
        DEBUG_PRINTLN(F("Unlocking Car..."));
        unlockCar();
      } else if ((recText != NULL) && !strcmp(recText, "lock")) {
        DEBUG_PRINTLN(F("Locking Car..."));
        lockCar();
      } else if ((recText != NULL) && !strcmp(recText, "alarm")) {
        DEBUG_PRINTLN(F("Triggering Alarm..."));
        alarmCar();
      } else if ((recText != NULL) && !strcmp(recText, "trunk")) {
        DEBUG_PRINTLN(F("Opening Trunk..."));
        openTrunk();
      } else if ((recText != NULL) && !strcmp(recText, "reset")) {
        DEBUG_PRINTLN(F("Forcing a Watchdog Reset..."));
        while(1);
      } else {
        DEBUG_PRINTLN(F("Invalid Command..."));
      }
    } else {
      DEBUG_PRINTLN(F("Bad Password or spam..."));
    }
  #ifndef SLEEP_ENABLED
    Watchdog.reset();
  #endif
    // delete the original msg after it is processed
    //   otherwise, we will fill up all the slots
    //   and then we won't be able to receive SMS anymore
    if (fona.deleteSMS(i)) {
      DEBUG_PRINTLN(F("REMOVED SMS!"));
    } else {
      DEBUG_PRINTLN(F("Couldn't delete"));
    }
  #ifndef SLEEP_ENABLED
    Watchdog.reset();
  #endif
  }
}

void enterSleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  sleep_enable();

  sleep_mode();

  sleep_disable();
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

boolean openTrunk() {
  pressFobButton(FOB_TRUNK, 2000, 0);
  //check some pin
  return true;
}
