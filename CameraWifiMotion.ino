 /*******************************************************************************************************************
 *
 *             ESP32Camera with motion detection and web server -  using Arduino IDE 
 *             
 *             Serves a web page whilst detecting motion on a camera (uses ESP32Cam module)
 *             
 *             Included files: gmail-esp32.h, standard.h and wifi.h, motion.h
 *             Bult using Arduino IDE 1.8.10, esp32 boards v1.0.4
 *             
 *             Note: The flash can not be used if using an SD Card as they both use pin 4
 *             
 *             GPIO16 is used as an input pin for external sensors etc.
 *             
 *             IMPORTANT! - If you are getting weird problems (motion detection retriggering all the time, slow wifi
 *                          response times.....chances are there is a problem with the power to the board.
 *                          It needs a good 500ma supply and ideally a good size smoothing capacitor.
 *             
 *      First time the ESP starts it will create an access point "ESPConfig" which you need to connect to in order to enter your wifi details.  
 *             default password = "12345678"   (note-it may not work if anything other than 8 characters long for some reason?)
 *             see: https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password
 *
 *      Motion detection based on: https://eloquentarduino.github.io/2020/01/motion-detection-with-esp32-cam-only-arduino-version/
 *      
 *      camera troubleshooting: https://randomnerdtutorials.com/esp32-cam-troubleshooting-guide/
 *                  
 *                                                                                              www.alanesq.eu5.net
 *      
 ********************************************************************************************************************/



// ---------------------------------------------------------------
//                          -SETTINGS
// ---------------------------------------------------------------


  const String stitle = "CameraWifiMotion";              // title of this sketch

  const String sversion = "04Feb20";                     // version of this sketch

  int MaxSpiffsImages = 10;                              // number of images to store in camera (Spiffs)
  
  const int datarefresh = 4000;                          // Refresh rate of the updating data on web page (1000 = 1 second)
  String JavaRefreshTime = "500";                        // time delay when loading url in web pages (Javascript)
  
  const int LogNumber = 40;                              // number of entries to store in the system log

  const int ServerPort = 80;                             // ip port to serve web pages on

  const int Illumination_led = 4;                        // illumination LED pin

  const byte gioPin = 16;                                // I/O pin (for external sensor input)
  
  const int SystemCheckRate = 5000;                      // how often to do routine system checks (milliseconds)
  
  const boolean ledON = HIGH;                            // Status LED control 
  const boolean ledOFF = LOW;
  
  const int FrameRefreshTime = 200;                      // how often to check camera for motion (milliseconds)
  
  int TriggerLimitTime = 2;                              // min time between motion detection triggers (seconds)

  int EmailLimitTime = 60;                               // min time between email sends (seconds)

  bool UseFlash = 1;                                     // use flash when taking a picture
  

// ---------------------------------------------------------------


#include "soc/soc.h"                    // Disable brownout problems
#include "soc/rtc_cntl_reg.h"           // Disable brownout problems

// spiffs used to store images and settings
  #include <SPIFFS.h>
  #include <FS.h>                       // gives file access on spiffs
  int SpiffsFileCounter = 0;            // counter of last image stored

// sd card - see https://randomnerdtutorials.com/esp32-cam-take-photo-save-microsd-card/
  #include "SD_MMC.h"
  // #include <SPI.h>                   // (already loaded)
  // #include <FS.h>                    // gives file access on spiffs (already loaded)
  #define SD_CS 5                       // sd chip select pin
  bool SD_Present;                      // flag if sd card is found (0 = no)

#include "wifi.h"                       // Load the Wifi / NTP stuff

#include "standard.h"                   // Standard procedures

#include "gmail_esp32.h"                // send email via smtp

#include "motion.h"                     // motion detection / camera

// misc
  unsigned long CAMERAtimer = 0;             // used for timing camera motion refresh timing
  unsigned long TRIGGERtimer = 0;            // used for limiting camera motion trigger rate
  unsigned long EMAILtimer = 0;              // used for limiting rate emails can be sent
  byte DetectionEnabled = 1;                 // flag if capturing motion is enabled (0=stopped, 1=enabled, 2=paused)
  String TriggerTime = "Not yet triggered";  // Time of last motion trigger
  unsigned long MaintTiming = millis();      // used for timing maintenance tasks
  bool emailWhenTriggered = 0;               // flag if to send emails when motion detection triggers
  bool ReqLEDStatus = 0;                     // desired status of the illuminator led (i.e. should it be on or off when not being used as a flash)
  
  
// ---------------------------------------------------------------
//    -SETUP     SETUP     SETUP     SETUP     SETUP     SETUP
// ---------------------------------------------------------------
//
// setup section (runs once at startup)

