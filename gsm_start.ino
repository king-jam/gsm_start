/***************************************************
  Adafruit MQTT Library FONA Example

  Designed specifically to work with the Adafruit FONA
  ----> http://www.adafruit.com/products/1946
  ----> http://www.adafruit.com/products/1963
  ----> http://www.adafruit.com/products/2468
  ----> http://www.adafruit.com/products/2542

  These cellular modules use TTL Serial to communicate, 2 pins are
  required to interface.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/
#include <Adafruit_SleepyDog.h>
#include "Adafruit_FONA.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_FONA.h"

/*************************** FONA Pins ***********************************/

// Default pins for Feather 32u4 FONA
#define FONA_RST 3
#define FONA_PS 4
#define FONA_KEY 5
#define FONA_RI 6
#define FONA_TX 7
#define FONA_RX 8
#define FONA_NS 9

//SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
HardwareSerial *fonaSS = &Serial1;
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

/************************* FOB Pins ******************************************/
#define FOB_LOCK 12
#define FOB_UNLOCK 13
#define FOB_START 14
#define FOB_ALARM 15
#define FOB_TRUNK 16

/************************* WiFi Access Point *********************************/

  // Optionally configure a GPRS APN, username, and password.
  // You might need to do this to access your network's GPRS/data
  // network.  Contact your provider for the exact APN, username,
  // and password values.  Username and password are optional and
  // can be removed, but APN is required.
#define FONA_APN       ""
#define FONA_USERNAME  ""
#define FONA_PASSWORD  ""

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "jamesk"
#define AIO_KEY         "...your AIO key..."

/************ Global State (you don't need to change this!) ******************/

