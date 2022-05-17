/*******************************************************************************************************************
 *
 *                      Basic web server For ESP8266/ESP32 using Arduino IDE or PlatformIO
 *
 *                                   https://github.com/alanesq/BasicWebserver
 *
 *                     Tested with ESP32 board managers espressif32@3.5.0 / espressif8266@3.2.0
 *
 *                   Included files: email.h, standard.h, ota.h, oled.h, neopixel.h, gsm.h & wifi.h
 *
 *
 *      I use this sketch as the starting point for most of my ESP based projects.   It is the simplest way
 *      I have found to provide a basic web page displaying updating information, control buttons etc..
 *      It also has the ability to retrieve a web page as text (see: requestWebPage in wifi.h).
 *      For a more advanced method examples see:
 *                 https://github.com/alanesq/BasicWebserver/blob/master/misc/VeryBasicWebserver.ino
 *
 *
 *      Note:  To add ESP8266/32 ability to the Arduino IDE enter the below two lines in to FILE/PREFERENCES/BOARDS MANAGER
 *                 http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *                 https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
 *             You can then add them in TOOLS/BOARD/BOARDS MANAGER (search for esp8266 or ESP32)
 *
 *      First time the ESP starts it will create an access point "ESPPortal" which you need to connect to in order to enter your wifi details.
 *             default password = "12345678"   (change this in wifi.h)
 *             see: https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password
 *
 *      Much of the sketch is the code from other people's work which I have combined together, I believe I have links
 *      to all the sources but let me know if I have missed anyone.
 *
 *                                                                                      Created by: www.alanesq.eu5.net
 *
 *        Distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 *        implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 ********************************************************************************************************************

 Lots of great info: https://randomnerdtutorials.com  or  https://techtutorialsx.com or https://www.baldengineer.com/

 Handy way to try out your C++ code: https://coliru.stacked-crooked.com/

 GPIO pin info:  esp32: https://electrorules.com/esp32-pinout-reference/
               esp8266: https://electrorules.com/esp8266-nodemcu-pinout-reference/

*/

#if (!defined ESP8266 && !defined ESP32)
  #error This code is for ESP8266 or ESP32 only
#endif


// ---------------------------------------------------------------


// Required by PlatformIO

  #include <Arduino.h>                      // required by PlatformIO

  // forward declarations
    void log_system_message(String smes);   // in standard.h
    void displayMessage(String, String);    // in oled.h
    void handleRoot();
    void handleData();
    void handlePing();
    void settingsEeprom(bool eDirection);
    void handleTest();


// ---------------------------------------------------------------
//                          -SETTINGS
// ---------------------------------------------------------------

  const char* stitle = "BasicWebServer";                 // title of this sketch

  const char* sversion = "17May22";                      // version of this sketch

  const bool serialDebug = 1;                            // provide debug info on serial port

  #define ENABLE_EEPROM 1                                // if some settings are to be stored in eeprom
  // Note on esp32 this should really be replaced with preferences.h - see: by https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/

  #define ENABLE_OLED_MENU 1                             // Enable OLED / rotary encoder based menu

  #define ENABLE_GSM 0                                   // Enable GSM board support (not yet fully working - oct21)

  #define ENABLE_EMAIL 1                                 // Enable E-mail support

  #define ENABLE_NEOPIXEL 0                              // Enable Neopixel support

  #define ENABLE_OTA 1                                   // Enable Over The Air updates (OTA)
  const String OTAPassword = "password";                 // Password to enable OTA service (supplied as - http://<ip address>?pwd=xxxx )

  const char HomeLink[] = "/";                           // Where home button on web pages links to (usually "/")

  int dataRefresh = 2;                                   // Refresh rate of the updating data on web page (seconds)

  const byte LogNumber = 30;                             // number of entries in the system log
  const uint16_t ServerPort = 80;                        // ip port to serve web pages on

  const byte onboardLED = 2;                             // indicator LED pin - 2 or 16 on esp8266 nodemcu, 3 on esp8266-01, 2 on ESP32, 22 on esp32 lolin lite

  const byte onboardButton = 0;                          // onboard button gpio (FLASH)

  const bool ledBlinkEnabled = 1;                        // enable blinking status LED
  const uint16_t ledBlinkRate = 1500;                    // Speed to blink the status LED (milliseconds) - also perform some system tasks

  const int serialSpeed = 115200;                        // Serial data speed to use


// ---------------------------------------------------------------


int _TEMPVARIABLE_ = 1;                 // Temporary variable used in demo radio buttons on root web page and neopixel demo

