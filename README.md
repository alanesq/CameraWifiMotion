<p align="center"><img src="/Images/CameraWifiMotion.jpg" width="90%"/></p>
           
This is a Arduino IDE sketch to use one of the cheap (7ukp from eBay) ESP32 camera modules as a motion detecting security 
camera.
It captures an image from the camera around 4 times a second, each time comparing this with the previous image looking for 
any changes.  If significant change is detected it captures a higher res image and stores it in internal memory.  
It also has the options to email or FTP the captured images or store them to sd card.

NOTE: I have now updated it to work with the latest ESP32 board manager (v3), it may no longer work with older versions?
You need to set the partition table to CUSTOM and if you want to include image attachments the ESP_Mail.FS.h file in the 
library needs to be modified:
              delete block of lines around 73-92 and replace with<br>
                  #if defined(ESP32)<br>
                  #include <SPIFFS.h><br>
                  #endif<br>
                  #define ESP_MAIL_DEFAULT_FLASH_FS SPIFFS      <br> 
              see: https://github.com/mobizt/ESP-Mail-Client/blob/master/src/ESP_Mail_FS.h <br><br>

<table><tr>
  <td><img src="/Images/screen1.png" /></td>
  <td><img src="/Images/screen2.png" /></td>
</tr></table>   

NOTE: It is important that if using Arduino IDE you select board "ESP32 Dev Module" with PSRAM enabled, Partition scheme "CUSTOM".<br>

There is now a very cheap motherboard available for the esp32cam which make it as easy to use as any other esp development board. 
Search eBay for "esp32cam mb" - see http://www.hpcba.com/en/latest/source/DevelopmentBoard/HK-ESP32-CAM-MB.html 
It looks like the esp32cam suplied with them are not standard and have one of the GND pins modified to act as a reset pin?
So on esp32cam modules without this feature you have to plug the USB in whilst holding the program button to upload a sketch 
also I find I have to use the lowest upload speed or it fails to upload.  The wifi is very poor whilst in the motherboard (I find this happens if you have something near the antenna on the esp32cam modules) but if I rest my thumb above the antenna I find the signal works great).  
So they are far from perfect but still for the price I think well worth having.

Tips / Mods:
These cheap cameras are surprisingly good apart from very poor performance in low light conditions, I have done all I can
in software to improve this but if you want to use the cameras in anything other than full daylight you need to fit a
better lens.        see: https://www.youtube.com/watch?v=T0P37aEneto

Removing the I.R. filter and fitting a suitable lens completely transforms these cameras in low light conditions, going 
from completely useless to ok.  You need a lens with a large iris, I am still searching for the best option (I think 'M12 F1.4' is the type of thing we need)...
You can remove the casing from the supplied camera leaving just the ccd (see pic) and then glue a lens case over the top of it. If you have a 3d printer you can print an adaptor from https://www.thingiverse.com/thing:4663521, mine has a 20mm spacing on the holes though so I have created a modified version here: https://github.com/alanesq/CameraWifiMotion/blob/master/misc/esp32cam-lensAdaptor-20mm.step
<br>Latest news (Nov23): You can now buy ov5640 cameras with this type lense fitted on Aliexpress so no need to do the above
<br><table><tr>
  <td><img src="/Images/replacement-lens.jpg" width="200px" /</td>
  <td><img src="/Images/replacement-lens-2.jpg" width="200px" /></td>
</tr></table> 
I find that as soon as you try to install the camera in any kind of case the wifi signal becomes very weak.  This can be
rectified by installing an external wifi antenna (note: you have to move the jumper resistor on the board to enable the
external antenna socket).  Search eBay for "2.4G Antenna IPX13".
You really need a good power source for these cameras otherwise you they can be very unstable (wifi dropping, reboots, 
strange error messages etc.).  It needs to be capable of providing a minimum of 500ma and really needs a good smoothing 
capacitor fitting as both the LED and wifi can cause a lot of spikes/voltage drop otherwise.

The sketch can use OTA (Over the air updates) to update the software, this can be enabled/disabled in the main settings of 
the sketch.
If you use OTA do not select the "ESP32-cam" board in the Arduino IDE, use "ESP32 Dev Module" and make sure PSRAM is 
enabled and partition table "custom" selected, otherwise OTA will not work.
In an attempt to give some form of security to OTA I have set up the sketch so that when OTA is enabled you can not access 
it until you have first entered a password, accessed via "http://x.x.x.x/ota"
You can change this password in the main settings (OTAPassword).

-----------------

If you wish to use the email facility you need to enter your email details in gmail_esp.h and note the security settings 
may need changing on the gmail account for it to accept them  see: https://myaccount.google.com/lesssecureapps
I have found gmail's security to mean it proves unreliable and find gmx.com to be more relaibe / easy to set up (you just need to enable POP access).

There is a zip file containing the libraries used.  The main ones you will need to install are:
  ESP32_mail_client, ESP_wifimanager and Time.
To add ESP32 suport to the Arduino IDE see: https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/

The last 8 images captured are stored in the onboard Spiffs memory and these can be viewed on the web page this device 
generates. If you install a sd card it will also store all captured images on it.  It has the ability to capture images at 
a higher resolution but will not be able to store 8 images if you icrease it (also in my experience it may become unstable 
as it seems to struggle with the bandwidth?). 

