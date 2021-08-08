#include <MIDI.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>

#define OUT_CLOCK 10
#define OUT_CLOCK_16TH 6
#define OUT_GATE 12
#define OUT_CV_LED 8

#define RATIO_POT A2

#define PULSE_PER_QUARTER 24
#define MIN_C 24

MIDI_CREATE_DEFAULT_INSTANCE();

Adafruit_MCP4725 dac;

float npqList[9] = {12,8,4,3,2,1,0.5,1./3., 0.25};
int npqListSize = 9;

float currentNpq = 0;

uint32_t midiTick = 0;

void setup() {
  Serial.begin(9600);

  Serial.print(npqListSize);

  // For Adafruit MCP4725A1 the address is 0x62 (default) or 0x63 (ADDR pin tied to VCC)
  // For MCP4725A0 the address is 0x60 or 0x61
  // For MCP4725A2 the address is 0x64 or 0x65
  dac.begin(0x62);
  
  MIDI.setHandleClock(handleClock);
  MIDI.setHandleStart(handleStart);
  
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  
  MIDI.begin(MIDI_CHANNEL_OMNI);
}

void loop() {

  int potValue = analogRead(RATIO_POT);
  
  int npqIndex = round((potValue/1023.0) * (npqListSize - 1));
  
  currentNpq = npqList[npqIndex];
 
  MIDI.read();
}

void handleStart() {
  midiTick = 0;
}

int noteToVolt(int noteIn) {
    return (int)round(((float)(4095.0/5.0)/12.0) * noteIn);
}

void outputClock(float notesPerQuarter, int pin) {
  int noteLength = PULSE_PER_QUARTER/notesPerQuarter;
  bool clockStatus = (midiTick % noteLength) < (noteLength/2.0);
  digitalWrite(pin, clockStatus ? HIGH : LOW);
}

void handleClock() {
  outputClock(4, OUT_CLOCK_16TH);
  outputClock(currentNpq, OUT_CLOCK);
  midiTick = midiTick + 1;
}

void doNoteOff() {
  digitalWrite(OUT_GATE, LOW);
  digitalWrite(OUT_CV_LED, LOW);
}

void handleNoteOn(byte channel, byte note, byte velocity) {
  if (velocity > 0) {
    byte noteToSend = note - MIN_C;
    noteToSend = fmax(fmin(noteToSend, 60),0);
    int volt = noteToVolt(noteToSend);
    dac.setVoltage(volt, false);
    
    digitalWrite(OUT_GATE, HIGH);
    digitalWrite(OUT_CV_LED, HIGH);
  } else {
    doNoteOff();
  }
}

void handleNoteOff(byte channel, byte note, byte velocity) {
  doNoteOff();
}
