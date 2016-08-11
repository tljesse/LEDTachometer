/*
 * LED Tachometer
 * Version: 1.0.5
 * Written by Tristan Jesse
 * 
 * Required Libraries:  FreqMeasure
 *                      FastLED
 *                      
 * Reads frequency through negative ignition coil and converts to RPM
 * Lights WS2812 LED strip for assistance with shifting
 */

#include <EEPROM.h>
#include <FreqMeasure.h>
#include "FastLED.h"

#define ALPHA 0.08
#define SELECT_PIN 3
#define OPTION_PIN 2
#define DATA_PIN 4
#define NUM_LEDS 8

#define LOW_ADDR 0
#define HIGH_ADDR 2
#define PTN_ADDR 4
#define BRT_ADDR 5

int stateMachine = 1;
int buttonState;
int selectState;
int lastButtonState = LOW;
long lastDebounceTime = 0;
long debounceDelay = 50;

unsigned long rpmAvg = 1000;
unsigned long rpm = 0;
unsigned long rpmLow = 1000;
unsigned long rpmHigh = 3000;
unsigned long flashTime = millis();
int cal = 1;

volatile bool select = false;
volatile bool normalPress = false;
volatile int selectVal = 0;
volatile int optionVal = 0;
int whichBrightness = 1;
int whichPattern = 0;
int optionLast = 0;
int selectLast = 0;
bool ignoreUp = false;
bool optionUp = false;
bool enableRpm = true;

unsigned long optionDnTm = 0;
unsigned long optionUpTm = 0;
unsigned long selectDnTm = 0;
unsigned long selectUpTm = 0;

CRGB leds[NUM_LEDS];

/******************
 *     SETUP      *
 ******************/
void setup() {
  FastLED.addLeds<WS2812,DATA_PIN,GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  
  turnOnLeds();

  int alpha = EEPROM.read(LOW_ADDR);
  int beta = EEPROM.read(LOW_ADDR + 1);
  rpmLow = alpha*256 + beta;

  alpha = EEPROM.read(HIGH_ADDR);
  beta = EEPROM.read(HIGH_ADDR + 1);
  rpmHigh = alpha*256 + beta;

  whichPattern = EEPROM.read(PTN_ADDR);
  whichBrightness = EEPROM.read(BRT_ADDR);
  alpha = 4 * whichBrightness;
  if(alpha > 255) alpha = 255;
  FastLED.setBrightness(alpha);

  pinMode(SELECT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(SELECT_PIN), selectIsr, CHANGE);

  pinMode(OPTION_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(OPTION_PIN), optionIsr, CHANGE);

  FreqMeasure.begin();
  Serial.begin(9600);
}

/******************
 *      LOOP       *
 ******************/
