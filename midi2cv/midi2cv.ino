#include <MIDI.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>

#define OUT_CLOCK 10
#define OUT_CLOCK_16TH 6
#define OUT_GATE 12
#define OUT_CV_LED 8

#define RATIO_POT A2
#define RATCHET_POT A1
#define SWITCH_CLOCK_OUT 3
#define SWITCH_DAC_MODE 2

#define PULSE_PER_QUARTER 24
#define MIN_C 24

MIDI_CREATE_DEFAULT_INSTANCE();

Adafruit_MCP4725 dac;

byte actionCounter = 0;

uint32_t midiTick = 0;

unsigned long freeClock = 0;
bool freeClockStatus = false;
bool syncClockStatus = false;
bool sixteenthClockStatus = false;
bool ratchetState = false;
bool isPlaying = false;

unsigned long delayStart = 0;
bool delayIsRunning = false;

int dacCCValue = 0;
int dacRandValue = 0;

void setup() {
  Serial.begin(9600);

  //Serial.print(npqListSize);

  dac.begin(0x62);

  pinMode(SWITCH_CLOCK_OUT, INPUT_PULLUP);
  pinMode(SWITCH_DAC_MODE, INPUT_PULLUP);
  
  MIDI.setHandleClock(handleClock);
  MIDI.setHandleStart(handleStart);
  MIDI.setHandleStop(handleStop);

  MIDI.setHandleControlChange(handleControlChange);

  randomSeed(analogRead(7));
  
  MIDI.begin(MIDI_CHANNEL_OMNI);
}

void performFreeClock() {
  int potValue = analogRead(RATIO_POT);
  double dpot = (double)potValue/1023.0;

  dpot = dpot*dpot*dpot;

  static const int minTime = 5;
  static const int maxTime = 2000;

  int miliTime = dpot*(maxTime - minTime) + minTime;
  
  if ((millis() - freeClock) > miliTime) {
    freeClock = millis();
    freeClockStatus = !freeClockStatus;
  }
}

void loop() {

  actionCounter++;

  if (actionCounter > 100) {

    performFreeClock();

    digitalWrite(OUT_CLOCK_16TH, (syncClockStatus && isPlaying) ? HIGH : LOW);
    digitalWrite(OUT_CLOCK, freeClockStatus ? HIGH : LOW);

    bool outSelector = digitalRead(SWITCH_CLOCK_OUT) == HIGH;
    digitalWrite(OUT_GATE, (outSelector ? (syncClockStatus && isPlaying) : freeClockStatus) ? HIGH : LOW);

    if ((millis() - delayStart) >= 500 && delayIsRunning) {
      delayIsRunning = false;
    }

    bool dacMode = digitalRead(SWITCH_DAC_MODE) == HIGH;

    if (dacMode) {
      dac.setVoltage(dacCCValue, false);
      digitalWrite(OUT_CV_LED, delayIsRunning ? HIGH : LOW);
    } else {
      dac.setVoltage(dacRandValue, false);
      digitalWrite(OUT_CV_LED, (syncClockStatus && isPlaying) ? HIGH : LOW);
    }

  }
  MIDI.read();
}

void handleStart() {
  midiTick = 0;
  isPlaying = true;
}

void handleStop() {
  isPlaying = false;
}

int noteToVolt(int noteIn) {
    return (int)round(((float)(4095.0/5.0)/12.0) * noteIn);
}

bool getSyncClockStatus(float notesPerQuarter) {
  int noteLength = PULSE_PER_QUARTER/notesPerQuarter;
  bool clockStatus = (midiTick % noteLength) < (noteLength/2.0);
  return clockStatus;
}

void handleClock() {
  bool previous = sixteenthClockStatus;
  sixteenthClockStatus = getSyncClockStatus(4);

  bool ratchetClockStatus = getSyncClockStatus(8);
  
  if (sixteenthClockStatus && !previous) {
    long randomValue = random(1024);
    float potValue = analogRead(RATCHET_POT);
    ratchetState = potValue >= randomValue;
  }

  previous = syncClockStatus;
  syncClockStatus = ratchetState ? ratchetClockStatus : sixteenthClockStatus;

  if (syncClockStatus && !previous && isPlaying) {
    dacRandValue = random(1024);
  }
  
  midiTick = midiTick + 1;
}

void handleControlChange(byte channel, byte control, byte value) {

  if (channel == 10) {
    if (control == 3) {
      dacCCValue = noteToVolt(value);

      delayStart = millis();
      delayIsRunning = true;
    }
  }
}
