/**************************************************************************************************
 *
 *                                    Wifi / NTP Connections - 26Feb20
 *             
 *                               Set up wifi for esp32 plus NTP (network time)
 *                    
 *                    Libraries used: 
 *                      ESP_Wifimanager - https://github.com/khoih-prog/ESP_WiFiManager
 *                      TimeLib
 *                      ESPmDNS
 *                    
 *  
 **************************************************************************************************/



// **************************************** S e t t i n g s ****************************************

    
    // Configuration Portal (Wifimanager)
      const String portalName = "espcam1";
      const String portalPassword = "12345678";
      // String portalName = "ESP_" + String(ESP_getChipId(), HEX);               // use chip id
      // String portalName = stitle;                                              // use sketch title

    // mDNS name
      const String mDNS_name = "espcam1";
      // const String mDNS_name = stitle;                                         // use sketch title
      

// *************************************************************************************************
    

// forward declarations
  void startWifiManager();
  String currentTime();
  bool IsBST();
  void sendNTPpacket();
  time_t getNTPTime();
  void ClearWifimanagerSettings();
  String formatDateNumber(int);


// ----------------------------------------------------------------
//                              -Startup
// ----------------------------------------------------------------

byte wifiok = 0;          // flag if wifi is connected ok (1 = ok)
  
// wifi for esp32
  #ifdef ESP32
    #include <esp_wifi.h>
    #include <WiFi.h>
    #include <WiFiClient.h>
    #include <WebServer.h>
    #define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())
    WebServer server(ServerPort);
    const String ESPType = "ESP32";
  #else
      #error "Only works with ESP32-cam board"
  #endif
 
  // SSID and Password for wifi 
    String Router_SSID;
    String Router_Pass;

  #define USE_AVAILABLE_PAGES     false      // Use false if you don't like to display Available Pages in Information Page of Config Portal
  
  #include <ESP_WiFiManager.h>      
  #include <ESPmDNS.h>                // see https://github.com/espressif/arduino-esp32/tree/master/libraries/ESPmDNS      



// Time from NTP server
//      from https://raw.githubusercontent.com/RalphBacon/No-Real-Time-Clock-RTC-required---use-an-NTP/master
  #include <TimeLib.h>
  #include <WiFiUdp.h>                         // UDP library which is how we communicate with Time Server
  const uint16_t localPort = 8888;              // Just an open port we can use for the UDP packets coming back in
  const char timeServer[] = "uk.pool.ntp.org"; 
  const uint16_t NTP_PACKET_SIZE = 48;          // NTP time stamp is in the first 48 bytes of the message
  byte packetBuffer[NTP_PACKET_SIZE];          // buffer to hold incoming and outgoing packets
  WiFiUDP NTPUdp;                              // A UDP instance to let us send and receive packets over UDP
  const uint16_t timeZone = 0;                  // timezone (0=GMT)
  const String DoW[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  // How often to resync the time (under normal and error conditions)
    const uint16_t _resyncSeconds = 7200;       // 7200 = 2 hours
    const uint16_t _resyncErrorSeconds = 60;    // 60 = 1 min
  bool NTPok = 0;                              // Flag if NTP is curently connecting ok



// ----------------------------------------------------------------
//                 -wifi initialise  (called from 'setup')
// ----------------------------------------------------------------

void startWifiManager() {

  // ClearWifimanagerSettings();      // Erase stored wifi configuration (wifimanager)
 
  uint32_t startedAt = millis();

  ESP_WiFiManager ESP_wifiManager(stitle.c_str());     
  
  ESP_wifiManager.setMinimumSignalQuality(-1);

  ESP_wifiManager.setDebugOutput(true);

  // wifimanager config portal settings
    ESP_wifiManager.setSTAStaticIPConfig(IPAddress(192,168,2,114), IPAddress(192,168,2,1), IPAddress(255,255,255,0), 
                                         IPAddress(192,168,2,1), IPAddress(8,8,8,8));
  
  // We can't use WiFi.SSID() in ESP32as it's only valid after connected. 
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS for this purpose
    Router_SSID = ESP_wifiManager.WiFi_SSID();
    Router_Pass = ESP_wifiManager.WiFi_Pass();
  
  //  Serial.println("Stored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);    // show stored wifi password

  // if no stored wifi credentials open config portal
  if (Router_SSID == "") {   
    Serial.println("No stored access point credentials, starting access point"); 
    ESP_wifiManager.setConfigPortalTimeout(600);       // Config portal timeout  
    if (ESP_wifiManager.startConfigPortal((const char *) portalName.c_str(), portalPassword.c_str())) {
      Serial.println("Portal config sucessful");
    }
    else Serial.println("Portal config failed");    
  }

  // connect to wifi
    #define WIFI_CONNECT_TIMEOUT        30000L
    #define WHILE_LOOP_DELAY            200L
    #define WHILE_LOOP_STEPS            (WIFI_CONNECT_TIMEOUT / ( 3 * WHILE_LOOP_DELAY ))
    
    startedAt = millis();
    
    while ( (WiFi.status() != WL_CONNECTED) && (millis() - startedAt < WIFI_CONNECT_TIMEOUT ) )
    {   
      WiFi.mode(WIFI_STA);
      WiFi.persistent (true);
      Serial.print("Connecting to ");
      Serial.println(Router_SSID);
      WiFi.begin(Router_SSID.c_str(), Router_Pass.c_str());
      int i = 0;
      while((!WiFi.status() || WiFi.status() >= WL_DISCONNECTED) && i++ < WHILE_LOOP_STEPS) delay(WHILE_LOOP_DELAY);   
    }
  
    Serial.print("After waiting ");
    Serial.print((millis()- startedAt) / 1000);
    Serial.print(" secs more in setup() connection result is: \n ");
  
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("connected. Local IP: ");
      Serial.println(WiFi.localIP());
      wifiok = 1;     // flag wifi now ok
    } else {
      Serial.println("Failed to connect to Wifi");
      Serial.println(ESP_wifiManager.getStatus(WiFi.status()));
          
      // open config portal 
      ESP_wifiManager.setConfigPortalTimeout(120);       // Config portal timeout
      if (!ESP_wifiManager.startConfigPortal((const char *) portalName.c_str(), portalPassword.c_str())) {
        wifiok = 0;     // flag wifi not connected
        Serial.println("failed to connect to wifi / access point timed out so rebooting to try again...");
        delay(500);
        ESP.restart();                                   // reboot and try again
        delay(5000);                                     // restart will fail without this delay
      }
      else {
        Serial.println("Wifi connected");
        wifiok = 1;                                      // flag wifi now ok
      }
    }  


  // Set up mDNS responder:
    Serial.println( MDNS.begin(mDNS_name.c_str()) ? "mDNS responder started ok" : "Error setting up mDNS responder" );

    
  // start NTP
    NTPUdp.begin(localPort);                  // What port will the UDP/NTP packet respond on?
    setSyncProvider(getNTPTime);              // What is the function that gets the time (in ms since 01/01/1900)?
    setSyncInterval(_resyncErrorSeconds);     // How often should we synchronise the time on this machine (in seconds) 
           
}


