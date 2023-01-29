//---------------------------------------------------------+
// Developed by: Austin Hinkel                             |
//---------------------------------------------------------+
// Created:     31 - Dec - 2020                            |
// Last Update: 31 - Oct - 2021                            |
//---------------------------------------------------------+
// V irtual                                                |
// E nergy                                                 |
// L oss                                                   |
// M anagement                                             |
// A ssistant                                              |
//---------------------------------------------------------+
//                     VERSION 0.3.3                       |
//---------------------------------------------------------+
// This program reads temperatures and makes energy saving |
// recommendations via txt msg and email to the user.      |
// Intended for use with: ESP32.                           |
//---------------------------------------------------------+


// REQUIRES the following Arduino libraries:
// - DHT Sensor Library: https://github.com/adafruit/DHT-sensor-library
// - Adafruit Unified Sensor Lib: https://github.com/adafruit/Adafruit_Sensor
#include "DHT.h"
#include "WiFi.h"
#include "time.h"
#include "ESP32_MailClient.h"  // 
#include <ArduinoJson.h>       //https://arduinojson.org/?utm_source=meta&utm_medium=library.properties
#include <EEPROM.h>

//EEPROM
#define EEPROM_SIZE            7
//STATS:
#define WEEKLY_ACTIONS_ADDRESS 0
#define SCORE_DIGIT_1          1 //1-255 1's digit in base 256
#define SCORE_DIGIT_2          2 //1-255 256's digit in base 256
#define SCORE_DIGIT_3          3 //1-255 (256*256)'s digit in base 256
//STATES:
#define NEW_USER               4 //holds bool: true(1) if new, false(0) if returning
#define WINDOW_STATE           5 //is window open (1) or closed (0)
#define BLINDS_STATE           6 //are curtains/blinds open (1) or closed (0)
//NOTE: got creative with the different addresses-even tho each one can only hold up to 255, 
//have 3 of them and then it's: 255*256^2 + 255*256 + 255 = 16,777,215 as a max score. 
//careful, this is higher than unsigned long int... fix later?
const int base255_digit1 = 1;
const int base255_digit2 = 256;
const long base255_digit3 = 256*256;

const int coolBaseline = 73; //deg F (for scorekeeping, thermostat setting nudge) NOT PREFERENCE DO NOT CHANGE
const int heatBaseline = 70; //deg F (for scorekeeping, thermostat setting nudge) NOT PREFERENCE DO NOT CHANGE


//DHT Pin:
#define DHTPIN 4        // Digital pin connected to DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

// Initialize DHT sensor.
DHT dht(DHTPIN, DHTTYPE);

//DHT22 pin tutorial from randomnerdtutorials:
// Connect pin 1 (on the left) of the sensor to +3.3V (note 5V causes issues when powered through laptop connection... pops says 5V lasts longer, product development wise?)
// Connect pin 2 of the sensor to whatever DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor
// See: https://randomnerdtutorials.com/esp32-dht11-dht22-temperature-humidity-sensor-arduino-ide/


//Constants for Functionality:
const int           numReadings   = 10;                          //number of readings each loop (num of readings to average over.)
const int           MAX_MSGs      = 10;                          //max number of messages that can be sent during one loop.
const int           numTips       = 16;                          //max number of tips in the random tip bank.
const unsigned long sleepDuration = (((59 * 60) + 9) * 1000000); //how long in microseconds the microcontroller sleeps for. (saves energy) //59min, 8s
const float         CONV          = 3.141592/180.0;              //degrees to radians conversion
const float         temp_offset   = 1.18;                        //23.92 if not deep sleep, 1.18 if deep sleep? // in degrees F. offset HIGH due to sensor being in the housing and reading its own heated environment and not the house's actual temp. Assumes thermal equilibrium. subtract this!!!!
//***NOTE***: temp_offset will change depending on if a housing is created for the ESP32 or not.  You will likely have to callibrate this on your own.




//Time Keeping:
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -25200;                //change for each user
const int   daylightOffset_sec = 3600;
const int   utc_offset_hours = -7;                 //change for each user

//Location Data:
const String city = "Colorado Springs";                   //change for each user
const String state = "CO";                                //change for each user                
const String country = "US";                              //change for each user
const String latitude = "38.8339";                        //change for each user
const String longitude = "-104.8214";                     //change for each user
//const String solcast_site_id = "cb5c-067b-aa8d-b140";



//API KEYs:
const String openweathermap_API_KEY = "redacted";      //https://create.arduino.cc/projecthub/officine/getting-weather-data-655720
//future features?
//const String solcast_API_KEY = "redacted";     //https://docs.solcast.com.au/?&_ga=2.164385471.1612248269.1609862073-129385193.1608927738#introduction
//const String nrel_API_KEY = "redacted";        //https://developer.nrel.gov/docs/solar/nsrdb/guide/
//const String nasa_API_KEY = "redacted";        //https://power.larc.nasa.gov/docs/tutorials/api-getting-started/
//const String noaa_API_KEY = "redacted";        //

//Servers:
char openweathermap_server[] = "api.openweathermap.org";      
//char solcast_server[] = "https://api.solcast.com.au";    
//char nrel_server[] = ""; 



        
//WiFi:
const char* ssid       = "redacted";                    //change for each user
const char* password   = "redacted";                    //change for each user
int status = WL_IDLE_STATUS; 
WiFiClient client;



//Email:
SMTPData smtpData;
#define emailSenderAccount    "redacted@gmail.com"    //change for each user: must create an email address to send you alerts.
#define emailSenderPassword   "redacted"              //change for each user: must create an email address to send you alerts.
#define emailRecipient        "redacted2@gmail.com" //change for each user
#define textRecipient         "redactedCell@carrierwebsite"    //change for each user
#define smtpServer            "smtp.gmail.com"
#define smtpServerPort        465
#define emailSubject          "VELMA Recommendation"





/***************************************************/
/********** PREFERENCE RELATED CONSTANTS ***********/
/*change below for each user: **********************/
/***************************************************/
const bool hasSolar = false;                        //
const bool hasBattery = false;                      //
const bool hasEV = false;                           //
const bool virtualPowerPlantOptIn = false;          // //feature not developed
const bool awayMode_Mon = true;                    // 
const bool awayMode_Tue = true;                    //
const bool awayMode_Wed = true;                    //
const bool awayMode_Thu = true;                    //
const bool awayMode_Fri = true;                    //
const bool awayMode_Sat = false;                    //
const bool awayMode_Sun = false;                    //
const int awayStart = 8;                           //
const int awayEnd   = 17;                          //
const int earliestMsg = 7;                         // //EARLIEST MESSAGE MUST BE BEFORE AWAY BEGIN
const int latestMsg = 23;                          // //LATEST MESSAGE MUST BE AFTER AWAY END -- i need to fix this later
const float heatTemp = 66;                         //
const float coolTemp = 76;                         //
const String jan_pref = "heat";                    //
const String feb_pref = "heat";                    //
const String mar_pref = "heat";                    //
const String apr_pref = "heat";                    //
const String may_pref = "ask";                     //
const String jun_pref = "cool";                    //
const String jul_pref = "cool";                    //
const String aug_pref = "cool";                    //
const String sep_pref = "cool";                    //
const String oct_pref = "ask";                     //
const String nov_pref = "heat";                    //
const String dec_pref = "heat";                    //
/***************************************************/
/***************************************************/



/*
 * Energy saving advice for thermostats: https://www.energy.gov/energysaver/thermostats
 * Other energy saving tips: https://www.energy.gov/sites/prod/files/energy_savers.pdf
 * Heating and Cooling tips: https://www.washingtonpost.com/news/wonk/wp/2016/07/14/how-to-cool-your-house-like-a-wonk/
 */



/*
 * Note for cell carrier texting features:
 * @vzwpix.com - Verizon
 * @mms.att.net - AT&T
 */




/************************************ UNDERLYING FUNCTIONALITY ************************************/
/************************************ UNDERLYING FUNCTIONALITY ************************************/
/************************************ UNDERLYING FUNCTIONALITY ************************************/

//----------------------------------------------------------------------------
// convert Celsius to Farenheit
//----------------------------------------------------------------------------
float celsiusToFarenheit(float degC){
  float degF = (1.8 * degC) + 32.0;
  return degF;
}


//----------------------------------------------------------------------------
// convert Farenheit to Kelvin
//----------------------------------------------------------------------------
float farenheitToKelvin(float degF){
  float tempK = (5.0/9.0) * (degF - 32.0) + 273.15;
  return tempK;
}


//----------------------------------------------------------------------------
// A function to implement bubble sort on array passed to it as arr
//----------------------------------------------------------------------------
void bubbleSort(float arr[], int n){  
    float temp;
    for (int i = 0; i < n-1; i++) {    
      // Last i elements are already in place  
      for (int j = 0; j < n-i-1; j++)  {
        if (arr[j] > arr[j+1]) { 
            temp = arr[j+1];  
            arr[j+1] = arr[j];  
            arr[j] = temp;  
        }
      }
    }

    return;
}


