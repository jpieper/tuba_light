// LED Audio Spectrum Analyzer Display
// 
// Creates an impressive LED light show to music input
//   using Teensy 3.1 with the OctoWS2811 adaptor board
//   http://www.pjrc.com/store/teensy31.html
//   http://www.pjrc.com/store/octo28_adaptor.html
//
// Line Level Audio Input connects to analog pin A3
//   Recommended input circuit:
//   http://www.pjrc.com/teensy/gui/?info=AudioInputAnalog
//
// This example code is in the public domain.


#include <OctoWS2811.h>
#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>

// The display size and color to use
const unsigned int matrix_width = 60;
const unsigned int matrix_height = 5;
const unsigned int myColor = 0x400020;
const float minAutoGain = 0.05;

// These parameters adjust the vertical thresholds
const float maxLevel = 0.4;      // 1.0 = max, lower is more "sensitive"
const float dynamicRange = 40.0; // total range to display, in decibels
const float linearBlend = 0.1;   // useful range is 0 to 0.7

// OctoWS2811 objects
const int ledsPerPin = 300;
DMAMEM int displayMemory[ledsPerPin*6];
int drawingMemory[ledsPerPin*6];
const int config = WS2811_GRB | WS2811_800kHz;
OctoWS2811 leds(ledsPerPin, displayMemory, drawingMemory, config);

// Audio library objects
AudioInputI2S            adc1;       //xy=99,55
AudioAnalyzeFFT1024      fft;            //xy=265,75
AudioConnection          patchCord1(adc1, 0, fft, 0);
AudioControlSGTL5000     sgtl5000;


// This array holds the volume level (0 to 1.0) for each
// vertical pixel to turn on.  Computed in setup() using
// the 3 parameters above.
float thresholdVertical[matrix_height];

float autoGainLevel[matrix_width];
float hsvOffset = 0.0f;

// This array specifies how many of the FFT frequency bin
// to use for each horizontal pixel.  Because humans hear
// in octaves and FFT bins are linear, the low frequencies
// use a small number of bins, higher frequencies use more.
//
// UPDATED BY JOSH FOR TUBA... still not great since we're 
// only doing windows of 512, so the lowest freq it can identify 
// is like 160Hz 
int frequencyBinsHorizontal[matrix_width] = {
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
   2,  2,  2,  2,  2,  2,  2,  2,  2,  3,
  25, 26, 27, 28, 29, 30, 32, 33, 34, 150
};

float binMinimum[matrix_width] = {
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0.02,  0.05,  0.02,  0.02,  0.05,  0.02,  0,  0,  0.02,  0.05,
};



// Run setup once
void setup() {
  // the audio library needs to be given memory to start working
  AudioMemory(12);

  sgtl5000.enable();
  sgtl5000.headphoneSelect(AUDIO_HEADPHONE_LINEIN);
  sgtl5000.inputSelect(AUDIO_INPUT_LINEIN);
  sgtl5000.lineInLevel(15);
  sgtl5000.volume(1.0f);

  for (int i = 0; i < matrix_width; i++) {
    autoGainLevel[i] = minAutoGain;
  }

  // compute the vertical thresholds before starting
  computeVerticalLevels();

  // turn on the display
  leds.begin();
  leds.show();
}

// A simple xy() function to turn display matrix coordinates
// into the index numbers OctoWS2811 requires.  If your LEDs
// are arranged differently, edit this code...
unsigned int xy(unsigned int x, unsigned int y) {
  return x * matrix_height + y;
}

// Run repetitively
void loop() {
  unsigned int x, y, freqBin;
  float level;

  if (fft.available()) {
    hsvOffset += 0.001;
    if (hsvOffset >= 1.0) { hsvOffset -= 1.0; }

    
    // freqBin counts which FFT frequency data has been used,
    // starting at low frequency
    freqBin = 0;

    for (x=0; x < matrix_width; x++) {
      float hue = hsvOffset + (float)x / (float) (matrix_width - 1);
      if (hue > 1.0) { hue -= 1.0; }

      // From: https://www.rapidtables.com/convert/color/hsv-to-rgb.html
      const float c = 1.0;
      const float X = (1 - fabs(fmod(hue / 0.1666667, 2.0) - 1));
      const float m = 0;
      float red = 0;
      float green = 0;
      float blue = 0;
      if (hue < 0.166667) {
        red = 1.0;
        green = X;
        blue = 0;
      } else if (hue < 0.33333) {
        red = X;
        blue = 1.0;
        green = 0.0;
      } else if (hue < 0.5) {
        red = 0.0;
        green = 1.0;
        blue = X;
      } else if (hue < 0.666667) {
        red = 0;
        green = X;
        blue = 1.0;
      } else if (hue < 0.833333) {
        red = X;
        green = 0;
        blue = 1.0;
      } else {
        red = 1.0;
        green = 0;
        blue = X;
      }
      
      
      // get the volume for each horizontal pixel position
      level = fft.read(freqBin, freqBin + frequencyBinsHorizontal[x] - 1);

      if (level > autoGainLevel[x]) {
        autoGainLevel[x] = level;
      } else {
        autoGainLevel[x] *= 0.997;
        if (autoGainLevel[x] < minAutoGain) {
          autoGainLevel[x] = minAutoGain;
        }
      }

      level = max(0, level - binMinimum[x]);

      level = level / autoGainLevel[x];

      // uncomment to see the spectrum in Arduino's Serial Monitor
      Serial.print(level);
      Serial.print("  ");

      for (y=0; y < matrix_height; y++) {
        // for each vertical pixel, check if above the threshold
        // and turn the LED on or off
        if (level >= thresholdVertical[y]) {
          float delta = level - thresholdVertical[y];
          float bottom = thresholdVertical[y];
          float top = (y == 0) ? 1.0f : thresholdVertical[y - 1];
          float ratio = min(1.0, delta / (top - bottom));
          
          leds.setPixel(xy(x, y), 
                        static_cast<uint8_t>(max(0, min(255, ratio * red * 255))),
                        static_cast<uint8_t>(max(0, min(255, ratio * green * 255))),
                        static_cast<uint8_t>(max(0, min(255, ratio * blue * 255))));
        } else {
          leds.setPixel(xy(x, y), 0x000000);
        }
      }
      // increment the frequency bin count, so we display
      // low to higher frequency from left to right
      freqBin = freqBin + frequencyBinsHorizontal[x];
    }
    // after all pixels set, show them all at the same instant
    leds.show();
    Serial.println();
  }
}


// Run once from setup, the compute the vertical levels
void computeVerticalLevels() {
  unsigned int y;
  float n, logLevel, linearLevel;

  for (y=0; y < matrix_height; y++) {
    n = (float)y / (float)(matrix_height - 1);
    logLevel = pow10f(n * -1.0 * (dynamicRange / 20.0));
    linearLevel = 1.0 - n;
    linearLevel = linearLevel * linearBlend;
    logLevel = logLevel * (1.0 - linearBlend);
    thresholdVertical[y] = (logLevel + linearLevel) * maxLevel + 0.10;
  }
}
