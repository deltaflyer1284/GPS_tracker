/*
 GPS and Sensor datalogger for Koala Project
 Created by:  Adrian Rus
 School of Life and Environmental Sciences | Faculty of Science
 THE UNIVERSITY OF SYDNEY
 Room 226 Heydon-Laurence Building (A08) | The University of Sydney | NSW | 2006       
 Date:        17/06/18
 Version:     1.0
*/


#include "ArduinoLowPower.h"
#include <RTCZero.h>
#include <SPI.h>
#include <SdFat.h>
#include <Wire.h>
#include <Adafruit_SleepyDog.h>
#include <LSM303.h>
#include <NMEAGPS.h>
#include <GPSport.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

SdFat SD;            

#define cardSelect 4  // Set the pin used for uSD

#define GREEN 8 // Green LED on Pin #8
#define RED 13
#define VBATPIN A7    // Battery Voltage on Pin A7
#define gpsPower 12 

//extern "C" char *sbrk(int i); //  Used by FreeRAm Function

//////////////// Key Settings ///////////////////
#define SampleInt 5 // RTC - Sample interval in minutes
#define SamplesPerCycle 2  // Number of samples to buffer before uSD card flush is called. 

// 65536 (2^16) is the maximum number of spreadsheet rows supported by Excel 97, Excel 2000, Excel 2002 and Excel 2003 
// Excel 2007, 2010 and 2013 support 1,048,576 rows (2^20)). Text files that are larger than 65536 rows 
// cannot be imported to these versions of Excel.
//#define SamplesPerFile 1440 // 1 per minute = 1440 per day = 10080 per week and ¬380Kb file (assumes 38bytes per sample)

/* Change these values to set the current initial time */
byte hours = 10;
byte minutes = 1;
byte seconds = 0;
/* Change these values to set the current initial date */
byte day = 27;
byte month = 5;
byte year = 18;
const int aquasitionTime = 90;
int fixDuration = 0;

/////////////// Global Objects ////////////////////
RTCZero rtc;          // Create RTC object
File gpsLogFile;         // Create file object
File sensorLogFile;
char filename[15];    // Array for file name data logged to named in setup
char sensorReport[80];
float measuredvbat;   // Variable for battery voltage
int NextAlarmSec;     // Variable to hold next alarm time seconds value
int NextAlarmMin;     // Variable to hold next alarm time minute value
float pressure;
float temperature;
float presCorr = 244.34;
float elevation;

unsigned int CurrentCycleCount=0;  // Num of smaples in current cycle, before uSD flush call
unsigned int CurrentFileCount=0;   // Num of samples in current file
unsigned int gpsCycle=0;




NMEAGPS  gps; // This parses the GPS characters
gps_fix  fix; // This holds on to the latest values


//////////////    Setup   ///////////////////
void setup() {



  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  digitalWrite(gpsPower,LOW);
  
  //Delay for programming
  delay(10000);
 
  //Set board LED pins as output

  pinMode(8, OUTPUT);
  
  // Setup acc and compass sensors
  Wire.begin();
  



  // Set RTC time
    rtcSetup();

  // Create log files
  strcpy(filename, "GPSLOG00.CSV");  
  CreateFile();

  

  
 
  

}  

////////////////////////////   Main Loop    /////////////////////////////////////////////
void loop() {

  


  //  Code to limit the number of power hungry writes to the uSD
  //  Don't sync too often - requires 2048 bytes of I/O to SD card. 512 bytes of I/O if using Fat16 library
  //  But this code forces it to sync at a fixd interval, i.e. once per hour etc depending on what is set
  if( CurrentCycleCount >= SamplesPerCycle ) {
    gpsLogFile.flush();
    CurrentCycleCount = 0;

    
  }


  
  ///////// Interval Timing and Sleep Code //////////

  

  CurrentCycleCount += 1;       //  Increment samples in current uSD flush cycle
  CurrentFileCount += 1;        //  Increment samples in current file


      getSensors();
      
      delay(100);
      getPress();
      //getGPS();  
      
LowPower.deepSleep(30000);                                    


  // Code re-starts here after sleep !

}   // End of Main Loop




