/*
 * LED Tachometer
 * Version: 1.0.2
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

#define ALPHA 0.05
#define BUTTON_PIN 9
#define DATA_PIN 2
#define NUM_LEDS 8
#define BRIGHTNESS 48

int stateMachine = 2;
int buttonState;
int lastButtonState = LOW;
long lastDebounceTime = 0;
long debounceDelay = 50;

unsigned long rpmAvg = 1000;
unsigned long rpm = 0;
unsigned long rpmLow = 1000;
unsigned long rpmHigh = 3000;
unsigned long flashTime = millis();
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
  boolean setPointsBad;
  if (buttonDebounce()) {
    delay(1000);
    Serial.println("In the button loop");
    switch(stateMachine){
      case 0: // State 0 for setting potentiometer - optoisolator sensitivity
        Serial.println("Pot state");
        flashLeds(CRGB::Red);
        while(!buttonDebounce()){
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
        }
        stateMachine++;
        break;
      case 1: // State 1 for setting low RPM and high RPM
        Serial.println("RPM State");
        setPointsBad = true;
        do {
          flashRpmLeds(2);
          turnOnRpm(2);
          Serial.println("RPM Set Low");
          while(!buttonDebounce()) {
            if (FreqMeasure.available()) {
              rpm = (60/cal)* FreqMeasure.countToFrequency(FreqMeasure.read());
              rpmAvg = rollAvg(rpmAvg, rpm);
              Serial.println(rpmAvg);
            }
          }
          rpmLow = rpmAvg;
          
          flashRpmLeds(8);
          turnOnRpm(8);
          Serial.println("RPM Set High");
          while(!buttonDebounce()) {
            if (FreqMeasure.available()) {
              rpm = (60/cal)* FreqMeasure.countToFrequency(FreqMeasure.read());
              rpmAvg = rollAvg(rpmAvg, rpm);
              Serial.println(rpmAvg);
            }
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
      case 2:
        Serial.println("First Press");
        flashLeds(CRGB::Blue);
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
        lastButtonState = reading;
        return true;
      }
    }
  }
  lastButtonState = reading;
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
    if (millis() - flashTime < 500){
      turnOnRpm(8);
    } else if (millis() - flashTime < 1000) {
      fill_solid(leds, NUM_LEDS, CRGB::Black);
    } else {
      flashTime = millis();
    }
  }

  FastLED.show();
}

unsigned long rollAvg(unsigned long accumulator, unsigned long new_value) {
  accumulator = (ALPHA * new_value) + (1.0 - ALPHA) * accumulator;
  return accumulator;
}

