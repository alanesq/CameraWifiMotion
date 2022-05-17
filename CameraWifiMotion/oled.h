/**************************************************************************************************
 *
 *    OLED / rotary encoder based simple none blocking menu System - i2c version SSD1306 - 17May22
 *
 *    part of the BasicWebserver sketch -  https://github.com/alanesq/BasicWebserver
 *
 **************************************************************************************************

 The sketch displays a menu on the oled and when an item is selected it sets a
 flag and waits until the event is acted upon.  Max menu items on a 128x64 oled
 is four.

 Notes:   text size 1 = 21 x 8 characters on the larger oLED display
          text size 2 = 10 x 4

 For more oled info    see: https://randomnerdtutorials.com/guide-for-oled-display-with-arduino/

 See the "menus below here" section for examples of how to use the menus

 see: https://randomnerdtutorials.com/esp32-ssd1306-oled-display-arduino-ide/
      https://lastminuteengineers.com/oled-display-esp32-tutorial/


 **************************************************************************************************/


#include <Wire.h>                 // i2c
#include <Adafruit_GFX.h>         // graphics
#include <Adafruit_SSD1306.h>     // oled display
#define SSD1306_NO_SPLASH         // disable oled splash screen


// ----------------------------------------------------------------
//                         S E T T I N G S
// ----------------------------------------------------------------

// Best pins to use on esp32: 4,5,13-19,21-23,25-27,32,33


    //  esp32 - cnc pcb
      #define encoder0PinA  21                  // Rotary encoder gpio pin - 16
      #define encoder0PinB  22                  // Rotary encoder gpio pin - 17
      #define encoder0Press 23                  // Rotary encoder button gpio pin - 23
      #define OLEDC 4                           // oled clock pin (set to -1 for default) - 26
      #define OLEDD 16                          // oled data pin - 27
      #define OLEDE -1                          // oled enable pin (set to -1 if not used)


/*
      //  esp32 - breadboard
        #define encoder0PinA  32                  // Rotary encoder gpio pin - 16
        #define encoder0PinB  33                  // Rotary encoder gpio pin - 17
        #define encoder0Press 23                  // Rotary encoder button gpio pin - 23
        #define OLEDC 26                          // oled clock pin (set to -1 for default) - 26
        #define OLEDD 27                          // oled data pin - 27
        #define OLEDE -1                          // oled enable pin (set to -1 if not used)
*/

//    //  esp32 HiLetGo gpio pins - https://robotzero.one/heltec-wifi-kit-32/
//      #define encoder0PinA  25                  // Rotary encoder gpio pin
//      #define encoder0PinB  33                  // Rotary encoder gpio pin
//      #define encoder0Press 32                  // Rotary encoder button gpio pin
//      #define OLEDC 15                          // oled clock pin (set to -1 for default)
//      #define OLEDD 4                           // oled data pin
//      #define OLEDE 16                          // oled enable pin (set to -1 if not used)

/*
    // esp8266 gpio pins
      #define encoder0PinA  14                  // Rotary encoder gpio pin, 14 = D5 on esp8266 - 14
      #define encoder0PinB  12                  // Rotary encoder gpio pin, 12 = D6 on esp8266 - 12
      #define encoder0Press 13                  // Rotary encoder button gpio pin, 13 = D7 on esp8266 - 13
      #define OLEDC -1                          // oled clock pin (set to -1 for default) - D1
      #define OLEDD -1                          // oled data pin - D2
      #define OLEDE -1                          // oled enable pin (set to -1 if not used)
*/


    // oLED settings
      #define OLED_ADDR 0x3C                    // OLED i2c address
      #define SCREEN_WIDTH 128                  // OLED display width, in pixels (usually 128)
      #define SCREEN_HEIGHT 64                  // OLED display height, in pixels (64 for larger oLEDs)
      #define OLED_RESET -1                     // Reset pin gpio (-1 if sharing Arduino reset pin)

    // Misc settings
      #define BUTTONPRESSEDSTATE 0              // rotary encoder button gpio pin logic level when the pressed (usually 0)
      #define DEBOUNCEDELAY 60                  // debounce delay for button inputs
      const uint32_t defaultMenuTimeout = 10;   // default menu inactivity timeout (seconds)
      const bool menuLargeText = 0;             // show larger text when possible (to help with readability)
      const int maxMenuItems = 12;              // max number of items used in any of the menus (keep as low as possible to save memory)
      const int itemTrigger = 1;                // rotary encoder - counts per tick (varies between encoders usually 1 or 2)
      const int topLine = 18;                   // y position of lower area of the display (18 with two colour displays)
      const byte lineSpace1 = 9;                // line spacing for textsize 1 (small text)
      const byte lineSpace2 = 17;               // line spacing for textsize 2 (large text)
      const int displayMaxLines = 5;            // max lines that can be displayed in lower section of display in textsize1 (5 on larger oLeds)
      const int MaxmenuTitleLength = 10;        // max characters per line when using text size 2 (usually 10)
      const int minOLEDrefreshTime = 50;        // minimum time between oled updates (ms)


