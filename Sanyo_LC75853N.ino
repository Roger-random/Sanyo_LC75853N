/*
 *   Interfacing with a Toyota 86120-08010 tape deck faceplate which primarily
 *   centers around the Sanyo LC75853N LCD driver.
 */
#include <Encoder.h>

#define MSG_OUT_COUNT 3
#define MSG_OUT_BYTES 7
#define SEG_PER_MSG 42

#define MSG_IN_BYTES 4

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

uint8_t msgIn[MSG_IN_BYTES];
uint8_t msgOut[MSG_OUT_COUNT][MSG_OUT_BYTES];
/*
// Control signals for "FM 1 CH 1 87.9"
{
  {0x00, 0x00, 0x00, 0xD0, 0xDD, 0x02, 0x00},
  {0x70, 0x55, 0x00, 0x00, 0x00, 0x00, 0x80},
  {0x06, 0x04, 0x0B, 0x00, 0x00, 0x00, 0x40}  
};
*/

/*
// Control signal to activate all 126 segments
{
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
  Serial.println("{");
  for(int m = 0; m < MSG_OUT_COUNT; m++)
  {
    Serial.print("  { ");
    for (int i = 0; i < MSG_OUT_BYTES; i++)
    {
      if(msgOut[m][i]<0x10)
      {
        Serial.print("0x0");
      }
      else
      {
        Serial.print("0x");
      }
      Serial.print(msgOut[m][i],HEX);
      if (i < MSG_OUT_BYTES-1)
      {
        Serial.print(", ");
      }
    }
    if (m < MSG_OUT_COUNT-1)
    {
      Serial.println("},");
    }
    else
    {
      Serial.println("}");
    }
  }
  Serial.println("};");
}

// Print keyscan data to serial terminal
void msgInPrint()
{
  Serial.print("{ ");
  for (int i = 0; i < MSG_IN_BYTES; i++)
  {
    if(msgIn[i]<0x10)
    {
      Serial.print("0x0");
    }
    else
    {
      Serial.print("0x");
    }
    Serial.print(msgIn[i],HEX);

    if (i < MSG_IN_BYTES-1)
    {
      Serial.print(", ");
    }
  }
  Serial.println("};");
}

// Turn the selected segment on (turnOn=true) or off (turnOn=false)
// Note segment is zero-based counting and datasheet diagram starts at 1.
void setSegment(uint8_t segment, bool turnOn)
{
  uint8_t message = 0;
  uint8_t index = 0;
  uint8_t segmentBit = 0;
  uint8_t bitPattern = 0;

  if (segment > 126)
  {
    Serial.print("Segment out of range ");
    Serial.println(segment);
    return;
  }
  message = segment/SEG_PER_MSG;
  segmentBit = segment%SEG_PER_MSG;
  index = segmentBit/8;
  bitPattern = 0x01 << (segmentBit%8);

  if(turnOn)
  {
    msgOut[message][index] |= bitPattern;
  }
  else
  {
    msgOut[message][index] &= ~bitPattern;
  }
}

bool getSegment(uint8_t segment)
{
  uint8_t message = 0;
  uint8_t index = 0;
  uint8_t segmentBit = 0;
  uint8_t bitPattern = 0;

  if (segment > 126)
  {
    Serial.print("Segment out of range ");
    Serial.println(segment);
    return;
  }
  message = segment/SEG_PER_MSG;
  segmentBit = segment%SEG_PER_MSG;
  index = segmentBit/8;
  bitPattern = 0x01 << (segmentBit%8);

  return 0x00 != (msgOut[message][index] & bitPattern);
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

// Couldn't use Arduino SPI library because it controls enable pin with SPI semantics,
// which is different from CCB.
// Couldn't use Arduino shiftOut() because it uses clock signal differently from CCB.
uint8_t readByte()
{
  uint8_t returnValue = 0;

  // Read 8 bits, least significant bit first.
  for(uint8_t i = 0; i < 8; i++)
  {
    digitalWrite(pinClock, LOW);
    hold();
    if (HIGH == digitalRead(pinDataIn))
    {
      returnValue |= 0x80;
    }
    else
    {
      returnValue &= 0x7F;
    }
    digitalWrite(pinClock, HIGH);
    hold();

    if (i < 7)
    {
      returnValue = returnValue >> 1;
    }
  }

  return returnValue;
}


// Etch-a-sketch mode
long cursorPosition;
bool cursorState;
bool cursorSegmentState;
unsigned long cursorNextToggle;
bool audioModePress;

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

  cursorNextToggle = millis();
  cursorState = true;
  audioModePress = false;
}

void cursorSegmentUpdate()
{
  setSegment(cursorPosition, cursorState);
  if (cursorState == cursorSegmentState)
  {
    cursorNextToggle = millis() + 500;
  }
  else
  {
    cursorNextToggle = millis() + 200;
  }
}

void loop() {

  long newPos = audioModeEncoder.read();
  if (newPos != audioModePosition)
  {
    setSegment(cursorPosition,cursorSegmentState);
    cursorPosition = newPos/2;
    cursorSegmentState = getSegment(cursorPosition);

    cursorState = true;
    cursorSegmentUpdate();
    
    audioModePosition = newPos;
  }
  else if (cursorNextToggle < millis())
  {
    cursorState = !cursorState;
    cursorSegmentUpdate();
  }

  if (HIGH == digitalRead(pinDataIn))
  {
    if (audioModePress)
    {
      cursorSegmentState = !cursorSegmentState;
      audioModePress = false;
      cursorState = cursorSegmentState;
      setSegment(cursorPosition,cursorSegmentState);
      msgOutPrint();
      cursorNextToggle = millis() + 1000;
    }
    
    for(int m = 0; m < MSG_OUT_COUNT; m++)
    {
      hold();
      digitalWrite(pinEnable, LOW);
      writeByte(0x42); // Sanyo LC75853N CCB address to control LCD segments
      digitalWrite(pinEnable, HIGH);
      for (int i = 0; i < MSG_OUT_BYTES; i++)
      {
        writeByte(msgOut[m][i]);
      }
      digitalWrite(pinEnable, LOW);
      hold();
    }
  }
  else
  {
    hold();
    digitalWrite(pinEnable, LOW);
    writeByte(0x43); // Sanyo LC75853N CCB address to report keyscan data
    digitalWrite(pinEnable, HIGH);
    for (int i = 0; i < MSG_IN_BYTES; i++)
    {
      msgIn[i] = readByte();
    }
    digitalWrite(pinEnable, LOW);
    hold();

    if (!audioModePress && (msgIn[2]&0x08))
    {
      audioModePress=true;
      msgInPrint();
    }
  }

  delay(50);
}
