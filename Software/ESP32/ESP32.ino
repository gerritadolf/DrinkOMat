
#include <SH1106.h>

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// BLE
#include <BLEDevice.h>
#include <BLEServer.h>
 #include <BLE2902.h>
#include <Preferences.h>

// BLE variables
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define MESSAGE_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define DEVINFO_UUID (uint16_t)0x180a
#define DEVINFO_MANUFACTURER_UUID (uint16_t)0x2a29
#define DEVINFO_NAME_UUID (uint16_t)0x2a24
#define DEVINFO_SERIAL_UUID (uint16_t)0x2a25

#define DEVICE_MANUFACTURER "GJM Systems"
#define DEVICE_NAME "DoM v1"


// MAX Amount for each drink in ml
#define MAX_AMOUNT 500

// GPIO button config
#define BUTTON 25
#define BUTTON_LED 33

// GPIO I2C for OLED
#define CLK 22
#define SDA 21

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




// OLED MESSAGES
#define WELCOME "Moin."
#define NO_RECEIPT "Kein Rezept ausgewählt"
#define INVALID_RECEIPT "Ungültiges Rezept"
#define PREPARING "Zubereiten..."
#define READY "Bereit"
#define PRESS_START "Zum Start Knopf drücken"
#define BON_APPETIT "Prost!"

bool hasStateChanged = false;
int displayState = 0;

String receiptName;
struct ReceiptStep{
  int valve;
  int amount;
  bool carbonated;
};
std::vector<ReceiptStep> steps;

int valves[] = {
  V1, // CHANGE TO 2
  V1, // CHANGE TO 2
  V2, // CHANGE TO 7
  V3, // CHANGE TO 4
  V4, // CHANGE TO 3
  V5, // NOT BELEGT
  V6, // NOT BELEGT
  V7, // NOT BELEGT
  V8 // REMOVE RESERVATION
};

bool isBlinking = false;
bool isBusy = false; // indicated if device is currently busy (i.e making a drink)
TaskHandle_t taskHandle = NULL;

WebServer server(80);
SH1106 display(0x3c, CLK, SDA); 

bool buttonPressed = false;
void IRAM_ATTR isr1() {
  if (!isBusy) {
        buttonPressed = true;
  } 
}

void handlePost() {
  StaticJsonDocument<250> jsonDocument;
  if (isBusy) {
    server.send(400, "application/json", "{\"error\": \"The device is making a drink and does not accept new receipts at this time.\"}");
    return;
  }

  if (server.hasArg("plain") == false) {
  }
  String body = server.arg("plain");
  deserializeJson(jsonDocument, body);

  JsonArray a = jsonDocument[ "receipt" ].as<JsonArray>();
  receiptName = jsonDocument[ "name" ].as<String>();
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
    displayState = 2;
    hasStateChanged = true;
    return;
  } else {
    server.send(200, "application/json", "{}");
  }

  digitalWrite(BUTTON_LED, true);
  displayState = 4;
  hasStateChanged = true;

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

const unsigned long debounceDelay = 50; // Entprellzeit in Millisekunden
bool lastButtonState = HIGH; // Letzter Zustand des Tasters (HIGH - nicht gedrückt)
unsigned long lastDebounceTime = 0; // Zeit des letzten Wechsels
void buttonPress(void * parameter) {
  for(;;) {
    int reading = digitalRead(BUTTON); // Lese den aktuellen Zustand des Tasters

    if (reading != lastButtonState) {
        lastDebounceTime = millis(); // Aktualisiere die Zeit des letzten Wechsels
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading == LOW && !buttonPressed) {
            buttonPressed = true; // Setze das Flag für gedrückten Taster
            Serial.println("Taster gedrückt!"); // Aktion durchführen
            vTaskDelay(550 / portTICK_PERIOD_MS);
        }
        else {
          buttonPressed = false;
        }
    }

    lastButtonState = reading; // Aktuellen Zustand speichern
  }
}

void prepareDrink(void * parameter) { 
  for(;;) {
  if (buttonPressed == true) {
    buttonPressed = false;

    if (steps.size() == 0) {
        displayState = 1;
        hasStateChanged = true;
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

    displayState = 3;
    hasStateChanged = true;
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

    displayState = 5;
    hasStateChanged = true;

    vTaskDelay(4000 / portTICK_PERIOD_MS);

    displayState = 4;
    hasStateChanged = true;
  }

  vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// BLE 
BLECharacteristic *characteristicMessage;
Preferences preferences;   


void connectToWiFi(const char* ssid, const char* password) {
      WiFi.begin(ssid, password);
      Serial.print(F("Connecting to WiFi"));
      while (WiFi.status() != WL_CONNECTED && WiFi.status() != WL_CONNECT_FAILED && WiFi.status() != WL_NO_SSID_AVAIL) {
          delay(500);
          Serial.println(WiFi.status());
      }

      if (WiFi.status() == WL_CONNECTED) {
        displayState = 7;
        hasStateChanged = true;

        // Save SSID
        preferences.begin("WiFi", false);
        preferences.putString("SSID", ssid);
        preferences.putString("Password", password);
        preferences.end();

        // Write the IP address to the BLE characteristic with format [IPAddress]IP_ADDRESS
        String ip = WiFi.localIP().toString();
        String message = "[IPAddress]" + ip;
        characteristicMessage->setValue(message.c_str());

        setup_routing();
      } else if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_NO_SSID_AVAIL) {
        String message = "[Error]InvalidCredentials";
        characteristicMessage->setValue(message.c_str());
        displayState = 6;
        hasStateChanged = true;
      } else {
        String message = "[Error]UnkownError";
        characteristicMessage->setValue(message.c_str());
        displayState = 6;
        hasStateChanged = true;
      }

      // Send data
      characteristicMessage->notify();

       delay(4000);

      displayState = 1;
      hasStateChanged = true;
}

class MyServerCallbacks : public BLEServerCallbacks
{
    void onDisconnect(BLEServer *server)
    {
        server->getAdvertising()->start();
    }
};

class MessageCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *characteristic)
    {
      // Serial.println("onWrite Vlaue");
        std::string data = characteristic->getValue();

        if (data.find("[ssid]") != 0) {
            // Serial.println("Invalid data format");
            return;
        }

        // Extract SSID and Password
        int ssidStart = data.find("[ssid]") + 6;
        int ssidEnd = data.find("[PWD]");
        int pwdStart = ssidEnd + 5;
        String ssid = data.substr(ssidStart, ssidEnd - ssidStart).c_str();
        String password = data.substr(pwdStart).c_str();

        // Connect to WiFi
        connectToWiFi(ssid.c_str(), password.c_str());
    }

    void onRead(BLECharacteristic *characteristic)
    {
        std::string data = characteristic->getValue();
        // Serial.println(data.c_str());
        // Serial.println("onRead Value");
    }
};