// -------------------------------------------------------------------------------------------------


// forward declarations (to allow the procedures to be out of logical order below)
  void IRAM_ATTR doEncoder();
  void resetMenu(bool);
  void serviceMenu();
  int checkEncoder();
  int serviceValue(bool);
  void arrayMenu(String, int, String *_list);
  void displayMessage(String, String);
  void defaultMenu();
  void reUpdateButton();


// misc variables
  uint32_t minOLEDrefreshTimer = 0;              // timer for oled updates


// menus
  // modes that the menu system can be in
  enum class menuModes : uint8_t {
      off = 0,                                // display is off
      menu,                                   // menu is active
      value,                                  // 'enter a value' none blocking is active
      message,                                // displaying a message is active
      blocking                                // a blocking procedure is in progress (see enter value-blocking)
  };
  menuModes menuMode = menuModes::off;

  // variables for the menu system
  struct oledMenus {
    String title = "";                        // the title of active mode
    int NoOfItems = 0;                        // number if menu items
    int result = 0;                           // when a menu item is selected or value entered it is flagged here for actioning
    int highlightedItem = 0;                  // which item is curently highlighted in the menu
    String items[maxMenuItems+1];             // store for the menu items
    uint32_t lastMenuActivity = 0;            // time the menu last saw any activity (used for timeout)
    int mValueLow = 0;                        // lowest allowed value (enter value)
    int mValueHigh = 0;                       // highest allowed value
    int mActionFlag = 0;                      // flag which can be used to signal desired action after user input
    int mValueStep = 0;                       // step size when encoder is turned (enter value)
    uint32_t menuTimeout = defaultMenuTimeout;// timeout (seconds)
  };
  oledMenus oledMenu;

  // variables for rotary encoder
  struct rotaryEncoders {
    volatile int encoder0Pos = 0;             // current value selected with rotary encoder (updated by interrupt routine)
    volatile bool encoderPrevA;               // used to debounced rotary encoder
    volatile bool encoderPrevB;               // used to debounced rotary encoder
    uint32_t reLastButtonChange = 0;          // last time state of button changed (for debouncing)
    bool encoderPrevButton = 0;               // used to debounce button
    int reButtonDebounced = 0;                // debounced current button state (1 when pressed)
    const bool reButtonPressedState = BUTTONPRESSEDSTATE;  // the logic level when the button is pressed
    const uint32_t reDebounceDelay = DEBOUNCEDELAY;        // button debounce delay setting
    bool reButtonPressed = 0;                 // flag set when the button is pressed (stays high until it is actioned)
  };
  rotaryEncoders rotaryEncoder;

// oled SSD1306 display connected to I2C
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// -------------------------------------------------------------------------------------------------
//                                         menus below here
// -------------------------------------------------------------------------------------------------

// forward declarations for this section (so they don't have to be in a specific order)
  void value1();
  void demoMenu();


// Called when default menu is required
void defaultMenu() {
  demoMenu();
}


//                -----------------------------------------------


