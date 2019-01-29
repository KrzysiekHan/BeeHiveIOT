//----------------------------------------------
//-------------- LIBS & DEFINITIONS ------------
//----------------------------------------------

#include <Wire.h>
#include "DS3231.h"
#include "HX711.h" 
#include <DHT.h> 
#include "LowPower.h" 
#include <SoftwareSerial.h> 

#define DEBUG 1
#define USE_SMS 1
#define USE_HTTP 0
#define USE_BATTERYSTATE 1
#define USE_DHT 1
#define USE_SCALE 1
#define USE_SCHEDULE 1 
#define USE_GPS 0

#define PERIPH_MOSFET_PIN 10 //MOSFET pin 

#define INTERVAL 30 //device work interval in seconds

#define calibration_factor 13650.0 
#define zero_factor 8457924 
#define DOUT  5 //HX711
#define CLK  4  //HX711

#define DHT_DEVICE1_PIN 2
#define DHT_DEVICE2_PIN 3

//SIM800 TX is connected to Arduino 
#define SIM800_TX_PIN 8

//SIM800 RX is connected to Arduino 
#define SIM800_RX_PIN 9
//----------------------------------------------
//--------- VARIABLES & OBJECTS ----------------
//----------------------------------------------
SoftwareSerial serialSIM800(SIM800_TX_PIN,SIM800_RX_PIN);
DHT dht1;
DHT dht2;
DS3231 Clock;
RTClib RTC;
HX711 scale(DOUT, CLK);

float weight;
float voltage = 0.0;
int humidity1 = 0;
int temperature1 = 0;
int humidity2 = 0;
int temperature2 = 0;

int sensorValue = 0;

//localization & time
// String location = "";
// String gpsLng = "";
// String gpsLat = "";

//sms sending schedule
int programmedHours[24] = {6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22};
int hourActual = 0;
int hourOld = 0;
int hour = 0;
bool requestSendSms = false;

int intervalIterations=0;

enum machineState {
    startProgram,
    wakeUpArduino,
    turnOnSupply,
    connectSIM800,
    getTimeRtc,
    getLocationSIM800,
    measureWeight,
    measureDHT,
    measureBattery,
    sendSmsSIM800,
    turnOffSupply,
    goSleep
  };

enum machineState thisMachine = startProgram;

DateTime now;
int countOK = 0;
int sim800connOK = 0;
//  char* text;
//  char* number;
//  bool error;
//----------------------------------------------
//--------------- S E T U P --------------------
//----------------------------------------------
int licznik = 0;

void setup() {
  pinMode(10, OUTPUT); // MOSFET pin as output
  digitalWrite(10, HIGH); // turn on MOSFET
  
  Wire.begin(); //I2C library
  Serial.begin(9600); //console debugger init
  while(!Serial); //wait for serial ready
  serialSIM800.begin(9600);
  
  //humidity & temp sensors initialization
  dht1.setup(DHT_DEVICE1_PIN);
  dht2.setup(DHT_DEVICE2_PIN);

  //SIM800L initialization
  delay(10000);

  //HX711 initialization with fixed values
  scale.set_scale(calibration_factor); //This value is obtained by using the SparkFun_HX711_Calibration sketch
  scale.set_offset(zero_factor); //Zero out the scale using a previously known zero_factor

  //Initial state for sequence
  thisMachine = turnOnSupply;

}
//----------------------------------------------
//--------------- L O O P ----------------------
//----------------------------------------------

