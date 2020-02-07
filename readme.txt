                        CameraWifiMotion - alanesq@disroot.org - Feb2020
                        ================================================

This is a Arduino IDE sketch to use one of the cheap (eBay) ESP32 camera boards as a motion detecting security camera
The idea is that the camera looks for movement in the image and when detected it captures an image storing it in
internal memory or on to an sd card and also emails it if required.


It stores the last 10 images captured in the onboard Spiffs memory and these can be viewed on the web page this device 
generates. If you install a sd card it will store all captured images on it along with a text file with the date and time 
the image was captured.  It has the ability to capture images at a higher resolution but will not be able to store 10 images 
if you set the highest.

The motion detection works by repeatedly capturing a greyscale image (320x240 pixels).  This image is split up in to 20x20 
pixel blocks (i.e. 16 x 12 blocks for the complete image).  All the pixel values in each block are averaged to give a single 
number for each block between 0 and 255 (i.e. average brightness of the block).
These resulting 16x12 blocks are then compared to the previously captured one and if the value of any block has varied by more 
then the "block" setting then this block is declared changed.
If enough blocks have changed (percentage of active image detection area, the "image" setting) then motion is detected.
So the two settings you can vary equate to:
        Block = how much brighness variation in the image is required to trigger
        Image = how much of the image area needs to change to count as movement detected

There are three settings for the motion detection, the first is how much change in average brightness in a block will count as
that block has changed, the second two numbers refer to the percentage of blocks in the image which have changed since the last 
image was captured and it will trigger as movement detected if this is between these two values.

There is a grid of tick boxes on the right of the main screen, this is a mask to set which parts of the image are used
when detecting motion (i.e. only the ticked areas are used).  This 4 x 3 grid results in mask sections of 16 blocks (4x4) 

It also has the following URLs you can use:
        http://<esp ip address> /ping - responds with OK, just so you know it is still working
                                /log - log page of the device activities
                                /test - used for testing bits of code etc.
                                /reboot - restarts the esp32 camera module
                                /default - sets all settings back to defaults
                                /live - capture and display a live image from the camera
                                /images - display the 10 images stored in Spiffs
                                /img - just display a plain jpg of the latest captured image
                                /bootlog - log of times the device has been switched on / rebooted (handy for checking it is stable)
                                /data - this is the updating text on the main page but handy for a quick check of status
                                

Note: I am a very amateur programmer so any help/advice improving this would be very greatly received. - alanesq@disroot.org
      This sketch includes a lot of other peoples code from many sources which I have included links to   
      Most specifically - https://eloquentarduino.github.io/2020/01/motion-detection-with-esp32-cam-only-arduino-version/
      which was just what I had been looking for and without which I could not have created this project.                  

I have it using my gmail account to send the emails
    Your gmail account needs to be set to "Allow less secure apps: ON"   see:  https://myaccount.google.com/lesssecureapps
    you then need to edit gmail_esp32.h with your details

Libraries used by this sketch are in the folder "libraries used"

It uses WifiManager so first time the ESP starts it will create an access point "ESPCamera" which you need to connect to in order to enter your wifi details.  
             default password = "12345678"   (note-it may not work if anything other than 8 characters long for some reason?)
             see: https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password
 
 
Note: It is vital that the ESP has a good 5volt supply (at least 0.5amp capable - although it draws around 
      100mA most of the time) otherwise you get all sorts of weird things happening including very slow network response times.
      The esp camera board also seems very sensitive to what is around the antenna and this can cause wifi to slow or stop
      if you put it in a case or mount it on a pcb etc.  (If it is going in a case I think an external antenna is required).
      Also I find that a smoothing capacitor is required on the 3.3v side otherwise the LED turning on/off can cause a
      lot of problems (specifically causing it to keep re-triggering and other random behaviour).
      
 
-----------------

compiling info:

    Bult using Arduino IDE 1.8.10, esp32 boards v1.0.4

    Using library SPIFFS at version 1.0 
    Using library FS at version 1.0 
    Using library WiFi at version 1.0
    Using library WebServer at version 1.0 
    Using library WiFiManager at version 0.99.9 
    Using library DNSServer at version 1.1.0 
    Using library Time at version 1.6 
    Using library ESP32_Mail_Client at version 2.1.1 
    Using library SD at version 1.0.5
    Using library SPI at version 1.0 
    Using library HTTPClient at version 1.2 
    Using library WiFiClientSecure at version 1.0 
    
---------------
