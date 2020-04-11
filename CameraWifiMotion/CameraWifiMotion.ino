 /*******************************************************************************************************************
 *
 *       ESP32-Cam based security camera with motion detection, email, ftp and web server -  using Arduino IDE 
 *             
 *             Included files: gmail-esp32.h, standard.h and wifi.h, motion.h, ota.h, ftp.h
 *             Bult using Arduino IDE 1.8.10, esp32 boards v1.0.4
 *                          
 *             GPIO13 is used as an input pin for external sensors etc. (just reports status change at the moment)
 *             GPIO16 also available for use although I have found it to be pulled high when I tried?
 *             GPIO12 can be used but must be low at boot
 *             GPIO1 / 03 Used for serial port
 *             
 *             IMPORTANT! - If you are getting weird problems (motion detection retriggering all the time, slow wifi
 *                          response times, random restarting - especially when using the LED) chances are there is a problem 
 *                          the power to the board.  It needs a good 500ma supply and ideally a good sized smoothing 
 *                          capacitor near the esp board.
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

  const String sversion = "11Apr20";                     // version of this sketch

  const char* MDNStitle = "ESPcam1";                     // Mdns title (access with: 'http://<MDNStitle>.local' )

  #define EMAIL_ENABLED 1                                // if emailing is enabled

  #define FTP_ENABLED 1                                  // if ftp uploads are enabled

  #define OTA_ENABLED 1                                  // if Over The Air updates (OTA) are enabled
  
  const String OTAPassword = "12345678";                 // Password to enable OTA service (supplied as - http://<ip address>?pwd=xxxx )

  #define IMAGE_SETTINGS 1                               // Implement adjustment of camera sensor settings

  int MaxSpiffsImages = 8;                               // number of images to store in camera (Spiffs)
  
  const uint16_t datarefresh = 5000;                     // Refresh rate of the updating data on web page (1000 = 1 second)

  String JavaRefreshTime = "600";                        // time delay when loading url in web pages (Javascript) to prevent failed requests
    
  const uint16_t LogNumber = 50;                         // number of entries to store in the system log

  const uint16_t ServerPort = 80;                        // ip port to serve web pages on

  const uint16_t Illumination_led = 4;                   // illumination LED pin

  const byte flashMode = 1;                              // 1=take picture with flash, 2=flash then take picture

  const byte gioPin = 13;                                // I/O pin (for external sensor input) 
  
  bool ioRequiredHighToTrigger = 0;                      // If motion detection only triggers if IO input is also high
  
  const uint16_t MaintCheckRate = 5;                     // how often to carry out routine system checks (seconds)
 
  uint8_t cameraImageInvert = 0;                         // flip image vertically (i.e. upside down), 1 or 0

  int8_t cameraImageBrightness = 0;                      // image brighness (-2 to 2) - Note: has no effect?

  int8_t cameraImageContrast = 0;                        // image contrast (-2 to 2) - Note: has no effect?

  float thresholdGainCompensation = 0.65;                // motion detection level compensation for increased noise in image when gain increased

  // to adjust other camera sensor settings see 'cameraImageSettings()' in 'motion.h'
  
  
// ---------------------------------------------------------------


  float cameraImageExposure = 20;            // Camera exposure (0 - 1200)
  float cameraImageGain = 0;                 // Image gain (0 to 30)
  uint32_t TRIGGERtimer = 0;                 // used for limiting camera motion trigger rate
  uint32_t EMAILtimer = 0;                   // used for limiting rate emails can be sent
  byte DetectionEnabled = 1;                 // flag if capturing motion is enabled (0=stopped, 1=enabled, 2=paused)
  String TriggerTime = "Not yet triggered";  // Time of last motion trigger
  uint32_t MaintTiming = millis();           // used for timing maintenance tasks
  bool emailWhenTriggered = 0;               // flag if to send emails when motion detection triggers
  bool ftpImages = 0;                        // if to FTP images up to server
  bool ReqLEDStatus = 0;                     // desired status of the illuminator led (i.e. should it be on or off when not being used as a flash)
  const bool ledON = HIGH;                   // Status LED control 
  const bool ledOFF = LOW;
  uint16_t TriggerLimitTime = 2;             // min time between motion detection triggers (seconds)
  uint16_t EmailLimitTime = 60;              // min time between email sends (seconds)
  bool UseFlash = 1;                         // use flash when taking a picture
  bool SensorStatus = 1;                     // Status of the sensor i/o pin (gioPin)
  bool OTAEnabled = 0;                       // flag if OTA has been enabled (via supply of password)

#include "soc/soc.h"                         // Disable brownout problems
#include "soc/rtc_cntl_reg.h"                // Disable brownout problems

// spiffs used to store images and settings
  #include <SPIFFS.h>
  #include <FS.h>                            // gives file access on spiffs
  int SpiffsFileCounter = 0;                 // counter of last image stored

// sd card - see https://randomnerdtutorials.com/esp32-cam-take-photo-save-microsd-card/
  #include "SD_MMC.h"
  // #include <SPI.h>                        // (already loaded)
  #include <FS.h>                            // gives file access (already loaded?)
  #define SD_CS 5                            // sd chip select pin
  bool SD_Present;                           // flag if an sd card was found (0 = no)


#include "wifi.h"                            // Load the Wifi / NTP stuff

#include "standard.h"                        // Standard procedures

#if EMAIL_ENABLED
  #include "gmail_esp32.h"                   // send email via smtp
#endif

#include "motion.h"                          // motion detection / camera

#if OTA_ENABLED
  #include "ota.h"                           // Over The Air updates (OTA)
#endif

// forward declarations
  void RestartCamera(pixformat_t);

#if FTP_ENABLED
  #include "ftp.h"                           // upload images via FTP
#endif

  
// ---------------------------------------------------------------
//    -SETUP     SETUP     SETUP     SETUP     SETUP     SETUP
// ---------------------------------------------------------------

void setup(void) {
    
  Serial.begin(115200);
  // Serial.setTimeout(2000);
  // while(!Serial) { }        // Wait for serial to initialize.

  Serial.println(("\n\n\n---------------------------------------"));
  Serial.println("Starting - " + stitle + " - " + sversion);
  Serial.println(("---------------------------------------"));
  
  // Serial.setDebugOutput(true);            // enable extra diagnostic info  


  // Spiffs - see: https://circuits4you.com/2018/01/31/example-of-esp8266-flash-file-system-spiffs/
    if (!SPIFFS.begin(true)) {
      Serial.println(("An Error has occurred while mounting SPIFFS"));
      delay(5000);
      ESP.restart();
      delay(5000);
    } else {
      Serial.print(("SPIFFS mounted successfully."));
      Serial.print("total bytes: " + String(SPIFFS.totalBytes()));
      Serial.println(", used bytes: " + String(SPIFFS.usedBytes()));
      LoadSettingsSpiffs();     // Load settings from text file in Spiffs
    }

  // start sd card
      SD_Present = 0;
      if (!SD_MMC.begin("/sdcard", true)) {         // if loading sd card fails     
        // note: ("/sdcard", true) = 1 wire - see: https://www.reddit.com/r/esp32/comments/d71es9/a_breakdown_of_my_experience_trying_to_talk_to_an/
        pinMode(2, INPUT_PULLUP);
        log_system_message("SD Card not found");   
      } else {
        uint8_t cardType = SD_MMC.cardType();
        if (cardType == CARD_NONE) {                // if no sd card found
            log_system_message("SD Card type detect failed"); 
        } else {
          uint16_t SDfreeSpace = (uint64_t)(SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024);
          log_system_message("SD Card found, free space = " + String(SDfreeSpace) + "MB"); 
          SD_Present = 1;                           // flag working sd card found
        }
      }
     
//fs::FS &fs = SD_MMC; 
//File file = fs.open("/test.txt", FILE_WRITE);
//file.println("hi");
//file.close();
    
  // configure the LED
      pinMode(Illumination_led, OUTPUT); 
      digitalWrite(Illumination_led, ledOFF); 
    
  BlinkLed(1);           // flash the led once

  // configure the I/O pin (with pullup resistor)
    pinMode(gioPin,  INPUT);      
    SensorStatus = 1;                          

  startWifiManager();                        // Connect to wifi (procedure is in wifi.h)
  
  if (MDNS.begin(MDNStitle)) {
    Serial.println(("MDNS responder started"));
  }
  
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
    server.on("/imagedata", handleImagedata);// Show raw image data
    // server.on("/download", handleDownload);  // download settings file from Spiffs
    server.onNotFound(handleNotFound);       // invalid page requested

  // start web server
    Serial.println(("Starting web server"));
    server.begin();

  // Finished connecting to network
    BlinkLed(2);                             // flash the led twice
    log_system_message(stitle + " Started");   
       
  // set up camera
    Serial.print(("Initialising camera: "));
    Serial.println(setupCameraHardware() ? "OK" : "ERR INIT");

  // Turn-off the 'brownout detector'
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  if (!psramFound()) log_system_message("Warning: No PSRam found");
  
  UpdateBootlogSpiffs("Booted");                   // store time of boot in bootlog

  TRIGGERtimer = millis();                         // reset retrigger timer to stop instant motion trigger
}


// blink the led 
void BlinkLed(byte Bcount) {
    for (int i = 0; i < Bcount; i++) { 
      digitalWrite(Illumination_led, ledON);
      delay(50);
      digitalWrite(Illumination_led, ledOFF);
      delay(300);
    }
}


// ----------------------------------------------------------------
//   -LOOP     LOOP     LOOP     LOOP     LOOP     LOOP     LOOP
// ----------------------------------------------------------------

void loop(void){

  server.handleClient();                                                                    // service any web requests 

  // camera motion detection 
    if (DetectionEnabled == 1) {    
      if (!capture_still()) RebootCamera(PIXFORMAT_GRAYSCALE);                                // capture image, if problem reboot camera and try again
        uint16_t changes = motion_detect();                                                   // find amount of change in current image frame compared to the last one     
        update_frame();                                                                       // Copy current frame to previous
        if ( (changes >= Image_thresholdL) && (changes <= Image_thresholdH) ) {               // if enough change to count as motion detected 
          if (tCounter >= tCounterTrigger) {                                                  // only trigger if movement detected in more than one consequitive frames
             tCounter = 0;
             if ((unsigned long)(millis() - TRIGGERtimer) >= (TriggerLimitTime * 1000) ) {    // limit time between triggers
                TRIGGERtimer = millis();                                                      // update last trigger time
                // run motion detected procedure (blocked if io high is required)
                  if (ioRequiredHighToTrigger == 0 || SensorStatus == 1) MotionDetected(changes);   
                  else log_system_message("Motion detected but io input low so ignored");
             } else Serial.println("Too soon to re-trigger");
          } else Serial.println("Not enough consecutive detections");
       }
    } 

  // log when sensor i/o input pin status changes 
    bool tstatus = digitalRead(gioPin);
    if (tstatus != SensorStatus) {
      delay(20);                             
      tstatus = digitalRead(gioPin);        // debounce input
      if (tstatus != SensorStatus) {
        // input pin status has changed
        if (tstatus == 1) {
          SensorStatus = 1;
          ioDetected(1);                    // trigger io status has changed procedure        
        } else {
          SensorStatus = 0;
          ioDetected(0);
        }
      }
    }

  // periodic system tasks
    if ((unsigned long)(millis() - MaintTiming) >= (MaintCheckRate * 1000) ) {   
      WIFIcheck();                                   // check if wifi connection is ok
      MaintTiming = millis();                        // reset system tasks timer
      time_t t=now();                                // read current time to ensure NTP auto refresh keeps triggering (otherwise only triggers when time is required causing a delay in response)
      // check status of illumination led is correct
        if (ReqLEDStatus) digitalWrite(Illumination_led, ledON); 
        else digitalWrite(Illumination_led, ledOFF);
      if (DetectionEnabled == 0) capture_still();    // capture a frame to get a current brightness reading
      if (targetBrightness > 0) AutoAdjustImage();   // auto adjust image sensor settings
    }
    
  delay(30);
} // loop



// Auto image adjustment 
//   runs every few seconds, called from loop
void AutoAdjustImage() {
          float exposureAdjustmentSteps = (cameraImageExposure / 25) + 0.2;    // adjust by higher amount when at higher level
          float gainAdjustmentSteps = 0.5;    
          float hyster = 20.0;                                                 // Hysteresis on brightness level
          if (AveragePix > (targetBrightness + hyster)) {
            // too bright
            if (cameraImageGain > 0) cameraImageGain -= gainAdjustmentSteps;
            else cameraImageExposure -= exposureAdjustmentSteps;
          }
          if (AveragePix < (targetBrightness - hyster)) {
            // too dark
            if (cameraImageExposure >= 1200) cameraImageGain += gainAdjustmentSteps;
            else cameraImageExposure += exposureAdjustmentSteps;
          }
          // check for over scale
            if (cameraImageExposure < 0) cameraImageExposure = 0;
            if (cameraImageExposure > 1200) cameraImageExposure = 1200;
            if (cameraImageGain < 0) cameraImageGain = 0;
            if (cameraImageGain > 30) cameraImageGain = 30;
          cameraImageSettings(FRAME_SIZE_MOTION);      // apply camera sensor settings
          capture_still();                             // update stored image with the changed image settings to prevent trigger
          update_frame(); 
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

    log_system_message("Loading settings from Spiffs");
    
    // read contents of file
      String line;
      uint16_t tnum;
  
    ReadLineSpiffs(&file, &line, &tnum);      // ignore first line as it is just a title 

    // line 1 - Block_threshold
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum < 1 || tnum > 255) log_system_message("invalid Block_threshold in settings");
      else Block_threshold = tnum;
      
    // line 2 - min Image_thresholdL
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum < 0 || tnum > 255) log_system_message("invalid min_day_image_threshold in settings");
      else Image_thresholdL = tnum;

    // line 3 - min Image_thresholdH
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum < 0 || tnum > 255) log_system_message("invalid max_day_image_threshold in settings");
      else Image_thresholdH = tnum;
            
    // line 4 - target brightness level
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum < 0 || tnum > 255) log_system_message("invalid night/night brightness cuttoff in settings");
      else targetBrightness = tnum;

    // line 5 - emailWhenTriggered
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum == 0) emailWhenTriggered = 0;
      else if (tnum == 1) emailWhenTriggered = 1;
      else log_system_message("Invalid emailWhenTriggered in settings: " + line);
      
    // line 6 - TriggerLimitTime
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum < 1 || tnum > 3600) log_system_message("invalid TriggerLimitTime in settings");
      else TriggerLimitTime = tnum;

    // line 7 - DetectionEnabled
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum == 2) tnum = 1;     // if it was paused restart it
      if (tnum == 0) DetectionEnabled = 0;
      else if (tnum == 1) DetectionEnabled = 1;
      else log_system_message("Invalid DetectionEnabled in settings: " + line);

    // line 8 - EmailLimitTime
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum < 60 || tnum > 10000) log_system_message("invalid EmailLimitTime in settings");
      else EmailLimitTime = tnum;

    // line 9 - UseFlash
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum == 0) UseFlash = 0;
      else if (tnum == 1) UseFlash = 1;
      else log_system_message("Invalid UseFlash in settings: " + line);
      
    // line 10 - SpiffsFileCounter
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum > MaxSpiffsImages) log_system_message("invalid SpiffsFileCounter in settings");
      else SpiffsFileCounter = tnum;

    // line 11 - cameraImageExposure
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum < 0 || tnum > 1200) log_system_message("invalid exposure in settings");
      else cameraImageExposure = tnum;
      
    // line 12 - cameraImageGain
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum > 31) log_system_message("invalid gain in settings");
      else cameraImageGain = tnum;

    // line 13 - tCounterTrigger
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum > 31) log_system_message("invalid consecutive detections in settings");
      else tCounterTrigger = tnum;
      
    // line 14 - ftpImages
      ReadLineSpiffs(&file, &line, &tnum);
      if (tnum == 0) ftpImages = 0;
      else if (tnum == 1) ftpImages = 1;
      else log_system_message("Invalid FTP in settings: " + line);
            
    // Detection mask grid
      bool gerr = 0;
      mask_active = 0;
      for (int y = 0; y < mask_rows; y++) {
        for (int x = 0; x < mask_columns; x++) {
          ReadLineSpiffs(&file, &line, &tnum);
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
}


// read a line of text from Spiffs file and parse an integer from it
//     I realise this is complicating things for no real benefit but I wanted to learn to use pointers ;-)
void ReadLineSpiffs(File* file, String* line, uint16_t* tnum) {
      File tfile = *file;
      String tline = *line;
      tline = tfile.readStringUntil('\n');
      *tnum = tline.toInt();
}


// ----------------------------------------------------------------
//              Save settings to a text file in Spiffs
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
        file.println("CameraWifiMotion settings file " + currentTime());   // title
        file.println(String(Block_threshold));
        file.println(String(Image_thresholdL));
        file.println(String(Image_thresholdH));      
        file.println(String(targetBrightness));
        file.println(String(emailWhenTriggered));
        file.println(String(TriggerLimitTime));
        file.println(String(DetectionEnabled));
        file.println(String(EmailLimitTime));
        file.println(String(UseFlash));
        file.println(String(SpiffsFileCounter));
        file.println(String(cameraImageExposure));
        file.println(String(cameraImageGain));
        file.println(String(tCounterTrigger));
        file.println(String(ftpImages));

       // Detection mask grid
          for (int y = 0; y < mask_rows; y++) {
            for (int x = 0; x < mask_columns; x++) {
              file.println(String(mask_frame[x][y]));
            }
          }

    file.close();
}


// ----------------------------------------------------------------
//                     Update boot log - Spiffs
// ----------------------------------------------------------------
// keeps a log of esp32 startups along with reasons why it was restarted

void UpdateBootlogSpiffs(String Info) {
  
    Serial.println("Updating bootlog: " + Info);
    String TFileName = "/bootlog.txt";
    File file = SPIFFS.open(TFileName, FILE_APPEND);
    if (!file) {
      log_system_message("Error: Unable to open boot log in Spiffs");
    } else {
      file.println(currentTime() + " - " + Info);       // add entry to log file   
      file.close();
    }
}


// ----------------------------------------------------------------
//                reset back to default settings
// ----------------------------------------------------------------
// sets all settings to a standard default

void handleDefault() {

    // default settings
      emailWhenTriggered = 0;
      targetBrightness = 130;
      Block_threshold = 7;
      Image_thresholdL= 15;
      Image_thresholdH= 192;
      TriggerLimitTime = 20;
      EmailLimitTime = 600;
      DetectionEnabled = 1;
      UseFlash = 0;
      cameraImageExposure = 30;
      cameraImageGain = 0;
      tCounterTrigger = 1;
      ftpImages = 0;

      // Detection mask grid
        for (int y = 0; y < mask_rows; y++) 
          for (int x = 0; x < mask_columns; x++) 
            mask_frame[x][y] = 1;
        mask_active = 12;

    SaveSettingsSpiffs();                      // save settings in Spiffs
    TRIGGERtimer = millis();                   // reset last image captured timer (to prevent instant trigger)

    log_system_message("Defauls web page request");      
    String message = "reset to default";

    server.send(404, "text/plain", message);   // send reply as plain text
    message = "";      // clear string
      
}

// ----------------------------------------------------------------
//       -root web page requested    i.e. http://x.x.x.x/
// ----------------------------------------------------------------

void handleRoot() {

  log_system_message("root webpage requested");     

  rootButtons();                                                    // handle any user input from page         

  latestChanges = 0;                                                // reset stored motion values as could be out of date 

  // build the HTML code 
  
    String message = webheader("#stdLink:hover { background-color: rgb(180, 180, 0);}");                                   // add the standard html header
    message += "<FORM action='/' method='post'>\n";                 // used by the buttons (action = the page send it to)
    message += "<P>";                                               // start of section

  // insert an iFrame containing changing data in to the page
    uint16_t frameHeight = 160;
    message += "<BR><iframe id='dataframe' height=" + String(frameHeight) + "; width=600; frameborder='0';></iframe>\n";

  // javascript to refresh the iFrame every few seconds
  //      also refreshes after short delay (bug fix as it often rejects the first request)
    message +=  "<script type='text/javascript'>\n"
                "  window.setTimeout(function() {document.getElementById('dataframe').src='/data';}, " + String(JavaRefreshTime) + ");\n"
                "  window.setInterval(function() {document.getElementById('dataframe').src='/data';}, " + String(datarefresh) + ");\n"
                "</script>\n";

    // detection mask check grid (right of screen)
      message += "<div style='float: right;'>Detection Mask<br>";
      for (int y = 0; y < mask_rows; y++) {
        for (int x = 0; x < mask_columns; x++) {
          message += "<input type='checkbox' name='" + String(x) + String(y) + "' ";
          if (mask_frame[x][y]) message += "checked ";
          message += ">\n";
        }
        message += "<BR>";
      }
      message += "<BR>" + String(mask_active) + " active";
      message += "<BR>(" + String(mask_active * blocksPerMaskUnit) + " blocks)";
      message += "</div>\n";

    // link to show live image in popup window
      message += blue + "<a id='stdLink' target='popup' onclick=\"window.open('/img' ,'popup','width=320,height=240,left=50,top=50'); return false; \">DISPLAY CURRENT IMAGE</a>" + endcolour + " - \n ";
    
    // link to help/instructions page on github
      message += blue + " <a href='https://github.com/alanesq/CameraWifiMotion/blob/master/readme.txt'>INSTRUCTIONS</a>" + endcolour + " \n";
      // <a id='stdLink' target='popup' onclick=\"window.open('https://github.com/alanesq/CameraWifiMotion/blob/master/readme.txt' ,'popup', 'width=600,height=480'); return false; \">INSTRUCTIONS</a>" + endcolour + " \n";

    
    #if IMAGE_SETTINGS      // Implement adjustment of image settings 
      message += "<BR>";
      message +=  "Exposure: <input type='number' style='width: 50px' name='exp' min='0' max='1200' value=''>\n";
      message += " Gain: <input type='number' style='width: 50px' name='gain' min='0' max='30' value=''>\n";       
      // message += " Brightness: <input type='number' name='bright' style='width: 50px' min='-2' max='2' value='" + String(cameraImageBrightness) + "'>\n";  
      // message += " Contrast: <input type='number' name='cont' style='width: 50px' min='-2' max='2' value='" + String(cameraImageContrast) + "'>\n";  
        
    // Target brightness brightness cuttoff point
      message += "<BR>Auto image adjustment, target image brightness: "; 
      message += "<input type='number' style='width: 40px' name='daynight' title='Brightness level system aims to maintain' min='0' max='255' value='" + String(targetBrightness) + "'>";
      message += "(0 = disabled)\n";
    #else
      targetBrightness = 0;      
    #endif

    // minimum seconds between triggers
      message += "<BR>Minimum time between triggers:";
      message += "<input type='number' style='width: 50px' name='triggertime' min='1' max='3600' value='" + String(TriggerLimitTime) + "'>seconds \n";

    // consecutive detections required
      message += ", Consecutive detections required to trigger:";
      message += "<input type='number' style='width: 40px' name='consec' title='The number of changed images detected in a row required to trigger motion detected' min='1' max='100' value='" + String(tCounterTrigger) + "'>\n";

#if EMAIL_ENABLED
    // minimum seconds between email sends
      if (emailWhenTriggered) {
        message += "<BR>Minimum time between E-mails:";
        message += "<input type='number' style='width: 60px' name='emailtime' min='60' max='10000' value='" + String(EmailLimitTime) + "'>seconds \n";
      }
#endif

    // detection parameters 
      if (Image_thresholdH > (mask_active * blocksPerMaskUnit)) Image_thresholdH = (mask_active * blocksPerMaskUnit);    // make sure high threshold is not greater than max possible
      message += "<BR>Detection threshold: <input type='number' style='width: 40px' name='dblockt' title='Brightness variation in block required to count as changed (0-255)' min='1' max='255' value='" + String(Block_threshold) + "'>, \n";
      message += "Trigger when between<input type='number' style='width: 40px' name='dimagetl' title='Minimum changed blocks in image required to count as motion detected' min='0' max='" + String(mask_active * blocksPerMaskUnit) + "' value='" + String(Image_thresholdL) + "'> \n"; 
      message += " and <input type='number' style='width: 40px' name='dimageth' title='Maximum changed blocks in image required to count as motion detected' min='1' max='" + String(mask_active * blocksPerMaskUnit) + "' value='" + String(Image_thresholdH) + "'> blocks changed";
      message += " out of " + String(mask_active * blocksPerMaskUnit); 
               
    // input submit button  
      message += "<BR><BR><input type='submit' name='submit'><BR><BR>\n";

    // Toggle illuminator LED button
      message += "<input style='height: 30px;' name='illuminator' title='Toggle the Illumination LED On/Off' value='Light' type='submit'> \n";

    // Toggle 'use flash' button
      message += "<input style='height: 30px;' name='flash' title='Toggle use of flash when capturing image On/Off' value='Flash' type='submit'> \n";

    // Toggle motion detection
      message += "<input style='height: 30px;' name='detection' title='Motion detection enable/disable' value='Detection' type='submit'> \n";

#if EMAIL_ENABLED
    // Toggle email when motion detection
      message += "<input style='height: 30px;' name='email' value='Email' title='Send email when motion detected enable/disable' type='submit'> \n";
#endif

#if FTP_ENABLED
    // toggle FTP
      message += "<input style='height: 30px;' name='ftp' value='ftp' title='FTP images when motion detected enable/disable' type='submit'> \n";
#endif

    // Clear images in spiffs
      message += "<input style='height: 30px;' name='wipeS' value='Wipe Store' title='Delete all images stored in Spiffs' type='submit'> \n";
    
//    // Clear images on SD Card
//      message += "<input style='height: 30px;' name='wipeSD' value='Wipe SDCard' title='Delete all images on SD Card' type='submit'> \n";

    message += "</span></P>\n";    // end of section    
    message += webfooter();        // add the standard footer

    server.send(200, "text/html", message);      // send the web page
    message = "";      // clear string

}   // handle root 



// Action any user input on root web page
void rootButtons() {
    #if OTA_ENABLED
      // enable OTA if password supplied in url parameters   (?pass=xxx)
        if (server.hasArg("pwd")) {
            String Tvalue = server.arg("pwd");   // read value
              if (Tvalue == OTAPassword) {
                otaSetup();    // Over The Air updates (OTA)
                log_system_message("OTA enabled");
                OTAEnabled = 1;
              }
        }
    #endif

    // email was clicked -  if an email is sent when triggered 
      if (server.hasArg("email")) {
        if (!emailWhenTriggered) {
              log_system_message("Email when motion detected enabled");
              EMAILtimer = 0;
              emailWhenTriggered = 1;
        } else {
          log_system_message("Email when motion detected disabled"); 
          emailWhenTriggered = 0;
        }
        SaveSettingsSpiffs();     // save settings in Spiffs
      }
      
    // FTP was clicked -  FTP images when triggered 
      if (server.hasArg("ftp")) {
        if (!ftpImages) {
              log_system_message("FTP when motion detected enabled");
              ftpImages = 1;
        } else {
          log_system_message("FTP when motion detected disabled"); 
          ftpImages = 0;
        }
        SaveSettingsSpiffs();     // save settings in Spiffs
      }

   // if wipeS was entered  - clear Spiffs
      if (server.hasArg("wipeS")) WipeSpiffs();        // format Spiffs 

//    // if wipeSD was entered  - clear all stored images on SD Card    (I do not know how to do this)
//      if (server.hasArg("wipeSD")) {
//        log_system_message("Clearing all stored images (SD Card)"); 
//        fs::FS &fs = SD_MMC;
//        fs.format();     // not a valid command
//      }

    // if target brightness was entered - targetBrightness
      if (server.hasArg("daynight")) {
        String Tvalue = server.arg("daynight");   // read value
        int val = Tvalue.toInt();
        if (val >= 0 && val < 256 && val != targetBrightness) { 
          log_system_message("Target brightness changed to " + Tvalue ); 
          targetBrightness = val;
          SaveSettingsSpiffs();     // save settings in Spiffs
        }
      }
      
      // if dblockt was entered - Block_threshold
      if (server.hasArg("dblockt")) {
        String Tvalue = server.arg("dblockt");   // read value
        int val = Tvalue.toInt();
        if (val > 0 && val < 256 && val != Block_threshold) { 
          log_system_message("Block_threshold changed to " + Tvalue ); 
          Block_threshold = val;
          SaveSettingsSpiffs();     // save settings in Spiffs
        }
      }
      
    // if dimagetl was entered - min-image_threshold
      if (server.hasArg("dimagetl")) {
        String Tvalue = server.arg("dimagetl");   // read value
        int val = Tvalue.toInt();
        if (val >= 0 && val < 192 && val != Image_thresholdL) { 
          log_system_message("Min_day_image_threshold changed to " + Tvalue ); 
          Image_thresholdL = val;
          SaveSettingsSpiffs();     // save settings in Spiffs
        }
      }

    // if dimageth was entered - min-image_threshold
      if (server.hasArg("dimageth")) {
        String Tvalue = server.arg("dimageth");   // read value
        int val = Tvalue.toInt();
        if (val > 0 && val <= 192 && val != Image_thresholdH) { 
          log_system_message("Max_day_image_threshold changed to " + Tvalue ); 
          Image_thresholdH = val;
          SaveSettingsSpiffs();     // save settings in Spiffs
        }
      }
            
    #if IMAGE_SETTINGS       
      // if exposure was adjusted - cameraImageExposure
        if (server.hasArg("exp")) {
          String Tvalue = server.arg("exp");   // read value
          if (Tvalue != NULL) {
            int val = Tvalue.toInt();
            if (val >= 0 && val <= 1200 && val != cameraImageExposure) { 
              log_system_message("Camera exposure changed to " + Tvalue ); 
              cameraImageExposure = val;
              SaveSettingsSpiffs();         // save settings in Spiffs
              TRIGGERtimer = millis();      // reset last image captured timer (to prevent instant trigger)
            }
          }
        }

      // if image gain was adjusted - cameraImageGain
        if (server.hasArg("gain")) {
          String Tvalue = server.arg("gain");   // read value
            if (Tvalue != NULL) {
              int val = Tvalue.toInt();
              if (val >= 0 && val <= 31 && val != cameraImageGain) { 
                log_system_message("Camera gain changed to " + Tvalue ); 
                cameraImageGain = val;
                SaveSettingsSpiffs();        // save settings in Spiffs
                TRIGGERtimer = millis();     // reset last image captured timer (to prevent instant trigger)
              }
            }
         }
    #endif

//    // if image brightness was adjusted - cameraImageBrightness
//      if (server.hasArg("bright")) {
//        String Tvalue = server.arg("bright");   // read value
//          if (Tvalue != NULL) {
//            int val = Tvalue.toInt();
//            if (val >= -2 && val <= 2 && val != cameraImageBrightness) { 
//              log_system_message("Camera brightness changed to " + Tvalue ); 
//              cameraImageBrightness = val;
//              SaveSettingsSpiffs();        // save settings in Spiffs
//              TRIGGERtimer = millis();     // reset last image captured timer (to prevent instant trigger)
//            }
//          }
//       }
//
//    // if image contrast was adjusted - cameraImageContrast
//      if (server.hasArg("cont")) {
//        String Tvalue = server.arg("cont");   // read value
//          if (Tvalue != NULL) {
//            int val = Tvalue.toInt();
//            if (val >= -2 && val <= 2 && val != cameraImageContrast) { 
//              log_system_message("Camera contrast changed to " + Tvalue ); 
//              cameraImageContrast = val;
//              SaveSettingsSpiffs();        // save settings in Spiffs
//              TRIGGERtimer = millis();     // reset last image captured timer (to prevent instant trigger)
//            }
//          }
//       }

    
    // if mask grid check box array was altered
      if (server.hasArg("submit")) {                           // if submit button was pressed
        mask_active = 0;  
        bool maskChanged = 0;                                  // flag if the mask has changed
        for (int y = 0; y < mask_rows; y++) {
          for (int x = 0; x < mask_columns; x++) {
            if (server.hasArg(String(x) + String(y))) { 
              // set to active
              if (mask_frame[x][y] == 0) maskChanged = 1;      
              mask_frame[x][y] = 1;
              mask_active ++;
            } else {
              // set to disabled
              if (mask_frame[x][y] == 1) maskChanged = 1;    
              mask_frame[x][y] = 0;
            }
          }
        }
        if (maskChanged) {
          Image_thresholdH = mask_active * blocksPerMaskUnit;      // reset max trigger setting to max possible
          SaveSettingsSpiffs();                                       // save settings in Spiffs
          log_system_message("Detection mask updated"); 
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

      // if consecutive detections required - tCounterTrigger
      if (server.hasArg("consec")) {
        String Tvalue = server.arg("consec");   // read value
        int val = Tvalue.toInt();
        if (val > 0 && val <= 100 && val != tCounterTrigger) { 
          log_system_message("Consecutive detections required changed to " + Tvalue); 
          tCounterTrigger = val;
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
      
    // if button "toggle motion detection" was pressed  
      if (server.hasArg("detection")) {
        // button was pressed 
          if (DetectionEnabled == 0) {
            TRIGGERtimer = millis();                                // reset last image captured timer (to prevent instant 
            DetectionEnabled = 1;
            log_system_message("Motion detection enabled"); 
            TriggerTime = "Not since detection enabled";
          } else {
            DetectionEnabled = 0;
            log_system_message("Motion detection disabled");  
          }
          SaveSettingsSpiffs();                                     // save settings in Spiffs
      }
}

  
// ----------------------------------------------------------------
//     -data web page requested     i.e. http://x.x.x.x/data
// ----------------------------------------------------------------
// information displayed on the root web page which refreshes every few seconds

void handleData(){

  String message = 
      "<!DOCTYPE HTML>\n"
      "<html><body>\n"; 
          
  // Motion detection
    message += "<BR>Motion detection last triggered: " + TriggerTime + "\n";

  // display adnl info if detection is enabled
    if (DetectionEnabled == 1) {
        message += "<BR>Current detection level: " + String(latestChanges) + " changed blocks out of " + String(mask_active * blocksPerMaskUnit);
        latestChanges = 0;           // reset stored values once displayed
    }
         
  // show current time and current day/night mode
    message += "<BR>Current time: " + currentTime() +"\n";   

  // show image adjustments
    message += "<BR>Image brightness: " + String(AveragePix);
    message += ", Gain: " + String((int)cameraImageGain);
    message += ", Exposure: " + String((int)cameraImageExposure) + "\n";

  message += "<BR><BR>";

  // Motion detection
    if (DetectionEnabled == 1) message += " {" + green + "Detection enabled" + endcolour + "} ";
    else message += " {" + red + "Detection disabled" + endcolour + "} ";
 
  // Illumination LED
    if (digitalRead(Illumination_led) == ledON) message +=  " {" + red + "Illumination LED is On" + endcolour + "} ";
    if (UseFlash) message += " {Flash enabled} ";
          
  // OTA status
    #if OTA_ENABLED
      if (OTAEnabled) message += " {" + red + "OTA UPDATES ENABLED" + endcolour + "} ";
    #endif

  // FTP status
    #if FTP_ENABLED
      if (ftpImages) message += " {FTP enabled} ";
    #endif

  // email status
    #if EMAIL_ENABLED
        if (emailWhenTriggered) message += " {" + red + "Email sending enabled" + endcolour + "} ";
    #endif

//  // show io pin status
//    if (digitalRead(gioPin)) message += " {IO pin " + red + "High" + endcolour + "} ";
//    else message += " {IO pin " + green + "Low" + endcolour + "} ";

  // show if a sd card is present
    if (SD_Present) {
      uint16_t SDfreeSpace = (uint64_t)(SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024); 
      message += "<BR>SD-Card present - free space = " + String(SDfreeSpace) + "MB";
    }

  message += "</body></htlm>\n";
  
  server.send(200, "text/html", message);   // send reply as plain text
  message = "";      // clear string
}



// ----------------------------------------------------------------
//           -Display live image     i.e. http://x.x.x.x/live
// ----------------------------------------------------------------
// captures an image and displays it using the image view page

void handleLive(){

  log_system_message("Live image requested");      

  capturePhotoSaveSpiffs(UseFlash);          // capture an image from camera

  handleImages();
}


// ----------------------------------------------------------------
//    -Display captured images     i.e. http://x.x.x.x/images
// ----------------------------------------------------------------
// Image width in percent can be specified in URL with http://x.x.x.x/images?width=90

void handleImages(){

  log_system_message("Stored images page requested");   
  uint16_t ImageToShow = SpiffsFileCounter;     // set current image to display when /img called
  String ImageWidthSetting = "90";              // percentage of screen width to use for displaying the image

  // action any user input on page

    // if a image select button was pressed
      if (server.hasArg("button")) {
        String Bvalue = server.arg("button");   // read value
        int val = Bvalue.toInt();
        Serial.println("Button " + Bvalue + " was pressed");
        ImageToShow = val;     // select which image to display when /img called
      }
          
      // if a image width is specified in the URL    (i.e.  '...?width=')
      if (server.hasArg("width")) {
        String Bvalue = server.arg("width");   // read value
        uint16_t val = Bvalue.toInt();
        if (val >= 10 && val <= 100) ImageWidthSetting = String(val);
        else log_system_message("Error: Invalid image width specified in URL: " + Bvalue);
      }
      
  String message = webheader("#stdLink:hover { background-color: rgb(180, 180, 0);}");       // add the standard html header plus some extra styling

  message += "<FORM action='/images' method='post'>\n";               // used by the buttons (action = the page to send it to)

  message += "<H1>Stored Images</H1>\n";
  
  // create the image selection buttons
    for(int i=1; i <= MaxSpiffsImages; i++) {
        message += "<input style='height: 25px; ";
        if (i == ImageToShow) message += "background-color: #0f8;";
        message += "' name='button' value='" + String(i) + "' type='submit'>\n";
    }

  // Insert image time info. from text file
    String TFileName = "/" + String(ImageToShow) + ".txt";
    File file = SPIFFS.open(TFileName, "r");
    if (!file) message += red + "<BR>File not found" + endcolour + "\n";
    else {
      String line = file.readStringUntil('\n');      // read first line of text file
      message += "<BR>" + line +"\n";
    }
    file.close();

  // button to show small version of image in popup window
    message += blue + "<BR><a id='stdLink' target='popup' onclick=\"window.open('/img?pic=" + String(ImageToShow + 100) + "' ,'popup','width=320,height=240'); return false;\">PRE CAPTURE IMAGE</a>" + endcolour + "\n";

  // insert image in to html 
    message += "<BR><img id='img' alt='Camera Image' onerror='QpageRefresh();' width='" + ImageWidthSetting + "%' src='/img?pic=" + String(ImageToShow) + "'>\n";   

  // javascript to refresh the image if it fails to load (bug fix as it often rejects the request otherwise)
    message +=  "<script type='text/javascript'>\n"
                "  function QpageRefresh() {\n"
                "    setTimeout(function(){ document.getElementById('img').src='/img?pic=" + String(ImageToShow) + "'; }, " + JavaRefreshTime + ");\n"
                "  }\n"
                "</script>\n";

  message += webfooter();                      // add the standard footer

  server.send(200, "text/html", message);      // send the web page
  message = "";      // clear string
  
}


// ----------------------------------------------------------------
//      -ping web page requested     i.e. http://x.x.x.x/ping
// ----------------------------------------------------------------
// Responds with either 'enabled' or 'disabled'
// this can be used by automated scripts etc. to check the camera is operating ok

void handlePing(){

  log_system_message("ping web page requested");      
  String message = DetectionEnabled ? "enabled" : "disabled";

  server.send(404, "text/plain", message);   // send reply as plain text
  message = "";      // clear string
}


// ----------------------------------------------------------------
// -download the settings file from Spiffs     i.e. http://x.x.x.x/download
// ----------------------------------------------------------------
// Note - for future development, how to upload a file - https://tttapa.github.io/ESP8266/Chap12%20-%20Uploading%20to%20Server.html

//void handleDownload() {
//
//    log_system_message("Download settings web page requested");    
//
//    String TFileName = "settings.txt";
//    File download = SPIFFS.open("/" + TFileName, "r");
//    if (download) {
//      server.sendHeader("Content-Type", "text/text");
//      server.sendHeader("Content-Disposition", "attachment; filename="+TFileName);
//      server.sendHeader("Connection", "close");
//      server.streamFile(download, "application/octet-stream");
//      download.close();
//    }
//}


// ----------------------------------------------------------------
//   -Imagedata web page requested    i.e. http://x.x.x.x/imagedata
// ----------------------------------------------------------------
// display the raw greyscale image block data

void handleImagedata() {

    log_system_message("Imagedata webpage requested");     

    capture_still();         // capture current image

    // build the html 

    // add the standard header with some adnl style 
    String message = webheader("td {border: 1px solid grey; width: 30px; color: red;}");       

      message += "<P>\n";                // start of section
  
      message += "<br>RAW IMAGE DATA (Blocks) - Detection is ";
      message += DetectionEnabled ? "enabled" : "disabled";
     
      // show raw image data in html tables

      // difference between images table
      message += "<BR><center>Difference<BR><table>\n";
      for (int y = 0; y < H; y++) {
        message += "<tr>";
        for (int x = 0; x < W; x++) {
          uint16_t timg = abs(current_frame[y][x] - prev_frame[y][x]);
          message += generateTD(timg);
        }         
        message += "</tr>\n";
      }
      message += "</table>";
      
      // current image table
      message += "<BR><BR>Current Frame<BR><table>\n";
      for (int y = 0; y < H; y++) {
        message += "<tr>";
        for (int x = 0; x < W; x++) {
          message += generateTD(current_frame[y][x]);       
        }
        message += "</tr>\n";
      }
      message += "</table>";

      // previous image table
      message += "<BR><BR>Previous Frame<BR><table>\n";
      for (int y = 0; y < H; y++) {
        message += "<tr>";
        for (int x = 0; x < W; x++) {
          message += generateTD(prev_frame[y][x]);       
        }
        message += "</tr>\n";
      }
      message += "</table></center>\n";

      message += "<BR>If detection is disabled the previous frame only updates when this page is refreshed, ";
      message += "otherwise it automatically refreshes around twice a second";
      message += "<BR>Each block shown here is the average reading from 16x12 pixels on the camera image, ";
      message += "The detection mask selection works on 4x4 groups of blocks";
      
      message += "<BR>\n" + webfooter();     // add standard footer html
    

    server.send(200, "text/html", message);    // send the web page

    if (!DetectionEnabled) update_frame();     // if detection disabled copy this frame to previous 
}


// generate the html for a table cell 
String generateTD(uint16_t idat) {
          String bcol = String(idat, HEX);     // block color in hex  (for style command 'background-color: #fff;')
          if (bcol.length() == 1) bcol = "0" + bcol;
          return "<td style='background-color: #" + bcol + bcol + bcol + ";'>" + String(idat) + "</td>";
}


// ----------------------------------------------------------------
//   -bootlog web page requested    i.e. http://x.x.x.x/bootlog
// ----------------------------------------------------------------
// display boot log from Spiffs

void handleBootLog() {

   log_system_message("bootlog webpage requested");     

    // build the html for /bootlog page

    String message = webheader();     // add the standard header

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
//  last stored image page requested     i.e. http://x.x.x.x/img
// ----------------------------------------------------------------
// pic parameter on url selects which file to display
//   if this is (MaxSpiffsImages + 1) this means display of live greyscale image is required

void handleImg(){
    
    uint16_t ImageToShow = MaxSpiffsImages + 1;     // select live greyscale image as default
        
    // if a image to show is specified in url
      if (server.hasArg("pic")) {
        String Bvalue = server.arg("pic");
        ImageToShow = Bvalue.toInt();
        // if (ImageToShow < 0 || ImageToShow > (MaxSpiffsImages + 1)) ImageToShow = MaxSpiffsImages + 1;
      } 
      if (ImageToShow == 0) ImageToShow = SpiffsFileCounter;   // set to most recent image

    String TFileName = "/" + String(ImageToShow) + ".jpg";
    
    if (ImageToShow > 100) {               // show small image    e.g. 101 = '1s.jpg' 
        ImageToShow = ImageToShow -100;
        TFileName = "/" + String(ImageToShow) + "s.jpg";
    }
      
    if (ImageToShow == (MaxSpiffsImages + 1)) {           // live greyscale image requested ("grey");                         // capture live greyscale image
      saveGreyscaleFrame("grey");                         // capture live greyscale image
      TFileName = "/grey.jpg";
    } else log_system_message("display stored image requested: " + String(ImageToShow));

    // send image file
        File f = SPIFFS.open(TFileName, "r");                         // read file from spiffs
            if (!f) Serial.println("Error reading " + TFileName);
            else {
                size_t sent = server.streamFile(f, "image/jpeg");     // send file to web page
                if (!sent) Serial.println("Error sending " + TFileName);
                f.close();               
            }
}


// ----------------------------------------------------------------
//        capture images - store in Spiffs/SD card and FTP
// ----------------------------------------------------------------

bool capturePhotoSaveSpiffs(bool UseFlash) {

  if (DetectionEnabled == 1) DetectionEnabled = 2;      // pause motion detecting while photo is captured (don't think this is required as only one core on esp32-cam?)

  // increment image count
    SpiffsFileCounter++;
    if (SpiffsFileCounter > MaxSpiffsImages) SpiffsFileCounter = 1;
    SaveSettingsSpiffs();     // save settings in Spiffs
    
  // first quickly grab a greyscale image 
    saveGreyscaleFrame(String(SpiffsFileCounter) + "s");
    
  // Capture a high res image
    
    RestartCamera(PIXFORMAT_JPEG);      // restart camera in jpg mode to take a photo (uses greyscale mode for motion detection)
  
    bool ok = 0;          // Boolean to indicate if the picture has been taken correctly
    byte TryCount = 0;    // attempt counter to limit retries
  
    do {           // try up to 3 times to capture/save image
      TryCount ++;     
      if (UseFlash && cameraImageGain > 0)  digitalWrite(Illumination_led, ledON);   // turn Illuminator LED on if it is dark and it is enabled
      Serial.println("Taking a photo... attempt #" + String(TryCount));
      saveJpgFrame(String(SpiffsFileCounter));                                 // capture and save/ftp image
      ok = checkPhoto(SPIFFS, "/" + String(SpiffsFileCounter) + ".jpg");       // check if file has been correctly saved in SPIFFS
    } while ( !ok && TryCount < 3);                                            // if there was a problem taking photo try again 
  
    RestartCamera(PIXFORMAT_GRAYSCALE);                                        // restart camera back to greyscale mode for motion detection
  
    TRIGGERtimer = millis();                                                   // reset retrigger timer to stop instant motion trigger
    if (DetectionEnabled == 2) DetectionEnabled = 1;                           // restart paused motion detecting 
  
    bool tres = (TryCount == 3);
    if (tres) log_system_message("Error: Unable to capture/store image");
    return (!tres);
}


// check file saved to spiffs ok - by making sure file exists and is greater than 100 bytes
bool checkPhoto( fs::FS &fs, String IFileName ) {
  File f_pic = fs.open( IFileName );
  uint16_t pic_sz = f_pic.size();
  bool tres = ( pic_sz > 100 );
  if (!tres) log_system_message("Error: Problem detected verifying image stored in Spiffs");
  f_pic.close();
  return (tres);
}


// ----------------------------------------------------------------
//              -restart the camera in different mode
// ----------------------------------------------------------------
// switches camera mode - format = PIXFORMAT_GRAYSCALE or PIXFORMAT_JPEG

void RestartCamera(pixformat_t format) {
    esp_camera_deinit();
      if (format == PIXFORMAT_JPEG) config.frame_size = FRAME_SIZE_PHOTO;
      else if (format == PIXFORMAT_GRAYSCALE) config.frame_size = FRAME_SIZE_MOTION;
      else Serial.println("ERROR: Invalid image format");
      config.pixel_format = format;
    bool ok = esp_camera_init(&config);
    if (ok == ESP_OK) Serial.println("Camera mode switched ok");
    else {
      // failed so try again
        esp_camera_deinit();
        delay(50);
        ok = esp_camera_init(&config);
        if (ok == ESP_OK) Serial.println("Camera mode switched ok - 2nd attempt");
        else {
          UpdateBootlogSpiffs("Camera failed to restart so rebooting camera");        // store in bootlog
          RebootCamera(format);
        }
    }
    TRIGGERtimer = millis();        // reset last image captured timer (to prevent instant trigger)
}


// reboot camera (used if camera is failing to respond)
//      restart camera in motion mode, capture a test frame to check it is now responding ok
//      format = PIXFORMAT_GRAYSCALE or PIXFORMAT_JPEG

void RebootCamera(pixformat_t format) {  
    log_system_message("ERROR: Problem with camera detected so resetting it"); 
    // turn camera off then back on      
        digitalWrite(PWDN_GPIO_NUM, HIGH);
        delay(300);
        digitalWrite(PWDN_GPIO_NUM, LOW); 
        delay(300);
    RestartCamera(PIXFORMAT_GRAYSCALE);    // restart camera in motion mode
    delay(300);
    // try capturing a frame, if still problem reboot esp32
        if (!capture_still()) {
            UpdateBootlogSpiffs("Camera failed to reboot so rebooting esp32");    // store in bootlog
            delay(500);
            ESP.restart();   
            delay(5000);      // restart will fail without this delay
         }
    if (format == PIXFORMAT_JPEG) RestartCamera(PIXFORMAT_JPEG);                  // if jpg mode required restart camera again
}


// ----------------------------------------------------------------
//                       -wipe/format Spiffs
// ----------------------------------------------------------------

bool WipeSpiffs() {
        log_system_message("Formatting/Wiping Spiffs"); 
        bool wres = SPIFFS.format();
        if (!wres) {
          log_system_message("Error: Unable to format Spiffs");
          return 0;
        }
        SpiffsFileCounter = 0;
        TriggerTime = "Not since Spiffs wiped";
        UpdateBootlogSpiffs("Spiffs Wiped");                           // store event in bootlog file
        SaveSettingsSpiffs();                                          // save settings in Spiffs
        return 1;
}

      
// ----------------------------------------------------------------
//              Save jpg in spiffs/sd card and FTP
// ----------------------------------------------------------------
// filesName = name of jpg to save as in spiffs

void saveJpgFrame(String filesName) {

    // file names to use 
      String IFileName = "/" + String(SpiffsFileCounter) + ".jpg";      // file name for Spiffs
      String TFileName = "/" + String(SpiffsFileCounter) + ".txt";
      String SDfilename = "/" + currentTime() + ".jpg";                 // file name for sd card
      String FTPfilename = currentTime() + "-L";                        // file name for FTP
     
    // restore LED status after using it as a camera flash  (if flashmode = 2 turn it of before taking photo)
      if (flashMode == 2) {
        if (ReqLEDStatus) digitalWrite(Illumination_led, ledON);   
        else digitalWrite(Illumination_led, ledOFF);
      }

    // grab frame
      cameraImageSettings(FRAME_SIZE_PHOTO);            // apply camera sensor settings
      camera_fb_t *fb = esp_camera_fb_get();            // capture frame from camera
      if (!fb) {
        Serial.println("Camera capture failed - rebooting camera");
        RebootCamera(PIXFORMAT_JPEG);
        cameraImageSettings(FRAME_SIZE_PHOTO);          // apply camera sensor settings
        fb = esp_camera_fb_get();                       // try again to capture frame
      }

    // restore LED status after using it as a camera flash (any flashmode)
        if (ReqLEDStatus) digitalWrite(Illumination_led, ledON);   
        else digitalWrite(Illumination_led, ledOFF);
      
   if (fb) {        // only attempt to save images if one was captured ok 

      // ------------------- save image to Spiffs -------------------
        
      SPIFFS.remove(IFileName);                          // delete old image file if it exists
      File file = SPIFFS.open(IFileName, FILE_WRITE);
      if (!file) log_system_message("Failed to create file in Spiffs");
      else {
        if (file.write(fb->buf, fb->len)) {    
          Serial.print("The picture has been saved as ");
          Serial.print(IFileName);
          Serial.print(" - Size: ");
          Serial.print(file.size());
          Serial.println(" bytes");
        } else {
          log_system_message("Error: writing image to Spiffs...will format and try again");
          WipeSpiffs();     // format spiffs 
          // reset image counter and name
            SpiffsFileCounter = 1;
            IFileName = "/" + String(SpiffsFileCounter) + ".jpg";      // file name for Spiffs
            TFileName = "/" + String(SpiffsFileCounter) + ".txt";
          file = SPIFFS.open(IFileName, FILE_WRITE);
          if (!file.write(fb->buf, fb->len)) log_system_message("Error: Still unable to write image to Spiffs");
        }
      }
      file.close();
      
      // save text file to spiffs with time info. 
        SPIFFS.remove(TFileName);   // delete old file with same name if present
        file = SPIFFS.open(TFileName, "w");
        if (!file) log_system_message("Error: Failed to create date file in spiffs");
        else file.println(currentTime());
        file.close();
  
      // ------------------- save image to SD Card -------------------
      
      if (SD_Present) {
  
        fs::FS &fs = SD_MMC; 
                       
        // save image
          file = fs.open(SDfilename, FILE_WRITE);
          if (!file) log_system_message("Error: Failed to create file on sd-card: " + SDfilename);
          else {
              if (file.write(fb->buf, fb->len)) Serial.println("Saved image to sd card");
              else log_system_message("Error: failed to save image to sd card");
              file.close();
          }
      }    
      
      // ------------------ ftp images to server -------------------
  
      #if FTP_ENABLED
        if (ftpImages) uploadImageByFTP(fb->buf, fb->len, FTPfilename);
      #endif
  
    } else Serial.println("Capture of image failed");  

  esp_camera_fb_return(fb);    // return frame so memory can be released

}


// ----------------------------------------------------------------
//       Save greyscale frame as jpg in spiffs/sd card/FTP
// ----------------------------------------------------------------
// filesName = name of jpg to save as in spiffs

void saveGreyscaleFrame(String filesName) {

  // filenames to use
    String IFileName = "/" + filesName +".jpg";              // file name in spiffs
    String SDFileName = "/" + currentTime() + "-S.jpg";      // file name on sd card
    
  // grab greyscale frame
    uint8_t * _jpg_buf;
    size_t _jpg_buf_len;
    camera_fb_t * fb = NULL;    // pointer
    cameraImageSettings(FRAME_SIZE_MOTION);      // apply camera sensor settings
    fb = esp_camera_fb_get();

  // convert greyscale to jpg  
    bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
    esp_camera_fb_return(fb);
    if (!jpeg_converted){
        log_system_message("grey to jpg image conversion failed");
        return;
    }

  // save image to spiffs
    SPIFFS.remove(IFileName);                              // delete old image file if it exists
    File file = SPIFFS.open(IFileName, FILE_WRITE);
    if (!file) log_system_message("Error: creating grey file on Spiffs");
    else {
      if (!file.write(_jpg_buf, _jpg_buf_len)) log_system_message("Error: writing grey image to Spiffs");
    }
    file.close();

  // save image to sd card
    if (SD_Present) {
      fs::FS &fs = SD_MMC; 
      file = fs.open(SDFileName, FILE_WRITE);
      if (!file) log_system_message("Error: creating grey image on sd-card");
      else {
          if (file.write(_jpg_buf, _jpg_buf_len)) Serial.println("Saved grey image to sd card");
          else log_system_message("Error: writing grey image to sd card"); 
          file.close();
      }
    }   

  // ftp to server
    #if FTP_ENABLED
      if (ftpImages) uploadImageByFTP(_jpg_buf, _jpg_buf_len, currentTime() + "-S");
    #endif

  esp_camera_fb_return(fb);    // return frame so memory can be released

}

  
// ----------------------------------------------------------------
//                       -gpio input has triggered
// ----------------------------------------------------------------

void ioDetected(bool iostat) {

  if (DetectionEnabled == 1) DetectionEnabled = 2;                       // pause motion detecting (prob. not required?)
  
    log_system_message("IO input has triggered - status = " + String(iostat)); 

    // int capres = capturePhotoSaveSpiffs(UseFlash);                       // capture an image

    // TRIGGERtimer = millis();                                             // reset retrigger timer to stop instant motion trigger

  if (DetectionEnabled == 2) DetectionEnabled = 1;                       // restart paused motion detecting
}


// ----------------------------------------------------------------
//                       -motion has been detected
// ----------------------------------------------------------------

void MotionDetected(uint16_t changes) {

  if (DetectionEnabled == 1) DetectionEnabled = 2;                        // pause motion detecting (prob. not required?)
  
    log_system_message("Camera detected motion: " + String(changes)); 
    TriggerTime = currentTime() + " - " + String(changes) + " out of " + String(mask_active * blocksPerMaskUnit);    // store time of trigger and motion detected

    int capres = capturePhotoSaveSpiffs(UseFlash);                                     // capture an image

#if EMAIL_ENABLED
    // send email if long enough since last motion detection (or if this is the first one)
    if (emailWhenTriggered) {       // && cameraImageGain == 0
        unsigned long currentMillis = millis();        // get current time  
        if ( ((unsigned long)(currentMillis - EMAILtimer) >= (EmailLimitTime * 1000)) || (EMAILtimer == 0) ) {

          EMAILtimer = currentMillis;    // reset timer 
      
          // send an email
              String emessage = "Camera triggered at " + currentTime();
              if (!capres) emessage += "\nNote: there was a problem detected when capturing an image";
              byte q = sendEmail(emailReceiver,"Message from CameraWifiMotion", emessage);    
              if (q==0) log_system_message("email sent ok" );
              else log_system_message("Error: sending email, error code=" + String(q) );
  
         }
         else log_system_message("Too soon to send another email");
    }
#endif

  TRIGGERtimer = millis();                                       // reset retrigger timer to stop instant motion trigger
  if (DetectionEnabled == 2) DetectionEnabled = 1;               // restart paused motion detecting
}


// ----------------------------------------------------------------
//           -testing page     i.e. http://x.x.x.x/test
// ----------------------------------------------------------------

void handleTest(){

  log_system_message("Testing page requested");      

  String message = webheader();                                      // add the standard html header

  message += "<BR>TEST PAGE<BR><BR>\n";

  // ---------------------------- test section here ------------------------------


//  // send a test email
//    #if EMAIL_ENABLED     
//      capturePhotoSaveSpiffs(0);
//      // send email
//        String emessage = "Test email";
//        byte q = sendEmail(emailReceiver,"Message from CameraWifiMotion sketch", emessage);    
//        if (q==0) log_system_message("email sent ok" );
//        else log_system_message("Error: sending email code=" + String(q) );
//    #endif

  // list all files stored in spiffs
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    message += "Files stored in SPIFFS:<BR>";
    while(file) {
        message += String(file.name()) + " - " + String(file.size()) + "<BR>";
        file = root.openNextFile();
    }
    root.close();

       
  // -----------------------------------------------------------------------------

  message += webfooter();                      // add the standard footer

    
  server.send(200, "text/html", message);      // send the web page
  message = "";      // clear string
  
}


// --------------------------- E N D -----------------------------