void setup(void) {
    
  Serial.begin(115200);
  // Serial.setTimeout(2000);
  // while(!Serial) { }        // Wait for serial to initialize.

  Serial.println(F("\n\n\n---------------------------------------"));
  Serial.println("Starting - " + stitle + " - " + sversion);
  Serial.println(F("---------------------------------------"));
  
  // Serial.setDebugOutput(true);                                // enable extra diagnostic info  
   
  // configure the LED
    pinMode(Illumination_led, OUTPUT); 
    digitalWrite(Illumination_led, ledOFF); 

  BlinkLed(1);           // flash the led once

  // configure the I/O pin (with pullup resistor)
    pinMode(gioPin,  INPUT_PULLUP);                                
    
  startWifiManager();                                            // Connect to wifi (procedure is in wifi.h)
  
  WiFi.mode(WIFI_STA);     // turn off access point - options are WIFI_AP, WIFI_STA, WIFI_AP_STA or WIFI_OFF

  // set up web page request handling
    server.on("/", handleRoot);              // root page
    server.on("/data", handleData);          // This displays information which updates every few seconds (used by root web page)
    server.on("/ping", handlePing);          // ping requested
    server.on("/log", handleLogpage);        // system log
    server.on("/test", handleTest);          // testing page
    server.on("/reboot", handleReboot);      // reboot the esp
    server.on("/default", handleDefault);    // All settings to defaults
    server.on("/live", handleLive);          // capture and display live image
    server.on("/images", handleImages);      // display images
    server.on("/img", handleImg);            // latest captured image
    server.on("/bootlog", handleBootLog);    // Display boot log from Spiffs
    server.onNotFound(handleNotFound);       // invalid page requested
    
  // start web server
    Serial.println(F("Starting web server"));
    server.begin();

  // set up camera
    Serial.print(F("Initialising camera: "));
    Serial.println(setup_camera() ? "OK" : "ERR INIT");

  // Spiffs - see: https://circuits4you.com/2018/01/31/example-of-esp8266-flash-file-system-spiffs/
    if (!SPIFFS.begin(true)) {
      Serial.println("An Error has occurred while mounting SPIFFS");
      delay(5000);
      ESP.restart();
      delay(5000);
    } else {
      Serial.print("SPIFFS mounted successfully. ");
      Serial.print("total bytes: " + String(SPIFFS.totalBytes()));
      Serial.println(", used bytes: " + String(SPIFFS.usedBytes()));
      LoadSettingsSpiffs();     // Load settings from text file in Spiffs
    }

  // start sd card
      pinMode(Illumination_led, INPUT);            // disable led pin as sdcard uses it
      if(!SD_MMC.begin()){                         // if loading sd card fails     ("/sdcard", true = 1 wire?)
          log_system_message("SD Card not found"); 
          pinMode(Illumination_led, OUTPUT);       // re-enable led pin
          digitalWrite(Illumination_led, ledOFF); 
          SD_Present = 0;                          // flag no sd card found
      } else {
      uint8_t cardType = SD_MMC.cardType();
        if(cardType == CARD_NONE){                 // if no sd card found
            log_system_message("SD Card type detect failed"); 
            pinMode(Illumination_led, OUTPUT);     // re-enable led pin
            digitalWrite(Illumination_led, ledOFF); 
            SD_Present = 0;                        // flag no sd card found
        } else {
            log_system_message("SD Card found"); 
            SD_Present = 1;                        // flag working sd card found
        }
      }

  // Turn-off the 'brownout detector'
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // Finished connecting to network
    BlinkLed(2);      // flash the led twice
    log_system_message(stitle + " Started");   
    TRIGGERtimer = millis();                            // reset retrigger timer to stop instant movement trigger
    
}


// blink the led 
void BlinkLed(byte Bcount) {
  for (int i = 0; i < Bcount; i++) {                    // flash led
    digitalWrite(Illumination_led, ledON);
    delay(50);
    digitalWrite(Illumination_led, ledOFF);
    delay(300);
  }

UpdateBootlogSpiffs("Booted");                          // store time of boot in bootlog
}


// ----------------------------------------------------------------
//   -LOOP     LOOP     LOOP     LOOP     LOOP     LOOP     LOOP
// ----------------------------------------------------------------

void loop(void){

  server.handleClient();                         // service any web page requests 

  // camera motion detection 
  //        explanation of timing here: https://www.baldengineer.com/arduino-millis-plus-addition-does-not-add-up.html
  if (DetectionEnabled == 1) {    
    if ((unsigned long)(millis() - CAMERAtimer) >= FrameRefreshTime ) {                     // limit camera motion detection rate
        CAMERAtimer = millis();                                                             // reset timer
        if (!capture_still()) RebootCamera();                                               // capture image, if problem reboot camera and try again
        float changes = motion_detect();                                                    // find amount of change in current video frame      
        update_frame();                                                                     // Copy current frame to previous
        if (changes >= (float)(image_threshold / 100.0)) {                                  // if motion detected 
             if ((unsigned long)(millis() - TRIGGERtimer) >= (TriggerLimitTime * 1000) ) {  // limit time between detection triggers
                  TRIGGERtimer = millis();                                                  // reset last motion trigger time
                  MotionDetected(changes);                                                  // run motion detected procedure (passing change level)
             } 
         }
    }
  } else AveragePix = 0;        // clear brightness reading as frames are not being captured

  // periodically check Wifi is connected and refresh NTP time
    if ((unsigned long)(millis() - MaintTiming) >= SystemCheckRate ) {   
      WIFIcheck();                                 // check if wifi connection is ok
      MaintTiming = millis();                      // reset timer
      time_t t=now();                              // read current time to ensure NTP auto refresh keeps triggering (otherwise only triggers when time is required causing a delay in response)
      // check status of illumination led
        if (ReqLEDStatus && !SD_Present) digitalWrite(Illumination_led, ledON); 
        else if (!SD_Present) digitalWrite(Illumination_led, ledOFF);
    }

  delay(40);

} 


