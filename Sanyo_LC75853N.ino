/* Trying to control a Sanyo LC75853N LCD driver with an Arduino Nano */

void setup() {
  pinMode(13, OUTPUT);
}

void loop() {
  digitalWrite(13, HIGH);
  delay(10);
  digitalWrite(13, LOW);
  delay(100);
  digitalWrite(13, HIGH);
  delay(10);
  digitalWrite(13, LOW);
  delay(1000);
}
