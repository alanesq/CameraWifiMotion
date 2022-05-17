/**************************************************************************************************
 *
 *      Neopixels - Uses FastLED library - 17May22
 *
 *      part of the BasicWebserver sketch - https://github.com/alanesq/BasicWebserver
 *
 **************************************************************************************************

  Notes:

        On esp8266 when using wifi the first led can flash/behave strangely, see https://github.com/FastLED/FastLED/issues/1260
           see also: https://github.com/FastLED/FastLED/wiki/Interrupt-problems

           See: http://fastled.io/docs/3.1/
                https://github.com/FastLED/FastLED/wiki/Overview    http://fastled.io/    https://github.com/FastLED/FastLED
        videos: https://www.youtube.com/watch?v=4Ut4UK7612M&t=8s
        Turn led on with:   FastLED.clear();   g_LEDs[0] = CRGB::Green;   FastLED.show();

*/


//            --------------------------- Settings -------------------------------


  #define NUM_NEOPIXELS   15        // Number of LEDs in the string

  #define NEOPIXEL_PIN    5         // Neopixel data gpio pin (Note: should have 330k resistor ideally) (5 = D1 on esp8266)

  int g_PowerLimit =      900;      // power limit in milliamps for Neopixels (USB usually good for 800)

  uint8_t g_Brightness =  32;       // LED maximum brightness scale (1-255)


//            --------------------------------------------------------------------


//#define FASTLED_INTERRUPT_RETRY_COUNT 1       // attempt to stop the first led blinking when using esp8266
#define FASTLED_INTERNAL                      // Suppress build banner
#include <FastLED.h>                          // https://github.com/FastLED/FastLED
#include <sys/time.h>                         // For time-of-day
CRGB g_LEDs[NUM_NEOPIXELS] = {0};             // Frame buffer for FastLED


// forward declarations
  CRGB ColorFraction(CRGB, float);
  void smoothDraw(CRGB*, float, float, CRGB);
  void FireEffect(CRGB*, int, int, int, int, int , bool, bool);
  void DrawComet(CRGB*);
  void DrawMarquee(CRGB*);


// ----------------------------------------------------------------
//                              -setup
// ----------------------------------------------------------------
// Called from the main SETUP

 void neopixelSetup() {

    if (serialDebug) Serial.println("Initialising neopixels");
    pinMode(NEOPIXEL_PIN, OUTPUT);
    FastLED.addLeds<WS2812B, NEOPIXEL_PIN, GRB>(g_LEDs, NUM_NEOPIXELS);   // Add the LED strip to the FastLED library
    FastLED.setBrightness(g_Brightness);
    //set_max_power_indicator_LED(2);                                       // Light the builtin LED if we power throttle
    FastLED.setMaxPowerInMilliWatts(g_PowerLimit);                        // Set the power limit, above which brightness will be throttled
    FastLED.clear(true); 
    
 }


// ----------------------------------------------------------------
//                            -test area
// ----------------------------------------------------------------
// Call from the main loop

void neoLoop() {

  if (_TEMPVARIABLE_ == 1) {
    // smoothdraw demo
    static float posG = 0.0f;
    EVERY_N_MILLISECONDS(80) {
      FastLED.clear(false);
        g_LEDs[ beatsin16(4, 0, NUM_NEOPIXELS - 1) ] = CRGB::Red;
        g_LEDs[ beatsin16(6, 0, NUM_NEOPIXELS - 1) ] = CRGB::Blue;
        posG+=0.03f; if (posG > NUM_NEOPIXELS - 1) posG = 0.0f;     // slowly move green dot using smoothDraw
        smoothDraw(&g_LEDs[0], posG, 1, CRGB::Green);
      FastLED.show(g_Brightness);
    }
  }

  if (_TEMPVARIABLE_ == 2) {
    // marquee demo
      EVERY_N_MILLISECONDS(100) {
        FastLED.clear(false);
        DrawMarquee(&g_LEDs[0]);
        FastLED.show(g_Brightness);
      }
  }

  if (_TEMPVARIABLE_ == 3) {
    // comet demo
      EVERY_N_MILLISECONDS(60) {
        DrawComet(&g_LEDs[0]);
        FastLED.show(g_Brightness);
      }
  }

}


// =================================================================================
//                                 SAMPLE CODE BELOW HERE
// =================================================================================


// ----------------------------------------------------------------
//                         -smooth animation
// ----------------------------------------------------------------
// draw pixels on the led strip using smooth-animation
// as created by Dave Plummer https://github.com/davepl/DavesGarageLEDSeries
// @param: led array pointer, pixel location as a float allowing fractional led position, number of leds, colour to use
// example usage:    smoothDraw(&g_LEDs[0], 1.5f, 2, CRGB::Green);

void smoothDraw(CRGB* LEDarray, float fPos, float count, CRGB color) {
  // Calculate how much the first pixel will hold
  float availFirstPixel = 1.0f - (fPos - (long)(fPos));
  float amtFirstPixel = min(availFirstPixel, count);
  float remaining = min(count, NUM_NEOPIXELS-fPos);
  int iPos = fPos;

  // Blend (add) in the color of the first partial pixel
  if (remaining > 0.0f) {
    LEDarray[iPos++] += ColorFraction(color, amtFirstPixel);
    remaining -= amtFirstPixel;
  }

  // Now draw any full pixels in the middle
  while (remaining > 1.0f) {
    LEDarray[iPos++] += color;
    remaining--;
  }

  // Draw tail pixel, up to a single full pixel
  if (remaining > 0.0f) {
    LEDarray[iPos] += ColorFraction(color, remaining);
  }
}


