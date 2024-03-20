/**************************************************************************************************
 *
 *      Wifi / NTP Connections - has the option to use WifiManager - 04Feb24
 *
 *      part of the BasicWebserver sketch - https://github.com/alanesq/BasicWebserver
 *
 *      Set up wifi for either esp8266 or esp32 plus NTP (network time)
 *      with 3 options for wifi:   1=Use WifiManager, 2=specify wifi credentials, 3=Access point
 *
 *      Libraries used:
 *                      WiFiManager      https://github.com/tzapu/WiFiManager
 *
 **************************************************************************************************
 
  In setup start wifi with :    using wifi manager:     startWifiManager(); 
                                specifying credentials: startWifi();
                                create an access point: startAccessPoint(); 

*/

#include <Arduino.h>                      // required by PlatformIO
#define WIFIUSED 1                        // flag that wifi.h is in use rather than wifi2.h


// forward declarations
  void startWifiManager();                // Connect to wifi using Wifimanager 
  void startWifi();                       // Connect to wifi providing login details
  void startAccessPoint();                // Just act as an access point
  String currentTime(bool);
  bool IsBST();
  void sendNTPpacket();
  time_t getNTPTime();
  int requestWebPage(String*, String*, int);
  

// **************************************** S e t t i n g s ****************************************


// Settings for main wifi (if not using wifi manager)
  const char *SSID = "<wifi name>";
  const char *PWD = "<wifi password>";  
  const int wifiTimeout = 20;                              // timeout when connecting to wifi (seconds)

// Settings for the configuration Portal or Access Point
  const char* AP_SSID = "CWM01";
  const char* AP_PASS = "password";

// IP settings when using Access Point
    IPAddress Ip(192, 168, 2, 1);                          // ip address
    IPAddress NMask(255, 255, 255, 0);                     // sub-net mask


// *************************************************************************************************


// ----------------------------------------------------------------
//                              -Startup
// ----------------------------------------------------------------

// wifi for esp8266 / esp32
  #if defined ESP32
    #include <esp_wifi.h>
    #include <WiFi.h>
    #include <WiFiClient.h>
    //#include <WiFiMulti.h>
    #include <WebServer.h>                // https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer
    #include <HTTPClient.h>       
    #define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())
    WebServer server(ServerPort);         // allows use of different port for main web server
    //#include <ESPmDNS.h>                // see https://github.com/espressif/arduino-esp32/tree/master/libraries/ESPmDNS
    
  #elif defined ESP8266
    #include <ESP8266WiFi.h>     
    #include <DNSServer.h>
    //#include <ESP8266WiFiMulti.h>
    #include <ESP8266WebServer.h>
    #include <ESP8266HTTPClient.h>        // https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient
    #define ESP_getChipId()   (ESP.getChipId())
    ESP8266WebServer server(ServerPort);  // allows use of different port for main web server
    //#include <ESP8266mDNS.h>
    
  #else
      #error "wifi.h: This sketch only works with ESP8266 or ESP32"
  #endif

#include <WiFiManager.h>                  // https://github.com/tzapu/WiFiManager

