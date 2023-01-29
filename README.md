# V.E.L.M.A.
Virtual Energy Loss Management Assistant - an energy savings intelligence system notifying users when and how to save energy based on current weather, device readings, thermodynamics calculations, etc.  Written for the ESP-32.  

This is an older version of the software using some bits of open source code from RandomNerdTutorials.  Newer versions are quite a bit different and are available upon request.


## Functions/Features:


This early version is somewhat limited, but can still save around $100/yr for casual users.

- Texts and Emails user in order to provide energy saving/carbon emission reducing tips via smtp.
- Notifies user to open blinds/curtains to harness solar gain to save energy and lower carbon emissions during sunny days in cold months.
- Notifies user to close blinds/curtains to reduce losses and save energy and lower carbon emissions during nights in cold months.
- Notifies user to close blinds/curtains to prevent solar gain to save energy and lower carbon emissions during sunny days in warm months.
- Notifies user to open blinds/curtains to radiate heat to save energy and lower carbon emissions during nights in warm months.
- Notifies user when to open or close windows to heat/cool their house based on heat or cool mode.
- Reminds user to take certain energy saving actions, including setting back thermostat at night.
- Provides tips for energy savings more broadly.



## Dependencies


Please see the includes in the code: VELMA_early.ino



## How to Use:


1. Obtain ESP32 microcontroller or adapt code for another chip.
2. Create a new email that can send you alerts.  Using your own email is not recommended.
3. Fill in preferences, API keys (you may need to sign up for open weather -- its free), and SSID/WiFi info. (Search for "//change for each user")
4. Ensure configureNewUser() function has 0 in line: EEPROM.write(NEW_USER, 0);
5. Save, compile, and upload code to your ESP-32
6. (optional) 3D print a case for the chip.
7. IMPORTANT!!!!!!!!! -- depending on your choice of case or no case, you will need to calibrate the temperature reading against your thermostat! See constant "temp_offset"



### More on Preferences:


- heatTemp - the setting on your thermostat when you would like to begin heating your house
- coolTemp - the setting on your thermostat when you would like to begin cooling your house
- xxx_pref - the monthly setting for your thermostat (heat mode, cool mode, or "ask" mode for shoulder months.) Ex: jan_pref = "heat"
- earliest and latestMsg - mutes notifications for the night.
- awayStart and awayEnd - hours during the day that you are not home/at work/etc. *regularly.*



### More on Calibration:


- You may need to add a function to text you the measured temperature each hour and, after a few hours of warm up, compare VELMA's reading with your thermostat.  Change the offset value as needed.  Keep VELMA out of direct sunlight, away from hot air sources, etc. 
- You may need to ignore the first couple temperature readings while calibrating. 
- Offset may be different based on the particular chip you use. 
- Offset may be different if ESP32 delays or sleeps during idle time.  Sleeping is the default as it is more energy efficient.
