
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <iot.h>
#include <PubSubClient.h>
#include <stdlib.h>
#include <math.h>

#define MIC_PIN 2
#define LIGHT_PIN 5
#define DHT_PIN 16
#define PIR_PIN 14
#define WIFI_CONNECTED_LED 13
#define MQTT_CONNECTED_LED 12

#define DHTTYPE DHT22

DHT dht(DHT_PIN, DHTTYPE);

long unsigned int no_motion_timeout = 10000;
long unsigned int motion_detected_time;
boolean motion_already_detected = false;

const char* ssid = "GranumNet";
const char* password = "";

const char* mqtt_server = "192.168.2.154";
const uint16_t mqtt_port = 1883;
uint16_t message_interval = 2000;
const char* temperature_topic = "sensor/temperature";
const char* humidity_topic = "sensor/humidity";
const char* motion_topic = "sensor/motion";
const char* light_topic = "sensor/light";
const char* smoke_alarm_topic = "sensor/smoke_alarm";

long lastMsg = 0;
char msg[5];
int value = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  digitalWrite(WIFI_CONNECTED_LED, HIGH);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  if (WiFi.status() != WL_CONNECTED)
  {
    digitalWrite(MQTT_CONNECTED_LED, LOW);
    setup_wifi();
  }
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      digitalWrite(MQTT_CONNECTED_LED, HIGH);
      client.subscribe("inTopic");
    } else {
      digitalWrite(MQTT_CONNECTED_LED, LOW);
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void process_dht_sensor()
{
  char val[5];
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  dtostrf(t,4,2,val);
  client.publish(temperature_topic, val);
  dtostrf(h,4,2,val); 
  client.publish(humidity_topic, val);
  
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println(" *C ");
}

void process_motion_sensor()
{
  if(digitalRead(PIR_PIN) == HIGH)
  {
    motion_detected_time = millis();
//    Serial.println(motion_detected_time);
    if( !motion_already_detected )
    {
      motion_already_detected = true;
      client.publish(motion_topic, "true");
      Serial.println("Motion detected");
    }
  }
  else
  {
    if( ((millis() -  motion_detected_time) > no_motion_timeout) && motion_already_detected )
    {
      client.publish(motion_topic, "false");
      Serial.println("no motion timeout expired");
      motion_already_detected = false;
    }
  }
}

void setup() {
  pinMode(WIFI_CONNECTED_LED, OUTPUT);
  pinMode(MQTT_CONNECTED_LED, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(DHT_PIN, INPUT);

  digitalWrite(WIFI_CONNECTED_LED, LOW);
  digitalWrite(MQTT_CONNECTED_LED, LOW);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.print( "Using libiot version: ");
  Serial.println( iot_version_str() );
  
  dht.begin();

  // calibrate the PIR motion sensor
  int calibrationTime = 30;
  digitalWrite(PIR_PIN, LOW);
  delay(calibrationTime * 1000);
}

void loop() {

  if (!client.connected())
    reconnect();

  client.loop();

  process_motion_sensor();
  
  long now = millis();
  if (now - lastMsg > message_interval)
  {    
    lastMsg = now;
    process_dht_sensor();
  }
}