//----------------------------------------------------------------------------
// determine HVAC mode
//----------------------------------------------------------------------------
String heatOrCool(){
    String heatOrCool = "";
    //get month:
    int iCurrentMonth = getCurrentMonth();
    //decision tree based on month:
    if(iCurrentMonth == 1){
      heatOrCool = jan_pref;
    } else if(iCurrentMonth == 2) {
      heatOrCool = feb_pref;
    } else if(iCurrentMonth == 3) {
      heatOrCool = mar_pref;
    } else if(iCurrentMonth == 4) {
      heatOrCool = apr_pref;
    } else if(iCurrentMonth == 5) {
      heatOrCool = may_pref;
    } else if(iCurrentMonth == 6) {
      heatOrCool = jun_pref;
    } else if(iCurrentMonth == 7) {
      heatOrCool = jul_pref;
    } else if(iCurrentMonth == 8) {
      heatOrCool = aug_pref;
    } else if(iCurrentMonth == 9) {
      heatOrCool = sep_pref;
    } else if(iCurrentMonth == 10) {
      heatOrCool = oct_pref;
    } else if(iCurrentMonth == 11) {
      heatOrCool = nov_pref;
    } else {
      heatOrCool = dec_pref;
    } 

    //some months may not be so cut and dry.
    //if user has a preference of ask, try to guess based on the forecast.
    if(heatOrCool == "ask"){
        //forecast:
        float todayTemps[4]; //4 entries, 3 hours apart.
        int todayTimes[4];   //4 entries to list times of above forecast temps
        getDailyForecast(todayTemps, todayTimes);
        float avgTemp = 0;
        float highTemp = -999;
        float lowTemp = 999;
        for(int i=0; i<sizeof(todayTemps); i++){
            avgTemp += todayTemps[i]/sizeof(todayTemps);
            if(todayTemps[i] > highTemp){
              highTemp = todayTemps[i];
            }
            if(todayTemps[i] < lowTemp){
              lowTemp = todayTemps[i];
            }
        }//end for loop
        
        //use avg, low, and high to guess mode based on heat and cool -- can make this smarter later.
        if(highTemp > coolTemp){
          heatOrCool = "cool";
        } else if(lowTemp < heatTemp) {
          heatOrCool = "heat";
        } else if(avgTemp > (heatTemp + coolTemp)/2.0) {
          heatOrCool = "cool";
        } else {
          heatOrCool = "heat";
        }
    }
    
    return heatOrCool;
}


//----------------------------------------------------------------------------
// compute net power flow (for blinds logic) in W/m^2
//----------------------------------------------------------------------------
float computeNetPowerFlow(float insideT, float outsideT, float solarIrrad){
  float stefan_boltzmann_constant = 0.0000000567;  //units: Watts per sq meter per Kelvin^4
  
  //convert Temp from F to K:
  float insideKelvin = farenheitToKelvin(insideT);
  float outsideKelvin = farenheitToKelvin(outsideT);
  
  //Three different flows to consider:
  //(assumes perfect, blackbody radiation)
  //(ignores insulation factor of windows, etc.)
  //(ignores lowered solar gain from windows blocking high energy photons)
  //(ignores convection/conduction through window) Typical R value for double paned window is about 2 in imperial units.
  //STILL NEED TO UPDATE THIS FOR CONDUCTIVE LOSSES??
  
  //solar radiation heats the house.
  float solarGain = solarIrrad;
  //house radiates energy based on temperature.
  float radiativeLoss = stefan_boltzmann_constant * pow(insideKelvin, 4);        //temp must be in Kelvin
  //house receives radiated energy from environment based on temperature.   
  float radiativeGain = stefan_boltzmann_constant * pow(outsideKelvin, 4);       //temp must be in Kelvin
  
  //the sum of these is the net power flow.
  float netPower = solarGain + radiativeGain - radiativeLoss;  //Watts per sq. meter
  return netPower;
}


//----------------------------------------------------------------------------
// turn board LED on or off.
//----------------------------------------------------------------------------
void toggleOnBoardLED(int iOnOff) {

   // this function toggles tine ESP32's onboard LED (blue to right of red power LED), tied to GPIO2.
   unsigned int ESP32OnBoardLED = 2;
   pinMode(ESP32OnBoardLED, OUTPUT);
   if (iOnOff == 1) {
      digitalWrite(ESP32OnBoardLED, HIGH);   // turn the LED on (HIGH is the voltage level)
   } else {
      digitalWrite(ESP32OnBoardLED, LOW);    // turn the LED off by making the voltage LOW
   }

   return;
}


//----------------------------------------------------------------------------
// Flash board LED many times to signal something.
//----------------------------------------------------------------------------
void flashOnBoardLED() {
  for(int n=0; n<5; n++){
    toggleOnBoardLED(1);
    delay(300);
    toggleOnBoardLED(0);
    delay(300);
  }
}


//----------------------------------------------------------------------------
// Connect to WiFi to send message. Must manually disconnect later in order
// to save energy.  No need to stay connected at all times.
//----------------------------------------------------------------------------
void connectWiFi() {
   delay(200);
   Serial.println();
   Serial.print(F("Connecting to "));
   Serial.println(ssid);

   WiFi.mode(WIFI_STA);
   WiFi.begin(ssid, password);

   toggleOnBoardLED(1);       // turn on onboard LED, if stuck in connect loop.
   int iTmp = 0;
   while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.print(F("."));
     if (iTmp == 100) break;
     iTmp++;
   }
   toggleOnBoardLED(0);
   Serial.println("");
   Serial.println(F("WiFi connected"));
   Serial.println(F("IP address: "));
   Serial.println(WiFi.localIP());
}


//----------------------------------------------------------------------------
// Gets the month as an int
//----------------------------------------------------------------------------
int getCurrentMonth() {
    char buff[100];
    time_t now = time(0);
    delay(200);
    struct tm now_t = *localtime(&now);
    //strftime (buff, 100, "%d-%m-%Y %H:%M:%S", &now_t);
    strftime (buff, 100, "%m", &now_t);
    int iCurrentMonth = atoi(buff);
    return iCurrentMonth;
}


//----------------------------------------------------------------------------
// Gets the time's hour.
//----------------------------------------------------------------------------
int getCurrentHour() {
    // Grab current hour from todays dateTime, in military time.
    char buff[100];
    time_t now = time(0);
    struct tm now_t = *localtime(&now);
    //Serial.println(now_t.tm_isdst);
    //strftime (buff, 100, "%d-%m-%Y %H:%M:%S", &now_t);
    strftime (buff, 100, "%H", &now_t);
    //strftime (buff, 100, "%H", &now_t);
    int iCurrentHour = atoi(buff);
    //Serial.print("current hour in INT=");
    //Serial.println(iCurrentHour);
    return iCurrentHour;
}


//----------------------------------------------------------------------------
// Given today's date, determine the number of the day 0 - 6, SUN = 0
//----------------------------------------------------------------------------
int getDayNumber(){
    time_t rawtime;
    tm * timeinfo;
    time(&rawtime);
    timeinfo=localtime(&rawtime);
    int iDayNumber=timeinfo->tm_wday;
    return iDayNumber;
}


//----------------------------------------------------------------------------
// Determine Away Mode
//----------------------------------------------------------------------------
bool isTheUserAway(){
  bool awayBool = false;
  int dayNum = getDayNumber();
  int currHour = getCurrentHour();
  if(currHour < awayStart or currHour >= awayEnd){
    return awayBool;
  } else {
    if(dayNum == 0){
      //It's Sunday
      awayBool = awayMode_Sun;
    } else if(dayNum == 1) {
      awayBool = awayMode_Mon;
    } else if(dayNum == 2) {
      awayBool = awayMode_Tue;
    } else if(dayNum == 3) {
      awayBool = awayMode_Wed;
    } else if(dayNum == 4) {
      awayBool = awayMode_Thu;
    } else if(dayNum == 5) {
      awayBool = awayMode_Fri;
    } else {
      awayBool = awayMode_Sat;
    } //end day of week check
    
    return awayBool;
  }//close else in hour condition
}






/************************************ VELMA COMMUNICATION CAPABILITIES ************************************/
/************************************ VELMA COMMUNICATION CAPABILITIES ************************************/
/************************************ VELMA COMMUNICATION CAPABILITIES ************************************/

//----------------------------------------------------------------------------
// Callback function to get the Email sending status
//----------------------------------------------------------------------------
void sendCallback(SendStatus msg) {
  // Print the current status
  Serial.println(msg.info());

  // Do something when complete
  if (msg.success()) {
    Serial.println("----------------");
  }
}


//----------------------------------------------------------------------------
// Send text message.
//----------------------------------------------------------------------------
void sendTxtMsg(String messageToSend) {
  smtpData.setLogin(smtpServer, smtpServerPort, emailSenderAccount, emailSenderPassword);
  smtpData.setSender("VELMA", emailSenderAccount);
  smtpData.setPriority("High");
  smtpData.setSubject(emailSubject);
  // email will be in text format (raw text):
  smtpData.setMessage(messageToSend, false);
  smtpData.addRecipient(textRecipient);
  smtpData.setSendCallback(sendCallback);

  if (!MailClient.sendMail(smtpData))
    Serial.println("Error sending Email, " + MailClient.smtpErrorReason());
    
  // Clear Email object (this frees up memory):
  smtpData.empty();
  return;
}


//----------------------------------------------------------------------------
// Send weekly email message.
//----------------------------------------------------------------------------
void sendEmail(String messageToSend, String subjectLine) {
  // Set the SMTP Server Email host, port, account and password
  smtpData.setLogin(smtpServer, smtpServerPort, emailSenderAccount, emailSenderPassword);
  // For library version 1.2.0 and later which STARTTLS protocol was supported,the STARTTLS will be 
  //enabled automatically when port 587 was used, or enable it manually using setSTARTTLS function.
  //smtpData.setSTARTTLS(true);
  // Set the sender name and Email:
  smtpData.setSender("VELMA", emailSenderAccount);
  // Set Email priority or importance; High, Normal, Low:
  smtpData.setPriority("High");
  // Set the subject of the email:
  smtpData.setSubject(subjectLine);
  // Set the email message in HTML format:
  smtpData.setMessage(messageToSend, true);
  // Add recipients, (you can add more than one recipient):
  smtpData.addRecipient(emailRecipient);
  smtpData.setSendCallback(sendCallback);
  //Start sending Email, can be set callback function to track the status:
  if (!MailClient.sendMail(smtpData))
    Serial.println("Error sending Email, " + MailClient.smtpErrorReason());
  //Clear all data from Email object to free memory:
  smtpData.empty();
  return;
}


//----------------------------------------------------------------------------
// Combine an unspecified number of messages into 1, separated by blank lines.
//----------------------------------------------------------------------------
String combineMessages(String manyStrings[], int numMessages) {
  String combinedString = "";
  for(int k=0; k < numMessages; k++) {
    combinedString += manyStrings[k];
    if(k != numMessages - 1){
      combinedString += "\n\n";
    }
  }
  return combinedString;
}