void loop()
{
  boolean setPointsBad;
  if (normalPress) {
    delay(1000);
    Serial.println("In the button loop");
    int a;
    int b;
    bool option = false;
    switch(stateMachine){
      case 1: // State 1 for setting potentiometer - optoisolator sensitivity
        Serial.println("Pot state");
        flashLeds(CRGB::Red);
        normalPress = false;
        while(!normalPress){
          if (FreqMeasure.available()) {
            leds[0] = CRGB::Green;
            FastLED.show();
            rpm = (60/cal)* FreqMeasure.countToFrequency(FreqMeasure.read());
            rpmAvg = rollAvg(rpmAvg, rpm);
            Serial.println(rpmAvg);
            FastLED.delay(100);
          } else {
            leds[0] = CRGB::Red;
            FastLED.show();
            FastLED.delay(100);
          }
          normalPress = selectDebounce();
        }
        
        // Set the low and high RPM immediately after potentiometer
        Serial.println("RPM State");
        normalPress = false;
        setPointsBad = true;
        do {
          flashRpmLeds(2);
          turnOnRpm(2);
          Serial.println("RPM Set Low");
          while(!normalPress) {
            if (FreqMeasure.available()) {
              rpm = (60/cal)* FreqMeasure.countToFrequency(FreqMeasure.read());
              rpmAvg = rollAvg(rpmAvg, rpm);
              Serial.println(rpmAvg);
            }
            normalPress = selectDebounce();
          }
          rpmLow = rpmAvg;
          
          flashRpmLeds(8);
          turnOnRpm(8);
          Serial.println("RPM Set High");
          normalPress = false;
          while(!normalPress) {
            if (FreqMeasure.available()) {
              rpm = (60/cal)* FreqMeasure.countToFrequency(FreqMeasure.read());
              rpmAvg = rollAvg(rpmAvg, rpm);
              Serial.println(rpmAvg);
            }
            normalPress = selectDebounce();
          }
          rpmHigh = rpmAvg;

          setPointsBad = rpmLow > rpmHigh;
        } while(setPointsBad);
        a = rpmLow/256;
        b = rpmLow%256;
        EEPROM.write(LOW_ADDR, a);
        EEPROM.write(LOW_ADDR+1, b);
        a = rpmHigh/256;
        b = rpmHigh%256;
        EEPROM.write(HIGH_ADDR, a);
        EEPROM.write(HIGH_ADDR+1, b);
        flashLeds(CRGB::Green);
        stateMachine = 1;
        break;
      case 3:
        Serial.println("Brightness Select");

        do{
          select = false;
          option = false;
          Serial.println("First");
          if (select) Serial.println("Select Fault");
          if (option) Serial.println("Option Fault");
          while (!select && !option) {
            whichBrightness = 1;
            FastLED.setBrightness(64);
            turnOnRpm(8);
            select = selectDebounce();
            option = optionDebounce();
          }
          option = false;
          Serial.println("Second");
          while (!select && !option) {
            whichBrightness = 2;
            FastLED.setBrightness(128);
            turnOnRpm(8);
            select = selectDebounce();
            option = optionDebounce();
          }
          option = false;
          Serial.println("Third");
          while (!select && !option) {
            whichBrightness = 3;
            FastLED.setBrightness(192);
            turnOnRpm(8);
            select = selectDebounce();
            option = optionDebounce();
          }
          option = false;
          Serial.println("Fourth");
          while (!select && !option) {
            whichBrightness = 4;
            FastLED.setBrightness(255);
            turnOnRpm(8);
            select = selectDebounce();
            option = optionDebounce();
          }
          FastLED.delay(200);
        } while (!select);
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
        EEPROM.write(BRT_ADDR, whichBrightness);
        stateMachine = 4;
        select = false;
        option = false;

        // Set blinking pattern immediately after brightness
        Serial.println("Blinking Pattern");

        do{
          select = false;
          option = false;
          while (!select && !option) {
            whichPattern = 0;
            maxRpmPattern(0);
            FastLED.show();
            select = selectDebounce();
            option = optionDebounce();
          }
          option = false;
          while (!select && !option) {
            whichPattern = 1;
            maxRpmPattern(1);
            FastLED.show();
            select = selectDebounce();
            option = optionDebounce();
          }
          option = false;
          while (!select && !option) {
            whichPattern = 2;
            maxRpmPattern(2);
            FastLED.show();
            select = selectDebounce();
            option = optionDebounce();
          }
          option = false;
          while (!select && !option) {
            whichPattern = 3;
            maxRpmPattern(3);
            FastLED.show();
            select = selectDebounce();
            option = optionDebounce();
          }
          fill_solid(leds, NUM_LEDS, CRGB::Black);
          FastLED.show();
          FastLED.delay(200);
        } while (!select);
        EEPROM.write(PTN_ADDR, whichPattern);
        flashLeds(CRGB::Green);
        stateMachine = 1;
        select = false;
        option = false;
        break;
    } // end State Machine
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    normalPress = false;
  } // end button actions

   // check for option pressed
  if (optionVal == HIGH && optionLast == LOW && (millis() - optionUpTm) > debounceDelay){
    optionDnTm = millis();
  }
  // check if options is longer than 4 second hold -- goes to deep settings
  if (optionVal == HIGH && selectVal == LOW && enableRpm && (millis() - optionDnTm) > 4000) {
    if( !optionUp ){
      optionUp = true;
      normalPress = true;
      stateMachine = 1;
    }
    selectDnTm = millis();
  }
  
  // check for option release - less than 4 seconds to quick settings
  if (optionVal == LOW && optionLast == HIGH && enableRpm && (millis() - optionDnTm) > 1000){
    if(optionUp){
      optionUp = false;
    } else {
      normalPress = true;
      stateMachine = 3;
    }
    optionUpTm = millis();
  }

  optionLast = optionVal;

  // check for select pressed
  if (selectVal == HIGH && selectLast == LOW && (millis() - selectUpTm) > debounceDelay){
    selectDnTm = millis();
  }

  // check for select release
  if (selectVal == LOW && selectLast == HIGH && (millis() - selectDnTm) > debounceDelay){
    if ( ignoreUp ) ignoreUp = false;
    selectUpTm = millis();
  }

  // check for 1 second select hold
  if (selectVal == HIGH && optionVal == HIGH && (millis() - selectDnTm) > 1000 && (millis() - optionDnTm) > 1000){
    if ( !ignoreUp ) {
      enableRpm = !enableRpm;
      ignoreUp = true;
      optionUp = true;
    }
    selectDnTm = millis();
  }

  selectLast = selectVal;
  
  unsigned long rpm;
  if (FreqMeasure.available() && enableRpm) {
    rpm = (60/cal)* FreqMeasure.countToFrequency(FreqMeasure.read());
    if (filterNoise(rpm, rpmAvg))
      rpmAvg = rollAvg(rpmAvg, rpm);
    showRPM(rpmAvg);
    Serial.println(rpmAvg);
    //timeoutCounter = timeoutValue;
  } else if (!enableRpm) {
    rpm = 0;
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
  } else {
    rpm = 0;
  }
  //Serial.print("RPM = ");
  //Serial.print(lastPulseInterval);
  //Serial.println();
  //delay(200);
}