bool OTAEnabled = 0;                    // flag to show if OTA has been enabled (via supply of password in http://x.x.x.x/ota)
bool GSMconnected = 0;                  // flag toshow if the gsm module is connected ok (in gsm.h - not fully working)
bool wifiok = 0;                        // flag to show if wifi connection is ok

#include "wifi.h"                       // Load the Wifi / NTP stuff
#include "standard.h"                   // Some standard procedures

Led statusLed1(onboardLED, LOW);        // set up onboard LED - see standard.h
Button button1(onboardButton, HIGH);    // set up the onboard flash button - see standard.h

#if ENABLE_EEPROM
  #include <EEPROM.h>                     // for storing settings in eeprom
#endif

#if ENABLE_OTA
  #include "ota.h"                      // Over The Air updates (OTA)
#endif

#if ENABLE_GSM
  #include "gsm.h"                      // GSM board
#endif

#if ENABLE_NEOPIXEL
  #include "neopixel.h"                 // Neopixels
#endif

#if ENABLE_OLED_MENU
  #include "oled.h"                     // OLED display - i2c version SSD1306
#endif

#if ENABLE_EMAIL
    #define _SenderName "ESP"           // name of email sender (no spaces)
    #include "email.h"
#endif


// ---------------------------------------------------------------
//    -SETUP     SETUP     SETUP     SETUP     SETUP     SETUP
// ---------------------------------------------------------------
//
// setup section (runs once at startup)

void setup() {

  if (serialDebug) {       // if serialdebug info. is enabled
    Serial.begin(serialSpeed); while (!Serial); delay(50);       // start serial comms
    delay(2000);      // gives platformio chance to start serial monitor

    Serial.println("\n\n\n");                                    // some line feeds
    Serial.println("-----------------------------------");
    Serial.printf("Starting - %s - %s \n", stitle, sversion);
    Serial.println("-----------------------------------");
    Serial.println( "Device type: " + String(ARDUINO_BOARD) + ", chip id: " + String(ESP_getChipId(), HEX));
    // Serial.println(ESP.getFreeSketchSpace());
    #if defined(ESP8266)
        Serial.println("Chip ID: " + ESP.getChipId());
        rst_info *rinfo = ESP.getResetInfoPtr();
        Serial.println("ResetInfo: " + String((*rinfo).reason) + ": " + ESP.getResetReason());
        Serial.printf("Flash chip size: %d (bytes)\n", ESP.getFlashChipRealSize());
        Serial.printf("Flash chip frequency: %d (Hz)\n", ESP.getFlashChipSpeed());
        Serial.printf("\n");
    #endif
  }

  #if ENABLE_NEOPIXEL    // initialise Neopixels
    neopixelSetup();
  #endif

  #if ENABLE_GSM
    GSMSetup();          // initialise GSM board
  #endif

  // if (serialDebug) Serial.setDebugOutput(true);         // to enable extra diagnostic info

  statusLed1.on();                                         // turn status led on until wifi has connected (see standard.h)

  #if ENABLE_OLED_MENU
    oledSetup();         // initialise the oled display - see oled.h
    displayMessage(stitle, sversion);
  #endif

  #if ENABLE_EEPROM
    settingsEeprom(0);       // read stored settings from eeprom
  #endif

  startWifiManager();                                      // Connect to wifi (see wifi.h)

  WiFi.mode(WIFI_STA);     // turn off access point - options are WIFI_AP, WIFI_STA, WIFI_AP_STA or WIFI_OFF
    //    // configure as wifi access point as well
    //    Serial.println("starting access point");
    //    WiFi.softAP("ESP-AP", "password");               // access point settings (Note: password must be 8 characters for some reason - this may no longer be true?)
    //    WiFi.mode(WIFI_AP_STA);                          // enable as both Station and access point - options are WIFI_AP, WIFI_STA, WIFI_AP_STA or WIFI_OFF
    //    IPAddress myIP = WiFi.softAPIP();
    //    if (serialDebug) Serial.print("Access Point Started - IP address: ");
    //    Serial.println(myIP);

  // set up web page request handling
    server.on(HomeLink, handleRoot);         // root page
    server.on("/data", handleData);          // supplies information to update root page via Ajax)
    server.on("/ping", handlePing);          // ping requested
    server.on("/log", handleLogpage);        // system log (in standard.h)
    server.on("/test", handleTest);          // testing page
    server.on("/reboot", handleReboot);      // reboot the esp
    server.onNotFound(handleNotFound);       // invalid page requested
    #if ENABLE_OTA
      server.on("/ota", handleOTA);          // ota updates web page
    #endif

  // start web server
    if (serialDebug) Serial.println("Starting web server");
    server.begin();

  // Stop wifi going to sleep (if enabled it can cause wifi to drop out randomly especially on esp8266 boards)
    #if defined ESP8266
      WiFi.setSleepMode(WIFI_NONE_SLEEP);
    #elif defined ESP32
      WiFi.setSleep(false);
    #endif

  // Finished connecting to network
    statusLed1.off();                        // turn status led off
    log_system_message("Started, ip=" + WiFi.localIP().toString());
}