//----------------------------------------------------------------------------
// Add a message to the queue.
//----------------------------------------------------------------------------
void addMessageToQueue(String queue[], int &numMsgs, String msg2add){
  if(msg2add.length() != 0){
    queue[numMsgs] = msg2add;
    numMsgs += 1;
  }
  return;
}


//----------------------------------------------------------------------------
// Formats the weekly email message. //note to opensource users: this feature helps with calibration -- calibrate recent temp reading against your thermostat.
//----------------------------------------------------------------------------
String formatWeeklyEmailMessage(int &runningCountRecs, float currTemp){
  String emailMessage = "";
  
  String line1 = "<div style=\"color:#0047ab;\"><h1>VELMA Weekly Summary</h1></div>";
  String line2 = "<div style=\"color:#004d78;\"><p>- The most recent temperature reading was: " + String(currTemp) + " degrees F. </p></div>";
  String line3 = "<div style=\"color:#004d78;\"><p>- You received " + String(runningCountRecs) + " energy saving recommendation(s) this week.</p></div>";
  String line4 = "<div style=\"color:#004d78;\"><p>- Your total score is: " + String(getUserScore()) + ".</p></div>";
  String line5 = "<div style=\"color:#004d78;\"><p>- Here are a couple energy savings tips: </p></div>";
  String line6 = selectRandomTip();
  String line7 = selectRandomTip();
  emailMessage = line1 + line2 + line3 + line4 + line5 + line6 + line7;

  runningCountRecs = 0; //zero out for the next week.
  return emailMessage;
}


//----------------------------------------------------------------------------
// Formats the new user email message.
//----------------------------------------------------------------------------
String formatNewUserEmailMessage(){
  String line1 = "<div style=\"color:#0047ab;\"><h1>Hi! I'm VELMA, your Virtual Energy Loss Management Advisor! I'm all set up and ready to make recommendations for you.  In the meantime, here are 5 energy savings tips you can get started with right now! </h1></div>";
  String line2 = "<div style=\"color:#004d78;\"><p>- Water heaters are sometimes set to needlessly high temperatures.  You can safely set yours back to 120 degrees F to save energy. (Do not go below 120 degrees F, however, as this allows unwanted bacteria to grow.) </p></div>";
  String line3 = "<div style=\"color:#004d78;\"><p>- Modern day washing machines can clean clothes very effectively with cold water.  Unless you have super-oily stains, skip the hot water in your wash cycles. </p></div>";
  String line4 = "<div style=\"color:#004d78;\"><p>- If you have a dishwasher with a delay option, consider letting it run at night, allowing it to dry without the heat dry option.  Some regions might even have cheaper electricity rates at night. </p></div>";
  String line5 = "<div style=\"color:#004d78;\"><p>- If you own a laser printer, consider unplugging it when it is not in use.  Even when not actively printing, laser printers often use a huge amount of energy. </p></div>";
  String line6 = "<div style=\"color:#004d78;\"><p>- Sealing air leaks with a tube of caulk will normally cost about $5.00 -- but the money you stand to save on utility bills is much higher, often paying for itself in as little as a few months. </p></div>";
  String emailMessage = line1 + line2 + line3 + line4 + line5 + line6;
  return emailMessage;
}


//----------------------------------------------------------------------------
// Select random energy savings tip for weekly email message.  
//----------------------------------------------------------------------------
String selectRandomTip(){ //this isn't finished yet...
  String randomTipSelected = "";
  String listOfTips[numTips];
  listOfTips[0]  = "<div style=\"color:#004d78;\"><p>- Make sure to replace your HVAC filter as scheduled.  If it gets too dirty, it will make your system work harder than it has to, which will cost you more for electricity. </p></div>";
  listOfTips[1]  = "<div style=\"color:#004d78;\"><p>- When cooking or boiling water on a stovetop, cover the pot with a lid if possible to conserve energy. </p></div>";
  listOfTips[2]  = "<div style=\"color:#004d78;\"><p>- When you're finished cooking in the oven and you want to heat your house, you can crack the oven door to let the heat from the oven heat your house. </p></div>";
  listOfTips[3]  = "<div style=\"color:#004d78;\"><p>- Be sure to empty your drier's lint collector after every use, so that it doesn't have to work as hard to dry your clothes. </p></div>";
  listOfTips[4]  = "<div style=\"color:#004d78;\"><p>- Some water heaters have exposed hot water pipes, allowing some heat to escape.  By insulating these hot water pipes, you'll help your water heater work smarter -- not harder. </p></div>";
  listOfTips[5]  = "<div style=\"color:#004d78;\"><p>- After a winter snow, check your roof for areas with melted snow.  These areas give away heat leakage through your roof's insulation.  If you see melted patches of snow on your roof, try to add insulation to the affected areas. </p></div>";
  listOfTips[6]  = "<div style=\"color:#004d78;\"><p>- Switching to LED light bulbs can save you a lot of money compared to incandescent bulbs. </p></div>";
  listOfTips[7]  = "<div style=\"color:#004d78;\"><p>- If you have a grill, consider grilling outside in the summer to avoid having your oven's heat work against your air conditioner. </p></div>";
  listOfTips[8]  = "<div style=\"color:#004d78;\"><p>- Some utilities provide smart meters to their customers at no cost.  Smart meters are a great tool for learning more about your particular energy consumption patterns and help to highlight areas ripe for improvement. </p></div>";
  listOfTips[9]  = "<div style=\"color:#004d78;\"><p>- Interested in taking efficiency on the road?  Make sure your car tires are inflated properly as it helps improve gas mileage, saving you money. </p></div>";
  listOfTips[10] = "<div style=\"color:#004d78;\"><p>- You can put electronics like gaming consoles and a TV on a power strip, turning them all off when not needed.  Even when not in use, these electronics can use more energy than you might think! </p></div>";
  listOfTips[11] = "<div style=\"color:#004d78;\"><p>- Try shortening your shower by a couple minutes -- it'll save you on both water and electricity/gas bills. </p></div>";
  listOfTips[12] = "<div style=\"color:#004d78;\"><p>- When opening multiple windows to cool your house/apartment, try to make the air take a long path through your house/apartment. As the air flows in one window, air flows out the others, so forcing the air to take a longer path means more area of your house/apartment will feel cooler. </p></div>";
  listOfTips[13] = "<div style=\"color:#004d78;\"><p>- If you leave your house or apartment for an extended time (a vacation, for example), set the thermostat back several degrees.  There's no sense in paying to cool or heat a house/apartment you're not using. </p></div>";
  listOfTips[14] = "<div style=\"color:#004d78;\"><p>- Trees with leaves on the sun-facing side of your house (south side for Northern Hemisphere and vice versa) can help block sunlight from heating your house in the summer, while letting sunlight through in the winter to help heat your house after they have dropped their leaves. </p></div>";
  listOfTips[15] = "<div style=\"color:#004d78;\"><p>- During the summer, close any air vents in your basement if you don't frequent that space.  Rooms below ground are naturally cool due to the Earth's temperature, so there's little need to cool them. </p></div>";
  //Select a random tip from the above list:
  int randomNumber = random(0, numTips); //0 to numTips exclusive
  randomTipSelected = listOfTips[randomNumber];
  //return the random tip for use the weekly email.
  return randomTipSelected;
}







/************************************ STATS AND GAMIFICATION ************************************/
/************************************ STATS AND GAMIFICATION ************************************/
/************************************ STATS AND GAMIFICATION ************************************/

//----------------------------------------------------------------------------
// Configure new user
//----------------------------------------------------------------------------
void configureNewUser() {
    //Score:
    EEPROM.write(SCORE_DIGIT_1, 0);
    EEPROM.commit();
    EEPROM.write(SCORE_DIGIT_2, 0);
    EEPROM.commit();
    EEPROM.write(SCORE_DIGIT_3, 0);
    EEPROM.commit();
    EEPROM.write(WEEKLY_ACTIONS_ADDRESS, 0);
    EEPROM.commit(); 
    //New user config state change:    
    EEPROM.write(NEW_USER, 0);                 //CHANGE THIS 0 TO A 1 FOR TESTING, redo before distribution!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!~
    EEPROM.commit();   
    //House states:
    EEPROM.write(WINDOW_STATE, 0);
    EEPROM.commit();   
    EEPROM.write(BLINDS_STATE, 0);
    EEPROM.commit(); 

    sendEmail(formatNewUserEmailMessage(), "Welcome!");
    Serial.println(F("New user has been configured."));
    sendTxtMsg("Hi! I'm Velma, your personal energy advisor!  I can make recommendations throughout the day to help you save energy and lower your carbon footprint as well as your utility bill! ");
    flashOnBoardLED(); //flash onboard LED if new user config successful.
  return;
}


//----------------------------------------------------------------------------
// Determines cumulative score
//----------------------------------------------------------------------------
unsigned long getUserScore(){
  Serial.println(F("Getting user score..."));
  unsigned long lowDigit = EEPROM.read(SCORE_DIGIT_1)*base255_digit1;
  unsigned long medDigit = EEPROM.read(SCORE_DIGIT_2)*base255_digit2;
  unsigned long highDigit = EEPROM.read(SCORE_DIGIT_3)*base255_digit3;
  return (lowDigit + medDigit + highDigit);
}


//----------------------------------------------------------------------------
// Updates cumulative score
//----------------------------------------------------------------------------
void updateUserScore(int score2add){
  Serial.println(F("Updating user score..."));
  unsigned long startScore = getUserScore();
  unsigned long endScore = startScore + score2add;

  int digit3 = int(endScore / base255_digit3);                     //largest digit
  //Serial.println(endScore / base255_digit3);
  int digit2 = int((endScore % base255_digit3) / base255_digit2);  //medium digit
  //Serial.println(endScore % base255_digit3);
  int digit1 = int(int(endScore % base255_digit3) % base255_digit2);  //one's place

  /*
  Serial.print(F("Score is: ")); 
  Serial.println(endScore);
  Serial.println(F("Score in base 256: "));
  Serial.println(digit3);  
  Serial.println(digit2);  
  Serial.println(digit1);
  */
  
  EEPROM.write(SCORE_DIGIT_3, digit3);
  EEPROM.commit();

  EEPROM.write(SCORE_DIGIT_2, digit2);
  EEPROM.commit();

  EEPROM.write(SCORE_DIGIT_1, digit1);
  EEPROM.commit();
  return;
}