// reboot camera (used if camera is failing to respond)
void RebootCamera() {  
    log_system_message("Camera failed to capture image - rebooting camera"); 
    // turn camera off then back on      
        digitalWrite(PWDN_GPIO_NUM, HIGH);
        delay(500);
        digitalWrite(PWDN_GPIO_NUM, LOW); 
        delay(500);
    RestartCamera(FRAME_SIZE_MOTION, PIXFORMAT_GRAYSCALE); 
    delay(1000);
    // try camera again, if still problem reboot esp32
        if (!capture_still()) {
            Serial.println("unable to reboot camera so rebooting esp...");
            UpdateBootlogSpiffs("Camera fault - rebooting");                     // store in bootlog
            delay(500);
            ESP.restart();   
            delay(5000);      // restart will fail without this delay
         }
}
         

// ----------------------------------------------------------------
//              Load settings from text file in Spiffs
// ----------------------------------------------------------------

void LoadSettingsSpiffs() {
    String TFileName = "/settings.txt";
    if (!SPIFFS.exists(TFileName)) {
      log_system_message("Settings file not found on Spiffs");
      return;
    }
    File file = SPIFFS.open(TFileName, "r");
    if (!file) {
      log_system_message("Unable to open settings file from Spiffs");
      return;
    } 

    // read contents of file
      String line;
      int tnum;
  
      // line 1 - emailWhenTriggered
        line = file.readStringUntil('\n');
        tnum = line.toInt();
        if (tnum == 0) emailWhenTriggered = 0;
        else if (tnum == 1) emailWhenTriggered = 1;
        else {
          log_system_message("Invalid emailWhenTriggered in settings: " + line);
          return;
        }
        
      // line 2 - block_threshold
        line = file.readStringUntil('\n');
        tnum = line.toInt();
        if (tnum < 1 || tnum > 100) {
          log_system_message("invalid block_threshold in settings");
          return;
        }
        block_threshold = tnum;
        
      // line 3 - image_threshold
        line = file.readStringUntil('\n');
        tnum = line.toInt();
        if (tnum < 1 || tnum > 100) {
          log_system_message("invalid image_threshold in settings");
          return;
        }
        image_threshold = tnum;
  
      // line 4 - TriggerLimitTime
        line = file.readStringUntil('\n');
        tnum = line.toInt();
        if (tnum < 1 || tnum > 3600) {
          log_system_message("invalid TriggerLimitTime in settings");
          return;
        }
        TriggerLimitTime = tnum;
  
      // line 5 - DetectionEnabled
        line = file.readStringUntil('\n');
        tnum = line.toInt();
        if (tnum == 2) tnum = 1;     // if it was paused restart it
        if (tnum == 0) DetectionEnabled = 0;
        else if (tnum == 1) DetectionEnabled = 1;
        else {
          log_system_message("Invalid DetectionEnabled in settings: " + line);
          return;
        }
  
      // line 6 - EmailLimitTime
        line = file.readStringUntil('\n');
        tnum = line.toInt();
        if (tnum < 60 || tnum > 10000) {
          log_system_message("invalid EmailLimitTime in settings");
          return;
        }
        EmailLimitTime = tnum;
  
      // line 7 - UseFlash
        line = file.readStringUntil('\n');
        tnum = line.toInt();
        if (tnum == 0) UseFlash = 0;
        else if (tnum == 1) UseFlash = 1;
        else {
          log_system_message("Invalid UseFlash in settings: " + line);
          return;
        }
        
      // line 8 - SpiffsFileCounter
        line = file.readStringUntil('\n');
        tnum = line.toInt();
        if (tnum > MaxSpiffsImages) {
          log_system_message("invalid SpiffsFileCounter in settings");
          return;
        }
        SpiffsFileCounter = tnum;

      // Detection mask grid
        bool gerr = 0;
        mask_active = 0;
        for (int y = 0; y < 3; y++) {
          for (int x = 0; x < 3; x++) {
            line = file.readStringUntil('\n');
            tnum = line.toInt();
            if (tnum == 1) {
              mask_frame[x][y] = 1;
              mask_active ++;  
            }
            else if (tnum == 0) mask_frame[x][y] = 0;
            else gerr = 1;    // flag invalid entry
          }
        }
        if (gerr) log_system_message("invalid mask entry in settings");
        
        
      file.close();
      log_system_message("Settings loaded from Spiffs");
}


// ----------------------------------------------------------------
//              Save settings to text file in Spiffs
// ----------------------------------------------------------------

void SaveSettingsSpiffs() {
    String TFileName = "/settings.txt";
    SPIFFS.remove(TFileName);   // delete old file if present
    File file = SPIFFS.open(TFileName, "w");
    if (!file) {
      log_system_message("Unable to open settings file in Spiffs");
      return;
    } 

    // save settings in to file
        file.println(String(emailWhenTriggered));
        file.println(String(block_threshold));
        file.println(String(image_threshold));
        file.println(String(TriggerLimitTime));
        file.println(String(DetectionEnabled));
        file.println(String(EmailLimitTime));
        file.println(String(UseFlash));
        file.println(String(SpiffsFileCounter));

       // Detection mask grid
          for (int y = 0; y < 3; y++) {
            for (int x = 0; x < 3; x++) {
              file.println(String(mask_frame[x][y]));
            }
          }

    file.close();
}


