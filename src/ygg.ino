#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

ADC_MODE(ADC_VCC);

const char* ssid = "ssid";
const char* password = "pass";
const char* ota_password = "pass";

#define SERVER      "192.168.1.254"
#define SERVERPORT  1883  

int ledPin = 13; // GPIO13
int ledR = 16; // 
int ledG = 14; //
int ledB = 12; // 

WiFiServer server(80);

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, SERVER, SERVERPORT);
Adafruit_MQTT_Publish state = Adafruit_MQTT_Publish(&mqtt, "/feeds/socket/state");
Adafruit_MQTT_Publish volt = Adafruit_MQTT_Publish(&mqtt, "/feeds/socket/voltage");
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, "/feeds/socket/onoff");

void MQTT_connect();

void setup() {
  Serial.begin(115200);
  delay(10);
 
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  pinMode(12, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(16, OUTPUT);
  
  digitalWrite(ledR, HIGH);
  
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
    
  uint8_t timeout = 20;
  while ((WiFi.status() != WL_CONNECTED) && timeout--) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("connected!");
    Serial.print("STA IP address "); 
    Serial.println(WiFi.localIP());
    digitalWrite(ledR, LOW);
    digitalWrite(ledG, HIGH);

  } else {
    Serial.println("Configuring access point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP-TESTAP",password);
    WiFi.softAPIP();
    Serial.print("AP IP address "); 
    Serial.println(WiFi.softAPIP());
  digitalWrite(ledR, LOW);
  digitalWrite(ledB, HIGH);
  }
 
  // Start the server
  server.begin();
  Serial.println("Server started");
 
  // Print the IP address
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");

  ArduinoOTA.setPassword(ota_password);
  // Init ota update
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  mqtt.subscribe(&onoffbutton);
}
 
void loop() {
  ArduinoOTA.handle();
  MQTT_connect();

  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &onoffbutton) {
      Serial.print(F("Got: "));
      Serial.println((char *)onoffbutton.lastread);
      
      if (strcmp((char *)onoffbutton.lastread, "OFF") == 0) {
        digitalWrite(ledPin, LOW);
        state.publish("OFF");
      }
      if (strcmp((char *)onoffbutton.lastread, "ON") == 0) {
        digitalWrite(ledPin, HIGH);
        state.publish("ON");
      }      
    }
  }
  volt.publish(ESP.getVcc());

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
 
  // Wait until the client sends some data
  Serial.println("new client");
  while(!client.available()){
    delay(1);
  }
 
  // Read the first line of the request
  String request = client.readStringUntil('\r');
  Serial.println(request);
  client.flush();
 
  // Match the request
 
  int value = LOW;
  if (request.indexOf("/LED=ON") != -1)  {
    digitalWrite(ledPin, HIGH);
    value = HIGH;
  }
  if (request.indexOf("/LED=OFF") != -1)  {
    digitalWrite(ledPin, LOW);
    value = LOW;
  }
  if (request.indexOf("/reset") != -1)  {
    digitalWrite(ledPin, LOW);
    digitalWrite(ledR, LOW);
    digitalWrite(ledG, LOW);
    digitalWrite(ledB, LOW);
    ESP.restart();
  }
 
  // Return the response
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println(""); //  do not forget this one
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
 
  client.print("Led pin is now: ");
 
  if(value == HIGH) {
    client.print("On");
  } else {
    client.print("Off");
  }
  client.println("<br><br>");
  client.println("<a href=\"/LED=ON\"\"><button>Turn On </button></a>");
  client.println("<a href=\"/LED=OFF\"\"><button>Turn Off </button></a><br />");  
  client.println("</html>");
 
  delay(1);
  Serial.println("Client disonnected");
  Serial.println("");
 
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

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}
 
