#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define ArrowR 2//16
#define ArrowL 16//5
//#define Red 0
#define AmberR 0//4
#define AmberL 5//2
#define TallLight 15//14
#define Red 4


// Load credentials from environment variables
const char *ssid = getenv("WIFI_SSID");
const char *passwd = getenv("WIFI_PASS");


bool enable = true;
bool alert = false;
int W77 = 0;
int LEXP = 0;
int N151 = 0;
int S151 = 0;
int NWELL = 0;
int SWELL = 0;

String activeAlert = "";

int slow = 900;
int medium = 600;
int fast = 360;
int sfast = 180;


//setup timeout clock
int mSec = 0;
int seconds = 0;
int minutes = 0;
bool minuteFLAG = false;
bool pausebusbotFLAG = false;
int busbotTOUT = 0;

int timeoutperiod = 20; //timeout in minutes to turn off light, no timeout if 0

//timer
int MCLKmsec, MCLKsec, MCLKminutes, MCLKhours;
int TMRmsec, TMRsec, TMRminutes, TMRhours;

int minutehold;







WiFiClient wlanclient;
PubSubClient mqttClient(wlanclient);

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print ("\nMessage arrived on Topic:");
  Serial.println (topic);
  //  String topicString = String(topic);
  //  Serial.println(topicString);
  Serial.println("===");



  char message[5] = {0x00};

  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  Serial.print(length);
  message[length] = 0x00;
  Serial.print (":");
  Serial.print (message);

  //int eta=atoi(message.c_str());
  //int eta = int(message);
  int eta = String(message).toInt();
  Serial.print("ETA");
  Serial.print(eta);
  Serial.print("-");

  if (strcmp(topic, "devices/busotron/enable") == 0) {
    Serial.println("ENABLE SWITCH");
    if (strcmp(message, "ON") == 0) {
      Serial.println("BOX ON");
      enable = true;
    }
    else {
      Serial.println("BOX OFF");
      enable = false;
    }
  }
  else if (strcmp(topic, "CTApredictions/alert/active") == 0) {
    Serial.println("ENABLE MECHANICAL SWITCH");
    if (strcmp(message, "ON") == 0) {
      Serial.println("MECHANICAL ON");
      alert = true;
    }
    else {
      alert = false;

    }
  }
  else if (strcmp(topic, "CTApredictions/BUS/dwtnEXP") == 0) {
    Serial.println("LEXP");
    LEXP = eta;
    Serial.println("LEXP" + LEXP);
  }
  else if (strcmp(topic, "CTApredictions/BUS/1151/77") == 0) {
    Serial.println("W77");
    W77 = eta;
    Serial.println("W77");
    Serial.println(W77);
  }
  else if (strcmp(topic, "CTApredictions/RAIL/30231") == 0) {
    Serial.println("NWELL");
    NWELL = eta;
    Serial.println("NWELL" + NWELL);
  }
  else if (strcmp(topic, "CTApredictions/RAIL/30232") == 0) {
    Serial.println("SWELL");
    SWELL = eta;
    Serial.println("SWELL" + SWELL);
  }
  else if (strcmp(topic, "CTApredictions/BUS/1074/151") == 0) {
    Serial.println("S151");
    S151 = eta;
    Serial.println("S151" + S151);
  }
  else if (strcmp(topic, "CTApredictions/BUS/1151/151") == 0) {
    Serial.println("N151");
    N151 = eta;
    Serial.println("N151" + S151);
  }



  Serial.println("END HANDLER");
}

void setup() {
  Serial.begin (115200);
  pinMode (ArrowR, OUTPUT);
  pinMode (ArrowL, OUTPUT);
  pinMode (AmberR, OUTPUT);
  pinMode (AmberL, OUTPUT);
  pinMode (TallLight, OUTPUT);
  pinMode (Red, OUTPUT);

  digitalWrite (ArrowR, LOW);
  digitalWrite (ArrowL, LOW);
  digitalWrite (AmberR, LOW);
  digitalWrite (AmberL, LOW);
  digitalWrite (TallLight, HIGH);
  digitalWrite (Red, LOW);





  WiFi.begin(ssid, passwd);

  Serial.print ("Connecting to AP");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print (".");
    delay(100);
  }
  Serial.println ("Connected to WiFi AP, Got an IP address :");
  Serial.print (WiFi.localIP());
  mqttClient.setServer(getenv("MQTT_SERVER"), atoi(getenv("MQTT_PORT")));
  mqttClient.setCallback(mqttCallback);

  if (mqttClient.connect("ESP-Client", getenv("MQTT_USER"), getenv("MQTT_PASS"))) {

    Serial.println ("Connected to MQTT Broker");
  } else {
    Serial.print("MQTT Broker connection failed");
    Serial.print (mqttClient.state());
    delay(200);
  }

  mqttClient.subscribe("devices/busotron/enable");
  mqttClient.subscribe("CTApredictions/alert/active");
  mqttClient.subscribe("CTApredictions/BUS/dtwnEXP");
  mqttClient.subscribe("CTApredictions/BUS/1151/77");
  mqttClient.subscribe("CTApredictions/BUS/1151/151");
  mqttClient.subscribe("CTApredictions/BUS/1074/151");
  mqttClient.subscribe("CTApredictions/RAIL/30231");
  mqttClient.subscribe("CTApredictions/RAIL/30232");



  digitalWrite (TallLight, HIGH);

}