// ----------------------------------------------------------------
//                     Update boot log - Spiffs
// ----------------------------------------------------------------

void UpdateBootlogSpiffs(String Info) {
    Serial.println("Updating bootlog: " + Info);
    String TFileName = "/bootlog.txt";
    File file = SPIFFS.open(TFileName, FILE_APPEND);
    if (!file) {
      log_system_message("Unable to open boot log file in Spiffs");
      return;
    } 

    // add entry including time to file
      file.println(currentTime() + " - " + Info);
    
    file.close();
}


// ----------------------------------------------------------------
//                reset back to default settings
// ----------------------------------------------------------------

void handleDefault() {

    // default settings
      emailWhenTriggered = 0;
      block_threshold = 15;
      image_threshold= 20;
      TriggerLimitTime = 3;
      TRIGGERtimer = millis();            // reset last image captured timer (to prevent instant trigger)
      DetectionEnabled = 1;
      EmailLimitTime = 600;
      UseFlash = 1;

      // Detection mask grid
        for (int y = 0; y < 3; y++) 
          for (int x = 0; x < 3; x++) 
            mask_frame[x][y] = 1;
        mask_active = 9;

    SaveSettingsSpiffs();     // save settings in Spiffs

    log_system_message("Defauls web page request");      
    String message = "reset to default";

    server.send(404, "text/plain", message);   // send reply as plain text
    message = "";      // clear variable
      
}

// ----------------------------------------------------------------
//       -root web page requested    i.e. http://x.x.x.x/
// ----------------------------------------------------------------