// Time from NTP server
//    from https://raw.githubusercontent.com/RalphBacon/No-Real-Time-Clock-RTC-required---use-an-NTP/master
  #include <TimeLib.h>
  #include <WiFiUdp.h>                          // UDP library which is how we communicate with Time Server
  const uint16_t localPort = 8888;              // Just an open port we can use for the UDP packets coming back in
  const char timeServer[] = "pool.ntp.org";
  const uint16_t NTP_PACKET_SIZE = 48;          // NTP time stamp is in the first 48 bytes of the message
  byte packetBuffer[NTP_PACKET_SIZE];           // buffer to hold incoming and outgoing packets
  WiFiUDP NTPUdp;                               // A UDP instance to let us send and receive packets over UDP
  const uint16_t timeZone = 0;                  // timezone (0=GMT)
  const String DoW[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  const uint16_t _resyncSeconds = 7200;         // How often to resync the time (under normal conditions) 7200 = 2 hours
  const uint16_t _resyncErrorSeconds = 300;     // How often to resync the time (under error conditions) 300 = 5 minutes


// ----------------------------------------------------------------
//                 -wifi initialise  (using wifi manager)
// ----------------------------------------------------------------
// 'startWifiManager' is called from main SETUP

// Callback for when AP is started (i.e. failed to connect to wifi)
void configModeCallback(WiFiManager *myWiFiManager) {
  if (serialDebug) {
    Serial.println("Failed to connect to wifi - Access Port starting");
    Serial.print("Config SSID: ");
    Serial.println(myWiFiManager->getConfigPortalSSID());
    Serial.print("Config IP Address: ");
    Serial.println(WiFi.softAPIP());    
  }
}


void startWifiManager() {
    WiFi.disconnect();
    WiFiManager wm;
    // wm.resetSettings();                            // wipe stored settings
    wm.setConfigPortalTimeout(120);                   // set timeout for config portal
    wm.setAPCallback(configModeCallback);             // function called when AP is started (i.e. failed to connect to wifi)
    bool res = wm.autoConnect(AP_SSID, AP_PASS);      // password protected ap
    WiFi.setAutoReconnect(0);

  // connect to wifi
    if(!res) {
      if (serialDebug) Serial.print("Wifi connection failed so rebooting: ");
      delay(2000);
      ESP.restart();
      delay(5000);           // restart will fail without this delay
    } else {
      if (serialDebug) Serial.println("WiFi connected: " + WiFi.localIP().toString());
      wifiok = 1;            // flag that wifi has connected
    }

// set wifi not to to auto re-connect if connection is lost as the sketch handles this
  WiFi.setAutoReconnect(0);

//  // Set up mDNS responder:
//    if (serialDebug) Serial.println( MDNS.begin(mDNS_name.c_str()) ? "mDNS responder started ok" : "Error setting up mDNS responder" );

  // start NTP (Time)
    NTPUdp.begin(localPort);
    setSyncProvider(getNTPTime);              // the function that gets the time from NTP
    setSyncInterval(_resyncErrorSeconds);     // How often to re-synchronise the time (in seconds)
}  // startwifimanager


// ----------------------------------------------------------------
//             -wifi initialise  (not using wifi manager)
// ----------------------------------------------------------------
// called from main SETUP

void startWifi() {
  WiFi.disconnect();  

  // connect to wifi
    wifiok = 0;
    if (serialDebug) {
        Serial.print("Wifi: Connecting to ");
        Serial.print(SSID);
    }
    WiFi.begin(SSID, PWD);
    
    int cntr = wifiTimeout;
    while (WiFi.status() != WL_CONNECTED && cntr > 0) {
      if (serialDebug) Serial.print(".");
      delay(1000);
      cntr -= 1;
    }  
    if (WiFi.status() == WL_CONNECTED) {
      wifiok = 1;
      log_system_message("Wifi connected: " + WiFi.localIP().toString());
    } else {
       log_system_message("Wifi failed to connect");
    }

  // set wifi not to auto re-connect if connection is lost as the sketch handles this
    WiFi.setAutoReconnect(0);

  //  // Set up mDNS responder:
  //    if (serialDebug) Serial.println( MDNS.begin(mDNS_name.c_str()) ? "mDNS responder started ok" : "Error setting up mDNS responder" );

  // start NTP (Time)
    NTPUdp.begin(localPort);
    setSyncProvider(getNTPTime);              // the function that gets the time from NTP
    setSyncInterval(_resyncErrorSeconds);     // How often to re-synchronise the time (in seconds)
}  // startwifi


// ----------------------------------------------------------------
//                   -create wifi access point
// ----------------------------------------------------------------
// called from main SETUP 

void startAccessPoint() {
    WiFi.disconnect();  
    WiFiServer server(ServerPort);
    if (serialDebug) Serial.print("Starting access point...");
    WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 8);      // access point settings (ssid, password, channel, ssid_hidden, max_connections)
    delay(150);
    WiFi.softAPConfig(Ip, Ip, NMask);
    WiFi.mode(WIFI_AP);              // enable as access point only - options are WIFI_AP, WIFI_STA, WIFI_AP_STA or WIFI_OFF
    server.begin();
    log_system_message("Access point started: " + String(AP_SSID) + ", IP=" + WiFi.softAPIP().toString());
}


