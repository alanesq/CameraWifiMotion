 /*******************************************************************************************************************
 *            
 *                                 ESP32Cam development board demo sketch using Arduino IDE
 *                                 
 *                                 demo using greyscale data 
 * 
 * 
 *******************************************************************************************************************/

 #if !defined ESP32
  #error This sketch is only for an ESP32Cam module
#endif

#include "esp_camera.h"       // https://github.com/espressif/esp32-camera
// #include "camera_pins.h"


// ******************************************************************************************************************




//   ---------------------------------------------------------------------------------------------------------

//                                      Wifi Settings

#include <wifiSettings.h>       // delete this line, un-comment the below two lines and enter your wifi details

//const char *SSID = "your_wifi_ssid";

//const char *PWD = "your_wifi_pwd";


//   ---------------------------------------------------------------------------------------------------------




// ---------------------------------------------------------------
//                           -SETTINGS
// ---------------------------------------------------------------

  const char* stitle = "ESP32Cam-demo-gs";               // title of this sketch
  const char* sversion = "07Jun21";                      // Sketch version

  const bool serialDebug = 1;                            // show info. on serial port (1=enabled, disable if using pins 1 and 3 as gpio)

  #define useMCP23017 0                                  // if MCP23017 IO expander chip is being used (on pins 12 and 13)

  // Camera related
    const bool flashRequired = 1;                        // If flash to be used when capturing image (1 = yes)
    const framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_QQVGA;// Image resolution:   
                                                         //               default = "const framesize_t FRAME_SIZE_IMAGE = FRAMESIZE_VGA"
                                                         //               160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 
                                                         //               320x240 (QVGA), 400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 
                                                         //               1024x768 (XGA), 1280x1024 (SXGA), 1600x1200 (UXGA)
    #define PIXFORMAT PIXFORMAT_GRAYSCALE;               // image format, Options =  YUV422, GRAYSCALE, RGB565, JPEG, RGB888                                                         
    #define WIDTH 160                                    // image size
    #define HEIGHT 120

    int cameraImageExposure = 0;                         // Camera exposure (0 - 1200)   If gain and exposure both set to zero then auto adjust is enabled
    int cameraImageGain = 0;                             // Image gain (0 - 30)

  const int TimeBetweenStatus = 600;                     // speed of flashing system running ok status light (milliseconds)

  const int indicatorLED = 33;                           // onboard small LED pin (33)

  const int brightLED = 4;                               // onboard Illumination/flash LED pin (4)

  const int iopinA = 13;                                 // general io pin 13
  const int iopinB = 12;                                 // general io pin 12 (must not be high at boot)
  const int iopinC = 16;                                 // input only pin 16 (used by PSRam but you may get away with using it for a button)
  
  const int serialSpeed = 115200;                        // Serial data speed to use

    
// camera settings (for the standard - OV2640 - CAMERA_MODEL_AI_THINKER)
// see: https://randomnerdtutorials.com/esp32-cam-camera-pin-gpios/
// set camera resolution etc. in 'initialiseCamera()' and 'cameraImageSettings()'
  #define CAMERA_MODEL_AI_THINKER
  #define PWDN_GPIO_NUM     32      // power to camera (on/off)
  #define RESET_GPIO_NUM    -1      // -1 = not used
  #define XCLK_GPIO_NUM      0
  #define SIOD_GPIO_NUM     26      // i2c sda
  #define SIOC_GPIO_NUM     27      // i2c scl
  #define Y9_GPIO_NUM       35
  #define Y8_GPIO_NUM       34
  #define Y7_GPIO_NUM       39
  #define Y6_GPIO_NUM       36
  #define Y5_GPIO_NUM       21
  #define Y4_GPIO_NUM       19
  #define Y3_GPIO_NUM       18
  #define Y2_GPIO_NUM        5
  #define VSYNC_GPIO_NUM    25      // vsync_pin
  #define HREF_GPIO_NUM     23      // href_pin
  #define PCLK_GPIO_NUM     22      // pixel_clock_pin


  
// ******************************************************************************************************************


#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>       // used by requestWebPage()
#include "driver/ledc.h"      // used to configure pwm on illumination led