void ChannelTOGGLE(int pin) {
  if (digitalRead(pin) == HIGH) {
    digitalWrite(pin, LOW);
  } else {
    digitalWrite(pin, HIGH);
  }
}



void runBusLights() {
  //run bus lights
  //turn off route lights

  digitalWrite(AmberR, LOW);
  digitalWrite(AmberL, LOW);
  digitalWrite(Red, LOW);
  //  digitalWrite(TallLight, LOW);
  digitalWrite(ArrowR, LOW);
  digitalWrite(ArrowL, LOW);

  if (MCLKmsec % 1000 == 0 && MCLKsec % 5 == 0) {
    //print status
    Serial.println("\nCURRENT ETAs");
    Serial.println(W77);
    Serial.println(LEXP);
    Serial.println(N151);
    Serial.println(S151);
    Serial.println(NWELL);
    Serial.println(SWELL);
  }

  if (MCLKsec < 10) { 
    digitalWrite(AmberL, HIGH);
    if (LEXP > 0) {
      if (LEXP < 180) {
        if (MCLKmsec % sfast == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowL);
        digitalWrite(ArrowR, LOW);
      }
      else if (LEXP < 300) {
        if (MCLKmsec % fast == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowL);
        digitalWrite(ArrowR, LOW);
      }
      else if (LEXP < 600) {
        if (MCLKmsec % medium == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowL);
        digitalWrite(ArrowR, LOW);
      }
      else if (LEXP < 900) {
        if (MCLKmsec % slow == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowL);
        digitalWrite(ArrowR, LOW);
      }
      else {
        digitalWrite(ArrowR, LOW);
        digitalWrite(ArrowL, LOW);

      }
    }
  }
  else if (MCLKsec < 20) { //West X9
    digitalWrite(AmberL, HIGH);
    if (W77 > 0) {
      if (W77 < 90) {
        if (MCLKmsec % sfast == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowR);
        digitalWrite(ArrowL, LOW);

      }
      else if (W77 < 360) {
        if (MCLKmsec % fast == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowR);
        digitalWrite(ArrowL, LOW);
      }
      else if (W77 < 1000) {
        if (MCLKmsec % medium == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowR);
        digitalWrite(ArrowL, LOW);
      }
      else if (W77 < 5000) {
        if (MCLKmsec % slow == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowR);
        digitalWrite(ArrowL, LOW);
      }
      else {
        digitalWrite(ArrowR, LOW);
        digitalWrite(ArrowL, LOW);

      }
    }
  }
  //BROWN LINE

  else if (MCLKsec < 30) { //Northound Red
    digitalWrite(Red, HIGH);
    if (SWELL > 0) {
      if (SWELL < 120) {
        if (MCLKmsec % sfast == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowL);
        digitalWrite(ArrowR, LOW);
      }
      else if (SWELL < 300) {
        if (MCLKmsec % fast == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowL);
        digitalWrite(ArrowR, LOW);
      }
      else if (SWELL < 600) {
        if (MCLKmsec % medium == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowL);
        digitalWrite(ArrowR, LOW);
      }
      else if (SWELL < 900) {
        if (MCLKmsec % slow == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowL);
        digitalWrite(ArrowR, LOW);
      }
      else {
        digitalWrite(ArrowR, LOW);
        digitalWrite(ArrowL, LOW);

      }
    }
  }
  else if (MCLKsec < 40) { //Soundbound Red
    digitalWrite(Red, HIGH);
    if (NWELL > 0) { //southbound red ling
      if (NWELL < 120) {
        if (MCLKmsec % sfast == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowR);
        digitalWrite(ArrowL, LOW);
      }
      else if (NWELL < 300) {
        if (MCLKmsec % fast == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowR);
        digitalWrite(ArrowL, LOW);
      }
      else if (NWELL < 600) {
        if (MCLKmsec % medium == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowR);
        digitalWrite(ArrowL, LOW);
      }
      else if (NWELL < 900) {
        if (MCLKmsec % slow == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowR);
        digitalWrite(ArrowL, LOW);
      }
      else {
        digitalWrite(ArrowR, LOW);
        digitalWrite(ArrowL, LOW);

      }
    }
  }
  else if (MCLKsec < 50) { //Northbound 151
    digitalWrite(AmberR, HIGH);
    if (S151 > 0) { //
      if (S151 < 180) {
        if (MCLKmsec % sfast == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowL);
        digitalWrite(ArrowR, LOW);
      }
      else if (S151 < 300) {
        if (MCLKmsec % fast == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowL);
        digitalWrite(ArrowR, LOW);
      }
      else if (S151 < 600) {
        if (MCLKmsec % medium == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowL);
        digitalWrite(ArrowR, LOW);
      }
      else if (S151 < 900) {
        if (MCLKmsec % slow == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowL);
        digitalWrite(ArrowR, LOW);
      }
      else {
        digitalWrite(ArrowR, HIGH);
        digitalWrite(ArrowL, LOW);
      }
    }
  }
  else if (MCLKsec < 60) { //Soundbound 151
    digitalWrite(AmberR, HIGH);
    if (N151 > 0) {
      if (N151 < 180) {
        if (MCLKmsec % sfast == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowR);
        digitalWrite(ArrowL, LOW);
      }
      else if (N151 < 300) {
        if (MCLKmsec % fast == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowR);
        digitalWrite(ArrowL, LOW);
      }
      else if (N151 < 600) {
        if (MCLKmsec % medium == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowR);
        digitalWrite(ArrowL, LOW);
      }
      else if (N151 < 900) {
        if (MCLKmsec % slow == 0 && MCLKmsec > 0)
          ChannelTOGGLE(ArrowR);
        digitalWrite(ArrowL, LOW);
      }
      else {
        digitalWrite(ArrowR, HIGH);
        digitalWrite(ArrowL, LOW);

      }
    }
  }
}



