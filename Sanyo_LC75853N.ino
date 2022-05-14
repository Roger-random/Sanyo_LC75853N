/* Trying to control a Sanyo LC75853N LCD driver with an Arduino Nano */

// Arduino digital pins to use for communication
int pinDataIn = 2;
int pinDataOut = 3;
int pinClock = 4;
int pinEnable = 5;

// Logic analyzer trace of communication indicates values are held
// for 2.7us. However, according to documentation Arduino can only
// guarantee down to 3us.
// https://www.arduino.cc/reference/en/language/functions/time/delaymicroseconds/
int usHold = 3;

// Control signals
uint8_t control[][7] = {
  {0x00, 0x00, 0x00, 0xD0, 0xDD, 0x02, 0x00},
  {0x70, 0x55, 0x00, 0x00, 0x00, 0x00, 0x80},
  {0x06, 0x04, 0x0B, 0x00, 0x00, 0x00, 0x40}  
};

void hold()
{
  delayMicroseconds(usHold);
}

// Couldn't use Arduino SPI library because it controls enable pin with SPI semantics,
// which is different from CCB.
// Couldn't use Arduino shiftOut() because it uses clock signal different from CCB.
void writeByte(uint8_t dataByte)
{
  uint8_t shift = dataByte;

  // Send 8 bits, least significant bit first.
  for(uint8_t i = 0; i < 8; i++)
  {
    digitalWrite(pinClock, LOW);
    hold();
    if( shift & 0x01 ){
      digitalWrite(pinDataOut, HIGH);
    } else {
      digitalWrite(pinDataOut, LOW);
    }
    shift = shift >> 1;
    hold();
    digitalWrite(pinClock, HIGH);
    hold();
  }
    
  return;
}

void setup() {
  pinMode(pinDataOut, OUTPUT);
  pinMode(pinDataIn, INPUT);
  pinMode(pinClock, OUTPUT);
  pinMode(pinEnable, OUTPUT);

  // Clock is active low, so initialize to high.
  digitalWrite(pinClock, HIGH);

  // Enable is active high, so initialize to low.
  digitalWrite(pinEnable, LOW);

  // DataOut initial value shouldn't matter.
  // DataIn is input and not our business to initialize.
}

void loop() {
  for(int message = 0; message < 3; message++)
  {
    hold();
    digitalWrite(pinEnable, LOW);
    writeByte(0x42); // address
    digitalWrite(pinEnable, HIGH);
    for (int i = 0; i < 7; i++)
    {
      writeByte(control[message][i]);
    }
    digitalWrite(pinEnable, LOW);
    hold();
  }
  
  delay(50);
}