//----------------------------------------------------------------------------
// Calculate thermostat nudge score - user gets points for using reasonable
// amount of heating/cooling based on settings/preferences.
//----------------------------------------------------------------------------
int calcThermoNudgeScore(String HVACsetting){
  int nudgeScore = 0;
  
  if(HVACsetting == "heat" and heatTemp < heatBaseline){
    nudgeScore += int(max(abs(heatTemp - heatBaseline), (float) 5.0));
  } else if(HVACsetting == "cool" and coolTemp > coolBaseline) {
    nudgeScore += int(max(abs(coolTemp - coolBaseline), (float) 5.0));
  }

  return nudgeScore; 
}


//----------------------------------------------------------------------------
// Determines if it is a time of peak demand //feature not really used right now
//----------------------------------------------------------------------------
bool isPeak(){
  bool isItPeak = false;

  int dayNum = getDayNumber();      //SUN=0, MON=1, etc.
  int currHour = getCurrentHour();  //24h time
  int currMonth = getCurrentMonth();
  //peak is M-F only
  if(dayNum > 0 and dayNum < 6){
      //KU Winter peak time 7a-12p, Nov-Mar:
      if((currMonth>=11 or currMonth <=3) and (currHour >= 7 and currHour <= 11)){
          isItPeak = true;
      } else if((currMonth>=4 and currMonth <=10) and (currHour >= 14 and currHour <= 17)){
          //KU Summer Peak is 2p-6p, Apr-Oct
          isItPeak = true;
      } 
  }
  
  return isItPeak;
}



//ignore:
//----------------------------------------------------------------------------
// Update user badges -- store bool in EEPROM if badge is earned.
// Also, add html for weekly email displaying earned badges if bool is one?
//----------------------------------------------------------------------------
/*
void updateBadges(){ 
  return;
}
*/






/************************************ MEASUREMENT OF OBSERVABLES ************************************/
/************************************ MEASUREMENT OF OBSERVABLES ************************************/
/************************************ MEASUREMENT OF OBSERVABLES ************************************/

//----------------------------------------------------------------------------
// Determines the inside temperature via multiple readings from DHT22 sensor.
//----------------------------------------------------------------------------
void getInsideTemp(float &insideT){
  delay(10*1000); //burn some time to account for deep sleep delay?
  float delmeTemp = dht.readTemperature(true); // burn one read to clear old data.
  float tempLog[numReadings];
  delay(8*1000); //burn more time to allow deep sleep issue to go away.
  
  //initialize
  //float tempSum = 0;         //running sum of temps in F

  //take numReading measurements to average and get a good estimate of temp.
  for(int i=0; i < numReadings; i++){
    // Wait 2 seconds between measurements.
    delay(2500);
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    // Read temperature as Fahrenheit (isFahrenheit = true)
    float f = dht.readTemperature(true);
    
    // Check if any reads failed and exit early (to try again).
    if (isnan(f)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      Serial.println(F("Trying again..."));
      getInsideTemp(insideT);  //CAREFUL, IF FAILURE RATE IS HIGH THIS COULD RESULT IN INFINITE LOOP. failure rate is low, though, so I feel okay doing it this way for now.
      return;
    }
    
    //add to running sum.
    Serial.println(f);
    //tempSum += f;
    tempLog[i] = f;
  }

  float tempMedian;
  bubbleSort(tempLog, numReadings);
  if(numReadings % 2 == 1) {
    tempMedian = tempLog[numReadings/2 ];
  } else {
    tempMedian = 0.5 * (tempLog[numReadings/2 - 1] + tempLog[numReadings/2]);
  }
  
  //insideT = (tempSum / numReadings) - temp_offset;  //mean is dangerous, use median instead in v0.2
  insideT = tempMedian - temp_offset;                 //subtract offset from the housing collecting heat.

  return;
}


//----------------------------------------------------------------------------
// Determines the outside temperature from openweathermap.org/api
// url for checking API data, note redacted key you must fill in
// api.openweathermap.org/data/2.5/forecast?q=ALBANY,NY,US&appid=REDACTEDAPIKEY&cnt=1&units=metric
//----------------------------------------------------------------------------
void getOutsideTemp(float &outsideT){
  Serial.println(F("\nStarting connection to server...")); 
  // if you get a connection, report back via serial: 
  if (client.connect(openweathermap_server, 80)) { 
    Serial.println(F("connected to openweathermap server")); 
    String location = city + "," + state + "," + country;
    // Make a HTTP request: 
    client.print("GET /data/2.5/forecast?"); 
    client.print("q=" + location); 
    client.print("&appid=" + openweathermap_API_KEY); 
    client.print("&cnt=1"); 
    client.println("&units=metric"); 
    client.println("Host: api.openweathermap.org"); 
    client.println("Connection: close"); 
    client.println(); 
  } else { 
    Serial.println(F("unable to connect to weather API")); 
  } 
  delay(1000); 
  String line = ""; 
  while (client.connected()) { 
    line = client.readStringUntil('\n'); 
    //Serial.println(line);   
    Serial.println("Parsing Temperature Values..."); 
    //create a json buffer where to store the json data 
    StaticJsonBuffer<5000> jsonBuffer; 
    JsonObject& data = jsonBuffer.parseObject(line); 
    if (!data.success()) { 
      Serial.println(F("parseObject() failed")); 
      return; 
    }
    //get data from json:
    float currentOutsideTemp = data["list"][0]["main"]["temp"]; 

    outsideT = celsiusToFarenheit(currentOutsideTemp);
    client.stop();  //aph?
    //print data for debug:
    //Serial.println(outsideT); 
  }
  return;
}


//----------------------------------------------------------------------------
// Grabs an estimate for the cloud cover from open weather map API
//----------------------------------------------------------------------------
void getConditions(float &currentCond){
  Serial.println(F("\nStarting connection to server...")); 
  // if you get a connection, report back via serial: 
  if (client.connect(openweathermap_server, 80)) { 
    Serial.println(F("connected to openweathermap server")); 
    String location = city + "," + state + "," + country;
    // Make a HTTP request: 
    client.print("GET /data/2.5/forecast?"); //forecasts are every 3 hrs.
    client.print("q=" + location); 
    client.print("&appid=" + openweathermap_API_KEY); 
    client.print("&cnt=1"); 
    client.println("&units=metric"); 
    client.println("Host: api.openweathermap.org"); 
    client.println("Connection: close"); 
    client.println(); 
  } else { 
    Serial.println(F("unable to connect to weather API")); 
  } 
  delay(1000); 
  String line = ""; 
  while (client.connected()) { 
    line = client.readStringUntil('\n'); 
    //Serial.println(line);   
    Serial.println("Parsing Cloud Values..."); 
    //create a json buffer where to store the json data 
    StaticJsonBuffer<5000> jsonBuffer; 
    JsonObject& data = jsonBuffer.parseObject(line); 
    if (!data.success()) { 
      Serial.println(F("parseObject() failed")); 
      return; 
    }
    //get data from json:
    currentCond = data["list"][0]["clouds"]["all"]; //can use clouds as a proxy for solar irradiance. test this by comparison with solcast!!! but also need to account for time of day!!!!
    //Serial.println(currentCond);
    client.stop();  //aph?
  }
  return;
}


//----------------------------------------------------------------------------
// Estimates the solar irradiation via cloud cover and time of year
// old, ad hoc version, may need an update based on your location
//----------------------------------------------------------------------------
float estimateGHI(float cloudPercentage, int month, int hour){
  float estimatedGHI; //global horizontal irradiance
  
  float sunPercent = 100 - cloudPercentage;   // 0 - 100 technically some still scatters -- maybe alter this a bit.
  //of order 10% of irradiance is from scatter and not direct, 
  //presumably a small part of cloud scattered light still goes through, 
  //correct the sun percentage by 1.05-ish:
  float scatterCorrection = 1.04;
  sunPercent = scatterCorrection*sunPercent;

  // slight modification to modulate the location, higher in summer:
  float seasonalScaleFactor;               //could try to redo this with air mass 1/sin(z) for peak z each month
  if(month == 12 || month == 1){
    seasonalScaleFactor = 0.82;
  } else if(month == 2 || month == 11){
    seasonalScaleFactor = 0.87;
  } else if(month == 5 || month == 8){
    seasonalScaleFactor = 0.95;
  } else if(month == 6 || month == 7){
    seasonalScaleFactor = 0.98;
  } else {
    seasonalScaleFactor = 0.92;
  }

  float hourlyScaleFactor;                 // 0 - 1, zero when sun is not up, is different in dif seasons....
  if(hour >= 12 && hour <= 14){
    hourlyScaleFactor = 1.0;
  } else if((hour == 15) || (hour == 11 )){
    hourlyScaleFactor = 0.8;
  } else if((hour == 10) || (hour >= 16 && hour <= 17)){
    hourlyScaleFactor = 0.6;
    if(month >= 11 || month <= 2) {
      hourlyScaleFactor = 0.4;
    }
  } else if(hour == 18 || hour == 19){
    hourlyScaleFactor = 0.4;
    if(month >= 11 || month <= 2) {
      hourlyScaleFactor = 0.0;
    }
  } else {
    hourlyScaleFactor = 0;
  }
      
  //0 - 1, smallest at poles, highest at equator -- cosine(abs(latitude)) 
  //need to account for Earth's tilt + season, too
  float locationScaleFactor;               
  if(month >= 6 && month <= 8) {
    locationScaleFactor = cos(CONV * (latitude.toFloat() - 23.5));
  } else if(month == 12 || month <= 2) {
    locationScaleFactor = cos(CONV * (latitude.toFloat() + 23.5));
    if(locationScaleFactor < 0) {
      locationScaleFactor = 0;
    }
  } else {
    locationScaleFactor = cos( CONV * latitude.toFloat() );
  }
  
  float nominalYearlyMeanGHI = 1120;       // Watts/meter^2 includes direct and indirect summed
  
  estimatedGHI = seasonalScaleFactor * nominalYearlyMeanGHI * hourlyScaleFactor * (sunPercent/100.0) * locationScaleFactor;
  return estimatedGHI;
}


