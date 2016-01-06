/***************************************************
*/

#include "Adafruit_FONA.h"
#include <EEPROM.h>

#define FONA_RST 4

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
    bool stat = 
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
      char* pass = readPassword();
      char* recText = strtok(replybuffer, ":");
      if (strcmp(recText, pass)) {
        Serial.println
      }
      if (badMessage) {
        // spam or failed message
        
      }
      /// do actions to start car here!!!!!
      /// do actions to start car here!!!!!

      //Send back an automatic response
      Serial.println("Sending reponse...");
      if (true) {
        stat = fona.sendSMS(replybuffer, "Success!");
      } else {
        stat = fona.sendSMS(replybuffer, "Failed!");
      }
      
      if (!stat) {
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

void writePassword(const char * password) {
  EEPROM.put(0, password);
}

char* readPassword() {
  char* password;
  EEPROM.get(0, password)
  return password;
}