void handleRoot() {

  log_system_message("root webpage requested");     


  // action any buttons presses etc.

    // Mask grid check array
      if (server.hasArg("submit")) {                    // if submit button was pressed
        mask_active = 0;  
        for (int y = 0; y < 3; y++) {
          for (int x = 0; x < 3; x++) {
            if (server.hasArg(String(x) + String(y))) {
              mask_frame[x][y] = 1;
              mask_active ++;
            } else mask_frame[x][y] = 0;
          }
          Serial.println(" ");
        }
        log_system_message("Detection mask updated"); 
        SaveSettingsSpiffs();     // save settings in Spiffs
      }
      
    // email was clicked -  if an email is sent when triggered 
      if (server.hasArg("email")) {
        if (!emailWhenTriggered) {
              log_system_message("Email when motion detected enabled"); 
              emailWhenTriggered = 1;
        } else {
          log_system_message("Email when motion detected disabled"); 
          emailWhenTriggered = 0;
        }
        SaveSettingsSpiffs();     // save settings in Spiffs
      }
      
    // if wipeS was entered  - clear Spiffs
      if (server.hasArg("wipeS")) {
        log_system_message("Clearing all stored images (Spiffs)"); 
        if (DetectionEnabled == 1) DetectionEnabled = 2;               // pause motion detecting 
        SPIFFS.format();
        SpiffsFileCounter = 0;
        TriggerTime = "Not since images wiped";
        UpdateBootlogSpiffs("Spiffs Wiped");                           // store event in bootlog file
        SaveSettingsSpiffs();                                          // save settings in Spiffs
        if (DetectionEnabled == 2) DetectionEnabled = 1;               // restart paused motion detecting
      }

//    // if wipeSD was entered  - clear all stored images on SD Card
//      if (server.hasArg("wipeSD")) {
//        log_system_message("Clearing all stored images (SD Card)"); 
//        fs::FS &fs = SD_MMC;
//        fs.format();     // not a valid command
//      }
      
    // if blockt was entered - block_threshold
      if (server.hasArg("blockt")) {
        String Tvalue = server.arg("blockt");   // read value
        int val = Tvalue.toInt();
        if (val > 0 && val < 100 && val != block_threshold) { 
          log_system_message("block_threshold changed to " + Tvalue ); 
          block_threshold = val;
          SaveSettingsSpiffs();     // save settings in Spiffs
        }
      }
      
    // if imaget was entered - image_threshold
      if (server.hasArg("imaget")) {
        String Tvalue = server.arg("imaget");   // read value
        int val = Tvalue.toInt();
        if (val > 0 && val < 100 && val != image_threshold) { 
          log_system_message("image_threshold changed to " + Tvalue ); 
          image_threshold = val;
          SaveSettingsSpiffs();     // save settings in Spiffs
        }
      }
      
    // if emailtime was entered - min time between email sends
      if (server.hasArg("emailtime")) {
        String Tvalue = server.arg("emailtime");   // read value
        int val = Tvalue.toInt();
        if (val > 59 && val < 10000 && val != EmailLimitTime) { 
          log_system_message("EmailLimitTime changed to " + Tvalue + " seconds"); 
          EmailLimitTime = val;
          SaveSettingsSpiffs();     // save settings in Spiffs
        }
      }
      
      // if triggertime was entered - min time between triggers
      if (server.hasArg("triggertime")) {
        String Tvalue = server.arg("triggertime");   // read value
        int val = Tvalue.toInt();
        if (val > 0 && val < 3600 && val != TriggerLimitTime) { 
          log_system_message("Triggertime changed to " + Tvalue + " seconds"); 
          TriggerLimitTime = val;
          SaveSettingsSpiffs();     // save settings in Spiffs
        }
      }
  
    // if button "toggle illuminator LED" was pressed  
      if (server.hasArg("illuminator")) {
        // button was pressed 
          if (DetectionEnabled == 1) DetectionEnabled = 2;    // pause motion detecting (to stop light triggering it)
          if (!ReqLEDStatus) {
            ReqLEDStatus = 1;
            digitalWrite(Illumination_led, ledON);  
            log_system_message("Illuminator LED turned on");    
          } else {
            ReqLEDStatus = 0;
            digitalWrite(Illumination_led, ledOFF);  
            log_system_message("Illuminator LED turned off"); 
          }
          TRIGGERtimer = millis();                                // reset last image captured timer (to prevent instant trigger)
          if (DetectionEnabled == 2) DetectionEnabled = 1;        // re enable detection if it was paused
      }
      
    // if button "flash" was pressed  - toggle flash enabled
      if (server.hasArg("flash")) {
        // button was pressed 
        if (UseFlash == 0) {
            UseFlash = 1;
            log_system_message("Flash enabled");    
          } else {
            UseFlash = 0;
            log_system_message("Flash disabled");    
          }
          SaveSettingsSpiffs();     // save settings in Spiffs
      }
      
    // if button "toggle movement detection" was pressed  
      if (server.hasArg("detection")) {
        // button was pressed 
          if (DetectionEnabled == 0) {
            TRIGGERtimer = millis();                                // reset last image captured timer (to prevent instant 
            DetectionEnabled = 1;
            log_system_message("Movement detection enabled"); 
            TriggerTime = "Not since detection enabled";
          } else {
            DetectionEnabled = 0;
            log_system_message("Movement detection disabled");    
          }
          SaveSettingsSpiffs();     // save settings in Spiffs
      }


  // build the HTML code 
  
    String message = webheader(0);                                      // add the standard html header
    message += "<FORM action='/' method='post'>\n";                     // used by the buttons (action = the page send it to)
    message += "<P>";                                                   // start of section
    

    // insert an iframe containing the changing data (updates every few seconds using java script)
       message += "<BR><iframe id='dataframe' height=160; width=600; frameborder='0';></iframe>\n"
      "<script type='text/javascript'>\n"
         "setTimeout(function() {document.getElementById('dataframe').src='/data';}, " + JavaRefreshTime +");\n"
         "window.setInterval(function() {document.getElementById('dataframe').src='/data';}, " + String(datarefresh) + ");\n"
      "</script>\n"; 

    // detection mask check grid (right of screen)
      message += "<div style='float: right;'>Detection Mask<br>";
      for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
          message += "<input type='checkbox' name='" + String(x) + String(y) + "' ";
          if (mask_frame[x][y]) message += "checked ";
          message += ">\n";
        }
        message += "<BR>";
      }
      message += String(mask_active) + " active";
      message += "</div>\n";
      
    // minimum seconds between triggers
      message += "<BR>Minimum time between triggers:";
      message += "<input type='number' style='width: 60px' name='triggertime' min='1' max='3600' value='" + String(TriggerLimitTime) + "'>seconds \n";

    // minimum seconds between email sends
      message += "<BR>Minimum time between E-mails:";
      message += "<input type='number' style='width: 60px' name='emailtime' min='60' max='10000' value='" + String(EmailLimitTime) + "'>seconds \n";

    // detection parameters
      message += "<BR>Detection thresholds: ";
      message += "Block<input type='number' style='width: 35px' name='blockt' title='Variation in a block to count as block has changed' min='1' max='99' value='" + String(block_threshold) + "'>%, \n";
      message += "Image<input type='number' style='width: 35px' name='imaget' title='Changed blocks to count as motion is detected' min='1' max='99' value='" + String(image_threshold) + "'>% \n"; 
               
    // input submit button  
      message += "<BR><input type='submit' name='submit'><BR><BR>\n";

    // Toggle illuminator LED button
      if (!SD_Present) message += "<input style='height: 30px;' name='illuminator' title='Toggle the Illumination LED On/Off' value='Light' type='submit'> \n";

    // Toggle 'use flash' button
      if (!SD_Present) message += "<input style='height: 30px;' name='flash' title='Toggle use of flash when capturing image On/Off' value='Flash' type='submit'> \n";

    // Toggle movement detection
      message += "<input style='height: 30px;' name='detection' title='Movement detection enable/disable' value='Detection' type='submit'> \n";

    // Toggle email when movement detection
      message += "<input style='height: 30px;' name='email' value='Email' title='Send email when motion detected enable/disable' type='submit'> \n";

    // Clear images in spiffs
      message += "<input style='height: 30px;' name='wipeS' value='Wipe Store' title='Delete all images stored in Spiffs' type='submit'> \n";
    
//    // Clear images on SD Card
//      if (SD_Present) message += "<input style='height: 30px;' name='wipeSD' value='Wipe SDCard' title='Delete all images on SD Card' type='submit'> \n";

    message += "</span></P>\n";    // end of section    
    message += webfooter();        // add the standard footer

    server.send(200, "text/html", message);      // send the web page
    message = "";      // clear variable

}

  
// ----------------------------------------------------------------
//     -data web page requested     i.e. http://x.x.x.x/data
// ----------------------------------------------------------------
//
//   This shows information on the root web page which refreshes every few seconds

