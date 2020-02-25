                        CameraWifiMotion - alanesq@disroot.org - 23 Feb 2020
                        ====================================================

This is a Arduino IDE sketch to use one of the cheap (eBay) ESP32 camera boards as a motion detecting security camera
The idea is that the camera looks for movement in the image and when detected it captures an image storing it in
internal memory or on to an sd card and also emails it if required.

NOTE: I have recently re-written the wifi.h to use the more recent libraries, specifically https://github.com/khoih-prog/ESP_WiFiManager
so if you have used an older version of this sketch you may need to update the libraries you have installed.

The sketch can use OTA (Over the air updates) this can be enabled/disabled in the main settings of the sketch.
If you use OTA do not use the "ESP32-cam" board in the Arduino IDE, use "ESP32 Dev Module" and make sure PSRAM is enabled, 
if you do not do this it will just get to a few percent upload and stop.
In an attempt to give some form of security I have set up the sketch so that when OTA is enabled you can not access it
until you have entered a "secret password", the password is entered in the form "http://<ip address of esp>?pwd=12345678".
You can check it has worked by looking in the log and once this has been done you can then access "http://<ip address of esp>/ota". 
You can change this password in the main settings (OTAPassword).

BTW - Please let me know if you are using this (my email = alanesq@disroot.org), as I would be interested to know if 
people are finding this project of interest/use etc. it being my first one ;-)

                   -------------------------------------------------------------------------------------

If you wish to use the email facility you need to enter your email details in gmail_esp.h and note the security settings may need changing on the gmail account.
There is a zip file containing the libraries used.  The main ones you will need to install are:
  ESP32_mail_client, ESP_wifimanager and Time.


The last 10 images captured are stored in the onboard Spiffs memory and these can be viewed on the web page this device 
generates. If you install a sd card it will store all captured images on it.  It has the ability to capture images at a higher resolution but will not be able to store 10 images and I think it can struggle with the amount of data.

It uses WifiManager so first time the ESP starts it will create an access point "ESPCamera" which you need to connect to in order to enter your wifi details.  
             default password = "12345678"   (note-it may not work if anything other than 8 characters long for some reason?)
             see: https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password
             Once you have entered your wifi password it will restart and you can then connect to it in your browser
             at the address:     http://ESPcam1.local     (if your browser / config. supports it)

The motion detection is based on - https://eloquentarduino.github.io/2020/01/motion-detection-with-esp32-cam-only-arduino-version/
It works by repeatedly capturing a greyscale image (320x240 pixels).  This image is split up in to 20x20 
pixel blocks (i.e. 16 x 12 blocks for the complete image).  All the pixel values in each block are averaged to give a single 
number for each block between 0 and 255 (i.e. average brightness of the block).
These resulting 16x12 blocks are then compared to the previously captured one and if the value of any block has varied by more 
then the "block" setting then this block is declared changed.
If enough blocks have changed (percentage of active image detection area, the "image" setting) then motion is detected.
So the settings you can vary equate to:
        Block = how much brighness variation in a block is required to flag it as changed
        Image = how much of the image area needs to change to count as movement detected
When motion detecting is enabled it will show the current average image brightness along with what motion it is currently detecting
in the format "Readings: brightness:113, 0 changed blocks out of 64".  You can use this to fine tune your detection settings.

There are three settings for the motion detection, the first is how much change in average brightness in a block will count as
that block has changed, the second two numbers refer to the number of blocks in the image which have changed since the last 
image was captured and it will trigger as motion detected if this is between these two values.
So the easiest way to set it up is to set the two trigger levels to max and the trigger level to around 15, then in the "raw data"
page with motion detection disabled refresh a couple of times until you are seeing very low values in the top window (difference).
now if you move someone/something in to the view of the camera and refresh the page you will be able to see what differences
the software has detected in the image (fine tune the detection threshold for the best results).
Now if you go back to the main page and enable motion detection you can watch the levels being detected on the second line
on the page (i.e. "Readings: brightness:105, 1 changed blocks out of 64").