// ----------------------------------------------------------------
//               -Return current time and date as string
// ----------------------------------------------------------------
// supplies time in the format:   '23-04-2020_09-23-10_Mon'

String currentTime(){

   time_t t=now();     // get current time 

   if (IsBST()) t+=3600;     // add one hour if it is Summer Time

   String ttime = formatDateNumber(day(t));
   ttime += "-";
   ttime += formatDateNumber(month(t));
   ttime += "-";
   ttime += formatDateNumber(year(t));
   ttime += "_";
   ttime += formatDateNumber(hour(t));
   ttime += "-";
   ttime += formatDateNumber(minute(t));
   ttime += "-";
   ttime += formatDateNumber(second(t));
   ttime += "_";
   ttime += DoW[weekday(t)-1];

   return ttime;
}


// convert number to String and add leading zero if required
String formatDateNumber(int input) {
  String tval = "";
  if (input < 10) tval = "0";    // add leading zero if required   
  tval += String(input);
  return tval;
}



//-----------------------------------------------------------------------------
//                           -British Summer Time check
//-----------------------------------------------------------------------------

// returns true if it is British Summer time
//         code from https://my-small-projects.blogspot.com/2015/05/arduino-checking-for-british-summer-time.html

boolean IsBST()
{
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
      
      if( iday > lastMarSunday)
        return true;
      if( iday < lastMarSunday)
        return false;
      
      if (hr < 1)
        return false;
              
      return true; 
  
    }
    //In October we must be before the last sunday to be bst.
    //That means the previous sunday must be before the 1st.
    if (imonth == 10) { 

      if( iday < lastOctSunday)
        return true;
      if( iday > lastOctSunday)
        return false;  
      
      if (hr >= 1)
        return false;
        
      return true;  
    }

}



//-----------------------------------------------------------------------------
//        send an NTP request to the time server at the given address
//-----------------------------------------------------------------------------

void sendNTPpacket(const char* address) {
  
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
  
}



//-----------------------------------------------------------------------------
//                contact the NTP pool and retrieve the time
//-----------------------------------------------------------------------------
//
// code from https://github.com/RalphBacon/No-Real-Time-Clock-RTC-required---use-an-NTP

time_t getNTPTime() {

  // Send a UDP packet to the NTP pool address
  Serial.print("\nSending NTP packet to ");
  Serial.println(timeServer);
  sendNTPpacket(timeServer);

  // Wait to see if a reply is available - timeout after X seconds. At least
  // this way we exit the 'delay' as soon as we have a UDP packet to process
  #define UDPtimeoutSecs 3
  int timeOutCnt = 0;
  while (NTPUdp.parsePacket() == 0 && ++timeOutCnt < (UDPtimeoutSecs * 10)){
    delay(100);
    // yield();
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
    Serial.print("Seconds since Jan 1 1900 = ");
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    //Serial.print("Unix time = ");

    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;     // UL denotes it is 'unsigned long' 

    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;

    // Reset the interval to get the time from NTP server in case we previously changed it
    setSyncInterval(_resyncSeconds);
    NTPok = 1;       // flag NTP is currently connecting ok

    return epoch;
  }

  // Failed to get an NTP/UDP response
    Serial.println("No response received from NTP");
    setSyncInterval(_resyncErrorSeconds);       // try more frequently until a response is received
    NTPok = 0;                                  // flag NTP not currently connecting

    return 0;
  
}



//-----------------------------------------------------------------------------
//                     Clear stored wifi settings (Wifimanager)
//-----------------------------------------------------------------------------
//

void ClearWifimanagerSettings() {
  
    // clear stored wifimanager settings
          Serial.println("Clearing stored wifimanager settings");
          wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); //load the flash-saved configs
          esp_wifi_init(&cfg); //initiate and allocate wifi resources (does not matter if connection fails)
          delay(2000); //wait a bit
          if(esp_wifi_restore()!=ESP_OK)  Serial.println("WiFi is not initialized by esp_wifi_init");
          else Serial.println("Cleared!");
          Serial.println("System stopped...");
          while(1);   // stop

}


// --------------------------- E N D -----------------------------
