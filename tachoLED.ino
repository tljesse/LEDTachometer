/*
 * LED Tachometer
 * Version: 1.0.3
 * Written by Tristan Jesse
 * 
 * Required Libraries:  FreqMeasure
 *                      FastLED
 *                      
 * Reads frequency through negative ignition coil and converts to RPM
 * Lights WS2812 LED strip for assistance with shifting
 */

#include <FreqMeasure.h>
#include "FastLED.h"

#define ALPHA 0.08
#define SELECT_PIN 3
#define OPTION_PIN 2
// change LED data wire to 4
// change reset pin to 2
#define DATA_PIN 4
#define NUM_LEDS 8
#define BRIGHTNESS 48

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

volatile bool pattern = false;
volatile bool select = false;
volatile bool normalPress = false;
volatile int selectVal = 0;
volatile int optionVal = 0;
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
  FastLED.setBrightness(BRIGHTNESS);
  
  turnOnLeds();

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
    switch(stateMachine){
      case 1: // State 0 for setting potentiometer - optoisolator sensitivity
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
        stateMachine++;
        break;
      case 2: // State 1 for setting low RPM and high RPM
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
        Serial.print("RPM Low: ");
        Serial.println(rpmLow);
        Serial.print("RPM High: ");
        Serial.println(rpmHigh);
        flashLeds(CRGB::Green);
        stateMachine++;
        break;
      case 3:
        Serial.println("Blinking Pattern");
        pattern = true;
        select = false;
        unsigned long count = millis();
        while (millis() - count < 3000 && !select) {
          whichPattern = 0;
          patternInd(1);
          select = selectDebounce();
        }
        count = millis();
        while (millis() - count < 3000 && !select) {
          whichPattern = 1;
          patternInd(2);
          select = selectDebounce();
        }
        count = millis();
        while (millis() - count < 3000 && !select) {
          whichPattern = 2;
          patternInd(3);
          select = selectDebounce();
        }
        count = millis();
        while (millis() - count < 3000 && !select) {
          whichPattern = 3;
          patternInd(4);
          select = selectDebounce();
        }
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
        FastLED.delay(200);
        stateMachine = 1;
        pattern = false;
        select = false;
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
  // check if options is longer than 3 second hold -- goes to options menu
  if (optionVal == HIGH && (millis() - optionDnTm) > 2500) {
    optionUp = true;
    selectDnTm = millis();
  }
  
  // check for option release - less than 3 seconds to rpm level
  if (optionVal == LOW && optionLast == HIGH && (millis() - optionDnTm) > debounceDelay){
    if(optionUp){
      normalPress = true;
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
    if (ignoreUp == false) enableRpm = true;
    else ignoreUp = false;
    selectUpTm = millis();
  }

  // check for 3 second select hold
  if (selectVal == HIGH && (millis() - selectDnTm) > 3000){
    enableRpm = false;
    ignoreUp = true;
    selectDnTm = millis();
  }

  selectLast = selectVal;
  
  unsigned long rpm;
  if (FreqMeasure.available() && enableRpm) {
    rpm = (60/cal)* FreqMeasure.countToFrequency(FreqMeasure.read());
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
  int reading = digitalRead(OPTION_PIN);
  if (reading != lastButtonState){
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        lastButtonState = reading;
        return true;
      }
    }
  }
  lastButtonState = reading;
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
    leds[7] = CHSV(0, 255, 255);
    switch(whichPattern){
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

