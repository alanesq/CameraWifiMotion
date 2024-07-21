/**************************************************************************************************
 *
 *      Procedures (which are likely to be the same between projects) - 01Feb22
 *
 *      part of the BasicWebserver sketch but with modified 'header', 'footer' and iclusion of spiffs.h
 *
 *      Includes: log_system_message, webheader, webfooter, handleLogpage, handleReboot, WIFIcheck & decodeIP
 *                classes: Led, Button & repeatTimer.
 *
 **************************************************************************************************/

#include <SPIFFS.h>

// ----------------------------------------------------------------
//                              -Startup
// ----------------------------------------------------------------

// html text colour codes (obsolete html now and should really use CSS)
  const char colRed[] = "<font color='#FF0000'>";           // red text
  const char colGreen[] = "<font color='#006F00'>";         // green text
  const char colBlue[] = "<font color='#0000FF'>";          // blue text
  const char colEnd[] = "</font>";                          // end coloured text
  const char htmlSpace[] = "&ensp;";                        // leave a space

// misc variables
  String lastClient = "n/a";                  // IP address of most recent client connected
  int system_message_pointer = 0;             // pointer for current system message position
  String system_message[LogNumber + 1];       // system log message store  (suspect serial port issues caused if not +1 ???)


// ----------------------------------------------------------------
//                    -decode IP addresses
// ----------------------------------------------------------------
// Check for known IP addresses

String decodeIP(String IPadrs) {

    if (IPadrs == "192.168.1.176") IPadrs = "HA server";
    else if (IPadrs == "192.168.1.103") IPadrs = "Parlour laptop";
    else if (IPadrs == "192.168.1.101") IPadrs = "Bedroom laptop";
    else if (IPadrs == "192.168.1.169") IPadrs = "Linda's laptop";
    else if (IPadrs == "192.168.1.170") IPadrs = "Shed 1 laptop";
    else if (IPadrs == "192.168.1.143") IPadrs = "Shed 2 laptop";

    // log last IP client connected
      if (IPadrs != lastClient) {
        lastClient = IPadrs;
        log_system_message("New IP client connected: " + IPadrs);
      }

    return IPadrs;
}


// ----------------------------------------------------------------
//                      -log a system message
// ----------------------------------------------------------------

void log_system_message(String smes) {

  // increment position pointer
    system_message_pointer++;
    if (system_message_pointer >= LogNumber) system_message_pointer = 0;

  // add the new message to log
    system_message[system_message_pointer] = currentTime(1) + " - " + smes;

  // also send the message to serial
    if (serialDebug) Serial.println("Log:" + system_message[system_message_pointer]);
}


// ----------------------------------------------------------------
//                         -header (html)
// ----------------------------------------------------------------
// HTML at the top of each web page
//    additional style settings can be included and auto page refresh rate

void webheader(WiFiClient &client, const char* adnlStyle = " ", int refresh = 0) {

  // start html page
    client.write("HTTP/1.1 200 OK\r\n");
    client.write("Content-Type: text/html\r\n");
    client.write("Connection: close\r\n");
    client.write("\r\n");
    client.write("<!DOCTYPE HTML>\n");
    client.write("<html lang='en'>\n");
    client.write("<head>\n");
    client.write("  <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n");
  // page refresh
    if (refresh > 0) client.printf("  <meta http-equiv='refresh' content='%c'>\n", refresh);

  // HTML / CSS

  // This is the below html compacted to save flash memory via https://www.textfixer.com/html/compress-html-compression.php
  client.printf(R"=====( <title>%s</title> <style> body { color: black; background-color: #FFFF00; text-align: center; } ul {list-style-type: none; margin: 0; padding: 0; overflow: hidden; background-color: rgb(128, 64, 0);} li {float: left;} li a {display: inline-block; color: white; text-align: center; padding: 30px 20px; text-decoration: none;} li a:hover { background-color: rgb(100, 0, 0);} %s </style> </head> <body> <ul> <li><a href='%s'>Home</a></li> <li><a href='/log'>Log</a></li> <li><a href='/bootlog'>BootLog</a></li> <li><a href='/stream'>Live Video</a></li> <li><a href='/images'>Stored Images</a></li> <li><a href='/live'>Capture Image</a></li> <li><a href='/imagedata'>Raw Data</a></li> <h1> <font color='#FF0000'>%s</h1></font> </ul>)=====", stitle, adnlStyle, HomeLink, stitle);

  /*
    client.printf(R"=====(
        <title>%s</title>
        <style>
          body {
            color: black;
            background-color: #FFFF00;
            text-align: center;
          }
          ul {list-style-type: none; margin: 0; padding: 0; overflow: hidden; background-color: rgb(128, 64, 0);}
          li {float: left;}
          li a {display: inline-block; color: white; text-align: center; padding: 30px 20px; text-decoration: none;}
          li a:hover { background-color: rgb(100, 0, 0);}
          %s
        </style>
      </head>
      <body>
        <ul>
          <li><a href='%s'>Home</a></li>
          <li><a href='/log'>Log</a></li>
          <li><a href='/bootlog'>BootLog</a></li>
          <li><a href='/stream'>Live Video</a></li>
          <li><a href='/images'>Stored Images</a></li>
          <li><a href='/live'>Capture Image</a></li>
          <li><a href='/imagedata'>Raw Data</a></li>
          <h1> <font color='#FF0000'>%s</h1></font>
        </ul>
    )=====", stitle, adnlStyle, HomeLink, stitle);
*/

}