// Setup the FONA MQTT class by passing in the FONA class and MQTT server and login details.
Adafruit_MQTT_FONA mqtt(&fona, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// You don't need to change anything below this line!
#define halt(s) { Serial.println(F( s )); while(1);  }

// FONAconnect is a helper function that sets up the FONA and connects to
// the GPRS network. See the fonahelper.cpp tab above for the source!
boolean FONAconnect(const __FlashStringHelper *apn, const __FlashStringHelper *username, const __FlashStringHelper *password);

/****************************** Feeds ***************************************/

// Setup a feed called 'photocell' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
const char STATUS_FEED[] PROGMEM = AIO_USERNAME "/f/starter.status";
Adafruit_MQTT_Publish status = Adafruit_MQTT_Publish(&mqtt, STATUS_FEED, MQTT_QOS_1);

// Setup a feeds for subscribing to changes.
Adafruit_MQTT_Subscribe start = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/f/starter.start");
Adafruit_MQTT_Subscribe lock = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/f/starter.lock");
Adafruit_MQTT_Subscribe unlock = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/f/starter.unlock");
Adafruit_MQTT_Subscribe alarm = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/f/starter.alarm");
Adafruit_MQTT_Subscribe trunk = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/f/starter.trunk");
Adafruit_MQTT_Subscribe forceReset = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/f/starter.reset");
/*************************** Sketch Code ************************************/

// How many transmission failures in a row we're willing to be ok with before reset
uint8_t txfailures = 0;
#define MAXTXFAILURES 3
unsigned long previousPingMillis = 0; // ping timer
unsigned long previousStatusMillis = 0; // status timer
const long PING_INTERVAL = 60000; // every minute
const long STATUS_INTERVAL = 600000; // every 10 minutes

void setup() {
  while (!Serial);

  // Watchdog is optional!
  Watchdog.enable(8000);
  
  Serial.begin(115200);

  Serial.println(F("Adafruit FONA MQTT demo"));


  // disable unused pins to save some power
  disableUnusedPins();

  // setup the pins connected to the keyfob
  initFob();

  Watchdog.reset();
  // pin config for fona
  // FONA_RST handled by Fona library
  pinMode(FONA_PS, INPUT);
  digitalWrite(FONA_KEY, HIGH);
  pinMode(FONA_KEY, OUTPUT);
  pinMode(FONA_RI, INPUT);
  // FONA_TX -> Hardware Serial RX
  // FONA_RX -> Hardware Serial TX
  pinMode(FONA_NS, INPUT);

  // power on the Fona if it is off
  if (!digitalRead(FONA_PS)) {
    DEBUG_PRINTLN(F("Powering on Fona"));
    digitalWrite(FONA_KEY, LOW);
    delay(2000);
    digitalWrite(FONA_KEY, HIGH);
  }
  
  // setup the subscription
  Watchdog.reset();
  mqtt.subscribe(&start);
  mqtt.subscribe(&lock);
  mqtt.subscribe(&unlock);
  mqtt.subscribe(&alarm);
  mqtt.subscribe(&trunk);
  mqtt.subscribe(&forceReset);
  
  Watchdog.reset();
  delay(5000);  // wait a few seconds to stabilize connection
  Watchdog.reset();
  
  // Initialise the FONA module
  while (! FONAconnect(F(FONA_APN), F(FONA_USERNAME), F(FONA_PASSWORD))) {
    Serial.println("Retrying FONA");
  }

  Serial.println(F("Connected to Cellular!"));

  Watchdog.reset();
  delay(5000);  // wait a few seconds to stabilize connection
  Watchdog.reset();

  mqtt.will(STATUS_FEED, "Down");
  Watchdog.reset();
}

void loop() {
  unsigned long currentMillis;
  // Make sure to reset watchdog every loop iteration!
  Watchdog.reset();

  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  Watchdog.reset();
 
  // this is our 'wait for incoming subscription packets' busy subloop
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &start) {
      startCar();
      Watchdog.reset();
      publishResponse("Starting Car...");
    }
    if (subscription == &lock) {
      lockCar();
      Watchdog.reset();
      publishResponse("Locking Car...");
    }
    if (subscription == &unlock) {
      unlockCar();
      Watchdog.reset();
      publishResponse("Unlocking Car...");
    }
    if (subscription == &alarm) {
      alarmCar();
      Watchdog.reset();
      publishResponse("Triggering Car Alarm...");
    }
    if (subscription == &trunk) {
      openTrunk();
      Watchdog.reset();
      publishResponse("Opening Trunk...");
    }
    if (subscription == &forceReset) {
      // cause Watchdog to reset us
      delay(10000);
    }
  }

  // Send an up status every 10 minutes or so to let us know if we should expect things to work.
  Watchdog.reset();
  currentMillis = millis();
  if(currentMillis - previousStatusMillis >= STATUS_INTERVAL) {
    previousStatusMillis = currentMillis;
    publishResponse("Up");
  }
  
  Watchdog.reset();
  //ping the server to keep the mqtt connection alive, only needed if we're not publishing
  //often enough (within the KEEPALIVE setting)
  currentMillis = millis();
  if(currentMillis - previousPingMillis >= PING_INTERVAL) {
    previousPingMillis = currentMillis;
    if(! mqtt.ping()) {
      Serial.println(F("MQTT Ping failed."));
    }
  }
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
  }
  Serial.println("MQTT Connected!");
}

void publishResponse(const char *payload) {
    // Now we can publish results!
  if (! status.publish(payload)) {
    Serial.println(F("Failed to publish"));
    txfailures++;
  } else {
    Serial.println(F("OK!"));
    txfailures = 0;
  }
}


void initFob() {
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
  pressFobButton(FOB_TRUNK, 200, 200);
  //check some pin
  return true;
}

void disableUnusedPins() {
  // pin config for unused pins
  // unused pins array - used to save some power
  int unused[] = {0, 1, 2, 10, 11, 17, 18, 19, 20, 21, 22, 23};
  for(unsigned int i = 0; i < sizeof(unused)/sizeof(int); i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }
}