//----------------------------------------------------------------------------
// OLD/DEPRECATED Determines the outside temperature FORECAST from openweathermap.org/api
// run around 2 or 3 pm? try to see if next day is v. cold or hot? adjust mode as needed
// banks heat/cold
//----------------------------------------------------------------------------
void getDailyForecast(float forecastArray[], int forecastArrayTimes[]){
  Serial.println(F("\nStarting connection to server...")); 
  // if you get a connection, report back via serial: 
  if (client.connect(openweathermap_server, 80)) { 
    Serial.println(F("connected to openweathermap server")); 
    String location = city + "," + state + "," + country;
    // Make a HTTP request: 
    client.print("GET /data/2.5/forecast?");  //PLEASE NOTE: FORECAST DATA EVERY 3 HOURS
    client.print("q=" + location); 
    client.print("&appid=" + openweathermap_API_KEY); 
    client.print("&cnt=8"); //8 -> 8*3=24 hours
    client.println("&units=metric"); 
    client.println("Host: api.openweathermap.org"); 
    client.println("Connection: close"); 
    client.println(); 
  } else { 
    Serial.println(F("unable to connect to weather API")); 
  } 
  delay(1000); 
  String line = ""; 
  while (client.connected()) { 
    line = client.readStringUntil('\n'); 
    //Serial.println(line);   
    Serial.println("Parsing Forecast Data..."); 
    //create a json buffer where to store the json data 
    StaticJsonBuffer<5000> jsonBuffer; 
    JsonObject& data = jsonBuffer.parseObject(line); 
    if (!data.success()) { 
      Serial.println(F("parseObject() failed")); 
      return; 
    }
    forecastArray[0] = celsiusToFarenheit(data["list"][0]["main"]["temp"]);
    //Serial.println(forecastArray[0]); 
    forecastArray[1] = celsiusToFarenheit(data["list"][1]["main"]["temp"]);
    forecastArray[2] = celsiusToFarenheit(data["list"][2]["main"]["temp"]);
    forecastArray[3] = celsiusToFarenheit(data["list"][3]["main"]["temp"]);
    forecastArray[4] = celsiusToFarenheit(data["list"][4]["main"]["temp"]);
    forecastArray[5] = celsiusToFarenheit(data["list"][5]["main"]["temp"]);
    forecastArray[6] = celsiusToFarenheit(data["list"][6]["main"]["temp"]); 
    forecastArray[7] = celsiusToFarenheit(data["list"][7]["main"]["temp"]);

    String sAph0 = data["list"][0]["dt_txt"];
    forecastArrayTimes[0] = sAph0.substring(11,13).toInt(); //in UTC 
    String sAph1 = data["list"][1]["dt_txt"];
    forecastArrayTimes[1] = sAph1.substring(11,13).toInt(); //in UTC 
    String sAph2 = data["list"][2]["dt_txt"];
    forecastArrayTimes[2] = sAph2.substring(11,13).toInt(); //in UTC 
    String sAph3 = data["list"][3]["dt_txt"];
    forecastArrayTimes[3] = sAph3.substring(11,13).toInt(); //in UTC 
    String sAph4 = data["list"][4]["dt_txt"];
    forecastArrayTimes[4] = sAph4.substring(11,13).toInt(); //in UTC 
    String sAph5 = data["list"][5]["dt_txt"];
    forecastArrayTimes[5] = sAph5.substring(11,13).toInt(); //in UTC 
    String sAph6 = data["list"][6]["dt_txt"];
    forecastArrayTimes[6] = sAph6.substring(11,13).toInt(); //in UTC 
    String sAph7 = data["list"][7]["dt_txt"];
    forecastArrayTimes[7] = sAph7.substring(11,13).toInt(); //in UTC 
    //Serial.println(sAph.substring(11,13));

    //Check if DST or not
    time_t now = time(0);
    struct tm now_t = *localtime(&now);
    bool boolDST = now_t.tm_isdst;

    //Make times in home timezone:
    for (int g=0; g<8; g++){
      if(boolDST){
        forecastArrayTimes[g] += (1 + utc_offset_hours); //this can be negative! if it is, add 24.
        if(forecastArrayTimes[g] < 0){
          forecastArrayTimes[g] += 24;
        } else if(forecastArrayTimes[g] > 23) { //could also be larger than 23 -- if so subtract 24
          forecastArrayTimes[g] -= 24;
        }     
      } else {
        forecastArrayTimes[g] += utc_offset_hours;    //this can be negative! if it is, add 24.
        if(forecastArrayTimes[g] < 0){
          forecastArrayTimes[g] += 24;
        } else if(forecastArrayTimes[g] > 23) { //could also be larger than 23 -- if so subtract 24
          forecastArrayTimes[g] -= 24;
        }
      }
    }
    client.stop(); //aph?
  }
  return;
}


//----------------------------------------------------------------------------
// Determines the outside temperature FORECAST for away time from openweathermap.org/api
//----------------------------------------------------------------------------
void getAwayTempForecast(int awayBegin, int awayFinish, float forecastArray[], int forecastArrayTimes[], int &numPoints){
  Serial.println(F("\nStarting connection to server...")); 
  int intDuration = awayFinish - awayBegin;
  numPoints = int(intDuration/3); //pass by reference?
  // if you get a connection, report back via serial: 
  if (client.connect(openweathermap_server, 80)) { 
    Serial.println(F("connected to openweathermap server")); 
    String location = city + "," + state + "," + country;
    String awayDuration = String(numPoints);  // integer division -- this is the number of three hour periods, e.g. 13 hours would have 4 complete 3hr periods.
    // Make a HTTP request: 
    client.print("GET /data/2.5/forecast?");  //PLEASE NOTE: FORECAST DATA EVERY 3 HOURS
    client.print("q=" + location); 
    client.print("&appid=" + openweathermap_API_KEY); 
    client.print("&cnt=" + awayDuration); 
    client.println("&units=metric"); 
    client.println("Host: api.openweathermap.org"); 
    client.println("Connection: close"); 
    client.println(); 
  } else { 
    Serial.println(F("unable to connect to weather API")); 
  } 
  delay(1000); 
  String line = ""; 
  while (client.connected()) { 
    line = client.readStringUntil('\n'); 
    //Serial.println(line);   
    Serial.println("Parsing Forecast Data..."); 
    //create a json buffer where to store the json data 
    StaticJsonBuffer<5000> jsonBuffer; 
    JsonObject& data = jsonBuffer.parseObject(line); 
    if (!data.success()) { 
      Serial.println(F("parseObject() failed")); 
      return; 
    }
    forecastArray[0] = celsiusToFarenheit(data["list"][0]["main"]["temp"]);
    //Serial.println(forecastArray[0]); 
    forecastArray[1] = celsiusToFarenheit(data["list"][1]["main"]["temp"]);
    //Serial.println(forecastArray[1]);
    forecastArray[2] = celsiusToFarenheit(data["list"][2]["main"]["temp"]);
    //Serial.println(forecastArray[2]);    
    forecastArray[3] = celsiusToFarenheit(data["list"][3]["main"]["temp"]);
    
    String sAph0 = data["list"][0]["dt_txt"];
    forecastArrayTimes[0] = sAph0.substring(11,13).toInt(); //in UTC 
    String sAph1 = data["list"][1]["dt_txt"];
    forecastArrayTimes[1] = sAph1.substring(11,13).toInt(); //in UTC 
    String sAph2 = data["list"][2]["dt_txt"];
    forecastArrayTimes[2] = sAph2.substring(11,13).toInt(); //in UTC 
    String sAph3 = data["list"][3]["dt_txt"];
    forecastArrayTimes[3] = sAph2.substring(11,13).toInt(); //in UTC 
    //Serial.println(sAph.substring(11,13));

    //Check if DST or not
    time_t now = time(0);
    struct tm now_t = *localtime(&now);
    bool boolDST = now_t.tm_isdst;

    //Make times in home timezone:
    for (int g=0; g<4; g++){
      if(boolDST){
        forecastArrayTimes[g] += (1 + utc_offset_hours); //this can be negative! if it is, add 24.
        if(forecastArrayTimes[g] < 0){
          forecastArrayTimes[g] += 24;
        } else if(forecastArrayTimes[g] > 23) { //could also be larger than 23 -- if so subtract 24
          forecastArrayTimes[g] -= 24;
        }     
      } else {
        forecastArrayTimes[g] += utc_offset_hours;    //this can be negative! if it is, add 24.
        if(forecastArrayTimes[g] < 0){
          forecastArrayTimes[g] += 24;
        } else if(forecastArrayTimes[g] > 23) { //could also be larger than 23 -- if so subtract 24
          forecastArrayTimes[g] -= 24;
        }
      }
    }
    client.stop(); //aph?
  }
  return;
}