// ----------------------------------------------------------------
//          -Return current time and date as a string
// ----------------------------------------------------------------
// Notes: two formats available, for British summer time

String currentTime(bool dFormat = 1){
   time_t t=now();     // get current time
   String ttime;
   int tstore;
   if (year(t) < 2021) return "Time Unknown";
   if (IsBST()) t+=3600;     // add one hour if it is Summer Time

   if (dFormat == 0) {
    // format suitable for file names
        // date
          ttime += String(year(t));
          tstore = month(t);   if (tstore<10) ttime+="0";
          ttime += String(tstore);
          tstore = day(t);   if (tstore<10) ttime+="0";
          ttime += String(tstore) + "_";
        // time
          tstore = hour(t);   if (tstore<10) ttime+="0";
          ttime += String(tstore);
          tstore = minute(t);   if (tstore<10) ttime+="0";
          ttime += String(tstore);
          tstore = second(t);   if (tstore<10) ttime+="0";
          ttime += String(tstore);
          ttime += "_" + DoW[weekday(t)-1];
   } else {
    // format easily read by humans
        // date
          ttime += DoW[weekday(t)-1] + " ";
          tstore = day(t);   if (tstore<10) ttime+="0";
          ttime += String(tstore) + "/";
          tstore = month(t);   if (tstore<10) ttime+="0";
          ttime += String(tstore) + "/";
          ttime += String(year(t)) + " ";
        // time
          tstore = hour(t);   if (tstore<10) ttime+="0";
          ttime += String(tstore) + ":";
          tstore = minute(t);   if (tstore<10) ttime+="0";
          ttime += String(tstore) + ":";
          tstore = second(t);   if (tstore<10) ttime+="0";
          ttime += String(tstore);
   }

   return ttime;

}  // currentTime


//-----------------------------------------------------------------------------
//                           -British Summer Time check
//-----------------------------------------------------------------------------
// @return  true if it is British Summer time
// code from https://my-small-projects.blogspot.com/2015/05/arduino-checking-for-british-summer-time.html

bool IsBST() {
    int imonth = month();
    int iday = day();
    int hr = hour();

    //January, february, and november are out.
    if (imonth < 3 || imonth > 10) { return false; }

    //April to September are in
    if (imonth > 3 && imonth < 10) { return true; }

    // find last sun in mar and oct - quickest way I've found to do it
    // last sunday of march
    int lastMarSunday =  (31 - (5* year() /4 + 4) % 7);

    //last sunday of october
    int lastOctSunday = (31 - (5 * year() /4 + 1) % 7);

    //In march, we are BST if is the last sunday in the month
    if (imonth == 3) {
      if( iday > lastMarSunday) return true;
      if( iday < lastMarSunday) return false;
      if (hr < 1) return false;
      return true;
    }

    //In October we must be before the last sunday to be bst.
    //That means the previous sunday must be before the 1st.
    if (imonth == 10) {
      if( iday < lastOctSunday) return true;
      if( iday > lastOctSunday) return false;
      if (hr >= 1) return false;
      return true;
    }

    return true;   // this is here just to stop compiler getting upset ;-)

}  // IsBST


//-----------------------------------------------------------------------------
//        send an NTP request to the time server at the given address
//-----------------------------------------------------------------------------

