// OTA demo sketch - Mar21

#if defined ESP32
  #include <Update.h>
#endif


// ----------------------------------------------------------------

//                               Wifi Settings

#include <wifiSettings.h>       // delete this line, un-comment the below two lines and enter your wifi details

//const char *SSID = "your_wifi_ssid";

//const char *PWD = "your_wifi_pwd";


// ----------------------------------------------------------------

//                    G e n e r a l     s e t t i n g s

const String stitle = "OTAdemo";             // sketch title

const String sversion = "1.0";               // sketch version

bool serialDebug = 1;                        // enable debugging info on serial port

const String OTAPassword = "password";       // OTA password


// ----------------------------------------------------------------


bool OTAEnabled = 0;           // flag if correct OTA password has been entered or not

//#include <arduino.h>         // required by platformio?

#if defined ESP32
    // esp32
        byte LEDpin = 2; 
        #include <WiFi.h>
        #include <WebServer.h>
        #include <HTTPClient.h>     
        WebServer server(80);
#elif defined ESP8266
    //Esp8266
        byte LEDpin = D4; 
        #include <ESP8266WiFi.h>  
        #include <ESP8266WebServer.h>  
        #include "ESP8266HTTPClient.h"    
        ESP8266WebServer server(80);  
#else
      #error "This sketch only works with the ESP8266 or ESP32"
#endif



// ----------------------------------------------------------------
//                                -Setup
// ----------------------------------------------------------------
void setup() {

  Serial.begin(115200); while (!Serial); delay(200);       // start serial comms at speed 115200
  delay(200);
  Serial.println("\n\nOTA demo sketch");
  
  // onboard LEDs
    pinMode(LEDpin, OUTPUT);
    digitalWrite(LEDpin, HIGH);   
  
  // Connect to Wifi
    Serial.print("Connecting to ");
    Serial.println(SSID);
    WiFi.begin(SSID, PWD);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
    }
    Serial.print("\nConnected - IP: ");
    Serial.println(WiFi.localIP());

  // set up web server pages to serve
    server.on("/", handleRoot);                    // root web page (i.e. when root page is requested run procedure 'handleroot')
    server.on("/ota", handleOTA);                  // Handle OTA updates
    server.onNotFound(handleNotFound);             // if invalid url is requested

  // start web server
    server.begin();    
    
    // stop the wifi being turned off if not used for a while 
      #if defined ESP32
        WiFi.setSleep(false); 
      #else
        WiFi.setSleepMode(WIFI_NONE_SLEEP); 
      #endif
        
    WiFi.mode(WIFI_STA);                          // turn off access point - options are WIFI_AP, WIFI_STA, WIFI_AP_STA or WIFI_OFF 
        
}  // setup


// ----------------------------------------------------------------
//                                -Loop
// ----------------------------------------------------------------
void loop() {

  server.handleClient();                         // service any web page requests 
  
  #if defined(ESP8266)
     yield();                      // allow esp8266 to carry out wifi tasks (may restart randomly without this)
  #endif
    
}  // loop


// ----------------------------------------------------------------
//     -enable OTA
// ----------------------------------------------------------------
//
//   Enable OTA updates, called when correct password has been entered

void otaSetup() {

    OTAEnabled = 1;          // flag that OTA has been enabled

    // esp32 version (using webserver.h)
    #if defined ESP32
        server.on("/update", HTTP_POST, []() {
          server.sendHeader("Connection", "close");
          server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
          delay(2000);
          ESP.restart();
          delay(2000);
        }, []() {
          HTTPUpload& upload = server.upload();
          if (upload.status == UPLOAD_FILE_START) {
            if (serialDebug) Serial.setDebugOutput(true);
            if (serialDebug) Serial.printf("Update: %s\n", upload.filename.c_str());
            if (!Update.begin()) { //start with max available size
              if (serialDebug) Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
              if (serialDebug) Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) { //true to set the size to the current progress
              if (serialDebug) Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
            } else {
              if (serialDebug) Update.printError(Serial);
            }
            if (serialDebug) Serial.setDebugOutput(false);
          } else {
            if (serialDebug) Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
          }
        });
    #endif
    
    // esp8266 version  (using ESP8266WebServer.h)
    #if defined ESP8266
        server.on("/update", HTTP_POST, []() {
          server.sendHeader("Connection", "close");
          server.send(200, "text/plain", (Update.hasError()) ? "Update Failed!, rebooting" : "Update complete, device is rebooting...");
          delay(2000);
          ESP.restart();
          delay(2000);
        }, []() {
          HTTPUpload& upload = server.upload();
          if (upload.status == UPLOAD_FILE_START) {
            if (serialDebug) Serial.setDebugOutput(true);
            WiFiUDP::stopAll();
            if (serialDebug) Serial.printf("Update: %s\n", upload.filename.c_str());
            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (!Update.begin(maxSketchSpace)) { //start with max available size
              if (serialDebug) Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
              if (serialDebug) Update.printError(Serial);
            }
          } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) { //true to set the size to the current progress
              if (serialDebug) Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
            } else {
              if (serialDebug) Update.printError(Serial);
            }
            if (serialDebug) Serial.setDebugOutput(false);
          }
          yield();
        });
    #endif

}