//----------------------------------------------------------------------------
// Determines the cloud FORECAST from openweathermap.org/api in 3hr intervals
// returns an array of GHI values.
//----------------------------------------------------------------------------
void getSolarForecast(int awayBegin, int awayFinish, float forecastArray[], int forecastArrayTimes[], int &numPoints){
  Serial.println(F("\nStarting connection to server...")); 
  int intDuration = awayFinish - awayBegin;
  numPoints = intDuration/3; //pass by reference?
  // if get a connection, report back via serial: 
  if (client.connect(openweathermap_server, 80)) { 
    Serial.println(F("connected to openweathermap server")); 
    String location = city + "," + state + "," + country;
    String awayDuration = String(numPoints);  // integer division -- this is the number of three hour periods, e.g. 13 hours would have 4 complete 3hr periods.
    // Make a HTTP request: 
    client.print("GET /data/2.5/forecast?");  //PLEASE NOTE: FORECAST DATA EVERY 3 HOURS
    client.print("q=" + location); 
    client.print("&appid=" + openweathermap_API_KEY);
    client.print("&cnt=" + awayDuration); 
    client.println("&units=metric"); 
    client.println("Host: api.openweathermap.org"); 
    client.println("Connection: close"); 
    client.println(); 
  } else { 
    Serial.println(F("unable to connect to weather API")); 
  } 
  delay(1000); 
  String line = ""; 
  while (client.connected()) { 
    line = client.readStringUntil('\n'); 
    //Serial.println(line);   
    Serial.println("Parsing Forecast Data..."); 
    //create a json buffer where to store the json data 
    StaticJsonBuffer<5000> jsonBuffer; 
    JsonObject& data = jsonBuffer.parseObject(line); 
    if (!data.success()) { 
      Serial.println(F("parseObject() failed")); 
      return; 
    }
    for(int w=0; w<numPoints; w++){
      //forecastArray[0] = celsiusToFarenheit(data["list"][0]["main"]["temp"]);
      forecastArray[w] = data["list"][w]["clouds"]["all"];
      //Serial.println(forecastArray[w]); 
    
      String sAph0 = data["list"][w]["dt_txt"];
      forecastArrayTimes[w] = sAph0.substring(11,13).toInt(); //in UTC 
      //String sAph1 = data["list"][1]["dt_txt"];
      //forecastArrayTimes[1] = sAph1.substring(11,13).toInt(); //in UTC 
      //String sAph2 = data["list"][2]["dt_txt"];
      //forecastArrayTimes[2] = sAph2.substring(11,13).toInt(); //in UTC 
      //String sAph3 = data["list"][3]["dt_txt"];
      //forecastArrayTimes[3] = sAph2.substring(11,13).toInt(); //in UTC 
      //Serial.println(sAph.substring(11,13));
    }

    
    //Check if DST or not
    time_t now = time(0);
    struct tm now_t = *localtime(&now);
    bool boolDST = now_t.tm_isdst;
    //Make times in home timezone:
    for (int g=0; g<numPoints; g++){
      if(boolDST){
        forecastArrayTimes[g] += (1 + utc_offset_hours); //this can be negative! if it is, add 24.
        if(forecastArrayTimes[g] < 0){
          forecastArrayTimes[g] += 24;
        } else if(forecastArrayTimes[g] > 23) { //could also be larger than 23 -- if so subtract 24
          forecastArrayTimes[g] -= 24;
        }     
      } else {
        forecastArrayTimes[g] += utc_offset_hours;    //this can be negative! if it is, add 24.
        if(forecastArrayTimes[g] < 0){
          forecastArrayTimes[g] += 24;
        } else if(forecastArrayTimes[g] > 23) { //could also be larger than 23 -- if so subtract 24
          forecastArrayTimes[g] -= 24;
        }
      }
    }
    client.stop(); //aph?
  }

  for(int z=0; z<numPoints; z++){
    float temporary;
    temporary = forecastArray[z];
    forecastArray[z] = estimateGHI(temporary, getCurrentMonth(), forecastArrayTimes[z]); //this converts cloud percent to GHIestimate
  }
  
  return; //the GHI values are returned via pass by reference.
}








/************************************ ENERGY ADVISOR LOGIC ************************************/
/************************************ ENERGY ADVISOR LOGIC ************************************/
/************************************ ENERGY ADVISOR LOGIC ************************************/

//----------------------------------------------------------------------------
// Determines if opening windows 
// will aid in heating/cooling the building.
//hard coded some threshold values for depracated case -- need to update
//----------------------------------------------------------------------------
String shouldOpenWindows(float outsideTemp, float insideTemp, float targetTemp, String heatOrCool){
  float tempDifThreshold = 3.0; //F
  String openWindows = "";
  //opening windows is only effective if temp dif is large enough
  if((heatOrCool == "cool") and (insideTemp - outsideTemp > tempDifThreshold)){
    openWindows += "Now is a great time to open your windows to help cool your house.  Remember, if you have a multi-level house or apartment, opening windows upstairs helps vent the rising heat, while also opening windows downstairs can help bring in cooler air.  For the best effect, try to make the air travel a long path through your house.  ";
  } else if((heatOrCool == "heat") and (outsideTemp - insideTemp > tempDifThreshold)) {
    openWindows += "Now is a great time to open your windows to help heat your house.  Remember, hot air tends to rise, so if you have a multi-level house or apartment it is best to keep the upstairs windows closed.  ";
  } else if((heatOrCool == "maintain") and (abs(outsideTemp-targetTemp) < 4) and (abs(insideTemp - outsideTemp) < 4) and (abs(insideTemp - targetTemp) < 4)){
    //add further if statements for forecast -- can "bank" some extra heat or cold before temperature swing.
    openWindows += "Temperatures inside and outside are close to the same as your target temperature.  If you'd like some fresh air or to maintain your target temperature, now would be a good time to open the windows.  ";
  }
  return openWindows;
}


//----------------------------------------------------------------------------
// Determines if closing windows 
// will aid in heating/cooling the building.
//----------------------------------------------------------------------------
String shouldCloseWindows(float outsideTemp, float insideTemp, float targetTemp, String heatOrCool){
  float tempDifThreshold = 1.5; //F
  String closeWindows = "";
  //opening windows is only effective if temp dif is large enough
  if((heatOrCool == "cool") and (insideTemp - outsideTemp < tempDifThreshold)){
    closeWindows += "It's time to close your windows:  the external temperature is now too hot to effectively cool your house.  ";
  } else if((heatOrCool == "heat") and (outsideTemp - insideTemp < tempDifThreshold)) {
    closeWindows += "It's time to close your windows:  the external temperature is now too cold to effectively heat your house.  ";
  } else if((heatOrCool == "maintain") and (abs(outsideTemp-targetTemp) > 4)){
    closeWindows += "It's time to close your windows:  the external temperature is no longer close enough to your target temperature to maintain the intended temperature.  ";
  }
  return closeWindows;
}


//---------------------------------------------------------------------------
// Determines if opening blinds/curtains 
// will aid in cooling/heating the building.
//south facing windows most important (for Northern hemisphere)
//---------------------------------------------------------------------------
String shouldOpenBlinds(float outsideTemp, float insideTemp, float solarIrrad, String heatOrCool){
  String openBlinds = "";
  float heatThreshold = 60.0; //units: Watts per sq meter
  float coolThreshold = -10.0; //units: Watts per sq meter
  float netPowerFlow = computeNetPowerFlow(insideTemp, outsideTemp, solarIrrad);
  //if want to heat and sun is shining, open blinds.
  //if want to cool and sun is shining, close them.
  //if want to cool and sun is not shining, open them.
  //if want to heat and sun is not shining, close them.
  if((heatOrCool == "heat") and (netPowerFlow > heatThreshold)){
    openBlinds += "You should open your blinds and/or curtains to allow the sunlight to help heat your house.  ";
    if(latitude.toInt() > 0){
      //Northern hemisphere benefits most from south facing blinds
      openBlinds += "(Especially blinds and curtains on South-facing windows!) ";
    } else if(latitude.toInt() < 0) {
      //Southern hemisphere benefits most from north facing blinds
      openBlinds += "(Especially blinds and curtains on North-facing windows!) ";
    }
  } else if((heatOrCool == "cool") and (netPowerFlow < coolThreshold)){
    openBlinds += "You should open your blinds and/or curtains to allow heat to escape.  ";
  } 
  return openBlinds;
}


//---------------------------------------------------------------------------
// Determines if closing blinds/curtains 
// will aid in cooling/heating the building.
//south facing windows most important (for Northern hemisphere)
//---------------------------------------------------------------------------
String shouldCloseBlinds(float outsideTemp, float insideTemp, float solarIrrad, String heatOrCool){
  String closeBlinds = "";
  float heatThreshold = 20; //units: Watts per sq meter
  float coolThreshold = 0; //units: Watts per sq meter
  float netPowerFlow = computeNetPowerFlow(insideTemp, outsideTemp, solarIrrad);
  //if want to heat and sun is shining, open blinds.
  //if want to cool and sun is shining, close them.
  //if want to cool and sun is not shining, open them.
  //if want to heat and sun is not shining, close them.
  if((heatOrCool == "heat") and (netPowerFlow < heatThreshold)){
    closeBlinds += "You should close your blinds to help keep heat in your house.  ";
  } else if((heatOrCool == "cool") and (netPowerFlow > coolThreshold)){
    closeBlinds += "You should close your blinds to help block the sun from heating your house.  ";
  } 
  return closeBlinds;
}


//---------------------------------------------------------------------------
// Nightly reminder to turn off any unused
// power strips in the house for the night.
//---------------------------------------------------------------------------
String nightlyPowerStrip(){
  //randomize which tip-wording is sent to mix up alerts -- avoids notification desensitizing 
  int dayNum = getDayNumber();      //SUN=0, MON=1, etc.

  int whichTip = dayNum % 4; //0-6 taken %4 is 0-3
  
  String line[4];
  line[0] = "If you keep your TV, gaming consoles, or office equipment on a power strip, be sure to switch it off for the night to save energy. ";
  line[1] = "Don't forget to turn off power strips for the night! ";
  line[2] = "Turning off power strips at night can save you some money. ";
  line[3] = " "; //blank to avoid annoying user?
  
  return line[whichTip];
}


//---------------------------------------------------------------------------                                  
// Reminds user to turn lights off at night                                     
//---------------------------------------------------------------------------                                  
String nightlyLightsReminder(){
  String nightlyLights = "";
  String line[3];
  
  //randomize which tip-wording is sent to mix up alerts -- avoids notification desensitizing 
  int dayNum = getDayNumber();      //SUN=0, MON=1, etc.
  int whichTip = dayNum % 3; //0-6 taken %3 is 0-2
  
  line[0] = "Please don't forget to turn off the lights before bed.  Remember, outdoor lights are commonly missed! "; 
  line[1] = "Turning off lights for the night can save you some money. In fact, a 100 Watt light bulb left on all night every night for a year will cost about $30. ";
  line[2] = " "; //blank to avoid annoying user?
  return line[0];
}


