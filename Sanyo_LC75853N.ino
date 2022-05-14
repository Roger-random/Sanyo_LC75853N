/*
 *   Interfacing with a Toyota 86120-08010 tape deck faceplate which primarily
 *   centers around the Sanyo LC75853N LCD driver.
 */
#include <Encoder.h>

#define MSG_OUT_COUNT 3
#define MSG_OUT_BYTES 7

// For best performance Encoder prefers interrupt pins.
// https://www.pjrc.com/teensy/td_libs_Encoder.html
// On an Arduino Nano that means pins 2 and 3. Adjust as needed for other hardware.
// https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/
int pinEncoderA = 2;
int pinEncoderB = 3;

// Arduino digital pins to use for Sanyo CCB communication
int pinDataIn = 4;
int pinDataOut = 5;
int pinClock = 6;
int pinEnable = 7;

// Logic analyzer trace of communication indicates values are held
// for 2.7us. However, according to documentation Arduino can only
// guarantee down to 3us.
// https://www.arduino.cc/reference/en/language/functions/time/delaymicroseconds/
int usHold = 3;
void hold()
{
  delayMicroseconds(usHold);
}

Encoder audioModeEncoder(pinEncoderA, pinEncoderB);
long audioModePosition;

// Control signals for "FM 1 CH 1 87.9"
uint8_t msgOut[MSG_OUT_COUNT][MSG_OUT_BYTES] = {
  {0x00, 0x00, 0x00, 0xD0, 0xDD, 0x02, 0x00},
  {0x70, 0x55, 0x00, 0x00, 0x00, 0x00, 0x80},
  {0x06, 0x04, 0x0B, 0x00, 0x00, 0x00, 0x40}  
};

// Control signal to activate all 126 segments
/*
uint8_t msgOut[MSG_OUT_COUNT][MSG_OUT_BYTES] = {
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x03, 0x00},
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x03, 0x80},
  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x03, 0x40}
};
*/

// Couldn't use Arduino SPI library because it controls enable pin with SPI semantics,
// which is different from CCB.
// Couldn't use Arduino shiftOut() because it uses clock signal differently from CCB.
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

  Serial.begin(115200);
  Serial.println("Toyota 86120-08010 Faceplate Test");

  audioModePosition = audioModeEncoder.read();
}

void loop() {

  long newPos = audioModeEncoder.read();
  if (newPos != audioModePosition)
  {
    Serial.print("Audio Mode knob at ");
    Serial.print(newPos);
    Serial.println();
    audioModePosition = newPos;
  }

  for(int m = 0; m < MSG_OUT_COUNT; m++)
  {
    hold();
    digitalWrite(pinEnable, LOW);
    writeByte(0x42); // address
    digitalWrite(pinEnable, HIGH);
    for (int i = 0; i < MSG_OUT_BYTES; i++)
    {
      writeByte(msgOut[m][i]);
    }
    digitalWrite(pinEnable, LOW);
    hold();
  }

  
  delay(50);
}
