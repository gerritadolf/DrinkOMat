int VENTIL = 25;
int PUMPE = 26;

void setup() {
  // put your setup code here, to run once:
  pinMode(VENTIL, OUTPUT);
  pinMode(PUMPE, OUTPUT);


  digitalWrite(VENTIL, LOW);
  pinMode(PUMPE, LOW;);
}

bool istAn = false;

void loop() {
  // put your main code here, to run repeatedly:
  if (istAn) {
    digitalWrite(PUMPE, false);
    delay(250);
    digitalWrite(VENTIL, false);
  } else {
      digitalWrite(VENTIL, true);
      delay(250);
      digitalWrite(PUMPE, true);
  }
  
  istAn = !istAn;
  delay(5000);

}
