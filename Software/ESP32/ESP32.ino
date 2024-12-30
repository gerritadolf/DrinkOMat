#include <Wire.h>
#include <SH1106.h>

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <vector>
#include <ArduinoJson.h>

// MAX Amount for each drink in ml
#define MAX_AMOUNT 500

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

// Define the amount of fluid extruded by the pumps in 60 seconds in ml
#define PERISTALTIC_PUMP_FPM 500
#define MEMBRANE_PUMP_FPM 200 // TODO: CHANGE FOR REAL VALUE

// Time that fluid need for the pump and pipes to reach the extruder in ms
#define MEMBRANE_FLUID_DELAY 1000 // TODO: MEASURE REAL VALUES
#define PERISTALTIC_FLUID_DELAY 500 // TODO: MEASURE REAL VALUES

struct ReceiptStep{
  int valve;
  int amount;
  bool carbonated;
};
std::vector<ReceiptStep> steps;

int valves[] = {
  V1,
  V1,
  V2,
  V3,
  V4,
  V5,
  V6,
  V7,
  V8 // carbonated valve
};

bool isBlinking = false;
bool isBusy = false; // indicated if device is currently busy (i.e making a drink)

WebServer server(80);
StaticJsonDocument<250> jsonDocument;
char buffer[250];
const char *SSID = "<SSID>";
const char *PWD = "<PWD>";


SH1106 display(0x3c, CLK, SDA); 

bool buttonPressed = false;
void IRAM_ATTR isr2() {
  if (!isBusy) {
      buttonPressed = true;
  }
    
}

void handlePost() {
  if (isBusy) {
    server.send(400, "application/json", "{\"error\": \"The device is making a drink and does not accept new receipts at this time.\"}");
    return;
  }

  if (server.hasArg("plain") == false) {
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);

  JsonArray a = jsonDocument[ "receipt" ].as<JsonArray>();

  bool hasError = false;
  String error = "";

  steps.clear();
  steps.resize(a.size());
  int index = 0;
  int totalAmount = 0;
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

    if (step.amount <= 0 || step.amount > MAX_AMOUNT) {
      hasError = true;
      error = "Invalid amount detected! Valid values are between 1 and " + String(MAX_AMOUNT) + " ml";
      break;
    }

    totalAmount += step.amount;

    // When TotalAmount is reached, abort
    if (totalAmount > MAX_AMOUNT) {
        hasError = true;
        error = "Invalid receipt! Max drink size is " + String(MAX_AMOUNT) + " ml (this receipt has " + String(totalAmount) + " ml).";
        break;
    }

    steps[index] = step;

    index++;
  }

  if (hasError == true) {
    server.send(400, "application/json", "{\"error\": \"" + error + "\"}");
    steps.clear();
    return;
  } else {
    server.send(200, "application/json", "{}");
  }

  digitalWrite(BUTTON_LED, true);


}

void setup_routing() {     
 // server.on("/status", getStatus);          
  server.on("/drink", HTTP_POST, handlePost);    
          
  server.begin();    
}

void blinky(void * parameter) {
  for(;;) {
    if (isBusy) {
        isBlinking = !isBlinking;
        digitalWrite(BUTTON_LED, isBlinking);
    } else {
       isBlinking = false;
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void prepareDrink(void * parameter) { 
  for(;;) {
  if (buttonPressed == true) {
    buttonPressed = false;

    if (steps.size() == 0) {
      digitalWrite(BUTTON_LED, true);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      digitalWrite(BUTTON_LED, false);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      digitalWrite(BUTTON_LED, true);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      digitalWrite(BUTTON_LED, false);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      digitalWrite(BUTTON_LED, true);
      vTaskDelay(200 / portTICK_PERIOD_MS);
      digitalWrite(BUTTON_LED, false);
      continue;
    }
    // Now execute every step!
    // Set idle LED
    isBusy = true;
    for(ReceiptStep step: steps) {
        // Is this step carbonated?
        int pump;
        int duration;
        if (step.carbonated == true) {
            // Open carbonated valve, so the way of the fluid does not redirect via peristaltic pump
            digitalWrite(V8, HIGH);
            pump = MEMBRANE_PUMP;
            
            duration = MEMBRANE_FLUID_DELAY + ((60000 * step.amount) / MEMBRANE_PUMP_FPM);
        } else {
          pump = PERISTALTIC_PUMP;

          duration = PERISTALTIC_FLUID_DELAY + ((60000 * step.amount) / PERISTALTIC_PUMP_FPM);  
          digitalWrite(V8, LOW);
        }
        digitalWrite(valves[step.valve], HIGH); // Set valve for drink
         vTaskDelay(1000 / portTICK_PERIOD_MS);

        // Enable pump
        digitalWrite(pump, HIGH);
         vTaskDelay(duration / portTICK_PERIOD_MS);
        digitalWrite(pump, LOW);

        digitalWrite(V8, LOW);
        digitalWrite(valves[step.valve], LOW);
    }
    isBusy = false;
    digitalWrite(BUTTON_LED, true);
  }

  vTaskDelay(500 / portTICK_PERIOD_MS);
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

    xTaskCreate(     
  prepareDrink,      
  "Prepare task routine",      
  1000,      
  NULL,      
  1,     
  NULL     
  );     
}

void setup_debug() {
  for(int valve : valves) {
      digitalWrite(valve, HIGH);
  }
  digitalWrite(MEMBRANE_PUMP, HIGH);
  digitalWrite(PERISTALTIC_PUMP, HIGH);
  digitalWrite(BUTTON_LED, LOW);

  delay(1000);


  for(int valve : valves) {
      digitalWrite(valve, LOW);
  }
  digitalWrite(MEMBRANE_PUMP, LOW);
  digitalWrite(PERISTALTIC_PUMP, LOW);
}

void setup() {
  for(int valve : valves) {
      pinMode(valve, OUTPUT);
  }
  pinMode(MEMBRANE_PUMP, OUTPUT);
  pinMode(PERISTALTIC_PUMP, OUTPUT);

	Serial.begin(115200);
	pinMode(BUTTON, INPUT_PULLUP);
  pinMode(33, OUTPUT);
	attachInterrupt(BUTTON, isr2, FALLING);

  setup_debug();

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