void loop() {

  switch (thisMachine){   
    case turnOnSupply:
      //DEBUG?Serial.println("SQ mosfet"):true;
      digitalWrite(PERIPH_MOSFET_PIN, HIGH); // turn on MOSFET
      
      if (USE_SMS || USE_HTTP || USE_SCHEDULE || USE_GPS) delay(20000); //waiting for module initialisation       
      thisMachine = measureWeight;
    break;
    
    case startProgram:
    break;
    
    case wakeUpArduino:
    break;
    
    case measureWeight:
      if (USE_SCALE){
        //DEBUG?Serial.println("SQ weight"):true;
        getWeight();
      }
      thisMachine = measureDHT;
    break;
    
    case measureDHT:
      if (USE_DHT){
        //DEBUG?Serial.println("SQ dht"):true;
        readHumTemp(); 
      }
      thisMachine = measureBattery;
    break;
    
    case measureBattery:
      if (USE_BATTERYSTATE){
        //DEBUG?Serial.println("SQ battery"):true;
        readBatteryStatus(); 
      }
      thisMachine = connectSIM800;
    break;
    
    case connectSIM800:
    thisMachine = getTimeRtc;
    
    if (USE_SMS || USE_HTTP || USE_SCHEDULE || USE_GPS)
    {
      //DEBUG?Serial.println("SQ conn sim800"):true;
      sim800connOK = 1;
      if (sim800connOK){
          //DEBUG?Serial.println("SIM800 OK"):true;
          thisMachine = getTimeRtc;
        }
      else{
          //DEBUG?Serial.println("SIM800 ERROR"):true;
          thisMachine = turnOffSupply;
        }
    } else {
      thisMachine = turnOffSupply;
    }
    break;
        
    case getTimeRtc:      
      if(USE_SCHEDULE)
      {
        //DEBUG?Serial.println("SQ time"):true;
        getTimeAndCheckSchedule();
      }   
      if (USE_SCHEDULE == 0) //request sms if schedule disabled
      {
        requestSendSms = true;
      }  
      thisMachine = getLocationSIM800;
    break;
    
    case getLocationSIM800:
      if(USE_GPS)
      {
        //DEBUG?Serial.println("SQ location"):true;
        //Nie zmienione dla nowej biblioteki
        //getLocationGps();
      }
      thisMachine = sendSmsSIM800;
    break;
    
    case sendSmsSIM800: 
      
      if (USE_SMS && requestSendSms){
        //DEBUG?Serial.println("SQ sms"):true;
        sendSms(); 
      }
      thisMachine = turnOffSupply;
    break;
    
    case turnOffSupply:
      //DEBUG?Serial.println("SQ supply off"):true;  
      delay(20000);
      digitalWrite(PERIPH_MOSFET_PIN, LOW);          
      thisMachine = goSleep;
    break;
    
    case goSleep:
      
      //DEBUG?Serial.println("SQ sleep"):true; 
      //delay(5000); 
      intervalIterations = INTERVAL / 8;
      goSleep8s(intervalIterations);
      //software_Reset(); //bez resetu modem po kilku połączeniach się zawiesza   
      thisMachine = turnOnSupply;
    break;
    
    //default:
    //DEBUG?Serial.println("SQ ERROR"):true;
    //} 
  }
}

//----------------------------------------------
//----------- F U N C T I O N S ----------------
//----------------------------------------------  
void sendSms(){
  
  //--------------- test sms ----------------------- 
  /*         
  Serial.println("Sending SMS...");
   
  //Set SMS format to ASCII
  serialSIM800.write("AT+CMGF=1\r\n");
  delay(1000);
 
  //Send new SMS command and message number
  serialSIM800.write("AT+CMGS=\"+48723491046\"\r\n");
  delay(1000);
   
  //Send SMS content
  serialSIM800.write("TEST");
  delay(1000);
   
  //Send Ctrl+Z / ESC to denote SMS message is complete
  serialSIM800.write((char)26);
  delay(1000);
     
  Serial.println("SMS Sent!");
  */
  //---------------------------------------------------
  
  //--------------- real sms ---------------------  
  char smsContent [160] = {""};
  char * waga1 = "waga: "; //weight
  char * wilg1 = "\n wilg wew: "; //internal humidity
  char * temp1 = "\n temp wew: "; //internal temperature
  char * wilg2 = "\n wilg zew: "; //external humidity
  char * temp2 = "\n temp zew: "; //external temperature
  char * akum = "\n akum: "; //battery state
  char * jTemp = "*C "; 
  char * jWilg = "%RH ";
  char * jWaga = "kg ";
  char * jAku = "V ";

  char weightString[10] = "0";
  char voltageString[10] = "0";
  char humidity1String[10] = "0";
  char temperature1String[10] = "0";
  char humidity2String[10] = "0";
  char temperature2String[10] = "0";
  char licznikString[10] = "";
  
  dtostrf(weight, 6, 1, weightString);
  dtostrf(humidity1, 6, 1, humidity1String);
  dtostrf(temperature1, 6, 1, temperature1String);
  dtostrf(humidity2, 6, 1, humidity2String);
  dtostrf(temperature2, 6, 1, temperature2String);
  dtostrf(voltage, 6, 2, voltageString);

  Serial.println(weightString);
  Serial.println(humidity1String);
  Serial.println(temperature1String);
  Serial.println(humidity2String);
  Serial.println(temperature2String);
  Serial.println(voltageString);

  //Test counter
  //licznik++;
  //dtostrf(licznik, 4, 0, licznikString);
  //strcpy(smsContent, licznikString);
  //strcat(smsContent, "\n");

  strcat(smsContent, waga1);
  strcat(smsContent,weightString);
  strcat(smsContent, jWaga);
  
  strcat(smsContent, wilg1);
  strcat(smsContent,humidity1String);
  strcat(smsContent, jWilg);
  
  strcat(smsContent, temp1);
  strcat(smsContent,temperature1String);
  strcat(smsContent, jTemp);
  
  strcat(smsContent, wilg2);
  strcat(smsContent,humidity2String);
  strcat(smsContent, jWilg);
  
  strcat(smsContent, temp2);
  strcat(smsContent,temperature2String);
  strcat(smsContent, jTemp);
  
  strcat(smsContent, akum);
  strcat(smsContent, voltageString); 
  strcat(smsContent, jAku);

  //Set SMS format to ASCII
  serialSIM800.write("AT+CMGF=1\r\n");
  delay(1000);
 
  //Send new SMS command and message number
  serialSIM800.write("AT+CMGS=\"+48723491046\"\r\n");
  delay(1000);
   
  //Send SMS content
  serialSIM800.write(smsContent);
  delay(1000);
   
  //Send Ctrl+Z / ESC to denote SMS message is complete
  serialSIM800.write((char)26);
  delay(10000);
  requestSendSms = false;
  Serial.println(smsContent);  
  //------------------------------------------------------------
}