There is a grid of tick boxes on the right of the main screen, this is a mask to set which parts of the image are used
when detecting motion (i.e. only the ticked areas are used).  This 4 x 3 grid results in mask sections of 16 blocks (4x4) 


It also has the following URLs you can use:

        http://<esp ip address> /ping - responds with either 'enabled' or 'disabled', just so you know it is still working
                                /log - log page of the device activities
                                /test - used for testing bits of code etc.
                                /reboot - restarts the esp32 camera module
                                /default - sets all settings back to defaults
                                /live - capture and display a live image from the camera
                                /images - display the 10 images stored in Spiffs (Image width in percent can be specified 
                                    in URL with http://x.x.x.x/images?width=90)
                                /bootlog - log of times the device has been switched on / rebooted (handy for checking 
                                    it is stable)
                                /data - this is the updating text on the main page but handy for a quick check of status
                                /imagedata - show raw block data
                                /ota - update firmware (requires password entered first)
                                /img - just display a plain jpg 
                                       defaults to the live greyscale image 
                                       you can select stored images with /img?pic=x   where x is in the range 1 to 10 
                                       (add 100 to x to display the greyscale images)
                                


I have it using my gmail account to send the emails
    Your gmail account needs to be set to "Allow less secure apps: ON"   see:  https://myaccount.google.com/lesssecureapps
    you then need to edit gmail_esp32.h with your details

 

 
-----------------

Notes:

If you ever need to erase the stored wifi settings or if the esp32 goes in to a power loop where you are unable to get
Wifimanager to work see - https://www.robmiles.com/journal/2019/05/26/esp32reset

I am a very amateur programmer so any help/advice improving this would be very greatly received. - alanesq@disroot.org
This sketch includes a lot of other peoples code from many sources which I have included links to   
Most specifically - https://eloquentarduino.github.io/2020/01/motion-detection-with-esp32-cam-only-arduino-version/
which was just what I had been looking for and without which I could not have created this project.                  

It is vital that the ESP has a good 5volt supply (at least 0.5amp capable - although it draws around 
120mA most of the time) otherwise you get all sorts of weird things happening including very slow network response times.
The esp camera board also seems very sensitive to what is around the antenna and this can cause wifi to slow or stop
if you put it in a case or mount it on a pcb etc.  (If it is going in a case I think an external antenna is required).
Also I find that a smoothing capacitor is required on the 3.3v side otherwise the LED turning on/off can cause a
lot of problems (specifically causing it to keep re-triggering and other random behaviour).
Using the flash can often trigger such problems if there is any problem with the power supply.

The camera on these modules is not very good in dark conditions but I have found that if you take the lens of the camera 
(I heated it with a warm air gun to soften the glue first) you can remove the infra red filter (a small disk between the lens
and the chip) and this improves it a bit but this will make it look odd in normal conditions.
I tried fitting a larger lens to the camera but this surprisingly did not seem to help.   picture: http://www.alanesq.eu5.net/extlinkins/esp32-big-lens.htm
I think the camera can work a bit better in the dark than it does but I am having trouble setting the parameters,
I am looking in to this.  I have managed to add brightness adjustment now but I am not convinced even this is fully working,
if I try to add more options it seems to stop working all together.  I am looking in to this - see https://esp32.com/viewtopic.php?f=19&t=14376

Camera troubleshooting: https://randomnerdtutorials.com/esp32-cam-troubleshooting-guide/

The SD card now works with the flash as I am using "1 wire" to communicate with it, this is slower but does not require to use the pin the LED uses.

When motion detecting it runs at around 4 frames per second.

GPIO:
    I have configured gpio16 as input and if the status of this pin changes it is reported in the log.  The idea being
    that this can be used to external sensors although it is not implemented to do anything other than report the
    change at the moment.
    GPIO15 and GPIO16 are also free to be used for other purposes (as the sd card is in "1 bit" mode).
