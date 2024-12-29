#include <Wire.h>
#include <SH1106.h>

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// GPIO button config
#define BUTTON 25
#define BUTTON_LED 33

// GPIO I2C for OLED
#define CLK 21
#define SDA 22

// GPIO for Valves
#define V1 2
#define V2 0
#define V3 4
#define V4 16
#define V5 17
#define V6 5
#define V7 18
#define V8 19

// GPIO for pumps
#define MEMBRANE_PUMP 27
#define PERISTALTIC_PUMP 26 

struct Button {
	const uint8_t PIN;
	uint32_t numberKeyPresses;
	bool pressed;
};

Button button1 = {BUTTON, 0, false};
bool isBlinking = false;
bool shouldBlink = false;

WebServer server(80);
StaticJsonDocument<250> jsonDocument;
char buffer[250];
const char *SSID = "<SSID>";
const char *PWD = "<PWD>";


SH1106 display(0x3c, CLK, SDA); 

void IRAM_ATTR isr() {
	  shouldBlink = true;
    delay(10000);
    shouldBlink = false;
}

void handlePost() {
  if (server.hasArg("plain") == false) {
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);

  JsonArray a = jsonDocument[ "receipt" ].as<JsonArray>();

  for ( JsonObject o : a )
  {
    for ( JsonPair p : o)
    {
        Serial.print( p.key().c_str() );
        Serial.print( " = " );
        Serial.println( p.value().as<int>() );
    }
  }

  server.send(200, "application/json", "{}");

  shouldBlink = true;
  delay(10000);
  shouldBlink = false;
  
}

void setup_routing() {     
 // server.on("/status", getStatus);          
  server.on("/drink", HTTP_POST, handlePost);    
          
  server.begin();    
}

void blinky(void * parameter) {
  for(;;) {
    if (shouldBlink) {
        isBlinking = !isBlinking;
        digitalWrite(BUTTON_LED, isBlinking);
    } else {
       digitalWrite(BUTTON_LED, false);
       isBlinking = false;
    }
    delay(500);
  }
}

void setup_task() {    
  xTaskCreate(     
  blinky,      
  "Set the blinky led of button",      
  1000,      
  NULL,      
  1,     
  NULL     
  );     
}

void setup() {
	Serial.begin(115200);
	pinMode(button1.PIN, INPUT_PULLUP);
  pinMode(33, OUTPUT);
	attachInterrupt(button1.PIN, isr, FALLING);

  digitalWrite(BUTTON_LED, true);
  delay(1000);
  digitalWrite(BUTTON_LED, false);


  display.init();
  display.clear();
  display.println("Hello world!");
  display.display();


    WiFi.begin(SSID, PWD);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
    }
 
  Serial.print("Connected! IP Address: ");
  Serial.println(WiFi.localIP());  
  setup_routing();     
  setup_task();
}

void loop() {
  server.handleClient();
  delay(500);
}