WebServer server(80);                       // serve web pages on port 80

// Used to disable brownout detection 
  #include "soc/soc.h"                       
  #include "soc/rtc_cntl_reg.h"      

// sd-card
  #include "SD_MMC.h"                         // sd card - see https://randomnerdtutorials.com/esp32-cam-take-photo-save-microsd-card/
  #include <SPI.h>                       
  #include <FS.h>                             // gives file access 
  #define SD_CS 5                             // sd chip select pin = 5

// MCP23017 IO expander on pins 12 and 13 (optional)
  #if useMCP23017 == 1
    #include <Wire.h>
    #include "Adafruit_MCP23017.h"
    Adafruit_MCP23017 mcp;
    // Wire.setClock(1700000); // set frequency to 1.7mhz
  #endif
  
// Define some global variables:
  uint32_t lastStatus = millis();           // last time status light changed status (to flash all ok led)
  uint32_t lastCamera = millis();           // timer for periodic image capture
  bool sdcardPresent;                       // flag if an sd card is detected
  int imageCounter;                         // image file name on sd card counter
  uint32_t illuminationLEDstatus;           // current brightness setting of the illumination led
 
  
  
// ******************************************************************************************************************


// ---------------------------------------------------------------
//    -SETUP     SETUP     SETUP     SETUP     SETUP     SETUP
// ---------------------------------------------------------------

void setup() {
  
  if (serialDebug) {
    Serial.begin(serialSpeed);                     // Start serial communication 
  
    Serial.println("\n\n\n");                      // line feeds
    Serial.println("-----------------------------------");
    Serial.printf("Starting - %s - %s \n", stitle, sversion);  
    Serial.println("-----------------------------------");
    // Serial.print("Reset reason: " + ESP.getResetReason());
  }

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);     // Turn-off the 'brownout detector'

  // small indicator led on rear of esp32cam board
    pinMode(indicatorLED, OUTPUT);
    digitalWrite(indicatorLED,HIGH);

  // Connect to wifi
    digitalWrite(indicatorLED,LOW);               // small indicator led on
    if (serialDebug) {
      Serial.print("\nConnecting to ");
      Serial.print(SSID);
      Serial.print("\n   ");
    }
    WiFi.begin(SSID, PWD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        if (serialDebug) Serial.print(".");
    }
    if (serialDebug) {
      Serial.print("\nWiFi connected, ");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
    server.begin();                               // start web server
    digitalWrite(indicatorLED,HIGH);              // small indicator led off

  // define the web pages (i.e. call these procedures when url is requested)
    server.on("/", capture_still);                // demo converting image to RGB


  // set up camera
      if (serialDebug) Serial.print(("\nInitialising camera: "));
      if (initialiseCamera()) {
        if (serialDebug) Serial.println("OK");
      }
      else {
        if (serialDebug) Serial.println("Error!");
      }
   
  // define i/o pins 
    pinMode(indicatorLED, OUTPUT);            // defined again as sd card config can reset it
    digitalWrite(indicatorLED,HIGH);          // led off = High
    pinMode(iopinA, OUTPUT);                  // pin 13 - free io pin, can be used for input or output
    pinMode(iopinB, OUTPUT);                  // pin 12 - free io pin, can be used for input or output (must not be high at boot)
    pinMode(iopinC, INPUT);                   // pin 16 - free input only pin

  // MCP23017 io expander (requires adafruit MCP23017 library)
  #if useMCP23017 == 1
    Wire.begin(12,13);             // use pins 12 and 13 for i2c
    mcp.begin(&Wire);              // use default address 0
    mcp.pinMode(0, OUTPUT);        // Define GPA0 (physical pin 21) as output pin
    mcp.pinMode(8, INPUT);         // Define GPB0 (physical pin 1) as input pin
    mcp.pullUp(8, HIGH);           // turn on a 100K pullup internally
    // change pin state with   mcp.digitalWrite(0, HIGH);
    // read pin state with     mcp.digitalRead(8)
  #endif

  // startup complete
    if (serialDebug) Serial.println("\nSetup complete...");

}  // setup


// ******************************************************************************************************************