// set up the demonstration menu
void demoMenu() {
  resetMenu(1);                                // clear any previous menu settings (1=also reset timeout)
  menuMode = menuModes::menu;                  // enable menu mode
  oledMenu.NoOfItems = 8;                      // set the number of items in this menu
  oledMenu.title = "demo_menu";                // menus title (used to identify it)
  oledMenu.items[1] = "item1";                 // set the menu items
  oledMenu.items[2] = "item2";
  oledMenu.items[3] = "Array menu";
  oledMenu.items[4] = "On or Off";
  oledMenu.items[5] = "Enter value";
  oledMenu.items[6] = "Enter value-blocking";
  oledMenu.items[7] = "Message";
  oledMenu.items[8] = "Menus Off";
}  // demoMenu


// actions for menu selections are put in here
void menuActions() {

  // actions when an item is selected in "demo_menu"
  if (oledMenu.title == "demo_menu") {

    // demonstrate creating a menu from an array
    if (oledMenu.result == 3) {
      if (serialDebug) Serial.println("demo_menu: create menu from an array selected");
      String tList[]={"main menu", "item2", "item3", "item4", "item5", "item6"};
      arrayMenu("demo_list", 6, &tList[0]);     // 6 = number if items in the
    }

    // demonstrate selecting between 2 options only
    if (oledMenu.result == 4) {
      if (serialDebug) Serial.println("demo_menu: 'on or off' selected");
      resetMenu(1);
      menuMode = menuModes::value; oledMenu.title = "on or off"; oledMenu.mValueLow = 0; oledMenu.mValueHigh = 1; oledMenu.mValueStep = 1; oledMenu.result = 0;  // set parameters
    }

    // demonstrate usage of 'enter a value' (none blocking)
    if (oledMenu.result == 5) {
      if (serialDebug) Serial.println("demo_menu: none blocking enter value selected");
      value1();       // enter a value
    }

    // demonstrate usage of 'enter a value' (blocking) which is quick and easy but stops all other tasks whilst the value is entered
    if (oledMenu.result == 6) {
      if (serialDebug) Serial.println("demo_menu: blocking 'enter a value' selected");
      // request value
        resetMenu(1);
        menuMode = menuModes::value; oledMenu.title = "blocking"; oledMenu.mValueLow = 0; oledMenu.mValueHigh = 50; oledMenu.mValueStep = 1; oledMenu.result = 5;
        int tEntered = serviceValue(1);    // 1 = blocking mode
      Serial.println(" The value entered was " + String(tEntered));
      defaultMenu();
    }

    // demonstrate usage of message
    if (oledMenu.result == 7) {
      if (serialDebug) Serial.println("demo_menu: demo message selected");
      oledMenu.mActionFlag = 1;   // this is used to set the action when message is closed (0=default menu, 1=display off) - see 'oledLoop()' to modify this
      //oledMenu.menuTimeout = 30;       // set message to display for 30 seconds
      displayMessage("Message", "This is a demo\nmessage\ndisplay will turn off");    // 21 chars per line, "\n" = next line
    }

    // turn menu/oLED off
    else if (oledMenu.result == 8) {
      if (serialDebug) Serial.println("demo_menu: menu off selected");
      resetMenu(1);    // turn menus off
    }

    if (menuMode == menuModes::menu) oledMenu.result = 0;        // clear menu item selected flag if still in menu mode
  }


  // actions when an item is selected in the demo_list menu
  if (oledMenu.title == "demo_list") {
    // back to main menu
    if (oledMenu.result == 1) {
      if (serialDebug) Serial.println("demo_list: back to main menu selected");
      defaultMenu();
    }
    oledMenu.result = 0;                // clear menu item selected flag
  }

}  // menuActions


//                -----------------------------------------------


// set up demonstration 'enter a value' (none blocking)
void value1() {
  resetMenu(1);                                // clear any previous menu
  menuMode = menuModes::value;                            // enable value entry mode
  oledMenu.title = "demo_value";               // title (used to identify which number was entered)
  oledMenu.mValueLow = 0;                      // minimum value allowed
  oledMenu.mValueHigh = 25;                    // maximum value allowed
  oledMenu.mValueStep = 1;                     // step size
  oledMenu.result = 15;                        // starting value
}  // value1