void ticker() {
  //  Serial.println("delay length");
  //  delay(incr);
  MCLKmsec += 10;
  TMRmsec += 10;
  //  Serial.print("\nTICK");
  //  Serial.print(TMRmsec);

  if (MCLKmsec % 1000 == 0) {
    //    MCLKmsec = 0;
    MCLKsec++;
    // Serial.println(MCLKsec);
    //    secondFLAG = false;
  }
  if (MCLKsec > 60) {
    MCLKsec = 0;
    MCLKminutes++;
    minuteFLAG = false;
  }
  if (MCLKminutes > 60) {
    MCLKminutes = 0;
    MCLKhours++;
  }

}


void loop() {
  //    digitalWrite(AmberR, LOW);
  //    digitalWrite(AmberL, LOW);
  //    digitalWrite(Red, LOW);
  //    digitalWrite(TallLight, HIGH);
  //    digitalWrite(ArrowR, HIGH);
  //    digitalWrite(ArrowL, LOW);
  //    delay(1200);
  //
  //    digitalWrite(AmberR, LOW);
  //    digitalWrite(AmberL, LOW);
  //    digitalWrite(Red, LOW);
  //    digitalWrite(TallLight, HIGH);
  //    digitalWrite(ArrowR, LOW);
  //    digitalWrite(ArrowL, HIGH);
  //    delay(200);

  //  Serial.println("A1");
  mqttClient.loop();
  //  Serial.println("A2");
  delay(10);
  //  Serial.println("A3");
  ticker();
  //  Serial.println("B1");


  if (enable) {
    runBusLights();
  }
  else {
    digitalWrite(AmberR, LOW);
    digitalWrite(AmberL, LOW);
    digitalWrite(Red, LOW);
    //    digitalWrite(TallLight, LOW);
    digitalWrite(ArrowR, LOW);
    digitalWrite(ArrowL, LOW);
  }

  if (alert) {
    digitalWrite(TallLight, HIGH);
  }
  else {
    digitalWrite(TallLight, LOW);
  }
  //  Serial.println("B2");

}