void setupBLE() {
   // Serial.println("Setup BLE Server");
      // Setup BLE Server
    BLEDevice::init(DEVICE_NAME);
    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new MyServerCallbacks());

    // Register message service that can receive messages and reply with a static message.
    BLEService *service = server->createService(SERVICE_UUID);
    characteristicMessage = service->createCharacteristic(MESSAGE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_WRITE);
    characteristicMessage->setCallbacks(new MessageCallbacks());
    characteristicMessage->addDescriptor(new BLE2902());
    service->start();

    // Register device info service, that contains the device's UUID, manufacturer and name.
    service = server->createService(DEVINFO_UUID);
    BLECharacteristic *characteristic = service->createCharacteristic(DEVINFO_MANUFACTURER_UUID, BLECharacteristic::PROPERTY_READ);
    characteristic->setValue(DEVICE_MANUFACTURER);
    characteristic = service->createCharacteristic(DEVINFO_NAME_UUID, BLECharacteristic::PROPERTY_READ);
    characteristic->setValue(DEVICE_NAME);
    characteristic = service->createCharacteristic(DEVINFO_SERIAL_UUID, BLECharacteristic::PROPERTY_READ);
    String chipId = String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
    characteristic->setValue(chipId.c_str());
    service->start();

    // Advertise services
    BLEAdvertising *advertisement = server->getAdvertising();
    BLEAdvertisementData adv;
    adv.setName(DEVICE_NAME);
    adv.setCompleteServices(BLEUUID(SERVICE_UUID));
    advertisement->setAdvertisementData(adv);
    advertisement->start();
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
  &taskHandle     
  );   

      xTaskCreate(     
  buttonPress,      
  "Check for button press",      
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
  // digitalWrite(V1, LOW); // GPIO2
  // digitalWrite(V2, HIGH); // GPIO0

  digitalWrite(MEMBRANE_PUMP, LOW);
  digitalWrite(PERISTALTIC_PUMP, LOW);

  for(int valve : valves) {
      pinMode(valve, OUTPUT);
  }
  pinMode(MEMBRANE_PUMP, OUTPUT);
  pinMode(PERISTALTIC_PUMP, OUTPUT);

	Serial.begin(115200);
	pinMode(BUTTON, INPUT_PULLUP);
  pinMode(BUTTON_LED, OUTPUT);

  display.init();
  display.flipScreenVertically();
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(64, 32, WELCOME);
  display.display();

  displayState = 1;

  preferences.begin("WiFi", false);
  String savedSSID = preferences.getString("SSID");

  setupBLE();
  if (savedSSID.length() > 0) {
    String wifiPw = preferences.getString("Password");
    connectToWiFi(savedSSID.c_str(), wifiPw.c_str());
  } else {
      displayState = 6;
  }
  preferences.end();  
 
       
  setup_task();

  
  hasStateChanged = true;
}

void loop() {
  if (hasStateChanged) {
    hasStateChanged = false;
    display.clear();

    switch (displayState) {
      case 0:
        display.drawString(64, 32, WELCOME);
        break;
      case 1:
        display.drawString(64, 32, NO_RECEIPT);
        break;
      case 2:
        display.drawString(64, 32, INVALID_RECEIPT);
        break;
      case 3:
        if(receiptName.length() > 0) {
          display.drawString(64, 16, receiptName);
          display.drawString(64, 48, PREPARING);
        } else {
          display.drawString(64, 32, PREPARING);
        }
        break;
      case 4:
        if(receiptName.length() > 0) {
          display.drawString(64, 16, receiptName);
        } else {
          display.drawString(64, 16, READY);
        }
         
         display.drawString(64, 48, PRESS_START);
        break;
      case 5:
         display.drawString(64, 32, BON_APPETIT);
         break;
      case 6:
          display.drawString(64, 16, F("Setup"));
          display.drawString(64, 48, F("Einrichten per App"));
          break;
      case 7:
          display.drawString(64, 16, F("Verbunden"));
          display.drawString(64, 48,  WiFi.localIP().toString());
        break;
    }
    display.display();
  }

  server.handleClient();
  delay(500);
}