// ----------------------------------------------------------------
//   -LOOP     LOOP     LOOP     LOOP     LOOP     LOOP     LOOP
// ----------------------------------------------------------------

void loop(){

    #if defined(ESP8266)
        yield();                      // allow esp8266 to carry out wifi tasks (may restart randomly without this)
    #endif
    server.handleClient();            // service any web page requests

//    // check if the onboard button has been pressed
//      if button1.beenPressed() ......

    #if ENABLE_NEOPIXEL
      neoLoop();                      // handle neopixel updates
    #endif

    #if ENABLE_OLED_MENU
        oledLoop();                   // handle oled menu system
    #endif

    #if ENABLE_GSM
        GSMloop();                    // handle the GSM board
    #endif

    #if ENABLE_EMAIL
        EMAILloop();                  // handle emails
    #endif




           // YOUR CODE HERE !




    // Periodically change the LED status to indicate all is well
      static repeatTimer ledTimer;                          // set up a repeat timer (see standard.h)
      if (ledTimer.check(ledBlinkRate)) {                   // repeat at set interval (ms)
          bool allOK = 1;                                   // if all checks leave this as 1 then the LED is flashed
          if (!WIFIcheck()) allOK = 0;                      // if wifi connection is not ok
          if (timeStatus() != timeSet) allOK = 0;           // if NTP time is not updating ok
          #if ENABLE_GSM
            if (!GSMconnected) allOK = 0;                   // if GSM board is not responding ok
          #endif
          time_t t=now();                                   // read current time to ensure NTP auto refresh keeps triggering (otherwise only triggers when time is required causing a delay in response)
          if (t == 0) t=0;                                  // pointless line to stop PlatformIO getting upset that the variable is not used
          // blink status led
            if (ledBlinkEnabled && allOK) {
              statusLed1.flip(); // invert the LED status if all OK (see standard.h)
            } else {
              statusLed1.off();
            }
      }
}


// ----------------------------------------------------------------
//            -Action any user input on root web page
// ----------------------------------------------------------------

void rootUserInput(WiFiClient &client) {

  // if value1 was entered
    if (server.hasArg("value1")) {
    String Tvalue = server.arg("value1");   // read value
    if (Tvalue != NULL) {
      int val = Tvalue.toInt();
      log_system_message("Value entered on root page = " + Tvalue );
    }
  }

  // if radio button "RADIO1" was selected
    if (server.hasArg("RADIO1")) {
      String RADIOvalue = server.arg("RADIO1");   // read value of the "RADIO" argument
      //if radio button 1 selected
      if (RADIOvalue == "1") {
        log_system_message("radio button 1 selected");
        _TEMPVARIABLE_ = 1;
      }
      //if radio button 2 selected
      if (RADIOvalue == "2") {
        log_system_message("radio button 2 selected");
        _TEMPVARIABLE_ = 2;
      }
      //if radio button 3 selected
      if (RADIOvalue == "3") {
        log_system_message("radio button 2 selected");
        _TEMPVARIABLE_ = 3;
      }
    }

    // if button "demobutton" was pressed
      if (server.hasArg("demobutton")) {
          log_system_message("demo button was pressed");
      }
}


// ----------------------------------------------------------------
//       -root web page requested    i.e. http://x.x.x.x/
// ----------------------------------------------------------------

