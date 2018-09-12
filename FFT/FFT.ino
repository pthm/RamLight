#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <FastLED.h>
#include <math.h>

#define HWSERIAL Serial1

#define LED_PIN     5
#define COLOR_ORDER GRB
#define CHIPSET     WS2812B
#define NUM_LEDS    300

#define BRIGHTNESS  256
#define FRAMES_PER_SECOND 126

#define FFT_RES 43

#define LOW_PEAK 70
#define MID_PEAK 1400
#define HIGH_PEAK 10000

#define BEAT_SENSITIVITY 2.1

CRGB leds[NUM_LEDS];

const int myInput = AUDIO_INPUT_MIC;

AudioInputAnalog       audioInput;
AudioAnalyzeFFT1024    FFT;
AudioAnalyzeRMS        RMS;
AudioConnection        patchCord1(audioInput, FFT);
AudioConnection        patchCord2(audioInput, RMS);
CRGBPalette16          gPal;

void fadeall() { for(int i = 0; i < NUM_LEDS; i++) { leds[i].nscale8(240); } }

void printVal(String val){
  Serial.print(val);
  Serial.print(" ");
}

void printNamedVal(String name, String val) {
  Serial.print(name);
  Serial.print(": ");
  Serial.print(val);
  Serial.println();
}

int lowBinCount;
int midBinCount;
int highBinCount;

int lowLow;
int lowHigh;

int midLow;
int midHigh;

int highLow;
int highHigh;

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void calculateBinValues(){
  lowLow = 0;
  lowHigh = round(LOW_PEAK / FFT_RES);
  midLow = lowHigh;
  midHigh = round(MID_PEAK / FFT_RES);
  highLow = midHigh;
  highHigh = round(HIGH_PEAK / FFT_RES);
  lowBinCount = lowHigh - lowLow;
  midBinCount = midHigh - midLow;
  highBinCount = highHigh - highLow;
}

void setup() {
  delay(3000);
  HWSERIAL.begin(9600);
  calculateBinValues();
  AudioMemory(12);
  FFT.windowFunction(AudioWindowHanning1024);
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);
  FastLED.showColor(CRGB(255,255,255));
  fadeLightBy(leds, 300, 50);
  FastLED.show();
  HWSERIAL.print("AT+NAME?");
}

float rmsBrightnessPct = 0.2;
float rms = 0;
float avgRms = 0;
int rmsCount = 0;

float low = 0;
float lowAvg = 0;
int lowCount = 0;

int lastBeat = 0;

void loop() {
  if (HWSERIAL.available() > 0) {
    String incomingString = HWSERIAL.readString();
    Serial.print("I received: ");
    Serial.println(incomingString);
  }
  
  // Brightness
  if(RMS.available()){
    float currRms = RMS.read();
    rms = currRms;
    rmsCount++;

    float differential = (currRms - avgRms) / rmsCount;
    avgRms = avgRms + differential;

    float baseBrightness = BRIGHTNESS * (1 - rmsBrightnessPct);
    float rmsBrightness = avgRms * (BRIGHTNESS * rmsBrightnessPct);
    float newBrightness = baseBrightness + rmsBrightness;
    
    FastLED.setBrightness(newBrightness);
  }
  if((rmsCount % 10) == 0 && rmsCount > 0){
     rmsCount--;
  }

  // Color Mixing
  float low = 0;
  float mid = 0;
  float high = 0;

  if (FFT.available()) {
    for(int i = 0; i < 512; i++){
      float amp = FFT.read(i);
      if(i > lowLow && i <= lowHigh){
        low += amp;
      }
      if(i > midLow && i <= midHigh){
        mid += amp;
      }
      if(i > highLow && i <= highHigh){
        high += amp;
      }
    }
    low = (low / lowBinCount);
    mid = (mid / midBinCount);
    high = (high / highBinCount);
    
    lowCount++;
    float differential = (low - lowAvg) / lowCount;
    lowAvg = lowAvg + differential;
  }

  if((lowCount % 200) == 0 && lowCount > 0){
     lowCount--;
  }

  printVal(lowAvg * 100);
  printVal(lowAvg * 100 * BEAT_SENSITIVITY);

  CRGB prev = leds[150];

  leds[150] = CRGB(low * 255, mid * 255,  (mid + high / 2) * 255 * 3);
  int currMillis = millis();
  if(low > (lowAvg * BEAT_SENSITIVITY) && ((currMillis - lastBeat) > 100)){
    lastBeat = currMillis;
    leds[150] += CRGB::White;
  }
  
  for(int i = 150 ; i < NUM_LEDS; i++){
    CRGB curr = leds[i];
    leds[i] = prev;
    leds[150 - (i - 150)] = prev;
    prev = curr;
  }
  fadeall();
  
  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);    


  printVal(low * 100);
  printVal(mid * 100);
  printVal(high * 100);
  Serial.println();
}
