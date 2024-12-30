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

struct ReceiptStep{
  int valve;
  int amount;
  bool carbonated;
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

  bool hasError;
  String error = "";

  ReceiptStep steps[a.size()];
  int index = 0;
  for ( JsonObject o : a )
  {
    ReceiptStep step;
    for ( JsonPair p : o)
    {
        String key = p.key().c_str();
        if (key == "valve") {
            step.valve = p.value().as<int>();
        } else if (key == "amount"){
            step.amount = p.value().as<int>();
        } else if (key == "carbonated") {
            step.carbonated = p.value().as<bool>();
        } else {
            hasError = true;
            error = "Unknown field: " + key; 
        } 

        Serial.print( p.key().c_str() );
        Serial.print( " = " );
        Serial.println( p.value().as<int>() );
    }
    // Valve setup is invalid
    if ((step.valve <= 0) || (step.valve > 8)) {
      hasError = true;
      error = "Invalid valve selection detected!";
      break;
    }

    // TODO: Add up total amount of fluid and check for max capacity (e.g 500ml)

    if (step.amount <= 0 || step.amount > 1000) {
      hasError = true;
      error = "Invalid amount detected! Valid values are between 1 and 1000 ml";
      break;
    }

    steps[index] = step;

    index++;
  }

  if (hasError == true) {
    server.send(400, "application/json", "{\"error\": \"" + error + "\"}");
    return;
  } else {
    server.send(200, "application/json", "{}");
  }

  // Set idle LED
  shouldBlink = true;

  delay(10000);

  shouldBlink = true;
  
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