//---------------------------------------------------------------------------                                  
// Reminds user to charge Electric Vehicle (EV)                                   
//---------------------------------------------------------------------------                                  
String chargeEVReminder(){
  String line1 = "Don't forget to charge your electric vehicle! If you live in a region with little to no solar power and variable electricity rates, it may be cheaper to charge it at night. ";
  return line1;
}


//---------------------------------------------------------------------------
// Nightly reminder to set back thermostat a few degrees.
//---------------------------------------------------------------------------
String nightlyThermostat(String heatOrCool, float targetTemp){
  String thermoReminder = "";
  int dayNum = getDayNumber();      //SUN=0, MON=1, etc.
  
  //nominal reminder:
  if(heatOrCool == "heat"){
    thermoReminder += "Don't forget to set your thermostat several degrees cooler tonight in order to save energy!  ";
  } else if(heatOrCool == "cool") {
    thermoReminder += "Don't forget to set your thermostat several degrees warmer tonight in order to save energy!  If you have a ceiling fan, using it saves energy compared to using the A/C.  ";
  }
  
  //Add capabilities to handle forecast and make smarter decisions here, for banking heat before cold snap, e.g.

  //Add feature to nudge user towards more savings.  Quanitfy the expected savings of setting thermostat back 3 degrees for 8 hours.  Extrapolate to a monthly savings to encouraage behavior.
  if(dayNum % 2 == 1){
    thermoReminder += "As a general rule of thumb, every 1 degree you set back your thermostat for an 8 hour period, you'll save 1% on your electricity bill. ";
    if(dayNum == 1){
      thermoReminder += "If your annual electric bill is $1200, setting your thermostat back 3 degrees every night for 8 hours could save you about $36 a year. ";
    }
  }
  
  return thermoReminder;
}


//---------------------------------------------------------------------------
// Avoid temperatures diverging from comfortable levels when other
// management actions are too successful. This function reverses HVACmode
// and updates the user of the change. Beta testing still.
//---------------------------------------------------------------------------
String preventExcessiveGainOrLoss(String &heatOrCool, float insideTemp){
  String preventExcess = "";
  
  if((heatOrCool == "heat") and (insideTemp >= coolTemp)){
    preventExcess += "You've managed to heat your house with just the sun!  In fact, you've managed to heat it so well, you may want to take action to avoid over-heating it.  I've temporarily switched to cooling mode! ";
    //change HVAC mode to opposite:
    heatOrCool = "cool";
  } else if((heatOrCool == "cool") and (insideTemp <= heatTemp)) {
    preventExcess += "You've managed to cool your house quite a bit!  In fact, you've managed to cool it so well, you may want to take action to avoid it getting too cool.  I've temporarily switched to heating mode! ";
    //change HVAC mode to opposite:
    heatOrCool = "heat";
  }

  //future update: add capabilities within above if statements for Velma to check forcast and see if user is okay banking heat before a cold snap or cooling off extra to preempt a heat wave.
  //no use in switching HVAC mode if the temperature swings right back to cold/hot, necessitating another change and resulting in wasted energy.

  return preventExcess;
}


//---------------------------------------------------------------------------
// Morning thermostat recommendation from forecast check
//---------------------------------------------------------------------------
/*
String morningThermostat(String heatOrCool, float insideTemp, float forecastArray[], int forecastArrayTimes[]){
  String morningThermo = "";
  int numPoints = sizeof(forecastArrayTimes)
  
  //nominal reminder:
  if(heatOrCool == "heat"){
    morningThermo += "";
  } else if(heatOrCool == "cool") {
    morningThermo += "";
  }
  
  return morningThermo;
}
*/


//---------------------------------------------------------------------------                                  
// Reminds user to take advantage of solar energy opportunity                                  
//---------------------------------------------------------------------------     
/*                             
String solarOpportunity(){
  return "The sun is currently shining and your solar panels are producing energy.  If you are in an area without net metering laws, now would be a good time to do any energy intensive chores like charging an EV or doing the laundry. "; 
}
*/


//---------------------------------------------------------------------------                                  
// Reminds user to switch over to battery if variable rates are higher
// at certain times of the day or if rolling blackouts or grid management
// is needed at that time.                      
//---------------------------------------------------------------------------     
/*                             
String batteryRecommend(){
  return "Now is a good time to utilize energy stored in your battery system. "; 
}
*/


//---------------------------------------------------------------------------                                  
// Overrides home automation features to avoid grid strain as part of 
// distributed virtual power plant feature.                         
//---------------------------------------------------------------------------     
/*                             
void virtualPowerPlant(){
  //set thermostat back 1 deg. if day, 2 if night.
  //dim lights by 5% if on and dimmable. 
  //delay laundry and dishwashing?
  //switch to battery if user has battery.
  //reverse car charge if have EV and opted in
  //if blinds motorized, override and make sure they are open or closed based on temp/solar?
  //etc.
  //alert user via text of override, if they have opted into distributed virtual powerplant feature.
  return;
}
*/







//-----------------------------------------------------------------------------------------------
//---------------------------------------~~~~~~~~~~~~~~~~----------------------------------------
//-------------------------------------- ~ MAIN PROGRAM ~ ---------------------------------------
//---------------------------------------~~~~~~~~~~~~~~~~----------------------------------------
//-----------------------------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  delay(100);
  WiFi.disconnect();
  dht.begin();
  delay(100);
  EEPROM.begin(EEPROM_SIZE);
  delay(100);
  //uncomment out next two lines for pre-shipping setup, then recomment out before new user loop:
  //EEPROM.write(NEW_USER, 1);
  //EEPROM.commit(); //NEW USER MUST SET UP DEVICE -- see readme.
  delay(100);
}