void handleData(){

  String message = 
      "<!DOCTYPE HTML>\n"
      "<html><body>\n";
   
          
  // Movement detection
    message += "<BR>Movement detection is ";
    if (DetectionEnabled == 1) message +=  "enabled, last triggered: " + TriggerTime + "\n";
    else if (DetectionEnabled == 2) message += red + "disabled" + endcolour + "\n";
    else message += red + "disabled" + endcolour + "\n";

  // email when motion detected
    message += "<BR>Send an Email when motion detected: "; 
    if (emailWhenTriggered) message += red + "enabled" + endcolour;  
    else message += "disabled";
    
  // Illumination
    message += "<BR>Illumination LED is ";    
    if (digitalRead(Illumination_led) == ledON) message += red + "On" + endcolour;
    else message += "Off";
    if (!SD_Present) {    // if no sd card in use
      // show status of use flash 
      if (UseFlash) message += " - Flash enabled\n";
      else message += " - Flash disabled\n";
    }

  // show current time
    message += "<BR>Time: " + currentTime() + "\n";      // show current time

  // last log entry
  //  message += "<BR>log: " + system_message[LogNumber];

  // show if a sd card is present
    if (SD_Present) message += "<BR>SD-Card present (Flash disabled)\n";

//  // show io pin status
//    message += "<BR>External sensor pin is: ";
//    if (digitalRead(gioPin)) message += "High\n";
//    else message += "Low\n";

  if (AveragePix) message += "<BR>Average brightness: " + String(AveragePix);
  
  message += "</body></htlm>\n";
  
  server.send(200, "text/html", message);   // send reply as plain text
  message = "";      // clear variable

  
}


// ----------------------------------------------------------------
//           -Display live image     i.e. http://x.x.x.x/live
// ----------------------------------------------------------------

void handleLive(){

  log_system_message("Live image requested");      


  String message = webheader(0);                                      // add the standard html header

  message += "<BR><H1>Live Image - " + currentTime() +"</H1><BR>\n";

  capturePhotoSaveSpiffs(UseFlash);     // capture an image from camera

  // insert image in to html
    message += "<img  id='img' alt='Live Image' width='90%'>\n";       // content is set in javascript

  // javascript to refresh the image after short delay (bug fix as it often rejects the first request)
    message +=  "<script type='text/javascript'>\n"
                "  setTimeout(function(){ document.getElementById('img').src='/img'; }, " + JavaRefreshTime + ");\n"
                "</script>\n";
    
  message += webfooter();                                             // add the standard footer
  
  server.send(200, "text/html", message);      // send the web page
  message = "";      // clear variable  
}


// ----------------------------------------------------------------
//    -Display captured images     i.e. http://x.x.x.x/images
// ----------------------------------------------------------------

void handleImages(){

  log_system_message("Stored images page requested");   
  int ImageToShow = SpiffsFileCounter;     // set current image to display when /img called

  // action any buttons presses etc.

    // if a image select button was pressed
      if (server.hasArg("button")) {
        String Bvalue = server.arg("button");   // read value
        int val = Bvalue.toInt();
        Serial.println("Button " + Bvalue + " was pressed");
        ImageToShow = val;     // select which image to display when /img called
      }

      
  String message = webheader(0);                                      // add the standard html header
  message += "<FORM action='/images' method='post'>\n";               // used by the buttons (action = the page send it to)

  message += "<H1>Stored Images</H1>";
  
  // create image selection buttons
    String sins;   // style for button
    for(int i=1; i <= MaxSpiffsImages; i++) {
        if (i == ImageToShow) sins = "background-color: #0f8; height: 30px;";
        else sins = "height: 30px;";
        message += "<input style='" + sins + "' name='button' value='" + String(i) + "' type='submit'>\n";
    }

  // Insert time info. from text file
    String TFileName = "/" + String(ImageToShow) + ".txt";
    File file = SPIFFS.open(TFileName, "r");
    if (!file) message += red + "<BR>File not found" + endcolour + "\n";
    else {
      String line = file.readStringUntil('\n');      // read first line of text file
      message += "<BR>" + line +"\n";
    }
    file.close();
    
  // insert image in to html
    message += "<BR><img id='img' alt='Camera Image' width='90%'>\n";      // content is set in javascript

  // javascript to refresh the image after short delay (bug fix as it often rejects the request otherwise)
    message +=  "<script type='text/javascript'>\n"
                "  setTimeout(function(){ document.getElementById('img').src='/img?pic=" + String(ImageToShow) + "' ; }, " + JavaRefreshTime + ");\n"
                "</script>\n";

  message += webfooter();                                             // add the standard footer

    
  server.send(200, "text/html", message);      // send the web page
  message = "";      // clear variable
  
}


// ----------------------------------------------------------------
//      -ping web page requested     i.e. http://x.x.x.x/ping
// ----------------------------------------------------------------

void handlePing(){

  log_system_message("ping web page requested");      
  String message = "ok";

  server.send(404, "text/plain", message);   // send reply as plain text
  message = "";      // clear variable
  
}


