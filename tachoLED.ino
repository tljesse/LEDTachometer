/*
 * LED Tachometer
 * Version: 1.0.0
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

#define ALPHA 0.03
#define BUTTON_PIN 3
#define DATA_PIN 13
#define NUM_LEDS 8
#define BRIGHTNESS 48

int stateMachine = 0;
int buttonState;
int lastButtonState = LOW;
long lastDebounceTime = 0;
long debounceDelay = 50;

unsigned long rpmAvg = 1000;
unsigned long rpm = 0;
unsigned long rpmLow = 1000;
unsigned long rpmHigh = 3000;
int cal = 1;

CRGB leds[NUM_LEDS];

/******************
 *     SETUP      *
 ******************/
void setup() {
  FastLED.addLeds<WS2812,DATA_PIN,GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  
  turnOnLeds();

  pinMode(BUTTON_PIN, INPUT);

  FreqMeasure.begin();
  Serial.begin(9600);
}

/******************
 *      LOOP       *
 ******************/
void loop()
{
  if (buttonDebounce()) {
    switch(stateMachine){
      case '0': // State 0 for setting potentiometer - optoisolator sensitivity
        flashLeds(CRGB::Red);
        while(!buttonDebounce()){
          if (FreqMeasure.available()) {
            leds[0] = CRGB::Green;
            FastLED.show();
          } else {
            leds[0] = CRGB::Red;
            FastLED.show();
          }
        }
        stateMachine++;
        break;
      case '1': // State 1 for setting low RPM and high RPM
        boolean setPointsBad = true;
        do {
          flashRpmLeds(2);
          turnOnRpm(2);
          while(!buttonDebounce()) {
            if (FreqMeasure.available()) {
              rpm = (60/cal)* FreqMeasure.countToFrequency(FreqMeasure.read());
              rpmAvg = rollAvg(rpmAvg, rpm);
            }
          }
          rpmLow = rpmAvg;
          
          flashRpmLeds(8);
          turnOnRpm(8);
          while(!buttonDebounce()) {
            if (FreqMeasure.available()) {
              rpm = (60/cal)* FreqMeasure.countToFrequency(FreqMeasure.read());
              rpmAvg = rollAvg(rpmAvg, rpm);
            }
          }
          rpmHigh = rpmAvg;

          setPointsBad = rpmLow > rpmHigh;
        } while(setPointsBad);

        flashLeds(CRGB::Green);
        stateMachine = 0;
        break;
        
    } // end State Machine
  } // end button actions
  
  unsigned long rpm;
  if (FreqMeasure.available()) {
    rpm = (60/cal)* FreqMeasure.countToFrequency(FreqMeasure.read());
    rpmAvg = rollAvg(rpmAvg, rpm);
    showRPM(rpmAvg);
    Serial.println(rpmAvg);
    //timeoutCounter = timeoutValue;
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

boolean buttonDebounce(){
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState){
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        return true;
      }
    }
  }
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
  
  if (rpm > 500) {
    leds[0] = CHSV(96, 255, 255);
  }
  if (rpm > 800) {
    leds[1] = CHSV(96, 255, 255);
  }
  if (rpm > 1100) {
    leds[2] = CHSV(96, 255, 255);
  }
  if (rpm > 1400) {
    leds[3] = CHSV(64, 255, 255);
  }
  if (rpm > 1700) {
    leds[4] = CHSV(64, 255, 255);
  }
  if (rpm > 2000) {
    leds[5] = CHSV(64, 255, 255);
  }
  if (rpm > 2300) {
    leds[6] = CHSV(0, 255, 255);
  }
  if (rpm > 2600) {
    leds[7] = CHSV(0, 255, 255);
  }

  FastLED.show();
}

unsigned long rollAvg(unsigned long accumulator, unsigned long new_value) {
  accumulator = (ALPHA * new_value) + (1.0 - ALPHA) * accumulator;
  return accumulator;
}

