#include <Wire.h>
#include <SH1106.h>

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
uint32_t amountOfBlinks = 10;
bool isBlinkPinActive = false;
uint32_t counter = 0;
bool shouldBlink = false;


SH1106 display(0x3c, CLK, SDA); 

void IRAM_ATTR isr() {
	button1.numberKeyPresses++;
	button1.pressed = true;
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
}

void loop() {
	if (button1.pressed) {
		Serial.printf("Button has been pressed %u times\n", button1.numberKeyPresses);
		button1.pressed = false;
    shouldBlink = true;

    digitalWrite(BUTTON_LED, HIGH);
    isBlinkPinActive = true;
	}

  if (counter == amountOfBlinks * 2) {
    shouldBlink = false;
     digitalWrite(BUTTON_LED, false);
     isBlinkPinActive = false;
     counter = 0;
  }
  if (shouldBlink) {
      if (counter % 2 == 0) {
          isBlinkPinActive = !isBlinkPinActive;
          digitalWrite(BUTTON_LED, isBlinkPinActive);
      }
      counter++;
  }

  delay(500);
}