void loop() {
  
  //Connect to WiFi
  delay(500);
  connectWiFi();
  delay(1000);
  
  
  //check if the user is new, if so, zero out and initialize
  if(EEPROM.read(NEW_USER) != 0) {
    Serial.println(F("The new user loop has been entered. "));
    delay(10 * 1000); //delay 10 seconds in case programmer does not want to start new user.
    configureNewUser();
    //Serial.println(EEPROM.read(NEW_USER));
  }
  
  
  //Date and time:
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(1000);
  int currentHour = getCurrentHour(); //hour in 24 hour time.
  delay(500);                         //takes a sec for some reason
  currentHour = getCurrentHour();     //hour in 24 hour time.
  int currentDay = getDayNumber();    //Day as a number. SUN = 0, MON = 1, and so on.
  //Serial.print(F("The current hour is:  "));
  //Serial.println(currentHour);


  //Observables:
  float insideTemp;
  float outsideTemp;
  float cloudPercent;        //0-100
  float solarIrradiance;
  float forecastTemps[8];      //entries, 3 hours apart. don't need to use all (partialArray)
  int forecastTimes[8];        //entries to list times of above forecast temps
  float solarForecast[8];      //8 but dont need to use all (partialArray) get via: getSolarForecast(awayBegin, awayFinish, solarForecast, solarForecastTimes, numPoints)
  int solarForecastTimes[8];   //8 but don't need to use all (partialArray) ""
  
  //Settings:
  String HVACmode = heatOrCool();
  float tempGoal; 
  if(HVACmode == "heat"){
    tempGoal = heatTemp; 
  } else {
    tempGoal = coolTemp; 
  }
  
  
  //Mode: 
  bool isUserAway = false;
  //put code here for turning away mode on.
  isUserAway = isTheUserAway();


  //States:
  bool windowsOpen = EEPROM.read(WINDOW_STATE);   //0 if closed, 1 if open
  bool blindsOpen = EEPROM.read(BLINDS_STATE);    //0 if closed, 1 if open


  //Score:
  int thisWeeksActions = EEPROM.read(WEEKLY_ACTIONS_ADDRESS); //this is reset every week during weekly email summary.
  int runningLoopScore = 0; //start at 0 for each loop.


  //VELMA recommendations:
  String morningForecast = "";
  String windowRecommendation = "";
  String blindsRecommendation = "";
  String thermostatRecommendation = "";
  String powerStripRecommendation = "";
  String lightsReminder = "";
  String EVrecommendation = "";
  String excessWarning = "";


  //Messages:
  String messagesToSend[MAX_MSGs];
  int numMessages = 0;
  String finalMessage = "";

  
  //only need to check and notify during times user has specified.
  if(currentHour >= earliestMsg && currentHour < latestMsg) {

      //~~~~~~~~~~~~~~~-----------> VELMA Morning Routine: <-----------~~~~~~~~~~//
      
      if(currentHour == earliestMsg){
        //Update daily thermostat nudge score:
        updateUserScore(calcThermoNudgeScore(HVACmode));
        //runningLoopScore += calcThermoNudgeScore(HVACmode);
        /*
          getDailyForecast(forecastTemps, forecastTimes);
          morningForecast = morningThermostat(HVACmode, tempGoal, forecastTemps, forecastTimes);
          //debug:
          Serial.println(F("debug..."));
          Serial.println(F("Time      Temp(F)"));
          for(int m=0; m<4; m++){
              Serial.print(forecastTimes[m]);
              Serial.print(F("              "));
              Serial.println(forecastTemps[m]);
          }
          */
      }
      
      

      //~~~~~~~~~~~~~~~-----------> VELMA Measurements: <-----------~~~~~~~~~~//

      //Grab temps and other observables:
      getInsideTemp(insideTemp);
      getOutsideTemp(outsideTemp);
      float cloudPercent; 
      getConditions(cloudPercent); //0-100
      solarIrradiance = estimateGHI(cloudPercent, getCurrentMonth(), getCurrentHour());

      //APH TEST DELME BELOW:
      //addMessageToQueue(messagesToSend, numMessages, "The current GHI is: " + String(solarIrradiance));
      //addMessageToQueue(messagesToSend, numMessages, "The current T_out is: " + String(outsideTemp));
      //addMessageToQueue(messagesToSend, numMessages, "The current T_in is: " + String(insideTemp));
      //APH TEST DELME ABOVE

      //~~~~~~~~~~~~~~~-----------> VELMA Logic: <-----------~~~~~~~~~~//

      //Flag excessive heat gain or loss:
      excessWarning += preventExcessiveGainOrLoss(HVACmode, insideTemp);
      //adds a msg to queue at end if other messages exist. otherwise notification suppressed.

      
      //Window Recommendation: 
      //(only if user is home vis-a-via crime/rain/etc.):
      if(windowsOpen == true and isUserAway == false){
          windowRecommendation = shouldCloseWindows(outsideTemp, insideTemp, tempGoal, HVACmode);
          addMessageToQueue(messagesToSend, numMessages, windowRecommendation);
          if(windowRecommendation != ""){
              runningLoopScore += 1;
              EEPROM.write(WINDOW_STATE, 0); //if windows are open and VELMA recommends they are closed, assume the user closes them.
              EEPROM.commit();   
          } 
      } else if(windowsOpen == false and isUserAway == false){
          windowRecommendation = shouldOpenWindows(outsideTemp, insideTemp, tempGoal, HVACmode);
          addMessageToQueue(messagesToSend, numMessages, windowRecommendation);
          if(windowRecommendation != ""){
              runningLoopScore += 1;
              EEPROM.write(WINDOW_STATE, 1); //if windows are closed and VELMA recommends they are opened, assume the user opens them.
              EEPROM.commit();   
          }
      }//end window rec
      
      
      //Blinds Recommendation:
      if(blindsOpen == true and isUserAway == false and currentHour != awayStart - 1){ //this is if the user is home and not leaving in the next hour
          blindsRecommendation = shouldCloseBlinds(outsideTemp, insideTemp, solarIrradiance, HVACmode);
          addMessageToQueue(messagesToSend, numMessages, blindsRecommendation);
          if(blindsRecommendation != ""){
              runningLoopScore += 1;
              EEPROM.write(BLINDS_STATE, 0); //if blinds are open and VELMA recommends they are closed, assume the user closes them.
              EEPROM.commit();   
          } 
      } else if(blindsOpen == false and isUserAway == false and currentHour != awayStart - 1){ //this is if the user is home and not leaving in the next hour
          blindsRecommendation = shouldOpenBlinds(outsideTemp, insideTemp, solarIrradiance, HVACmode);
          addMessageToQueue(messagesToSend, numMessages, blindsRecommendation);
          if(blindsRecommendation != ""){
              runningLoopScore += 1;
              EEPROM.write(BLINDS_STATE, 1); //if blinds are open and VELMA recommends they are closed, assume the user closes them.
              EEPROM.commit();   
          } 
      } else if(blindsOpen == true and currentHour == awayStart - 1) { //beginning of the away period
        //get average GHI
        int numberOfPoints;
        Serial.print(F("numPoints in forecast:"));
        Serial.println(numberOfPoints);
        getSolarForecast(awayStart, awayEnd, solarForecast, solarForecastTimes, numberOfPoints);
        getAwayTempForecast(awayStart, awayEnd, forecastTemps, forecastTimes, numberOfPoints);
        float sumSol = 0;
        float sumTemps = 0;
        for(int y=0; y<numberOfPoints; y++){
          Serial.print(F("Blinds currently opened loop, solar forecast is: "));
          Serial.println(solarForecast[y]);
          Serial.print(F("Blinds currently opened loop, temp forecast is: "));
          Serial.println(forecastTemps[y]);
          sumSol += solarForecast[y];
          sumTemps += forecastTemps[y];
        }
        float avgSolarIrr = sumSol/numberOfPoints;
        float avgOutTemp = sumTemps/numberOfPoints;
        Serial.print(F("AVG solar forecast is: "));
        Serial.println(avgSolarIrr);
        Serial.print(F("AVG temp forecast is: "));
        Serial.println(avgOutTemp);
        //then do fxn call as normal but with average
        blindsRecommendation = shouldCloseBlinds(avgOutTemp, insideTemp, avgSolarIrr, HVACmode);
        addMessageToQueue(messagesToSend, numMessages, blindsRecommendation);
        if(blindsRecommendation != ""){
            runningLoopScore += 1;
            EEPROM.write(BLINDS_STATE, 0); //if blinds are open and VELMA recommends they are closed, assume the user closes them.
            EEPROM.commit();   
        }
      } else if(blindsOpen == false and currentHour == awayStart - 1) { //beginning of the away period
        //get average GHI
        int numberOfPoints;
        Serial.print(F("numPoints in forecast:"));
        Serial.println(numberOfPoints);
        getSolarForecast(awayStart, awayEnd, solarForecast, solarForecastTimes, numberOfPoints);
        getAwayTempForecast(awayStart, awayEnd, forecastTemps, forecastTimes, numberOfPoints);
        float sumSol = 0;
        float sumTemps = 0;
        for(int y=0; y<numberOfPoints; y++){
          Serial.print(F("Blinds currently closed loop, solar forecast is: "));
          Serial.println(solarForecast[y]);
          Serial.print(F("Blinds currently closed loop, temp forecast is: "));
          Serial.println(forecastTemps[y]);
          sumSol += solarForecast[y];
          sumTemps += forecastTemps[y];
        }
        float avgSolarIrr = sumSol/numberOfPoints;
        float avgOutTemp = sumTemps/numberOfPoints;
        Serial.print(F("AVG solar forecast is: "));
        Serial.println(avgSolarIrr);
        Serial.print(F("AVG temp forecast is: "));
        Serial.println(avgOutTemp);
        //then do fxn call as normal but with average
        blindsRecommendation = shouldOpenBlinds(avgOutTemp, insideTemp, avgSolarIrr, HVACmode);
        addMessageToQueue(messagesToSend, numMessages, blindsRecommendation);
        if(blindsRecommendation != ""){
            runningLoopScore += 1;
            EEPROM.write(BLINDS_STATE, 1); //if blinds are open and VELMA recommends they are closed, assume the user closes them.
            EEPROM.commit();   
        } 
      }//end blinds check

      
      //Nightly Reminders:
      if(currentHour == latestMsg - 1){
          //Power Strip Reminder:
          powerStripRecommendation = nightlyPowerStrip();
          addMessageToQueue(messagesToSend, numMessages, powerStripRecommendation);
          runningLoopScore += 1;

          //Turn Off Lights:
          lightsReminder = nightlyLightsReminder();
          addMessageToQueue(messagesToSend, numMessages, lightsReminder);
          runningLoopScore += 1;

          //Nightly Thermostat Reminder:
          thermostatRecommendation = nightlyThermostat(HVACmode, tempGoal);
          addMessageToQueue(messagesToSend, numMessages, thermostatRecommendation);
          runningLoopScore += 3;

          //Nightly charge EV reminder
          if(hasEV){
              EVrecommendation = chargeEVReminder();
              addMessageToQueue(messagesToSend, numMessages, EVrecommendation);
              runningLoopScore += 1;
          }

          //Need to add some reminder about forecast for the night etc. if windows still open?
          
      }//end nightly reminder


      //Solar Opportunity Check:
      //Virtual Power Plant Check:
      

      //Come back to excess heat/cool flag, and only mention the switch if a reccomendation was generated:
      if(excessWarning != "" and numMessages > 0){
        addMessageToQueue(messagesToSend, numMessages, excessWarning);
      }//might need to worry about this as the last rec. of the night? 


      //~~~~~~~~~~-----------> VELMA Score Keeping: <-----------~~~~~~~//
      //update cummulative score:
      //first, add multipliers if relevant:
      int multiplier = 1 + isPeak();
      //setting thermostat back does not get multiplier as it was added to score outside of the running score.
      
      //write score:
      updateUserScore(multiplier * runningLoopScore);

      
      //~~~~~~~~~~-----------> VELMA Communication: <-----------~~~~~~~//

      thisWeeksActions += numMessages; //score keeping purposes.
      //only compose message for recommendations with string length > 0. This is built in to addMessageToQueue()
      //compile the entire message:
      finalMessage = combineMessages(messagesToSend, numMessages);
      //send message:
      if(finalMessage.length() != 0){
          sendTxtMsg(finalMessage);
      }
      
      //weekly email summary (send every SUN):
      if((currentDay == 0) && (currentHour == latestMsg - 1)){
          //send an email every Sunday night.
          sendEmail(formatWeeklyEmailMessage(thisWeeksActions, insideTemp), "VELMA Weekly Summary"); //this process will update user cummulative score and resets the weekly count.
      }
      
  }   else {
      delay(35 * 1000); // delay 35 seconds to avoid some degree of drift at night.  Quantify better latter.
  }  //end main script

  


  //measurement debug output:
  Serial.print(F("The temperature inside is:  "));
  Serial.print(insideTemp);
  Serial.println(F("°F.  "));
  Serial.print(F("The temperature outside is:  "));
  Serial.print(outsideTemp);
  Serial.println(F("°F.  "));
  Serial.print(F("The Global Horizontal Irradiance is:  "));
  Serial.println(solarIrradiance);
  //Serial.println(windowRecommendation);
  //Serial.println(blindsRecommendation);
  //Serial.println(powerStripRecommendation);

  //write weekly score to save it.
  EEPROM.write(WEEKLY_ACTIONS_ADDRESS, thisWeeksActions);
  EEPROM.commit();

  //debug offset:
  //sendEmail(String(insideTemp) + "\n" + String(solarIrradiance), "Temp and GHI_estimate");

  //Enter energy saving/waiting mode. Deep sleep will force setup to run again before loop.
  WiFi.disconnect(); //disconnect to save energy?
  delay(1000);
  esp_sleep_enable_timer_wakeup(sleepDuration); //in microseconds not milli - 
  esp_deep_sleep_start();
  
  //delay(59 * 60000 + 20*1000); //60000 ms = 1 min, 
  //NOTE: delay should be different if overhead delays are counted during the day or skipped during night.
  //above delay should, counting the time for other processes, ensure that the messaging occurs every 1 hour.
  //NOTE: update above value to account for time spent doing all the above actions!!!! otherwise drift will possibly be an issue.
}