// ----------------------------------------------------------------
//   -bootlog web page requested    i.e. http://x.x.x.x/bootlog
// ----------------------------------------------------------------
// display boot log from Spiffs

void handleBootLog() {

   log_system_message("bootlog webpage requested");     

    // build the html for /bootlog page

    String message = webheader(0);     // add the standard header

      message += "<P>\n";                // start of section
  
      message += "<br>SYSTEM BOOT LOG<br><br>\n";
  
      // show contents of bootlog.txt in Spiffs
        File file = SPIFFS.open("/bootlog.txt", "r");
        if (!file) message += red + "No Boot Log Available" + endcolour + "<BR>\n";
        else {
          String line;
          while(file.available()){
            line = file.readStringUntil('\n');      // read first line of text file
            message += line +"<BR>\n";
          }
        }
        file.close();

      message += "<BR><BR>" + webfooter();     // add standard footer html
    

    server.send(200, "text/html", message);    // send the web page

}


// ----------------------------------------------------------------
// -last stored image page requested     i.e. http://x.x.x.x/img
// ----------------------------------------------------------------
// pic parameter on url selects which file to display

void handleImg(){
    
    int ImageToShow = SpiffsFileCounter;     // set image to display as current image
        
    // if a image to show is specified in url
        if (server.hasArg("pic")) {
        String Bvalue = server.arg("pic");   // read value
        int val = Bvalue.toInt();
        Serial.println("Button " + Bvalue + " was pressed");
        ImageToShow = val;     // select which image to display when /img called
        }

    log_system_message("display stored image requested " + String(ImageToShow));

    if (ImageToShow < 1 || ImageToShow > MaxSpiffsImages) ImageToShow=SpiffsFileCounter;
    
    // send image file
        String TFileName = "/" + String(ImageToShow) + ".jpg";
        File f = SPIFFS.open(TFileName, "r");                         // read file from spiffs
            if (!f) Serial.println("Error reading " + TFileName);
            else {
                size_t sent = server.streamFile(f, "image/jpeg");     // send file as web page
                if (!sent) Serial.println("Error sending " + TFileName);
            }
        f.close();


    ImageToShow = 0;     // reset image to display to most recent
    
}


// ----------------------------------------------------------------
//                       -spiffs procedures
// ----------------------------------------------------------------
// capture live image and save in spiffs (also on sd-card if present)

void capturePhotoSaveSpiffs(bool UseFlash) {

  if (DetectionEnabled == 1) DetectionEnabled = 2;               // pause motion detecting while photo is captured (don't think this is required?)

  bool ok;

  // increment image count
    SpiffsFileCounter++;
    if (SpiffsFileCounter > MaxSpiffsImages) SpiffsFileCounter = 1;
    SaveSettingsSpiffs();     // save settings in Spiffs
    

  // ------------------- capture an image -------------------
    
  RestartCamera(FRAME_SIZE_PHOTO, PIXFORMAT_JPEG);      // restart camera in jpg mode to take a photo (uses greyscale mode for movement detection)

  camera_fb_t * fb = NULL; // pointer
  ok = 0; // Boolean to indicate if the picture has been taken correctly
  byte TryCount = 0;    // attempt counter to limit retries

  do {

    TryCount ++;
      
    // use flash if required
      if (!SD_Present && UseFlash)  digitalWrite(Illumination_led, ledON);   // turn Illuminator LED on if no sd card and it is required
   
    Serial.println("Taking a photo... attempt #" + String(TryCount));
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // restore flash status after using it as a flash
      if (ReqLEDStatus && !SD_Present) digitalWrite(Illumination_led, ledON);   
      else if (!SD_Present) digitalWrite(Illumination_led, ledOFF);
       

    // ------------------- save image to Spiffs -------------------
    
    String IFileName = "/" + String(SpiffsFileCounter) + ".jpg";      // file names to store in Spiffs
    String TFileName = "/" + String(SpiffsFileCounter) + ".txt";
    // Serial.println("Picture file name: " + IFileName);

    File file = SPIFFS.open(IFileName, FILE_WRITE);
    if (!file) {
      Serial.println("Failed to open file in writing mode");
      return;
    }
    else {
      if (file.write(fb->buf, fb->len)) {              // payload (image), payload length
        Serial.print("The picture has been saved in ");
        Serial.print(IFileName);
        Serial.print(" - Size: ");
        Serial.print(file.size());
        Serial.println(" bytes");
      } else log_system_message("Error writing image to Spiffs");
    }
    file.close();
    
    // save text file to spiffs with time info. 
      SPIFFS.remove(TFileName);   // delete old file with same name if present
      file = SPIFFS.open(TFileName, "w");
      if (!file) {
        Serial.println("Failed to create text file");
        return;
      }
      else file.println(currentTime());
      file.close();


    // ------------------- save image to SD Card -------------------
    
    if (SD_Present) {

      fs::FS &fs = SD_MMC; 
      
      // read image number from counter text file
        int Inum = 0;  
        String CFileName = "/counter.txt";   
        file = fs.open(CFileName, FILE_READ);
        if (!file) Serial.println("Unable to read counter.txt from sd card"); 
        else {
          // read contents
          String line = file.readStringUntil('\n');    
          Inum = line.toInt();
          if (Inum > 0 && Inum < 2000) Serial.println("Last stored image on SD Card was #" + line);
          else Inum = 0;
        }
        file.close();
        Inum ++;
        
      // store new image number to counter text file
        if (fs.exists(CFileName)) fs.remove(CFileName);
        file = fs.open(CFileName, FILE_WRITE);
        if (!file) Serial.println("Unable to create counter file on sd card");
        else file.println(String(Inum));
        file.close();
        
      IFileName = "/" + String(Inum) + ".jpg";     // file names to store on sd card
      TFileName = "/" + String(Inum) + ".txt";
      
      // save image
        file = fs.open(IFileName, FILE_WRITE);
        if (!file) Serial.println("Failed to open sd-card image file in writing mode");
        else file.write(fb->buf, fb->len); // payload (image), payload length
        file.close();
        
      // save text (time and date info)
        file = fs.open(TFileName, FILE_WRITE);
        if (!file) Serial.println("Failed to create sd-card text file in writing mode");
        else file.println(currentTime());
        file.close();      
    }    // end of store on SD card
    
    
    // ------------------------------------------------------------

    
    esp_camera_fb_return(fb);    // return frame so memory can be released

    // check if file has been correctly saved in SPIFFS
      ok = checkPhoto(SPIFFS, IFileName);
    
  } while ( !ok && TryCount < 3);           // if there was a problem try again 

  if (TryCount == 3) log_system_message("Unable to capture/store image");
  
  RestartCamera(FRAME_SIZE_MOTION, PIXFORMAT_GRAYSCALE);    // restart camera back to greyscale mode for movement detection

  TRIGGERtimer = millis();                                  // reset retrigger timer to stop instant movement trigger
  if (DetectionEnabled == 2) DetectionEnabled = 1;          // restart paused motion detecting
}