// actions for value entered put in here
void menuValues() {

  // action for "demo_value"
  if (oledMenu.title == "demo_value") {
    if (serialDebug) Serial.println("demo_value: a value has been entered");
    String tString = String(oledMenu.result);
    displayMessage("ENTERED", "\nYou entered\nthe value\n    " + tString);
    // alternatively use 'resetMenu(1)' here to turn menus off after value entered - or use 'defaultMenu()' to re-start the default menu
  }

  // action for "on or off"
  if (oledMenu.title == "on or off") {
    if (serialDebug) Serial.println("demo_menu: 'on or off' selected, result was " + String(oledMenu.result));
    defaultMenu();
  }

}  // menuValues


// -------------------------------------------------------------------------------------------------
//                                         menus above here
// -------------------------------------------------------------------------------------------------


// ----------------------------------------------------------------
//                              -setup
// ----------------------------------------------------------------
// called from main setup

void oledSetup() {

  // configure gpio pins for rotary encoder
    pinMode(encoder0Press, INPUT_PULLUP);
    pinMode(encoder0PinA, INPUT);
    pinMode(encoder0PinB, INPUT);

  // initialise the oled display
    // enable pin
      if (OLEDE != -1) {
        pinMode(OLEDE , OUTPUT);
        digitalWrite(OLEDE, HIGH);
      }
    if (-1 == OLEDC) Wire.begin();
    else Wire.begin(OLEDD, OLEDC);
    //Wire.setClock(100000);    // change i2c bus speed
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
      if (serialDebug) Serial.println(("\nError initialising the oled display"));
    }

  // Interrupt for reading the rotary encoder position
    rotaryEncoder.encoder0Pos = 0;
    attachInterrupt(digitalPinToInterrupt(encoder0PinA), doEncoder, CHANGE);

  // display greeting message
    resetMenu(1);                  // clear display
    oledMenu.menuTimeout = 60;     // set timeout to 1min
    displayMessage(stitle, sversion);
    //defaultMenu();                 // alternatibely you can start the default menu

}  // oledSetup


// ----------------------------------------------------------------
//                              -loop
// ----------------------------------------------------------------
// called from main loop

void oledLoop() {

  reUpdateButton();                          // update rotary encoder button status (if pressed activate default menu)

  // check if too soon to do an update
    if ( (unsigned long)(millis() - minOLEDrefreshTimer) < minOLEDrefreshTime ) {
      return;
    } else {
      minOLEDrefreshTimer = millis();    // reset timer
    }

  if (menuMode == menuModes::off) return;    // if menu system is turned off do nothing more

  // if no recent activity then turn oled off
    if ( (unsigned long)(millis() - oledMenu.lastMenuActivity) > (oledMenu.menuTimeout * 1000) ) {
      resetMenu(0);
      return;
    }

    switch (menuMode) {

      // if menus are off or blocking
      case menuModes::off:
        break;
      case menuModes::blocking:
        break;

      // if there is an active menu
      case menuModes::menu:
        serviceMenu();
        menuActions();
        break;

      // if there is an active none blocking 'enter value'
      case menuModes::value:
        serviceValue(0);
        if (rotaryEncoder.reButtonPressed) {                        // if the button has been pressed
          menuValues();                                             // action the entered value
          break;
        }

      // if a message is being displayed
      case menuModes::message:
        if (rotaryEncoder.reButtonPressed == 1) {                   // if button has been pressed
            if (oledMenu.mActionFlag == 0) defaultMenu();           // return to default menu
            if (oledMenu.mActionFlag == 1) resetMenu(0);            // oLED off
            oledMenu.mActionFlag = 0;                               // reset action after message flag
        }
        break;
    }

}  // oledLoop


// ----------------------------------------------------------------
//                   -button debounce (rotary encoder)
// ----------------------------------------------------------------
// update rotary encoder button status