void getWeight(){
      weight = scale.get_units();
      if(weight<0){ weight = 0;}
       //DEBUG?Serial.print(weight):true;
       //DEBUG?Serial.println("kg"):true;
}

void readHumTemp(){
    delay(100);
    humidity1 = dht1.getHumidity();
    temperature1 = dht1.getTemperature();

  //debug info
  if ((dht1.getStatusString() == "OK") && (DEBUG))  {
    Serial.print("Czujnik 1: ");
    Serial.print(humidity1);
    Serial.print("%RH | ");
    Serial.print(temperature1);
    Serial.println("*C");
  }
  
  delay(dht1.getMinimumSamplingPeriod());
  humidity2 = dht2.getHumidity();
  temperature2 = dht2.getTemperature();
  
  if ((dht2.getStatusString() == "OK")&& (DEBUG)) {
    Serial.print("Czujnik 2: ");
    Serial.print(humidity2);
    Serial.print("%RH | ");
    Serial.print(temperature2);
    Serial.println("*C");
  }
  delay(dht2.getMinimumSamplingPeriod());
}

void readBatteryStatus()
{
    sensorValue = analogRead(A0);
    // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
    voltage = (sensorValue * (5.0 / 1023.0))*2 ;
    DEBUG?Serial.print(voltage):true;
    DEBUG?Serial.println("V"):true;
}

void goSleep8s(int iterations)
{
  for (int i = 0; i < iterations; i++) {
    DEBUG?Serial.print("resting sec: "):true; 
    DEBUG?Serial.println(i*8):true; 
    //LowPower.idle(SLEEP_8S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_OFF);
    delay(50);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);  
    delay(50);                
  }
}
  
 void getTimeAndCheckSchedule(){
    delay(1000);
    now = RTC.now();
    Serial.println(now.hour());
    
    //set up request for sending sms only once per hour according to programmed hours
    hour = now.hour();   
    hourActual = hour;
    if (hourOld != hourActual){
      DEBUG?Serial.println("actual != old"):true;
      for (int i=0; i<24;i++){      
        if (programmedHours[i] == hourActual){    
          DEBUG?Serial.println("sms request"):true;    
          requestSendSms = true;
        }
      }     
    } else {
        DEBUG?Serial.println("already checked "):true; 
    }
    hourOld = hourActual; //remember hour for next cycle  
  }

  void getLocationGps(){
     //Serial.print("Lokalizacja: ");
     //location = sim800.getLocation();
     //location = location.substring(2,21);
     //Serial.println(location);
 
    //gpsLng = location.substring(0,9);
    //Serial.println(gpsLng);
    //gpsLat = location.substring(10,21); 
    //Serial.println(gpsLat);
  }

  void setRTC(int Year, int Month, int Day, int DoW, 
    int Hour,int Minute, int Second){
        Clock.setClockMode(false);  // set to 24h
    //setClockMode(true); // set to 12h

    Clock.setYear(Year);
    Clock.setMonth(Month);
    Clock.setDate(Day);
    //Clock.setDoW(1);
    Clock.setHour(Hour);
    Clock.setMinute(Minute);
    Clock.setSecond(Second);
    }