// check file saved to Spiffs ok
bool checkPhoto( fs::FS &fs, String IFileName ) {
  File f_pic = fs.open( IFileName );
  unsigned int pic_sz = f_pic.size();
  bool tres = ( pic_sz > 100 );
  if (!tres) log_system_message("Problem detected taking/storing image");
  return ( tres );
}


// ----------------------------------------------------------------
//                       -restart the camera
// ----------------------------------------------------------------
//  pixformats = PIXFORMAT_ + YUV422,GRAYSCALE,RGB565,JPEG
//  framesizes = FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA

void RestartCamera(framesize_t fsize, pixformat_t format) {

    bool ok;
    esp_camera_deinit();
      config.frame_size = fsize;
      config.pixel_format = format;
    ok = esp_camera_init(&config);
    if (ok == ESP_OK) {
      Serial.println("Camera mode switched");
      TRIGGERtimer = millis();                                // reset last image captured timer (to prevent instant trigger)
    }
    else {
      // failed so try again
        delay(50);
        ok = esp_camera_init(&config);
        if (ok == ESP_OK) Serial.println("Camera restarted");
        else Serial.println("Camera failed to restart");
    }

}


// ----------------------------------------------------------------
//                       -motion has been detected
// ----------------------------------------------------------------

void MotionDetected(float changes) {

  if (DetectionEnabled == 1) DetectionEnabled = 2;               // pause motion detecting (prob. not required?)
  
    log_system_message("Camera detected motion: " + String(changes)); 
    TriggerTime = currentTime();                                 // store time of trigger
    capturePhotoSaveSpiffs(UseFlash);                            // capture an image

    // send email if long enough since last motion detection (or if this is the first one)
    if (emailWhenTriggered) {
        unsigned long currentMillis = millis();        // get current time  
        if ( ((unsigned long)(currentMillis - EMAILtimer) >= (EmailLimitTime * 1000)) || (EMAILtimer == 0) ) {

          EMAILtimer = currentMillis;    // reset timer 
      
          // send an email
              String emessage = "Camera triggered at " + currentTime();
              byte q = sendEmail(emailReceiver,"Message from CameraWifiMotion", emessage);    
              if (q==0) log_system_message("email sent ok" );
              else log_system_message("Error sending email, error code=" + String(q) );
  
         }
         else log_system_message("Too soon to send another email");
    }

  TRIGGERtimer = millis();                                       // reset retrigger timer to stop instant movement trigger
  if (DetectionEnabled == 2) DetectionEnabled = 1;               // restart paused motion detecting

}



// ----------------------------------------------------------------
//           -testing page     i.e. http://x.x.x.x/test
// ----------------------------------------------------------------

void handleTest(){

  log_system_message("Testing page requested");      

  String message = webheader(0);                                      // add the standard html header

  message += "<BR>Testing page<BR><BR>\n";

  // ---------------------------- test section here ------------------------------



//        capturePhotoSaveSpiffs(1);
//        
//        // send email
//          String emessage = "Test email";
//          byte q = sendEmail(emailReceiver,"Message from CameraWifiMotion sketch", emessage);    
//          if (q==0) log_system_message("email sent ok" );
//          else log_system_message("Error sending email code=" + String(q) );


       
  // -----------------------------------------------------------------------------

  message += webfooter();                                             // add the standard footer

    
  server.send(200, "text/html", message);      // send the web page
  message = "";      // clear variable
  
}


// --------------------------- E N D -----------------------------