void reUpdateButton() {

    bool tReading = digitalRead(encoder0Press);        // read current button state
    if (tReading != rotaryEncoder.encoderPrevButton) rotaryEncoder.reLastButtonChange = millis();          // if it has changed reset timer
    if ( (unsigned long)(millis() - rotaryEncoder.reLastButtonChange) > rotaryEncoder.reDebounceDelay ) {  // if button state is stable
      if (rotaryEncoder.encoderPrevButton == rotaryEncoder.reButtonPressedState) {                         // if the button is pressed
        if (rotaryEncoder.reButtonDebounced == 0) {    // if buton was previously flagged as released
          rotaryEncoder.reButtonPressed = 1;           // flag button has been pressed (this stays high until it is actioned)
          if (menuMode == menuModes::off) defaultMenu();          // if the display is off start the default menu
        }
        rotaryEncoder.reButtonDebounced = 1;           // the debouced current status of the button
      } else {
        rotaryEncoder.reButtonDebounced = 0;
      }
    }
    rotaryEncoder.encoderPrevButton = tReading;        // update previous button state

}  // reUpdateButton


// ----------------------------------------------------------------
//                             -menu
// ----------------------------------------------------------------
// service the active menu

void serviceMenu() {

    // rotary encoder
      if (rotaryEncoder.encoder0Pos >= itemTrigger) {
        rotaryEncoder.encoder0Pos -= itemTrigger;
        oledMenu.highlightedItem++;
        oledMenu.lastMenuActivity = millis();   // log time
      }
      if (rotaryEncoder.encoder0Pos <= -itemTrigger) {
        rotaryEncoder.encoder0Pos += itemTrigger;
        oledMenu.highlightedItem--;
        oledMenu.lastMenuActivity = millis();   // log time
      }
      if (rotaryEncoder.reButtonPressed == 1) {
        oledMenu.result = oledMenu.highlightedItem;     // flag that the item has been selected
        oledMenu.lastMenuActivity = millis();   // log time
        if (serialDebug) Serial.println("menu '" + oledMenu.title + "' item '" + oledMenu.items[oledMenu.highlightedItem] + "' selected");
      }

    const int _centreLine = displayMaxLines / 2 + 1;    // mid list point
    display.clearDisplay();
    display.setTextColor(WHITE);

    // verify valid highlighted item
      if (oledMenu.highlightedItem > oledMenu.NoOfItems) oledMenu.highlightedItem = oledMenu.NoOfItems;
      if (oledMenu.highlightedItem < 1) oledMenu.highlightedItem = 1;

    // title
      display.setCursor(0, 0);
      if (menuLargeText) {
        display.setTextSize(2);
        display.println(oledMenu.items[oledMenu.highlightedItem].substring(0, MaxmenuTitleLength));
      } else {
        if (oledMenu.title.length() > MaxmenuTitleLength) display.setTextSize(1);
        else display.setTextSize(2);
        display.println(oledMenu.title);
      }
      display.drawLine(0, topLine-1, display.width(), topLine-1, WHITE);       // draw horizontal line under title

    // menu
      display.setTextSize(1);
      display.setCursor(0, topLine);
      for (int i=1; i <= displayMaxLines; i++) {
        int item = oledMenu.highlightedItem - _centreLine + i;
        if (item == oledMenu.highlightedItem) display.setTextColor(BLACK, WHITE);
        else display.setTextColor(WHITE);
        if (item > 0 && item <= oledMenu.NoOfItems) display.println(oledMenu.items[item]);
        else display.println(" ");
      }

    display.display();

}  // serviceMenu


// ----------------------------------------------------------------
//                            -value entry
// ----------------------------------------------------------------
// service the value entry
// @param   _blocking   if set to 1 then repeat until a value is entered

