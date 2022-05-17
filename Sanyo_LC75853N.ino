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

// Bit patterns that toggle one of 16 groups of segments, arranged left to right.
// Used for implementing a Larson scanner with the LCD
uint8_t vertGroups[16][MSG_OUT_COUNT][MSG_OUT_BYTES] = {
  // AM DISC IN
  {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80},
    { 0x00, 0x80, 0x04, 0x00, 0x00, 0x00, 0x40}
  },
  // FM DISC IN
  {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80},
    { 0x00, 0x80, 0x08, 0x00, 0x00, 0x00, 0x40}
  },
  // 12CD<
  {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80},
    { 0x00, 0x50, 0x13, 0x00, 0x00, 0x00, 0x40}
  },
  // MD<>
  {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80},
    { 0x00, 0x62, 0x00, 0x00, 0x00, 0x00, 0x40}
  },
  // DISC CH 1
  {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80},
    { 0xC0, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x40}
  },
  // 8
  {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80},
    { 0x3F, 0x01, 0x00, 0x00, 0x00, 0x00, 0x40}
  },
  // TRACK DB
  {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    { 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x80},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40}
  },
  // 8+
  {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    { 0x00, 0x00, 0x00, 0x80, 0x7E, 0x02, 0x80},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40}
  },
  // 8^
  {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    { 0x00, 0x00, 0x40, 0x7F, 0x01, 0x00, 0x80},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40}
  },
  // 8:
  {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    { 0x00, 0xA0, 0xBF, 0x00, 0x00, 0x00, 0x80},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40}
  },
  // 8X
  {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    { 0xF0, 0x5F, 0x00, 0x00, 0x00, 0x00, 0x80},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40}
  },
  // 8+
  {
    { 0x00, 0x00, 0x00, 0x00, 0xC0, 0x03, 0x00},
    { 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40}
  },
  // :8
  {
    { 0x00, 0x00, 0x00, 0xD0, 0x3F, 0x00, 0x00},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40}
  },
  // 8
  {
    { 0x00, 0x00, 0x60, 0x2F, 0x00, 0x00, 0x00},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40}
  },
  // ST D. SKIP MTL D.
  {
    { 0x00, 0x80, 0x1E, 0x00, 0x00, 0x00, 0x00},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40}
  },
  // RAND D.RPT SCAN
  {
    { 0x00, 0x70, 0x01, 0x00, 0x00, 0x00, 0x00},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80},
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40}
  }
};

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
  segmentAccess(segment, false, turnOn);
}

// Queries state of the specified segment.
// Note segment is zero-based counting and datasheet diagram starts at 1.
bool getSegment(uint8_t segment)
{
  return segmentAccess(segment, true, false);
}

// Common helper for getSegment and setSegment
bool segmentAccess(uint8_t segment, bool getting, bool turnOn)
{
  // Calculate the bit that corresponds to the indicated segment.
  uint8_t message = 0;
  uint8_t index = 0;
  uint8_t segmentBit = 0;
  uint8_t bitPattern = 0;

  if (segment > 126)
  {
    Serial.print("Segment out of range ");
    Serial.println(segment);
    return false;
  }

  message = segment/SEG_PER_MSG;
  segmentBit = segment%SEG_PER_MSG;
  index = segmentBit/8;
  bitPattern = 0x01 << (segmentBit%8);

  if (getting)
  {
    return 0x00 != (msgOut[message][index] & bitPattern);
  }
  else
  {
    if(turnOn)
    {
      msgOut[message][index] |= bitPattern;
    }
    else
    {
      msgOut[message][index] &= ~bitPattern;
    }
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


// Drawing mode
long cursorPosition;
bool cursorState;
bool cursorSegmentState;
unsigned long cursorNextToggle;
bool audioModePress;

// Larson scanner mode
uint8_t scannerState;
int8_t scannerDirection;

// Mode switch button
bool powerPress;

uint8_t currentMode;

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

  scannerState = 0;
  scannerDirection = 1;

  powerPress = false;
  currentMode = 1;
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

void copyGroup(uint8_t groupIndex)
{
  for(int m = 0; m < MSG_OUT_COUNT; m++)
  {
    for (int i = 0; i < MSG_OUT_BYTES; i++)
    {
      msgOut[m][i] |= vertGroups[groupIndex][m][i];
    }
  }
}

void loop() {
  if (currentMode == 0)
  {
    // Drawing mode: see if the audio mode knob has been turned.
    long newPos = audioModeEncoder.read();
    if (newPos != audioModePosition)
    {
      // Knob moved. Leave old segment in the correct state and
      // get the state of the new segment.
      setSegment(cursorPosition,cursorSegmentState);
      cursorPosition = newPos/2;
      cursorSegmentState = getSegment(cursorPosition);

      cursorState = true;
      cursorSegmentUpdate();

      audioModePosition = newPos;
    }
    else if (cursorNextToggle < millis())
    {
      // Knob was not moved, see if we need to blink our cursor.
      cursorState = !cursorState;
      cursorSegmentUpdate();
    }
  }
  else if (currentMode == 1)
  {
    // Larson scanner mode: copy two out of 16 vertical groups.
    msgOutReset();
    copyGroup(scannerState);
    copyGroup(scannerState+1);

    if (scannerState==0 && scannerDirection==-1)
    {
      scannerDirection = 1;
    }
    else if (scannerState==14 && scannerDirection==1)
    {
      scannerDirection = -1;
    }
    else
    {
      scannerState += scannerDirection;
    }
  }

  if (HIGH == digitalRead(pinDataIn))
  {
    if (powerPress)
    {
      // Switch mode
      powerPress = false;
      currentMode = currentMode+1;
      if (currentMode >= 2)
      {
        currentMode = 0;
      }
      msgOutReset();
      cursorSegmentState = getSegment(cursorPosition);
      Serial.print("Mode ");
      Serial.println(currentMode);
    }

    if (0 == currentMode)
    {
      // Drawing mode
      if (audioModePress)
      {
        // Audio knob pressed, toggle current segment state.
        cursorSegmentState = !cursorSegmentState;
        audioModePress = false;
        cursorState = cursorSegmentState;
        setSegment(cursorPosition,cursorSegmentState);
        msgOutPrint();
        cursorNextToggle = millis() + 1000;
      }
    }

    // Draw the current canvas.
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
    // Data In pin pulled low means there is key press data for us to read.
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

    // See if it's a button we react to
    if (!powerPress && (msgIn[3]&0x01))
    {
      powerPress = true;
      msgInPrint();
    }
    else if (!audioModePress && (msgIn[2]&0x08))
    {
      audioModePress=true;
      msgInPrint();
    }
  }

  delay(50);
}