///////////////////////////////   Functions   /////////////////////////////////////////
// set the RTC time using GPS time
void rtcSetup(){
  gpsPort.begin(9600);
  digitalWrite(gpsPower,HIGH);

  uint32_t timer = millis();

 
  rtc.begin();    // Start the RTC in 24hr mode

   
  while((millis()-timer)<(3000)){
    
 
      blinkLED(RED,1);
    
      while(gps.available( gpsPort )) {
      
        fix = gps.read();
     
        if(fix.valid.location && fix.valid.time){
          
        hours = fix.dateTime.hours;
        minutes = fix.dateTime.minutes;
        seconds = fix.dateTime.seconds;
    
        day = fix.dateTime.date;
        month = fix.dateTime.month;
        year = fix.dateTime.year;
        rtc.setTime(hours, minutes, seconds);   // Set the time
        rtc.setDate(day, month, year);    // Set the date
        
        digitalWrite(gpsPower,LOW);
        
        return;
      }
      

    }
  }
    
 
  digitalWrite(gpsPower,LOW);
  
  rtc.setTime(hours, minutes, seconds);   // Set the time
  rtc.setDate(day, month, year);    // Set the date
  
}
// Create new file on uSD incrementing file name as required
void CreateFile()
{
  
  // see if the card is present and can be initialized:
  if (!SD.begin(cardSelect)) {

    error(2);     // Two red flashes means no card or card init failed.
  }
  
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = '0' + i/10;
    filename[7] = '0' + i%10;
    // create if does not exist, do not open existing, write, sync after write
    if (! SD.exists(filename)) {
      break;
    }
  }  
  
  gpsLogFile = SD.open(filename, FILE_WRITE);
  
  if( ! gpsLogFile ) {
    error(3);
  }
  


   // Grab time to use for file creation
   uint16_t C_year = rtc.getYear()+2000;
   uint8_t C_month = rtc.getMonth();
   uint8_t C_day = rtc.getDay();
   uint8_t C_hour = rtc.getHours();
   uint8_t C_minute = rtc.getMinutes();
   uint8_t C_second = rtc.getSeconds();

   // set creation date time
   gpsLogFile.timestamp(T_CREATE, C_year, C_month, C_day, C_hour, C_minute, C_second);
   gpsLogFile.timestamp(T_WRITE, C_year, C_month, C_day, C_hour, C_minute, C_second);
   gpsLogFile.timestamp(T_ACCESS, C_year, C_month, C_day, C_hour, C_minute, C_second);
  
  

  WriteFileHeader();
}

// Write data header to file of uSD.
void WriteFileHeader() {

 
  gpsLogFile.println("DD/MM/YYYY hh:mm:ss,vbat,Lat,Lon,FixTime,hdop,pressure,temp,mag.x,mag.y,mag.z,acc.x,acc.y,acc.z");




}