int serviceValue(bool _blocking) {

  const int _valueSpacingX = 30;            // spacing for the displayed value y position
  const int _valueSpacingY = 5;             // spacing for the displayed value y position

  if (_blocking) {
    menuMode = menuModes::blocking;
    oledMenu.lastMenuActivity = millis();   // log time of last activity (for timeout)
  }
  uint32_t tTime;

  do {

    // rotary encoder
      if (rotaryEncoder.encoder0Pos >= itemTrigger) {
        rotaryEncoder.encoder0Pos -= itemTrigger;
        oledMenu.result-= oledMenu.mValueStep;
        oledMenu.lastMenuActivity = millis();
      }
      if (rotaryEncoder.encoder0Pos <= -itemTrigger) {
        rotaryEncoder.encoder0Pos += itemTrigger;
        oledMenu.result+= oledMenu.mValueStep;
        oledMenu.lastMenuActivity = millis();
      }
      if (oledMenu.result < oledMenu.mValueLow) {
        oledMenu.result = oledMenu.mValueLow;
        oledMenu.lastMenuActivity = millis();
      }
      if (oledMenu.result > oledMenu.mValueHigh) {
        oledMenu.result = oledMenu.mValueHigh;
        oledMenu.lastMenuActivity = millis();
      }

      display.clearDisplay();
      display.setTextColor(WHITE);

      // title
        display.setCursor(0, 0);
        if (oledMenu.title.length() > MaxmenuTitleLength) display.setTextSize(1);
        else display.setTextSize(2);
        display.println(oledMenu.title);
        display.drawLine(0, topLine-1, display.width(), topLine-1, WHITE);       // draw horizontal line under title

      // value selected
        display.setCursor(_valueSpacingX, topLine + _valueSpacingY);
        display.setTextSize(3);
        display.println(oledMenu.result);

      // range
        display.setCursor(0, display.height() - lineSpace1 - 1 );   // bottom of display
        display.setTextSize(1);
        display.println(String(oledMenu.mValueLow) + " to " + String(oledMenu.mValueHigh));

      // bar
        int Tlinelength = map(oledMenu.result, oledMenu.mValueLow, oledMenu.mValueHigh, 0 , display.width());
        display.drawLine(0, display.height()-1, Tlinelength, display.height()-1, WHITE);

      display.display();

      reUpdateButton();        // check status of button
      tTime = (unsigned long)(millis() - oledMenu.lastMenuActivity);      // time since last activity

  } while (_blocking && rotaryEncoder.reButtonPressed == 0 && tTime < (oledMenu.menuTimeout * 1000));        // if in blocking mode repeat until button is pressed or timeout

  if (_blocking) menuMode = menuModes::off;

  return oledMenu.result;        // used when in blocking mode

}  // serviceValue


// ----------------------------------------------------------------
//                           -array menu
// ----------------------------------------------------------------
// create a menu from a String array
// Example usage:       String tList[]={"main menu", "2", "3", "4", "5", "6"};
//                      arrayMenu("demo_list", 6, &tList[0]);

void arrayMenu(String _title, int _noOfElements, String *_list) {

  if (_noOfElements > maxMenuItems) {
    displayMessage("Error:","You need to \nincrease \n'maxMenuItems'");
    return;
  }
  resetMenu(0);                           // clear any previous menu
  menuMode = menuModes::menu;                       // enable menu mode
  oledMenu.NoOfItems = _noOfElements;    // set the number of items in this menu
  oledMenu.title = _title;               // menus title (used to identify it)

  for (int i=1; i <= _noOfElements; i++) {
    oledMenu.items[i] = _list[i-1];      // set the menu items
  }

}  // createList


// ----------------------------------------------------------------
//                         -message display
// ----------------------------------------------------------------
// 21 characters per line, use "\n" for next line
// assistant:  <     line 1        ><     line 2        ><     line 3        ><     line 4         >

 void displayMessage(String _title, String _message) {

  resetMenu(0);
  menuMode = menuModes::message;

  display.clearDisplay();
  display.setTextColor(WHITE);

  // title
    display.setCursor(0, 0);
    if (menuLargeText) {
      display.setTextSize(2);
      display.println(_title.substring(0, MaxmenuTitleLength));
    } else {
      if (_title.length() > MaxmenuTitleLength) display.setTextSize(1);
      else display.setTextSize(2);
      display.println(_title);
    }

  // message
    display.setCursor(0, topLine);
    display.setTextSize(1);
    display.println(_message);

  display.display();

  // send to serial
  if (serialDebug) {
    _message.replace("\n","/ ");      // remove line feeds
    Serial.println("oLED Message: " + _title + ", " + _message);
  }

 }  // displayMessage