// return a dimmed fraction of the suplied colour
CRGB ColorFraction(CRGB colorIn, float fraction) {
  fraction = min(1.0f, fraction);
  return CRGB(colorIn).fadeToBlackBy(255 * (1.0f - fraction));
}


// ----------------------------------------------------------------
//                            -marquee effect
// ----------------------------------------------------------------
// as created by Dave Plummer

void DrawMarquee(CRGB* LEDarray)
{
    static byte j = 0;
    j+=4;
    byte k = j;

    // Roughly equivalent to fill_rainbow(g_LEDs, NUM_LEDS, j, 8);

    CRGB c;
    for (int i = 0; i < NUM_NEOPIXELS; i ++)
        LEDarray[i] = c.setHue(k+=8);

    static int scroll = 0;
    scroll++;

    for (int i = scroll % 5; i < NUM_NEOPIXELS - 1; i += 5)
    {
        LEDarray[i] = CRGB::Black;
    }
}

void DrawMarqueeMirrored(CRGB* LEDarray)
{
    static byte j = 0;
    j+=4;
    byte k = j;

    // Roughly equivalent to fill_rainbow(g_LEDs, NUM_LEDS, j, 8);

    CRGB c;
    for (int i = 0; i < (NUM_NEOPIXELS + 1) / 2; i ++)
    {
        LEDarray[i] = c.setHue(k);
        LEDarray[NUM_NEOPIXELS - 1 - i] = c.setHue(k);
        k+= 8;
    }


    static int scroll = 0;
    scroll++;

    for (int i = scroll % 5; i < NUM_NEOPIXELS / 2; i += 5)
    {
        LEDarray[i] = CRGB::Black;
        LEDarray[NUM_NEOPIXELS - 1 - i] = CRGB::Black;
    }
}


// ----------------------------------------------------------------
//                            -comet effect
// ----------------------------------------------------------------
// as created by Dave Plummer

void DrawComet(CRGB* LEDarray)
{
    const byte fadeAmt = 128;
    const int cometSize = 4;
    const int deltaHue  = 3;

    static byte hue = HUE_RED;
    static int iDirection = 1;
    static int iPos = 0;

    hue += deltaHue;

    iPos += iDirection;
    if (iPos == (NUM_NEOPIXELS - cometSize) || iPos == 0)
        iDirection *= -1;

    for (int i = 0; i < cometSize; i++)
        LEDarray[iPos + i].setHue(hue);

    // Randomly fade the LEDs
    for (int j = 0; j < NUM_NEOPIXELS; j++)
        if (random(10) > 5)
            LEDarray[j] = LEDarray[j].fadeToBlackBy(fadeAmt);
}


// ----------------------------------------------------------------
//                            -Oscillators
// ----------------------------------------------------------------
// generate oscillating pixels - demonstrates use of vector

class Oscillators {

  private:
    CRGB* oLEDarray;               // led data location
    int oMaxNum;                   // Maximum number of oscillators
    int oCounter = 0;              // Number of Oscillators running
    std::vector<int> oPosition;    // Oscillator position
    std::vector<bool> oDirection;  // Oscillator direction
    std::vector<CRGB> oColour;     // Oscillator colour

  public:
    Oscillators(CRGB* o_LEDarray, size_t o_maxNum) {
      this->oLEDarray = o_LEDarray;
      this->oMaxNum = o_maxNum;
      addone();
    }

    ~Oscillators() {
    }

    void addone() {
      if (oCounter == oMaxNum) return;
      oCounter ++;
      if (serialDebug) Serial.println("Adding one - " + String(oCounter));
      oColour.push_back(CHSV(random(255), 255, 255));
      (random(2) == 1) ? oDirection.push_back(0) : oDirection.push_back(1);
      oPosition.push_back(random(NUM_NEOPIXELS-2)+2);
    }

    void removeone() {
      if (oCounter == 0) return;
      oCounter --;
      if (serialDebug) Serial.println("Removing one - " + String(oCounter));
      oColour.pop_back();
      oDirection.pop_back();
      oPosition.pop_back();
    }

    void move() {
      for(int i=0; i<oPosition.size(); i++) {
        oDirection[i] ? oPosition[i]++ : oPosition[i]--;
        if (oPosition[i] == 0  || oPosition[i] == NUM_NEOPIXELS-1) oDirection[i]=!oDirection[i];  // reverse direction
      }
    }

    void show() {
      //for(int i : oPosition) {   ?
      for(int i=0; i<oPosition.size(); i++) {
        oLEDarray[oPosition[i]] = oColour[i];
      }
    }
};

//  // Oscillators test
//    static Oscillators otest(&g_LEDs[0], 5);
//    static repeatTimer timer1;
//    static repeatTimer timer2;
//    if (timer1.check(50)) {
//      FastLED.clear(false);
//      otest.move();
//      otest.show();
//      FastLED.show(g_Brightness);
//    }
//    if (timer2.check(2000)) {
//      if (random(2) == 1) otest.addone();
//      else otest.removeone();
//    }


 /****************************************** E N D *************************************************/