It uses the WifiManager library so first time the ESP starts it will create an access point "ESPCamera" which you need to connect to in
order to enter your wifi details.  
             default password = "12345678"   (note-it may not work if anything other than 8 characters long?)
             see: https://randomnerdtutorials.com/wifimanager-with-esp8266-autoconnect-custom-parameter-and-manage-your-ssid-and-password
             Once you have entered your wifi password it will restart and you can then connect to it in your browser
             at the address:     http://ESPcam1.local     (if your browser / config supports it)
             otherwise you need to check the serial output or your router to discover what IP address it has been assigned.

The motion detection is based on some code posted by eloquentarduino, this has since been updated but info on the subject can be seen [HERE](https://eloquentarduino.com/posts/esp32-cam-motion-detection)<br>
It works by repeatedly capturing a grayscale image (320x240 pixels).  This image is split up in to 20x20 
pixel blocks (i.e. 16 x 12 blocks for the complete image).  All the pixel values in each block are averaged to give a single 
number for each block between 0 and 255 (i.e. average of the block).
These resulting 16x12 blocks are then compared to the previously captured one and if the value of any block has varied by
more 
then the "block" setting then this block is declared changed.
If enough blocks have changed (percentage of active image detection area, the "image" setting) then motion is detected.
So the settings you can vary equate to:
        Block = how much brighness variation in a block is required to flag it as changed
        Image = how much of the image area needs to change to count as movement detected
When motion detecting is enabled it will show the current average image brightness along with what motion it is currently
detecting
in the format "Readings: brightness:113, 0 changed blocks out of 64".  You can use this to fine tune your detection 
settings.

There are three settings for the motion detection, the first is how much change in average brightness in a block will 
count as that block has changed, the second two numbers refer to the number of blocks in the image which have changed since
the last image was captured and it will trigger as motion detected if this is between these two values.
So the easiest way to set it up is to set the two trigger levels to max and the trigger level to around 15, then in the 
"raw data" page with motion detection disabled refresh a couple of times until you are seeing very low values in the top
window (difference).
now if you move someone/something in to the view of the camera and refresh the page you will be able to see what differences
the software has detected in the image (fine tune the detection threshold for the best results).
Now if you go back to the main page and enable motion detection you can watch the levels being detected on the second line
on the page (i.e. "Readings: brightness:105, 1 changed blocks out of 64").

There is a grid of tick boxes on the right of the main screen, this is a mask to set which parts of the image are used
when detecting motion (i.e. only the ticked areas are used).  This 4 x 3 grid results in mask sections of 16 blocks (4x4) 

NOTE: As the gain increases the motion detection sensativity automatically decreases to compensate for the added noise in
the camera image.  This is to prevent false triggers in low light conditions.

It also has the following URLs you can use:

        http://<esp ip address> /ping - responds with either 'enabled' or 'disabled', just so you know it is still working
                                /log - log page of the device activities
                                /test - used for testing bits of code etc.
                                /reboot - restarts the esp32 camera module
                                /default - sets all settings back to defaults
                                /live - capture and display a live image from the camera
                                /images - display the 10 images stored in Spiffs (Image width in percent can be specified in URL with http://x.x.x.x/images?width=90)
                                /bootlog - log of times the device has been switched on / rebooted (handy for checking it is stable)
                                /data - this is the updating text on the main page but handy for a quick check of status
                                /imagedata - show raw block data
                                /ota - update firmware (requires password entered first)
                                /img - just display a plain jpg 
                                       defaults to the live grayscale image or stored images selected with /img?pic=x
                                       for smaller pre capture images add 100 to x.
                                /stream - Shows a live video stream 
                                        Thanks to Uwe Gerlach for sending me the code showing how to do this 
                                /jpg - display the grayscale image the motion detecting is using

-----------------

Adjusting the settings:

if you check the "difference" section on the  "raw data" page, if all is well you should just be seeing 0s and 1s , meaning
each image it captures is pretty much identical to the last one (this will be much higher at night as the camera gain is
increased which also increases interference).  The two sections below being the current and last image captured which it
compares to detect movement.
The "Detection threshold" setting on the main page sets at what value it will declare one of these blocks has changed.

On the main page watch where it says "0 changed blocks out of 128"
ideally it should never change from 0 unless something is moving but tends to pick up the odd one as light levels change
etc.  

The trigger between settings are reffering to the number displayed in "Current detection level".  If you watch this as
something is moving in the image you will get an idea of what sort of value to be looking out for.


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

GPIO:
io pins available for use are 13 and 12 (12 must not be high at boot)
pin 16 is used for psram but you may get away with using it as input for a button etc.
You could also use 1 and 3 if you disable the use of Serial or 14,2&15 if not using SD
More info: https://randomnerdtutorials.com/esp32-cam-ai-thinker-pinout/
Another option (which I have not actually tried yet) would be to attach a MCP23017 Bidirectional 16-Bit I/O Expander (possibly need to use the serial tx and rx pins) which would then give you 16 gpio pins to play with :-)
      
I have heard reports of these modules getting very warm when in use although I have not experienced this myself, I suspect it may be when streaming video for long periods?  May be worth bearing in mind.

If you have several cameras there is a HTML page here which can be used to give a menu of all your available cameras
https://github.com/alanesq/CameraWifiMotion/blob/master/misc/menu-of-projects.htm

Camera troubleshooting: https://randomnerdtutorials.com/esp32-cam-troubleshooting-guide/

A more basic sketch which may be of interest/use if you wish to create your own custom project using one of these Esp32Cam modules
  https://github.com/alanesq/esp32cam-demo
                                                                                       https://github.com/alanesq/CameraWifiMotion