// ----------------------------------------------------------------
//                        -reset menu system
// ----------------------------------------------------------------
// @param   _timeout    if 1 then also reset the timeout setting

void resetMenu(bool _timeout) {
  // reset all menu variables / flags
    menuMode = menuModes::off;
    oledMenu.result = 0;
    rotaryEncoder.encoder0Pos = 0;
    oledMenu.NoOfItems = 0;
    oledMenu.title = "";
    oledMenu.highlightedItem = 0;
    oledMenu.result = 0;
    rotaryEncoder.reButtonPressed = 0;
    if (_timeout) oledMenu.menuTimeout = defaultMenuTimeout;   // set menu timeouts to default

  oledMenu.lastMenuActivity = millis();   // log time

  // clear oled display
    display.clearDisplay();
    display.display();

}  // resetMenu


// ----------------------------------------------------------------
//                     -interrupt for rotary encoder
// ----------------------------------------------------------------
// rotary encoder interrupt routine to update position counter when turned
//     interrupt info: https://www.gammon.com.au/forum/bbshowpost.php?id=11488

//void ICACHE_RAM_ATTR doEncoder() {
void IRAM_ATTR doEncoder() {

  bool pinA = digitalRead(encoder0PinA);
  bool pinB = digitalRead(encoder0PinB);

  if ( (rotaryEncoder.encoderPrevA == pinA && rotaryEncoder.encoderPrevB == pinB) ) return;  // no change since last time (i.e. reject bounce)

  // same direction (alternating between 0,1 and 1,0 in one direction or 1,1 and 0,0 in the other direction)
         if (rotaryEncoder.encoderPrevA == 1 && rotaryEncoder.encoderPrevB == 0 && pinA == 0 && pinB == 1) rotaryEncoder.encoder0Pos -= 1;
    else if (rotaryEncoder.encoderPrevA == 0 && rotaryEncoder.encoderPrevB == 1 && pinA == 1 && pinB == 0) rotaryEncoder.encoder0Pos -= 1;
    else if (rotaryEncoder.encoderPrevA == 0 && rotaryEncoder.encoderPrevB == 0 && pinA == 1 && pinB == 1) rotaryEncoder.encoder0Pos += 1;
    else if (rotaryEncoder.encoderPrevA == 1 && rotaryEncoder.encoderPrevB == 1 && pinA == 0 && pinB == 0) rotaryEncoder.encoder0Pos += 1;

  // change of direction
    else if (rotaryEncoder.encoderPrevA == 1 && rotaryEncoder.encoderPrevB == 0 && pinA == 0 && pinB == 0) rotaryEncoder.encoder0Pos += 1;
    else if (rotaryEncoder.encoderPrevA == 0 && rotaryEncoder.encoderPrevB == 1 && pinA == 1 && pinB == 1) rotaryEncoder.encoder0Pos += 1;
    else if (rotaryEncoder.encoderPrevA == 0 && rotaryEncoder.encoderPrevB == 0 && pinA == 1 && pinB == 0) rotaryEncoder.encoder0Pos -= 1;
    else if (rotaryEncoder.encoderPrevA == 1 && rotaryEncoder.encoderPrevB == 1 && pinA == 0 && pinB == 1) rotaryEncoder.encoder0Pos -= 1;

    //else if (serialDebug) Serial.println("Error: invalid rotary encoder pin state - prev=" + String(rotaryEncoder.encoderPrevA) + ","
    //                                      + String(rotaryEncoder.encoderPrevB) + " new=" + String(pinA) + "," + String(pinB));

  // update previous readings
    rotaryEncoder.encoderPrevA = pinA;
    rotaryEncoder.encoderPrevB = pinB;

}  // ICACHE_RAM_ATTR


// ---------------------------------------------- end ----------------------------------------------