void handleRoot() {

  int rNoLines = 4;      // number of updating information lines to be populated via http://x.x.x.x/data

  WiFiClient client = server.client();             // open link with client
  webheader(client);                               // html page header  (with extra formatting)

// set the web page's title?
//    client.printf("\n<script>document.title = \"%s\";</script>\n", stitle);

  // log page request including clients IP
    IPAddress cip = client.remoteIP();
    String clientIP = decodeIP(cip.toString());   // check for known IP addresses
    log_system_message("Root page requested from: " + clientIP);

  rootUserInput(client);     // Action any user input from this page

  // Build the HTML
    client.printf("<FORM action='%s' method='post'>\n", HomeLink);     // used by the buttons
    client.printf("<P><h2>Welcome to the BasicWebServer, running on an %s</h2>\n", ARDUINO_BOARD);


// ---------------------------------------------------------------------------------------------
//  info which is periodically updated using AJAX - https://www.w3schools.com/xml/ajax_intro.asp

// insert empty lines which are later populated via vbscript with live data from http://x.x.x.x/data in the form of comma separated text
  for (int i = 0; i < rNoLines; i++) {
    client.println("<span id='uline" + String(i) + "'></span><br>");
  }

// Javascript - to periodically update the above info lines from http://x.x.x.x/data
// This is the below javascript code compacted to save flash memory via https://www.textfixer.com/html/compress-html-compression.php
   client.printf(R"=====(  <script> function getData() { var xhttp = new XMLHttpRequest(); xhttp.onreadystatechange = function() { if (this.readyState == 4 && this.status == 200) { var receivedArr = this.responseText.split(','); for (let i = 0; i < receivedArr.length; i++) { document.getElementById('uline' + i).innerHTML = receivedArr[i]; } } }; xhttp.open('GET', 'data', true); xhttp.send();} getData(); setInterval(function() { getData(); }, %d); </script> )=====", dataRefresh * 1000);
/*
  // get a comma seperated list from http://x.x.x.x/data and populate the blank lines in html above
  client.printf(R"=====(
     <script>
        function getData() {
          var xhttp = new XMLHttpRequest();
          xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            var receivedArr = this.responseText.split(',');
            for (let i = 0; i < receivedArr.length; i++) {
              document.getElementById('uline' + i).innerHTML = receivedArr[i];
            }
          }
        };
        xhttp.open('GET', 'data', true);
        xhttp.send();}
        getData();
        setInterval(function() { getData(); }, %d);
     </script>
  )=====", dataRefresh * 1000);
*/


// ---------------------------------------------------------------------------------------------


    // demo enter a value
      client.println("<br><br>Enter a value: <input type='number' style='width: 40px' name='value1' title='additional info which pops up'> ");
      client.println("<input type='submit' name='submit'>");

    // demo radio buttons - "RADIO1"
      client.print(R"=====(
        <br><br>Demo radio buttons
        <br>Radio1 button1<INPUT type='radio' name='RADIO1' value='1'>
        <br>Radio1 button2<INPUT type='radio' name='RADIO1' value='2'>
        <br>Radio1 button3<INPUT type='radio' name='RADIO1' value='3'>
        <br><INPUT type='reset'> <INPUT type='submit' value='Action'>
      )=====");

    // demo standard button
    //    'name' is what is tested for above to detect when button is pressed, 'value' is the text displayed on the button
      client.println("<br><br><input style='");
      // if ( x == 1 ) client.println("background-color:red; ");      // to change button color depending on state
      client.println("height: 30px;' name='demobutton' value='Demonstration Button' type='submit'>\n");


    // demo of how to display an updating image   e.g. when using esp32cam
    /*
      client.write("<br><a href='/jpg'>");                                              // make it a link
      client.write("<img id='image1' src='/jpg' width='320' height='240' /> </a>");     // show image from http://x.x.x.x/jpg
      // javascript to refresh the image periodically
        int imagerefresh = 2;                                                           // how often to update (seconds)
        client.printf(R"=====(
           <script>
             function refreshImage(){
                 var timestamp = new Date().getTime();
                 var el = document.getElementById('image1');
                 var queryString = '?t=' + timestamp;
                 el.src = '/jpg' + queryString;
             }
             setInterval(function() { refreshImage(); }, %d);
           </script>
        )=====", imagerefresh * 1000);
    */

    // close page
      client.println("</P>");                                // end of section
      client.println("</form>\n");                           // end form section (used by buttons etc.)
      webfooter(client);                                     // html page footer
      delay(3);
      client.stop();
}


// ----------------------------------------------------------------
//     -data web page requested     i.e. http://x.x.x.x/data
// ----------------------------------------------------------------
// the root web page requests this periodically via Javascript in order to display updating information.
// information in the form of comma seperated text is supplied which are then inserted in to blank lines on the web page
// Note: to change the number of lines displayed update variable 'rNoLines' at the top of handleroot()