// ----------------------------------------------------------------
//                             -footer (html)
// ----------------------------------------------------------------
// HTML at the end of each web page
// @param   client    http client

void webfooter(WiFiClient &client) {

  // get mac address
    byte mac[6];
    WiFi.macAddress(mac);

  client.println("<br>");

  // Status display at bottom of screen 
    client.println("<div style='text-align: center;background-color:rgb(128, 64, 0)'>");
    client.printf("<small> %s", colRed);
    client.printf("%s %s", stitle, sversion);

    client.printf(" | Memory: %dK", ESP.getFreeHeap() /1000);

    client.printf(" | Wifi: %ddBm", WiFi.RSSI());

    // NTP server link status
    int tstat = timeStatus();   // ntp status
    if (tstat == timeSet) client.print(" | NTP OK");
    else if (tstat == timeNeedsSync) client.print(" | NTP Sync failed");
    else if (tstat == timeNotSet) client.print(" | NTP Failed");

    // free space on spiffs
      client.printf(" | Spiffs: %dK", ( SPIFFS.totalBytes() - SPIFFS.usedBytes() ) / 1024  );             // if using spiffs

    // mac address 
    //  client.printf(" | MAC: %2x%2x%2x%2x%2x%2x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);       // mac address

    // free PSRAM
      if (psramFound()) {
        client.printf(" | PSram: %dK", heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024 );
      }

  // end 
  client.printf("%s </small></div>\n", colEnd);
  client.println("</body></html>");

}


// ----------------------------------------------------------------
//       -log page requested    i.e. http://x.x.x.x/log
// ----------------------------------------------------------------

void handleLogpage() {

  WiFiClient client = server.client();                     // open link with client

/*
  // log page request including clients IP address
    IPAddress cip = client.remoteIP();
    String clientIP = decodeIP(cip.toString());   // get ip address and check if it is known
    log_system_message("Log page requested from: " + clientIP);
*/    
    
  // build the html 

    webheader(client);                         // send html page header

    client.println("<P>");                     // start of section

    client.println("<br>SYSTEM LOG<br><br>");

    // list all system messages
    int lpos = system_message_pointer;         // most recent entry
    for (int i=0; i < LogNumber; i++){         // count through number of entries
      client.print(system_message[lpos]);
      client.println("<br>");
      if (lpos == 0) lpos = LogNumber;
      lpos--;
    }

    // close html page
      webfooter(client);                       // send html page footer
      delay(3);
      client.stop();

}


// ----------------------------------------------------------------
//                      -invalid web page requested
// ----------------------------------------------------------------

void handleNotFound() {

  WiFiClient client = server.client();          // open link with client

  // log page request including clients IP address
    IPAddress cip = client.remoteIP();
    String clientIP = decodeIP(cip.toString());   // get ip address and check if it is known
    log_system_message("Invalid URL requested from: " + clientIP);

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
  message = "";      // clear variable

}


// ----------------------------------------------------------------
//   -reboot web page requested        i.e. http://x.x.x.x/reboot
// ----------------------------------------------------------------
// note: this can fail if the esp has just been reflashed and not restarted

void handleReboot(){

      String message = "Rebooting....";
      server.send(404, "text/plain", message);   // send reply as plain text

      // rebooting
        delay(500);          // give time to send the above html
        ESP.restart();
        delay(5000);         // restart fails without this delay

}


// --------------------------------------------------------------------------------------
//                                 -wifi connection check
// --------------------------------------------------------------------------------------

bool WIFIcheck() {

    if (WiFi.status() != WL_CONNECTED) {
      if ( wifiok == 1) {
        log_system_message("Wifi connection lost");
        wifiok = 0;
      }
    } else {
      // wifi is ok
      if ( wifiok == 0) {
        log_system_message("Wifi connection is back");
        wifiok = 1;
      }
    }

    return wifiok;

}