// ----------------------------------------------------------------
//   -LOOP     LOOP     LOOP     LOOP     LOOP     LOOP     LOOP
// ----------------------------------------------------------------


void loop() {

  server.handleClient();          // handle any incoming web page requests





  
  //                           <<< YOUR CODE HERE >>>






//  //  demo to Capture an image and save to sd card every 5 seconds (i.e. time lapse)
//      if ( ((unsigned long)(millis() - lastCamera) >= 5000) && sdcardPresent ) { 
//        lastCamera = millis();     // reset timer
//        storeImage();              // save an image to sd card
//      }
 
  // flash status LED to show sketch is running ok
    if ((unsigned long)(millis() - lastStatus) >= TimeBetweenStatus) { 
      lastStatus = millis();                                               // reset timer
      digitalWrite(indicatorLED,!digitalRead(indicatorLED));               // flip indicator led status
    }
    
}  // loop



// ******************************************************************************************************************


// ----------------------------------------------------------------
//                        Initialise the camera
// ----------------------------------------------------------------
// returns TRUE if successful

bool initialiseCamera() {
  
    camera_config_t config;
  
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;               // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    config.pixel_format = PIXFORMAT;              // Options =  YUV422, GRAYSCALE, RGB565, JPEG, RGB888
    config.frame_size = FRAME_SIZE_IMAGE;         // Image sizes: 160x120 (QQVGA), 128x160 (QQVGA2), 176x144 (QCIF), 240x176 (HQVGA), 320x240 (QVGA), 
                                                  //              400x296 (CIF), 640x480 (VGA, default), 800x600 (SVGA), 1024x768 (XGA), 1280x1024 (SXGA), 
                                                  //              1600x1200 (UXGA)
    config.jpeg_quality = 10;                     // 0-63 lower number means higher quality
    config.fb_count = 1;                          // if more than one, i2s runs in continuous mode. Use only with JPEG

    // check the esp32cam board has a psram chip installed (extra memory used for storing captured images)
    //    Note: if not using "AI thinker esp32 cam" in the Arduino IDE, PSRAM must be enabled
    if (!psramFound()) {
      if (serialDebug) Serial.println("Warning: No PSRam found so defaulting to image size 'CIF'");
      config.frame_size = FRAMESIZE_CIF;
    }
  
    //#if defined(CAMERA_MODEL_ESP_EYE)
    //  pinMode(13, INPUT_PULLUP);
    //  pinMode(14, INPUT_PULLUP);
    //#endif  
  
    esp_err_t camerr = esp_camera_init(&config);  // initialise the camera
    if (camerr != ESP_OK) {
      if (serialDebug) Serial.printf("ERROR: Camera init failed with error 0x%x", camerr);
    }

    cameraImageSettings();                        // apply custom camera settings  
    
    return (camerr == ESP_OK);                    // return boolean result of camera initialisation
}


// ******************************************************************************************************************


// ----------------------------------------------------------------
//                   -Change camera image settings
// ----------------------------------------------------------------
// Adjust image properties (brightness etc.)
// Defaults to auto adjustments if exposure and gain are both set to zero
// - Returns TRUE if successful
// BTW - some interesting info on exposure times here: https://github.com/raduprv/esp32-cam_ov2640-timelapse

bool cameraImageSettings() { 
   
    sensor_t *s = esp_camera_sensor_get();    
    // something to try?:     if (s->id.PID == OV3660_PID) 
    if (s == NULL) {
      if (serialDebug) Serial.println("Error: problem reading camera sensor settings");
      return 0;
    } 

    // if both set to zero enable auto adjust
    if (cameraImageExposure == 0 && cameraImageGain == 0) {              
      // enable auto adjust
        s->set_gain_ctrl(s, 1);                       // auto gain on 
        s->set_exposure_ctrl(s, 1);                   // auto exposure on 
        s->set_awb_gain(s, 1);                        // Auto White Balance enable (0 or 1)
    } else {
      // Apply manual settings
        s->set_gain_ctrl(s, 0);                       // auto gain off 
        s->set_awb_gain(s, 1);                        // Auto White Balance enable (0 or 1)
        s->set_exposure_ctrl(s, 0);                   // auto exposure off 
        s->set_agc_gain(s, cameraImageGain);          // set gain manually (0 - 30)
        s->set_aec_value(s, cameraImageExposure);     // set exposure manually  (0-1200)
    }

    return 1;
}  // cameraImageSettings


