/*
 *   Interfacing with a Toyota 86120-08010 tape deck faceplate which primarily
 *   centers around the Sanyo LC75853N LCD driver.
 */
#include <Encoder.h>

#define MSG_OUT_COUNT 3
#define MSG_OUT_BYTES 7
#define SEG_PER_MSG 42

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

// Change the control message to all segments OFF.
void msgOutReset()
{
  for(int m = 0; m < MSG_OUT_COUNT; m++)
  {
    for (int i = 0; i < MSG_OUT_BYTES; i++)
    {
      msgOut[m][i] = 0x00;
    }
  }
  msgOut[1][6] = 0x80;
  msgOut[2][6] = 0x40;
}

// Print control message to serial terminal
void msgOutPrint()
{
  for(int m = 0; m < MSG_OUT_COUNT; m++)
  {
    Serial.print(m);
    for (int i = 0; i < MSG_OUT_BYTES; i++)
    {
      Serial.print(" ");
      Serial.print(msgOut[m][i],BIN);
    }
    Serial.println();
  }
}

// Turn the selected segment on (turnOn=true) or off (turnOn=false)
// Note segment is zero-based counting and datasheet diagram starts at 1.
void msgOutSegment(uint8_t segment, bool turnOn)
{
  uint8_t message = 0;
  uint8_t index = 0;
  uint8_t segmentBit = 0;
  uint8_t bitPattern = 0;

  if (segment > 126)
  {
    Serial.print("msgOutSegment out of range ");
    Serial.println(segment);
    return;
  }
  message = segment/SEG_PER_MSG;
  segmentBit = segment%SEG_PER_MSG;
  index = segmentBit/8;
  bitPattern = 0x01 << (segmentBit%8);

  if(turnOn)
  {
    Serial.print("Turning on D");
    Serial.println(segment+1); // LC75853N datasheet uses 1-based counting for display bits
    msgOut[message][index] |= bitPattern;
  }
  else
  {
    msgOut[message][index] &= ~bitPattern;
  }
}

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

  msgOutReset();
}

void loop() {

  long newPos = audioModeEncoder.read();
  if (newPos != audioModePosition)
  {
    Serial.print("Audio Mode knob at ");
    Serial.print(newPos);
    Serial.println();

    msgOutSegment(audioModePosition/2,false);
    msgOutSegment(newPos/2,true);

    audioModePosition = newPos;
    msgOutPrint();
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