/**********************
 *      FUNCTIONS     *
 **********************/

boolean optionDebounce(){
  if (optionVal == HIGH && optionLast == LOW && (millis() - optionUpTm) > debounceDelay){
    optionDnTm = millis();
  }
  // check if options is longer than 3 second hold -- goes to options menu
  if (optionVal == LOW && optionLast == HIGH && (millis() - optionDnTm) > debounceDelay) {
    if (optionUp) {
      optionUp = false;
    } else {
      optionLast = selectVal;
      return true;
      optionDnTm = millis();
    }
  }
  optionLast = optionVal;
  return false;
}

boolean selectDebounce(){
  if (selectVal == HIGH && selectLast == LOW && (millis() - selectUpTm) > debounceDelay){
    selectDnTm = millis();
  }

  // check for select release
  if (selectVal == LOW && selectLast == HIGH && (millis() - selectDnTm) > debounceDelay){
    selectLast = selectVal;
    return true;
    selectUpTm = millis();
  }
  selectLast = selectVal;
  return false;
}

/*
 * turnOnLeds: Statup routine for the LEDs
 *              Fade green to red up and off down
 */
void turnOnLeds() {
  int hue = 96;
  for (int dot = 0; dot < NUM_LEDS; dot++){
    leds[dot] = CHSV(hue, 255, 255);
    delay(100);
    FastLED.show();

    hue -= 96/(NUM_LEDS-1);
  }

  delay(500);

  for (int dot = NUM_LEDS - 1; dot > -1; dot--){
    leds[dot] = CRGB::Black;
    FastLED.show();
    delay(100);
  }
}

void flashLeds(CRGB color) {
  for(int x = 0; x < 2; x++){
    fill_solid(leds, NUM_LEDS, color);
    FastLED.show();
    FastLED.delay(200);
  
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    FastLED.delay(200);
  }
}

