#include <Automaton.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>


#define SSID "SSID"
#define WIFI_PASSWORD "PASSWORD"
WiFiClient client;

#define MQTT_PORT 1883
#define MQTT_SERVER "SERVER URL"
#define MQTT_USER "MQTT Username"
#define MQTT_PASSWORD "MQTT Password"
#define TOPIC_ROOT "plant_water"
#define TOPIC_BUTTON "plant_water/button"
#define TOPIC_FLOAT "plant_water/float"
#define TOPIC_RELAY "plant_water/relay"
#define ID "1"
PubSubClient mqtt_client(client);

#define RELAY_PIN D2
#define FLOAT_PIN D3
#define BUTTON_PIN D4
#define MAX_WATER_TIME 5000 // don't water for more than X milliseconds

Atm_button button;
Atm_button float_switch;
Atm_timer water;
Atm_command commands;
char cmd_buffer[64];
enum {ON,OFF,RECONNECT_MQTT,RECONNECT};
const char cmd_list[] = "on off reconnect_mqtt reconnect";
bool button_state = false;
bool float_state = false;


void buttonPress(int id, int v, int up) {
  if (v) {
    button_state = true;
    mqtt_client.publish(TOPIC_BUTTON, "1");
    doWater();
  } else {
    button_state = false;
    mqtt_client.publish(TOPIC_BUTTON, "0");
  }
}

void floatPress(int id, int v, int up) {
  if (v) {
    float_state = true;
    mqtt_client.publish(TOPIC_FLOAT, "1");
  } else {
    float_state = false;
    mqtt_client.publish(TOPIC_FLOAT, "0");
  }
}


void wifiConnect() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(SSID, WIFI_PASSWORD);

    Serial.print("WiFi connecting...");
    while(WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
    }
    Serial.println(" Connected");
  }
}

void mqttConnect() {
  if (!mqtt_client.connected()) {
    Serial.print("MQTT connecting...");
    
    if (mqtt_client.connect(ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println(" Connected");

      mqtt_client.subscribe(TOPIC_ROOT);
    } else {
      Serial.print("Failed, rc=");
      Serial.println(mqtt_client.state());
    }
  }
}

void doWater() {
  Serial.println("Starting watering");
  startWater();
}

void startWater() {
  Serial.println("Watering");
  mqtt_client.publish(TOPIC_RELAY, "1");
  digitalWrite(RELAY_PIN, HIGH);
  water.start();
}

void stopWater(int idx=NULL, int v=NULL, int up=NULL) {
  mqtt_client.publish(TOPIC_RELAY, "0");
  water.stop();
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("Stopped watering");
}

// Receives messages from subscribed topics and dispatches the requested commands.
void mqttCallback(String topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // If a message is received on the topic room/lamp, you check if the message is either on or off. Turns the lamp GPIO according to the message
  if(topic == TOPIC_ROOT){
      Serial.print("Got MQTT message to power ");
      if(messageTemp == "on"){
        Serial.println("On");
        startWater();
      }
      else if(messageTemp == "off"){
        stopWater();
        Serial.println("Off");
      } else {
        Serial.print("Got unknown MQTT message ");
        Serial.print(topic);
        Serial.print("/");
        Serial.println(messageTemp);
      }
  } else {
    Serial.print("Got unknown MQTT message ");
    Serial.print(topic);
    Serial.print("/");
    Serial.println(messageTemp);
  }
  Serial.println();
}

// Interprets serial commands for debugging.
void cmdCallback(int idx, int v, int up) {
  Serial.print("Received command ");
  switch(v) {
  case ON:
    Serial.println("ON");
    startWater();
    return;
  case OFF:
    Serial.println("OFF");
    stopWater();
    return;
  case RECONNECT:
    Serial.println("RECONNECT");
    wifiConnect();
    mqttConnect();
    return;
  case RECONNECT_MQTT:
    Serial.println("RECONNECT_MQTT");
    mqttConnect();
    return;
  }
}


void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Starting");

  Serial.print("Setting pins...");
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FLOAT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println(" Done");

  Serial.print("Starting Automatons...");
  button.begin(BUTTON_PIN)
    .onPress(buttonPress)
    .onRelease(buttonPress)
    .trace(Serial);

  float_switch.begin(FLOAT_PIN)
    .onPress(floatPress)
    .onRelease(floatPress)
    .trace(Serial);

  water.begin(MAX_WATER_TIME)
    .onFinish(stopWater)
    .trace(Serial);

  commands.begin(Serial, cmd_buffer, sizeof(cmd_buffer))
    .list(cmd_list)
    .onCommand(cmdCallback);
  Serial.println(" Done");

  mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt_client.setCallback(mqttCallback);

  wifiConnect();
  mqttConnect();
}

void loop() {
  automaton.run();
  mqtt_client.loop();
}
