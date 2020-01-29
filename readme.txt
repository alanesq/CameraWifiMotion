This is a sketch to use one of the cheap (eBay) ESP32 camera boards as a motion detecting security camera
The idea is that the camera looks for movement and when detected it captures an image and emails it.

It stores the last 10 images captured in the onboard Spiffs memory and these can be viewed on the web page this device 
generates. If you install a sd card it will store all captured images on it along with a text file with the date and time 
the image was captured.
It also has the following URLs you can use:
    http://<esp ip address>     /ping - responds with OK, just so you know it is still working
                                /log - log page of the device activities
                                /test - used for testing bits of code etc.
                                /reboot - restarts the esp32 camera module
                                /default - sets all settings back to defaults
                                /live - capture and display a live image from the camera
                                /images - display the 10 images stored in Spiffs
                                /img - just display a plain jpg of the latest captured image

Note: I am a very amateur programmer so any help/advice improving this would be very greatly received. - alanesq@disroot.org
      This sketch includes a lot of other peoples code from many sources which I have included links to   
      Most specifically - https://eloquentarduino.github.io/2020/01/motion-detection-with-esp32-cam-only-arduino-version/
      which was just what I had been looking for and without which I could not have created this project.                  

I have it using my gmail account to send the emails
    Your gmail account needs to be set to "Allow less secure apps: ON"   see:  https://myaccount.google.com/lesssecureapps
    you then need to edit gmail_esp32.h with your details

Libraries used by this sketch are in the folder "libraries used"

It uses WifiManager so first time the ESP starts it will create an access point "ESPConfig" which you need to connect to in order to enter your wifi details.  
             default password = "12345678"   (note-it may not work if anything other than 8 characters long for some reason?)
             see: https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password
 
 
Note: As I discovered myself, it is vital that the ESP has a good 5volt supply (at least 0.5amp capable - although it draws around 
      100mA most of the time) otherwise you get all sorts of weird things happening including very slow network response times.
 
 
-----------------

compiling info:

    Bult using Arduino IDE 1.8.10, esp32 boards v1.0.4

    Using library SPIFFS at version 1.0 in folder: /home/alan/.arduino15/packages/esp32/hardware/esp32/1.0.4/libraries/SPIFFS 
    Using library FS at version 1.0 in folder: /home/alan/.arduino15/packages/esp32/hardware/esp32/1.0.4/libraries/FS 
    Using library WiFi at version 1.0 in folder: /home/alan/.arduino15/packages/esp32/hardware/esp32/1.0.4/libraries/WiFi 
    Using library WebServer at version 1.0 in folder: /home/alan/Arduino/libraries/WebServer 
    Using library WiFiManager at version 0.99.9 in folder: /home/alan/Arduino/libraries/WiFiManager 
    Using library DNSServer at version 1.1.0 in folder: /home/alan/Arduino/libraries/DNSServer 
    Using library Time at version 1.6 in folder: /home/alan/Arduino/libraries/Time 
    Using library ESP32_Mail_Client at version 2.1.1 in folder: /home/alan/Arduino/libraries/ESP32_Mail_Client 
    Using library SD at version 1.0.5 in folder: /home/alan/.arduino15/packages/esp32/hardware/esp32/1.0.4/libraries/SD 
    Using library SPI at version 1.0 in folder: /home/alan/.arduino15/packages/esp32/hardware/esp32/1.0.4/libraries/SPI 
    Using library HTTPClient at version 1.2 in folder: /home/alan/.arduino15/packages/esp32/hardware/esp32/1.0.4/libraries/HTTPClient 
    Using library WiFiClientSecure at version 1.0 in folder: /home/alan/.arduino15/packages/esp32/hardware/esp32/1.0.4/libraries/WiFiClientSecure 
    
---------------