void turnOnRpm(int howMany) {
  for(int dot = 0; dot < howMany; dot++) {
    dot < 4 ? leds[dot] = CRGB::Green : leds[dot] = CRGB::Yellow;
    if(dot > 5) leds[dot] = CRGB::Red;
  }
  FastLED.show();
}

void patternInd(int howMany) {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for(int dot = 0; dot < howMany; dot++) {
    leds[dot] = CRGB::Blue;
  }
  FastLED.show();
}

void flashRpmLeds(int howMany) {
  for(int x = 0; x < 2; x++) {
    turnOnRpm(howMany);
    FastLED.delay(200);

    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    FastLED.delay(200);
  }
}

/*
 * filterNoise: attempts to filter out errounous signals by checking
 *              if a reading is far outside the norm
 *              True = OK reading
 *              False = NOISE reading
 */
bool filterNoise(unsigned long rpmReading, unsigned long rpmLast){
  unsigned long rpmVariance = (rpmHigh - rpmLow)/6;
  bool result;
  rpmReading > (rpmLast + (3*rpmVariance)) ? result = false : result = true;
  return result;
}

/*
 * maxRpmPattern: executes the pattern for 
 *                top RPM based on input
 */
 void maxRpmPattern(int pattern){
  switch(pattern){
    case 0:
      if (millis() - flashTime < 120){
        turnOnRpm(8);
      } else if (millis() - flashTime < 240) {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
      } else {
        flashTime = millis();
      }
      break;
    case 1:
      turnOnRpm(6);
      if (millis() - flashTime < 120) {
        leds[6] = CHSV(0, 255, 255);
        leds[7] = CHSV(0, 255, 255);
      } else if (millis() - flashTime < 240) {
        leds[6] = CRGB::Black;
        leds[7] = CRGB::Black;
      } else {
        flashTime = millis();
      }
      break;
    case 2:
      turnOnRpm(8);
      break;
    case 3:
      if (millis() - flashTime < 120) {
        for (int x = 0; x < 4; x++){
          x < 3 ? leds[x] = CRGB::Green : leds[x] = CRGB::Yellow;
          leds[x+4] = CRGB::Black;
        }
      } else if (millis() - flashTime < 240) {
        for(int x = 0; x < 4; x++) {
          x < 2 ? leds[x+4] = CHSV(64, 255, 255) : leds[x+4] = CHSV(0, 255, 255);
          leds[x] = CRGB::Black;
        }
      } else {
        flashTime = millis();
      }
      break;
  } // end switch
}

/*
 * showRPM: determines what LEDs to turn on
 *          based off an RPM reading
 */
void showRPM(unsigned int rpm) {
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  unsigned long rpmVariance = (rpmHigh - rpmLow)/6;
  
  if (rpm > rpmLow - rpmVariance) {
    leds[0] = CHSV(96, 255, 255);
  }
  if (rpm > rpmLow) {
    leds[1] = CHSV(96, 255, 255);
  }
  if (rpm > rpmLow + rpmVariance) {
    leds[2] = CHSV(96, 255, 255);
  }
  if (rpm > rpmLow + 2*rpmVariance) {
    leds[3] = CHSV(64, 255, 255);
  }
  if (rpm > rpmLow + 3*rpmVariance) {
    leds[4] = CHSV(64, 255, 255);
  }
  if (rpm > rpmLow + 4*rpmVariance) {
    leds[5] = CHSV(64, 255, 255);
  }
  if (rpm > rpmLow + 5*rpmVariance) {
    leds[6] = CHSV(0, 255, 255);
  }
  if (rpm > rpmHigh) {
    //leds[7] = CHSV(0, 255, 255);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    maxRpmPattern(whichPattern);
  }

  FastLED.show();
}

unsigned long rollAvg(unsigned long accumulator, unsigned long new_value) {
  accumulator = (ALPHA * new_value) + (1.0 - ALPHA) * accumulator;
  return accumulator;
}

void selectIsr() {
  selectVal = digitalRead(SELECT_PIN);
}

void optionIsr() {
  optionVal = digitalRead(OPTION_PIN);
}

