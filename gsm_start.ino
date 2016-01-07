/***************************************************
*/

#include "Adafruit_FONA.h"
#include <EEPROM.h>
const int EEPROM_MIN_ADDR = 0;
const int EEPROM_MAX_ADDR = 511;

#define FONA_RST 4
#define FOB_LOCK 13
#define FOB_START 14
// this is a large buffer for replies
char replybuffer[255];

// Hardware serial is also possible!
HardwareSerial *fonaSerial = &Serial1;

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);

void setup() {
  while (!Serial);

  Serial.begin(115200);
  Serial.println(F("FONA SMS caller ID test"));
  Serial.println(F("Initializing....(May take 3 seconds)"));

  pinMode(FOB_LOCK, OUTPUT);
  pinMode(FOB_START, OUTPUT);

  // make it slow so its easy to read!
  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    Serial.println(F("Couldn't find FONA"));
    while(1);
  }
  Serial.println(F("FONA is OK"));

  // Print SIM card IMEI number.
  char imei[15] = {0}; // MUST use a 16 character buffer for IMEI!
  uint8_t imeiLen = fona.getIMEI(imei);
  if (imeiLen > 0) {
    Serial.print("SIM card IMEI: "); Serial.println(imei);
  }
  
  Serial.println("FONA Ready");
}

void loop() {
  
  char* bufPtr = replybuffer;    //handy buffer pointer
  
  if (fona.available())      //any data available from the FONA?
  {
    int slot = 0;            //this will be the slot number of the SMS
    int charCount = 0;
    uint16_t smslen;
    char password[20];
    //Read the notification into replybuffer
    do  {
      *bufPtr = fona.read();
      Serial.write(*bufPtr);
      delay(1);
    } while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(replybuffer)-1)));
    
    //Add a terminal NULL to the notification string
    *bufPtr = 0;
    
    //Scan the notification string for an SMS received notification.
    //  If it's an SMS message, we'll get the slot number in 'slot'
    if (1 == sscanf(replybuffer, "+CMTI: \"SM\",%d", &slot)) {
      Serial.print("slot: "); Serial.println(slot);

      char callerIDbuffer[32]; //we'll store the SMS sender number here
      
      // Retrieve SMS sender address/phone number.
      if (! fona.getSMSSender(slot, callerIDbuffer, 250)) {
        Serial.println("Didn't find SMS message in slot!");
      }
      Serial.print(F("FROM: ")); Serial.println(callerIDbuffer);
      
      if (! fona.readSMS(slot, replybuffer, 250, &smslen)) {
        Serial.println("Failed to read SMS!");
      }
      char responseBuffer[255]; //we'll store the response here
      
      eepromReadPass(0,password, sizeof(password));
      char* recText = strtok(replybuffer, ":");
      if ((recText != NULL) && strcmp(recText, password)) {
        recText = strtok(NULL, "");
        if ((recText != NULL) && strcmp(recText, "start")) {
          // successful command
          if(startCar()) {
            strcpy(responseBuffer, "Success");
          } else {
            strcpy(responseBuffer, "Start Failed");
          }
        } else {
          strcpy(responseBuffer, "Bad Command - Failed");
        }
      } else {
        strcpy(responseBuffer, "Bad Password or spam\n");
        strcat(responseBuffer,replybuffer);
      }
      
      //Send back an automatic response
      Serial.println("Sending reponse...");
      if (! fona.sendSMS(callerIDbuffer, responseBuffer)) {
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("Sent!"));
      }
      
      // delete the original msg after it is processed
      //   otherwise, we will fill up all the slots
      //   and then we won't be able to receive SMS anymore
      if (fona.deleteSMS(slot)) {
        Serial.println(F("OK!"));
      } else {
        Serial.println(F("Couldn't delete"));
      }
    }
  }
}

boolean eepromAddrOk(int addr) {
  return ((addr >= EEPROM_MIN_ADDR) && (addr <= EEPROM_MAX_ADDR));
}

boolean eepromWritePass(int startAddr, const byte* array, int numBytes) {
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
  digitalWrite(pin, HIGH);
  delay(onTime);
  digitalWrite(pin, LOW);
  delay(offTime);
}

boolean startCar() {
  pressFobButton(FOB_LOCK, 200, 200);
  pressFobButton(FOB_LOCK, 200, 200);
  pressFobButton(FOB_START, 2000, 0);
  //check some pin
  return true;
}