// ----------------------------------------------------------------
//     -OTA web page requested     i.e. http://x.x.x.x/ota
// ----------------------------------------------------------------
//
//   Request OTA password or implement OTA update

void handleOTA(){

  WiFiClient client = server.client();          // open link with client

  // log page request including clients IP address
      IPAddress cip = client.remoteIP();
      //log_system_message("OTA web page requested from: " + String(cip[0]) + "." + String(cip[1]) + "." + String(cip[2]) + "." + String(cip[3]));

  // check if valid password supplied
    if (server.hasArg("pwd")) {
      if (server.arg("pwd") == OTAPassword) otaSetup();    // Enable over The Air updates (OTA)
    }



  // -----------------------------------------

  if (OTAEnabled == 0) {
    
    // OTA is not enabled so request password to enable it
  
      // html header
        client.print("<!DOCTYPE html> <html lang='en'> <head> <title>Web Demo</title> </head> <body>\n");         // basic html header
      
      client.print (R"=====(
         <form name='loginForm'>
            <table width='20%' bgcolor='A09F9F' align='center'>
                <tr>
                    <td colspan=2>
                        <center><font size=4><b>Enter OTA password</b></font></center><br>
                    </td>
                        <br>
                </tr><tr>
                    <td>Password:</td>
                    <td><input type='Password' size=25 name='pwd'><br></td><br><br>
                </tr><tr>
                    <td><input type='submit' onclick='check(this.form)' value='Login'></td>
                </tr>
            </table>
        </form>
        <script>
            function check(form)
            {
              window.open('/ota?pwd=' + form.pwd.value , '_self')
            }
        </script>
      )=====");
      
      // end html
        client.print("</body></html>\n");
        delay(3);
        client.stop();

  }

  // -----------------------------------------

  if (OTAEnabled == 1) {
    
    // OTA is enabled so implement it
  
      // html header
        client.print("<!DOCTYPE html> <html lang='en'> <head> <title>Web Demo</title> </head> <body>\n");         // basic html header
    
      client.write("<br><H1>Update firmware</H1><br>\n");
      client.printf("Current version =  %s, %s \n\n", stitle, sversion);
      
      client.write("<form method='POST' action='/update' enctype='multipart/form-data'>\n");
      client.write("<input type='file' style='width: 300px' name='update'>\n");
      client.write("<br><br><input type='submit' value='Update'></form><br>\n");
    
      client.write("<br><br>Device will reboot when upload complete");
      client.write("<br>To disable OTA restart device<br>\n");
  
      // end html
        client.print("</body></html>\n");
        delay(3);
        client.stop();
  }
    
  // -----------------------------------------

                            
  // close html page
    delay(3);
    client.stop();
    
}


// ----------------------------------------------------------------
//                      -invalid web page requested
// ----------------------------------------------------------------
// send this reply to any invalid url requested

void handleNotFound() {

  if (serialDebug) Serial.println("Invalid page requested");

  String tReply;
  
  tReply = "File Not Found\n\n";
  tReply += "URI: ";
  tReply += server.uri();
  tReply += "\nMethod: ";
  tReply += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  tReply += "\nArguments: ";
  tReply += server.args();
  tReply += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    tReply += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", tReply );
  tReply = "";      // clear variable
  
}  // handleNotFound


// ----------------------------------------------------------------
//      -test web page requested     i.e. http://x.x.x.x/
// ----------------------------------------------------------------
// demonstrate sending a plain text reply

void handleRoot(){

  if (serialDebug) Serial.println("Root page requested");

  String message = "root web page";

  server.send(404, "text/plain", message);   // send reply as plain text
  
}  // handleRoot

// end