//    // More camera settings available:
//    // If you enable gain_ctrl or exposure_ctrl it will prevent a lot of the other settings having any effect
//    // more info on settings here: https://randomnerdtutorials.com/esp32-cam-ov2640-camera-settings/
//    s->set_gain_ctrl(s, 0);                       // auto gain off (1 or 0)
//    s->set_exposure_ctrl(s, 0);                   // auto exposure off (1 or 0)
//    s->set_agc_gain(s, cameraImageGain);          // set gain manually (0 - 30)
//    s->set_aec_value(s, cameraImageExposure);     // set exposure manually  (0-1200)
//    s->set_vflip(s, cameraImageInvert);           // Invert image (0 or 1)     
//    s->set_quality(s, 10);                        // (0 - 63)
//    s->set_gainceiling(s, GAINCEILING_32X);       // Image gain (GAINCEILING_x2, x4, x8, x16, x32, x64 or x128) 
//    s->set_brightness(s, cameraImageBrightness);  // (-2 to 2) - set brightness
//    s->set_lenc(s, 1);                            // lens correction? (1 or 0)
//    s->set_saturation(s, 0);                      // (-2 to 2)
//    s->set_contrast(s, cameraImageContrast);      // (-2 to 2)
//    s->set_sharpness(s, 0);                       // (-2 to 2)  
//    s->set_hmirror(s, 0);                         // (0 or 1) flip horizontally
//    s->set_colorbar(s, 0);                        // (0 or 1) - show a testcard
//    s->set_special_effect(s, 0);                  // (0 to 6?) apply special effect
//    s->set_whitebal(s, 0);                        // white balance enable (0 or 1)
//    s->set_awb_gain(s, 1);                        // Auto White Balance enable (0 or 1) 
//    s->set_wb_mode(s, 0);                         // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
//    s->set_dcw(s, 0);                             // downsize enable? (1 or 0)?
//    s->set_raw_gma(s, 1);                         // (1 or 0)
//    s->set_aec2(s, 0);                            // automatic exposure sensor?  (0 or 1)
//    s->set_ae_level(s, 0);                        // auto exposure levels (-2 to 2)
//    s->set_bpc(s, 0);                             // black pixel correction
//    s->set_wpc(s, 0);                             // white pixel correction



// ----------------------------------------------------------------
//      -access image as greyscale data - i.e. http://x.x.x.x/
// ----------------------------------------------------------------

bool capture_still() {

  WiFiClient client = server.client();                                                                   // open link with client

  // log page request including clients IP address
    if (serialDebug) {
      IPAddress cip = client.remoteIP();
      Serial.printf("Greyscale data requested from: %d.%d.%d.%d \n", cip[0], cip[1], cip[2], cip[3]);
    }

client.write("<!DOCTYPE html> <html lang='en'> <head> <title>photo</title> </head> <body>\n");          // basic html header
client.write("<br>Greyscale data<br>");
      
  
  camera_fb_t *frame = esp_camera_fb_get();

  if (!frame)
    return false;

  byte pixel;

  // for each pixel in image
  //    only shows forst 3 lines as otherwise there is an awful lot of data
  //        to show all use the line:    for (size_t i = 0; i < frame->len; i++) {
  for (size_t i = 0; i < (160 * 3); i++) {
    const uint16_t x = i % WIDTH;                   // x position in image
    const uint16_t y = floor(i / WIDTH);            // y position in image
    pixel = frame->buf[i];                          // pixel value

    // show data
      if (x==0) client.println("<br>");             // new line
      client.print(String(pixel));                  // print byte as a string
      client.print(", ");

  } 

  esp_camera_fb_return(frame);                      // return storage space

  // end html
    client.print("<br>Finished");
    client.print("</body></html>\n");
    delay(3);
    client.stop();       

  return true;
}


// ******************************************************************************************************************
// end