void handleData(){

   String reply = "";

   // line1 - current time
     reply += "Time: " + currentTime();
     reply += ",";        // end of line

   // line2 - last ip client connection
     reply += "Last IP client connected: " + lastClient;
     reply += ",";        // end of line

   // line3 - onboard button status
     reply += "The onboard FLASH button is ";
     if (button1.isPressed()) reply += "PRESSED";
     else reply += "not pressed";
     reply += ",";        // end of line

   // line4 - Misc adnl info section:
     reply += "Adnl info can go here....";


   server.send(200, "text/plane", reply); //Send millis value only to client ajax request
}


// ----------------------------------------------------------------
//      -ping web page requested     i.e. http://x.x.x.x/ping
// ----------------------------------------------------------------

void handlePing(){

  WiFiClient client = server.client();             // open link with client

  // log page request including clients IP address
  IPAddress cip = client.remoteIP();
  String clientIP = decodeIP(cip.toString());   // check for known IP addresses
  log_system_message("Ping page requested from: " + clientIP);

  String message = "ok";
  server.send(404, "text/plain", message);   // send reply as plain text
}


// ----------------------------------------------------------------
//                    -settings stored in eeprom
// ----------------------------------------------------------------
// @param   eDirection   1=write, 0=read
// Note on esp32 this has now been replaced by preferences.h although I still prefer this method - see: by https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/

#if ENABLE_EEPROM
void settingsEeprom(bool eDirection) {

    const int dataRequired = 30;   // total size of eeprom space required (bytes)

    uint32_t demoInt = 42;         // 4 byte variable to be stored in eeprom as an example of how (can be removed)

    int currentEPos = 0;           // current position in eeprom

    EEPROM.begin(dataRequired);

    if (eDirection == 0) {

      // read settings from Eeprom
        EEPROM.get(currentEPos, demoInt);             // read data
        if (demoInt < 1 || demoInt > 99) demoInt = 1; // validate
        currentEPos += sizeof(demoInt);               // increment to next free position in eeprom

    } else {

      // write settings to Eeprom
        EEPROM.put(currentEPos, demoInt);             // write demoInt to Eeprom
        currentEPos += sizeof(demoInt);               // increment to next free position in eeprom

      EEPROM.commit();                              // write the data out (required on esp devices as they simulate eeprom)
    }

}
#endif


// ----------------------------------------------------------------
//           -testing page     i.e. http://x.x.x.x/test
// ----------------------------------------------------------------
// Use this area for experimenting/testing code

void handleTest(){

  WiFiClient client = server.client();          // open link with client

  // log page request including clients IP address
    IPAddress cip = client.remoteIP();
    String clientIP = decodeIP(cip.toString());   // check for known IP addresses
    log_system_message("Test page requested from: " + clientIP);

  webheader(client);                 // add the standard html header
  client.println("<br><H2>TEST PAGE</H2><br>");


  // ---------------------------- test section here ------------------------------



// demo of how to send a sms message via GSM board (phone number is a String in the format "+447871862749")
//  sendSMS(phoneNumber, "this is a test message from the gsm demo sketch");


// demo of how to send an email over Wifi
  #if ENABLE_EMAIL
    client.println("<br>Sending test email");
    _recepient[0]=0; _message[0]=0; _subject[0]=0;                  // clear any existing text
    strcat(_recepient, _emailReceiver);                             // email address to send it to
    strcat(_subject,stitle);
    strcat(_subject,": test");
    strcat(_message,"test email from esp project");
    emailToSend=1; lastEmailAttempt=0; emailAttemptCounter=0; sendSMSflag=0;   // set flags that there is an email to be sent
  #endif


// demo of how to request a web page over GSM
//  requestWebPageGSM("webpage.com", "/q.txt", 80);


/*
// demo of how to request a web page
  // request web page
    String page = "http://192.168.1.166/ping";   // url to request
    //String page = "http://192.168.1.176:84/rover.asp?action=devE11on";   // url to request
    String response;                             // reply will be stored here
    int httpCode = requestWebPage(&page, &response);
  // display the reply
    client.println("Web page requested: '" + page + "' - http code: " + String(httpCode));
    client.print("<xmp>'");     // enables the html code to be displayed
    client.print(response);
    client.println("'</xmp>");
  // test the reply
    response.toLowerCase();                    // convert all text in the reply to lower case
    int tPos = response.indexOf("closed");     // search for text in the reply - see https://www.arduino.cc/en/Tutorial/BuiltInExamples
    if (tPos != -1) {
      client.println("rusult contains 'closed' at position " + String(tPos) + "<br>");
    }
*/



  // -----------------------------------------------------------------------------


  // end html page
    webfooter(client);            // add the standard web page footer
    delay(1);
    client.stop();
}


// --------------------------- E N D -----------------------------