// Print data and time followed by battery voltage to SD card
void WriteToSD() {

 
  // Formatting for file out put dd/mm/yyyy hh:mm:ss, [sensor output]  
  gpsLogFile.print(rtc.getDay());
  gpsLogFile.print("/");
  gpsLogFile.print(rtc.getMonth());
  gpsLogFile.print("/");
  gpsLogFile.print(rtc.getYear()+2000);
  gpsLogFile.print(" ");
  gpsLogFile.print(rtc.getHours());
  gpsLogFile.print(":");
  if(rtc.getMinutes() < 10)
    gpsLogFile.print('0');      // Trick to add leading zero for formatting
  gpsLogFile.print(rtc.getMinutes());
  gpsLogFile.print(":");
  if(rtc.getSeconds() < 10)
    gpsLogFile.print('0');      // Trick to add leading zero for formatting
  gpsLogFile.print(rtc.getSeconds());
  gpsLogFile.print(",");
  gpsLogFile.print(BatteryVoltage ());   // Print battery voltage
  gpsLogFile.print(",");
  //gpsLogFile.print(31+(random(1,1000)/1000));
  gpsLogFile.print(fix.latitudeL());
  gpsLogFile.print(",");
  //gpsLogFile.print(151+(random(1,1000)/1000));
  gpsLogFile.print(fix.longitudeL());
  gpsLogFile.print(",");
  gpsLogFile.print(fixDuration);
  gpsLogFile.print(",");
  gpsLogFile.print(fix.hdop/1000,2);
  gpsLogFile.print(",");
  gpsLogFile.print(pressure);
  gpsLogFile.print(",");
  gpsLogFile.print(temperature);
  gpsLogFile.print(",");
  gpsLogFile.println(sensorReport); 
  


}

// Read GPS unit
void getGPS(){
  digitalWrite(gpsPower,HIGH);
  gpsPort.begin(9600);


  uint32_t timer = millis();
  uint32_t sensorTimer=millis();
  int i = 0;
 
 



  // Look for valid gps fix for the duration of the aquisition time
  while((millis()-timer)<(aquasitionTime*1000)){ 


    
    while(gps.available( gpsPort ) && i<4) {
      
        

      
      fix = gps.read();
     
        if(fix.valid.location && fix.valid.time){
          i++;

        }
      }         
        if(i>=3){

        hours = fix.dateTime.hours;
        minutes = fix.dateTime.minutes;
        seconds = fix.dateTime.seconds;
    
        day = fix.dateTime.date;
        month = fix.dateTime.month;
        year = fix.dateTime.year;
        rtc.setTime(hours, minutes, seconds);   // Set the time
        rtc.setDate(day, month, year);    // Set the date



        
        WriteToSD();

        gpsCycle = 0;
        digitalWrite(gpsPower,LOW);

        fixDuration = (millis()-timer)/1000;
        return;
        
      }
     }
    
  WriteToSD();
  digitalWrite(gpsPower,LOW); // Turn off gps power
  gpsCycle=0;
  
}

  

// blink out an error code
void error(uint8_t errno) {
  while(1) {
    uint8_t i;
    for (i=0; i<errno; i++) {
      digitalWrite(13, HIGH);
      delay(100);
      digitalWrite(13, LOW);
      delay(100);
    }
    for (i=errno; i<10; i++) {
      delay(200);
    }
  }
}

// Led blink 
void blinkLED(uint8_t LED, uint8_t flashes) {
  uint8_t i;
  for (i=0; i<flashes; i++) {
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
    delay(500);
  }
}


void getSensors(){
LSM303 compass;
LSM303::vector<int16_t> running_min = {32767, 32767, 32767}, running_max = {-32768, -32768, -32768};
  
  compass.init();
  compass.enableDefault();
  compass.normal();
  compass.read();
  
  snprintf(sensorReport, sizeof(sensorReport), "%6d,%6d,%6d,%6d,%6d,%6d",
    compass.m.x, compass.m.y, compass.m.z,
    compass.a.x, compass.a.y, compass.a.z);




  
}

void getPress(){
  Adafruit_BMP280 bme; // I2C
  bme.begin();
  temperature = 0;
  pressure = 0;
  for(int i=0;i<10;i++){
  temperature = temperature + bme.readTemperature();
  pressure = pressure + (bme.readPressure()+presCorr);
  delay(100);
  }
  temperature = temperature/10;
  pressure = pressure/10;
  elevation = bme.readAltitude(pressure/100); 

}

//// Measure battery voltage using divider 
float BatteryVoltage () {
  measuredvbat = analogRead(VBATPIN);   //Measure the battery voltage at pin A7
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  return measuredvbat;
}



void alarmMatch() // Do something when interrupt called
{

}


 
