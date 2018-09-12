#include <WS2812Serial.h>
#define USE_WS2812SERIAL

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SerialFlash.h>
#include <FastLED.h>
#include <math.h>

#define HWSERIAL Serial3

#define LED_PIN     5
#define COLOR_ORDER BRG
#define CHIPSET     WS2812SERIAL
#define NUM_LEDS    300

#define BRIGHTNESS  256
#define FRAMES_PER_SECOND 120

#define FFT_RES 43

#define LOW_PEAK 70
#define MID_PEAK 1400
#define HIGH_PEAK 10000

#define BEAT_SENSITIVITY 2.1

CRGB leds[NUM_LEDS];
AudioInputAnalog       audioInput;
AudioAnalyzeFFT1024    FFT;
AudioAnalyzeRMS        RMS;
AudioConnection        patchCord1(audioInput, FFT);
AudioConnection        patchCord2(audioInput, RMS);

int lowBinCount;
int midBinCount;
int highBinCount;

int lowLow;
int lowHigh;
int midLow;
int midHigh;
int highLow;
int highHigh;

float lowAvg = 0;
int lowCount = 0;
float midAvg = 0;
int midCount = 0;
float highAvg = 0;
int highCount = 0;

float rmsBrightnessPct = 0.7;
float rms = 0;
float avgRms = 0;
int rmsCount = 0;

int lastBeat = 0;
int strobeCount = 0;

// Gradient palette "lava_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/neota/elem/tn/lava.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 52 bytes of program space.

DEFINE_GRADIENT_PALETTE( lava_gp ) {
    0,   0,  0,  0,
  128,   0,  0,  128,
  255,   0,  0,  255};
CRGBPalette16 lowPal = lava_gp;

// Gradient palette "poison_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/neota/elem/tn/poison.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 52 bytes of program space.

DEFINE_GRADIENT_PALETTE( poison_gp ) {
    0,   0,  0,  0,
   17,   1,  1,  1,
   34,   7,  1,  1,
   55,   7,  5, 15,
   76,   7, 13, 64,
  106,  12, 40, 68,
  137,  17, 85, 73,
  152,  30,107, 61,
  167,  47,131, 51,
  184,  87,144, 69,
  202, 142,156, 92,
  228, 194,203,160,
  255, 255,255,255};
CRGBPalette16 midPal = poison_gp;

// Gradient palette "icy_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/neota/elem/tn/icy.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.

DEFINE_GRADIENT_PALETTE( icy_gp ) {
    0,   1,  1,  1,
   35,   1,  1, 10,
   71,   0,  1, 53,
  124,  14, 26,119,
  177,  90,101,216,
  216, 159,169,235,
  255, 255,255,255};
CRGBPalette16 highPal = icy_gp;

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
  
  Serial.begin(9600);
  HWSERIAL.begin(57600);
  
  calculateBinValues();
  AudioMemory(12);
  
  FFT.windowFunction(AudioWindowHanning1024);
  
  FastLED.addLeds<WS2812SERIAL, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness( BRIGHTNESS );
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000);
}

void handleSerial() {
  String incomingString = HWSERIAL.readString();
  if(incomingString == "RS"){
    strobeCount = 0;
  }
  if(incomingString == "TEST"){
     digitalWrite(13, HIGH);
     delay(1000);
     digitalWrite(13, LOW);
  }
  Serial.print("I received: ");
  Serial.println(incomingString);
}

void handleRMS() {
  float currRms = RMS.read();
  rms = currRms;
  rmsCount++;

  float differential = (currRms - avgRms) / rmsCount;
  avgRms = avgRms + differential;

  float baseBrightness = BRIGHTNESS * (1 - rmsBrightnessPct);
  float rmsBrightness = avgRms * (BRIGHTNESS * rmsBrightnessPct);
  float newBrightness = baseBrightness + rmsBrightness;
  FastLED.setBrightness(newBrightness * 2);
}

void loop() {
  if (HWSERIAL.available() > 0) {
    handleSerial();
  }

  // RMS
  if(RMS.available()){
    handleRMS();
  }
  if((rmsCount % 20) == 0 && rmsCount > 0){
     rmsCount--;
  }

  // FFT Calculations & Moving Averages
  float low = 0;
  float mid = 0;
  float high = 0;
  float differential = 0;
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
    mid = (mid / midBinCount) * 2;
    high = (high / highBinCount) * 16;
    lowCount++;
    differential = (low - lowAvg) / lowCount;
    lowAvg = lowAvg + differential;
    midCount++;
    differential = (mid - midAvg) / midCount;
    midAvg = midAvg + differential;
    highCount++;
    differential = (high - highAvg) / highCount;
    highAvg = highAvg + differential;
  }
  if((lowCount % 200) == 0 && lowCount > 0){
     lowCount--;
  }
  if((midCount % 200) == 0 && midCount > 0){
     midCount--;
  }
  if((highCount % 200) == 0 && highCount > 0){
     highCount--;
  }

  // Beat Detection
  bool beat = false;
  bool strobe = false;
  int currMillis = millis();
  if(
    low > (lowAvg * BEAT_SENSITIVITY) &&
    ((currMillis - lastBeat) > 100) &&
    (lowAvg > (midAvg + highAvg) / 2) 
  ){
    beat = true;
    if(strobeCount < 10){
      strobe = true;
    }
    lastBeat = currMillis;
    strobeCount++;
  }
  if((currMillis - lastBeat) > 10000 && strobeCount != 0){
    strobeCount = 0;
  }

  float colorPosition;
  float six = 255 / 16;
  colorPosition = low * 255;

  CRGB col;
  if( low > mid && low > high){
    col = ColorFromPalette(lowPal, low * 255);
  }
  if( mid > low && mid > high){
    col = ColorFromPalette(midPal, mid * 255);
  }
  if( high > low && high > mid){
    col = ColorFromPalette(highPal, high * 255);
  }
  
  Serial.print(lowAvg*100);
  Serial.print(" ");
  Serial.print(midAvg*100);
  Serial.print(" ");
  Serial.print(highAvg*100);
  Serial.print(" ");
  Serial.println();
  
  // LED Animation
  CRGB prev = leds[0];
  leds[0] = col;
  //leds[0] = CRGB(low * 255, mid * 255,  high * 255);
  if(strobe){
    leds[0] = CRGB::Grey;
  }
  for(int i = 0 ; i < NUM_LEDS; i++){
    CRGB curr = leds[i];
    leds[i] = prev;
    prev = curr;
  }
  
  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);    
}