void sendNTPpacket(const char* address) {
  if (wifiok != 1) return ;

  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;            // Stratum, or type of clock
  packetBuffer[2] = 6;            // Polling Interval
  packetBuffer[3] = 0xEC;         // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // all NTP fields have been given values, now you can send a packet requesting a timestamp:
  // Note that Udp.begin will request automatic translation (via a DNS server) from a
  // name (eg pool.ntp.org) to an IP address. Never use a specific IP address yourself,
  // let the DNS give back a random server IP address
  NTPUdp.beginPacket(address, 123); //NTP requests are to port 123

  // Get the data back
  NTPUdp.write(packetBuffer, NTP_PACKET_SIZE);

  // All done, the underlying buffer is now updated
  NTPUdp.endPacket();

}  // sendNTPpacket


//-----------------------------------------------------------------------------
//                contact the NTP pool and retrieve the time
//-----------------------------------------------------------------------------

time_t getNTPTime() {
  if (wifiok != 1) return 0;

  // Send a UDP packet to the NTP pool address
  if (serialDebug) {
    Serial.print("\nSending NTP packet to ");
    Serial.print(timeServer);
    Serial.print(": ");
  }
  sendNTPpacket(timeServer);

  // Wait to see if a reply is available - timeout after X seconds. At least
  // this way we exit the 'delay' as soon as we have a UDP packet to process
  #define UDPtimeoutSecs 3
  int timeOutCnt = 0;
  while (NTPUdp.parsePacket() == 0 && ++timeOutCnt < (UDPtimeoutSecs * 10)){
    delay(100);
  }

  // Is there UDP data present to be processed? Sneak a peek!
  if (NTPUdp.peek() != -1) {
    // We've received a packet, read the data from it
    NTPUdp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    // The time-stamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);

    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900)
    unsigned long secsSince1900 = highWord << 16 | lowWord;     // shift highword 16 binary places to the left then combine with lowword
    if (serialDebug) {
      Serial.print("Seconds since Jan 1 1900 = ");
      Serial.println(secsSince1900);
    }

    // now convert NTP time into everyday time:

    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;     // UL denotes it is 'unsigned long'

    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;

    // apply GMT time zone
    epoch += timeZone * 60L*60L;

    // Reset the interval to get the time from NTP server in case we previously changed it
    setSyncInterval(_resyncSeconds);

    return epoch;
  }

  // Failed to get an NTP/UDP response
    if (serialDebug) Serial.println("No NTP response received");
    setSyncInterval(_resyncErrorSeconds);       // try more frequently until a response is received

    return 0;
}  // getNTPTime


// ----------------------------------------------------------------
//                        request a web page
// ----------------------------------------------------------------
//   @param    page         web page to request
//   @param    received     String to store response in
//   @param    maxWaitTime  maximum time to wait for reply (ms)
//   @returns  http code
/*
      see:  https://randomnerdtutorials.com/esp32-http-get-post-arduino/#http-get-1
      Example usage:
                              String page = "http://192.168.1.166/ping";   // url to request
                              String response;                             // reply will be stored here
                              int httpCode = requestWebPage(&page, &response);
                              // show results
                                Serial.println("Web page requested: '" + page + "' - http code: " + String(httpCode));
                                Serial.println(response);
*/

int requestWebPage(String* page, String* received, int maxWaitTime=5000){

  if (serialDebug) Serial.println("requesting web page: " + *page);
  #if (defined ESP32)  
    esp_task_wdt_reset();           // reset watchdog timer  
  #endif  
  WiFiClient client;
  HTTPClient http;     // see:  https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient
  http.setTimeout(maxWaitTime);
  http.begin(client, *page);      // for https requires (client, *page, thumbprint)  e.g.String thumbprint="08:3B:71:72:02:43:6E:CA:ED:42:86:93:BA:7E:DF:81:C4:BC:62:30";
  int httpCode = http.GET();      // http codes: https://en.wikipedia.org/wiki/List_of_HTTP_status_codes
  if (serialDebug) Serial.println("http code: " + String(httpCode));

  if (httpCode > 0) {
    *received = http.getString();
  } else {
    *received = "error:" + String(httpCode);
  }
  if (serialDebug) Serial.println(*received);

  http.end();   //Close connection
  if (serialDebug) Serial.println("Web connection closed");

  return httpCode;

}  // requestWebPage

// --------------------------- E N D -----------------------------
