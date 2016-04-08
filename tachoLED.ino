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

volatile bool newPulse = false;
volatile unsigned long lastPulseTime;
volatile unsigned long lastPulseInterval = 0;
volatile unsigned long pulseCount = 0;
volatile unsigned long intervalAverage = 60000;
unsigned long rpmAvg = 1000;
unsigned long rpm = 0;
int cal = 1;
#define ALPHA 0.03

#define DATA_PIN 13
#define NUM_LEDS 8
CRGB leds[NUM_LEDS];

#define BRIGHTNESS 48

void setup() {
  FastLED.addLeds<WS2812,DATA_PIN,GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  
  turnOnLeds();

  FreqMeasure.begin();
  Serial.begin(9600);
  //attachInterrupt(1, rpmtrigger, RISING);
}

void loop()
{
 unsigned long rpm;
 /*if (newPulse)
 {
    rpm = 60000000UL/intervalAverage;
    newPulse = false;
    Serial.println(rpm);
    showRPM(rpm);
 }*/
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

void rpmtrigger()
{
 unsigned long right_now = micros();
 unsigned long pulseInterval = right_now - lastPulseTime;
 if (pulseInterval > 1000UL)  // I have chosen 1ms as the minimum pulse interval, you may need to adjust it
 {
    lastPulseTime = right_now;
    lastPulseInterval = pulseInterval;
    intervalAverage = rollAvg(intervalAverage, lastPulseInterval);
    newPulse = true;
 }
}

