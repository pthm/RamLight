   #include <WS2812Serial.h>
#define USE_WS2812SERIAL

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SerialFlash.h>
#include <FastLED.h>
#include <math.h>

#define DEBUG_FFT false
#define DEBUG_RMS false
#define DEBUG_BEAT false
#define DEBUG_FREQUENCY false

#define HWSERIAL Serial3

#define LED_PIN     5
#define COLOR_ORDER BRG
#define CHIPSET     WS2812SERIAL
#define NUM_LEDS    300

#define BRIGHTNESS  64
#define FRAMES_PER_SECOND 60

#define FFT_RES 43

#define LOW_PEAK 70
#define MID_PEAK 1400
#define HIGH_PEAK 10000

#define BEAT_SENSITIVITY 2

DEFINE_GRADIENT_PALETTE(traktor){
  0,   255,    0,   0,
128,     0,  255,   0,
255,     0,    0, 255 };
DEFINE_GRADIENT_PALETTE(calm_darya){
  0, 95, 44, 130,
  128, 73, 160, 157,
  255, 73, 160, 157,
};
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
DEFINE_GRADIENT_PALETTE( fireandice_gp ) {
    0,  80,  2,  1,
   51, 206, 15,  1,
  101, 242, 34,  1,
  153,  16, 67,128,
  204,   2, 21, 69,
  255,   1,  2,  4};
DEFINE_GRADIENT_PALETTE( praire_gp ) {
    0,  66, 49,  6,
  255, 255,156, 25};
DEFINE_GRADIENT_PALETTE( purplefly_gp ) {
    0,   0,  0,  0,
   63, 239,  0,122,
  191, 252,255, 78,
  255,   0,  0,  0};
DEFINE_GRADIENT_PALETTE( scoutie_gp ) {
    0, 255,156,  0,
  127,   0,195, 18,
  216,   1,  0, 39,
  255,   1,  0, 39};
DEFINE_GRADIENT_PALETTE( butterflyfairy_gp ) {
    0,  84, 18,  2,
   63, 163,100, 27,
  127,  46,154,149,
  191,   1, 24, 54,
  255,   1,  1,  1};
DEFINE_GRADIENT_PALETTE( grey_gp ) {
    0,   0,  0,  0,
  127,  42, 55, 45,
  255, 255,255,255};

CRGB leds[NUM_LEDS];
AudioInputAnalog       audioInput;
AudioAnalyzeFFT1024    FFT;
AudioAnalyzeRMS        RMS;
AudioAnalyzePeak       Peak;
AudioConnection        patchCord1(audioInput, FFT);
AudioConnection        patchCord2(audioInput, RMS);
AudioConnection        patchCord3(audioInput, Peak);

int lowBinCount;
int midBinCount;
int highBinCount;
int totalBinCount;

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

float peakRms = 0;

int lastBeat = 0;
int strobeCount = 0;

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

CRGBPalette16 pallete = grey_gp;
void setPallete(int number) {
  printNamedVal("Setting Pallete", number);
  switch (number) {
    case 1:
      pallete = traktor;
      break;
    case 2:
      pallete = calm_darya;
      break;
    case 3:
      pallete = poison_gp;
      break;
    case 4:
      pallete = fireandice_gp;
      break;
    case 5:
      pallete = praire_gp;
      break;
    case 6:
      pallete = purplefly_gp;
      break;
    case 7:
      pallete = scoutie_gp;
      break;
    case 8:
      pallete = butterflyfairy_gp;
      break;
    case 9:
      pallete = grey_gp;
      break;
    default:
      pallete = traktor;
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
  totalBinCount = lowBinCount + midBinCount + highBinCount;
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
  if(incomingString.substring(0,2) == "SP"){
    setPallete(incomingString.substring(2,3).toInt());
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
  FastLED.setBrightness(newBrightness / 2);
}

void handlePeakRMS() {
  float currPeakRms = Peak.read();
  peakRms = currPeakRms; 
}

void loop() {
  // Serial Commands (Bluetooth)
  if (HWSERIAL.available() > 0) {
    handleSerial();
  }
  
  // RMS (Brightness)
  if(RMS.available()){
    handleRMS();
  }
  if((rmsCount % 20) == 0 && rmsCount > 0){
     rmsCount--;
  }

  // Peak RMS
  if(Peak.available()){
    handlePeakRMS();
  }

  // FFT Calculations & Moving Averages
  float approxFreq = 0;
  float low = 0;
  float mid = 0;
  float high = 0;
  float differential = 0;
  int palleteIndex = 0;
  if (FFT.available()) {
    // FFT Loop
    for(int i = 0; i < 512; i++){
      float amp = FFT.read(i);
      if(i<= highHigh){
        approxFreq += (amp * (i * FFT_RES));
      }
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

    // Frequency Mixing Calculations
    approxFreq = (approxFreq / totalBinCount) - 4; // Subtract 4 to account for mic noise floor
    palleteIndex = (approxFreq / 11) * 255;

    // Frequency Band Mixing Calculations
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
    ((currMillis - lastBeat) > 200)
  ){
    beat = true;
    if(
      strobeCount < 20 &&
      (lowAvg > (midAvg + highAvg) / 2)
    ){
      strobe = true;
    }
    lastBeat = currMillis;
    strobeCount++;
  }
  if((currMillis - lastBeat) > 10000 && strobeCount != 0){
    HWSERIAL.println("Reset Strobe");
    strobeCount = 0;
  }
  
  // LED Animation
  CRGB prev = leds[150];
  leds[150] = ColorFromPalette( pallete, (approxFreq/15) * 255 );
  //leds[150] = CRGB(low * 255, mid * 255,  high * 255);
  if(strobe){
    leds[150] = CRGB(255,255,255);
  }
  for(int i = 150 ; i < NUM_LEDS; i++){
    CRGB curr = leds[i];
    leds[i] = prev;
    leds[150 - (i - 150)] = prev;
    prev = curr;
  }
  FastLED.show(); // display this frame
  delay(1000 / FRAMES_PER_SECOND);

  // Debug
  if(DEBUG_FREQUENCY){
    printVal(palleteIndex);
  }
  if(DEBUG_FFT){
    printVal(low * 255);
    printVal(lowAvg * 255);
    printVal(mid * 255);
    printVal(midAvg * 255);
    printVal(high * 255);
    printVal(highAvg * 255);
  }
  if(DEBUG_RMS){
    printVal(rms * 255);
    printVal(peakRms * 255);
  }
  if(DEBUG_BEAT){
    printVal(beat * 10);
    printVal(low * 100);
    printVal(lowAvg * 100);
    printVal(lowAvg * 100 * BEAT_SENSITIVITY);
  }
  
  //Serial.println();
}