// --------------------------------------------------------------------------------------
//                                       -LED control
// --------------------------------------------------------------------------------------
// @param  gpio pin, if LED ON when pin is high or low
// useage:      Led led1(LED_1_GPIO, HIGH);
//              led1.on();

class Led {

  private:
    byte _gpioPin;                                     // gpio pin of the LED
    bool _onState;                                     // logic state when LED is on

  public:
    Led(byte gpioPin, bool onState=HIGH) {
      this->_gpioPin = gpioPin;
      this->_onState = onState;
      pinMode(_gpioPin, OUTPUT);
      off();
    }

    void on() {
      digitalWrite(_gpioPin, _onState);
    }

    void off() {
      digitalWrite(_gpioPin, !_onState);
    }

    void flip() {
      digitalWrite(_gpioPin, !digitalRead(_gpioPin));   // flip the led status
    }

    bool status() {                                     // returns 1 if LED is on
      bool sState = digitalRead(_gpioPin);
      if (_onState == HIGH) return sState;
      else return !sState;
    }

    void flash(int fLedDelay=350, int fReps=1) {
      if (fLedDelay < 0 || fLedDelay > 20000) fLedDelay=300;   // capture invalid delay
      bool fTempStat = digitalRead(_gpioPin);
      if (fTempStat == _onState) {                      // if led is already on
        off();
        delay(fLedDelay);
      }
      for (int i=0; i<fReps; i++) {
        on();
        delay(fLedDelay);
        off();
        delay(fLedDelay);
      }
      if (fTempStat == _onState) on();                  // return led status if it was on
    }
};


// --------------------------------------------------------------------------------------
//                                    -button control
// --------------------------------------------------------------------------------------
// @param  gpio pin, normal state (i.e. high or low when not pressed)
// example useage:      Button button1(12, HIGH);
//                      if (button1.beenPressed()) {do stuff};

class Button {

  private:
    int      _debounceDelay = 40;              // default debounce setting (ms)
    uint32_t _timer;                           // timer used for debouncing
    bool     _rawState;                        // raw state of the button
    byte     _gpiopin;                         // gpio pin of the button
    bool     _normalState;                     // gpio state when button not pressed
    bool     _debouncedState;                  // debounced state of button
    bool     _beenReleased = HIGH;             // flag to show the button has been released

  public:
    Button(byte bPin, bool bNormalState = LOW) {
      this->_gpiopin = bPin;
      this->_normalState = bNormalState;
      pinMode(_gpiopin, INPUT);
      _debouncedState = digitalRead(_gpiopin);
      _timer = millis();
    }

    bool update() {                            // update and return debounced button status
      bool newReading = digitalRead(_gpiopin);
      if (newReading != _rawState) {
        _timer = millis();
        _rawState = newReading;
      }
      if ( (unsigned long)(millis() - _timer) > _debounceDelay) _debouncedState = newReading;
      return _debouncedState;
    }

    bool isPressed() {                          // returns TRUE if button is curently pressed
      if (_normalState == LOW) return update();
      else return (!update());
    }

    bool beenPressed() {                        // returns TRUE once per button press
      bool cState = isPressed();
      if (cState == LOW) _beenReleased = HIGH;
      if (_beenReleased == LOW) cState = LOW;
      if (cState == HIGH) _beenReleased = LOW;
      return cState;
    }

    void debounce(int debounceDelay) {          // change debounce delay setting
      _debounceDelay = max(debounceDelay, 0);
    }

};


// ----------------------------------------------------------------
//                          -repeating Timer
// ----------------------------------------------------------------
// repeat an operation periodically
// usage example:   static repeatTimer timer1;             // set up a timer
//                  if (timer1.check(2000)) {do stuff};    // repeat every 2 seconds

class repeatTimer {

  private:
    uint32_t  _lastTime;                                              // store of last time the event triggered
    bool _enabled;                                                    // if timer is enabled

  public:
    repeatTimer() {
      reset();
      enable();
    }

    bool check(uint32_t timerInterval, bool timerReset=1) {           // check if supplied time has passed
      if ( (unsigned long)(millis() - _lastTime) >= timerInterval ) {
        if (timerReset) reset();
        if (_enabled) return 1;
      }
      return 0;
    }

    void disable() {                                                  // disable the timer
      _enabled = 0;
    }

    void enable() {                                                   // enable the timer
      _enabled = 1;
    }

    void reset() {                                                    // reset the timer
      _lastTime = millis();
    }
};


// --------------------------- E N D -----------------------------
