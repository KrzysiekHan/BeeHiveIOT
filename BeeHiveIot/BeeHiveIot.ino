//----------------------------------------------
//-------------- LIBS & DEFINITIONS ------------
//----------------------------------------------

#include "HX711.h" 
#include <DHT.h> 
#include "LowPower.h" 
#include <BareBoneSim800.h>  //https://github.com/thehapyone/BareBoneSim800

#define DEBUG 1
#define USE_SMS 1
#define USE_HTTP 0
#define USE_BATTERYSTATE 1
#define USE_DHT 1
#define USE_SCALE 1
#define USE_SCHEDULE 1 //without schedule sms sending won't work
#define USE_GPS 0

#define PHONE_NUMBER "+48723491046"

#define PERIPH_MOSFET_PIN 10 //MOSFET pin 

#define INTERVAL 30 //device work interval in seconds

#define calibration_factor 13650.0 
#define zero_factor 8457924 
#define DOUT  5 //HX711
#define CLK  4  //HX711

#define DHT_DEVICE1_PIN 2
#define DHT_DEVICE2_PIN 3


//----------------------------------------------
//--------- VARIABLES & OBJECTS ----------------
//----------------------------------------------
BareBoneSim800 sim800("internet"); //moduł GSM
DHT dht1;
DHT dht2;
HX711 scale(DOUT, CLK);

//SIM800 connection status
bool sim800connOK; 

float weight;
char weightString[10] = "0";

//battery state
float voltage = 0.0;
char voltageString[10] = "0";

//external sensors
int humidity1 = 0;
char humidity1String[10] = "0";
int temperature1 = 0;
char temperature1String[10] = "0";

//internal sensors
int humidity2 = 0;
char humidity2String[10] = "0";
int temperature2 = 0;
char temperature2String[10] = "0";

//localization & time
 String time1 = "";
 String location = "";
 String gpsLng = "";
 String gpsLat = "";

//sms sending schedule
int programmedHours[17] = {6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22};
int hourActual = 0;
int hourOld = 0;
bool requestSendSms = false;

int intervalIterations=0;
//sequence machine state
enum machineState {
    startProgram,
    wakeUpArduino,
    turnOnSupply,
    connectSIM800,
    getTimeSIM800,
    getLocationSIM800,
    measureWeight,
    measureDHT,
    measureBattery,
    sendSmsSIM800,
    turnOffSupply,
    goSleep
  };

enum machineState thisMachine = startProgram;

//----------------------------------------------
//--------------- S E T U P --------------------
//----------------------------------------------
  
void setup() {
  pinMode(PERIPH_MOSFET_PIN, OUTPUT); // MOSFET pin as output
  
  Serial.begin(9600); //console debugger init
  while(!Serial); 

  //humidity & temp sensors initialization
  dht1.setup(DHT_DEVICE1_PIN);
  dht2.setup(DHT_DEVICE2_PIN);

  //SIM800L initialization
  sim800.begin();

  //HX711 initialization with fixed values
  scale.set_scale(calibration_factor); //This value is obtained by using the SparkFun_HX711_Calibration sketch
  scale.set_offset(zero_factor); //Zero out the scale using a previously known zero_factor

  //initial state for sequence
  thisMachine = turnOnSupply;
}
//----------------------------------------------
//--------------- L O O P ----------------------
//----------------------------------------------

void loop() {
  
  switch (thisMachine){
     
    case turnOnSupply:
      DEBUG?Serial.println("SQ mosfet"):true;
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
        DEBUG?Serial.println("SQ weight"):true;
        getWeight();
      }
      thisMachine = measureDHT;
    break;
    
    case measureDHT:
      if (USE_DHT){
        DEBUG?Serial.println("SQ dht"):true;
        readHumTemp(); 
      }
      thisMachine = measureBattery;
    break;
    
    case measureBattery:
      if (USE_BATTERYSTATE){
        DEBUG?Serial.println("SQ battery"):true;
        readBatteryStatus(); 
      }
      thisMachine = connectSIM800;
    break;
    
    case connectSIM800:
    if (USE_SMS || USE_HTTP || USE_SCHEDULE || USE_GPS)
    {
      DEBUG?Serial.println("SQ conn sim800"):true;
      sim800connOK = sim800.isAttached();
      if (sim800connOK){
          DEBUG?Serial.println("SIM800 OK"):true;
          thisMachine = getTimeSIM800;
        }
      else{
          DEBUG?Serial.println("SIM800 ERROR"):true;
          thisMachine = turnOffSupply;
        }
    } else {
      thisMachine = turnOffSupply;
    }
    break;
        
    case getTimeSIM800:      
      if(USE_SCHEDULE)
      {
        DEBUG?Serial.println("SQ time"):true;
        getTimeAndCheckSchedule();
      }     
      thisMachine = getLocationSIM800;
    break;
    
    case getLocationSIM800:
      if(USE_GPS)
      {
        DEBUG?Serial.println("SQ location"):true;
        getLocationGps();
      }
      thisMachine = sendSmsSIM800;
    break;
    
    case sendSmsSIM800: 
      if (USE_SMS && requestSendSms){
        DEBUG?Serial.println("SQ sms"):true;
        sendSms(); 
      }
      thisMachine = turnOffSupply;
    break;
    
    case turnOffSupply:
      DEBUG?Serial.println("SQ supply off"):true;  
      digitalWrite(PERIPH_MOSFET_PIN, LOW);          
      thisMachine = goSleep;
    break;
    
    case goSleep:
      DEBUG?Serial.println("SQ sleep"):true;  
      intervalIterations = INTERVAL / 8;
      goSleep8s(intervalIterations);
      software_Reset(); //bez resetu modem po kilku połączeniach się zawiesza   
      thisMachine = turnOnSupply;
    break;
    
    default:
    DEBUG?Serial.println("SQ ERROR"):true;
    } 
}

//----------------------------------------------
//----------- F U N C T I O N S ----------------
//----------------------------------------------

void getWeight(){
      weight = scale.get_units();
      if(weight<0){ weight = 0;}
      dtostrf(weight, 6, 1, weightString); 
       DEBUG?Serial.print(weight):true;
       DEBUG?Serial.println("kg"):true;
  }
  
void sendSms(){
       const char* number = "+48723491046";
       char smsContent[255];
       String message = "";
       //waga
       message += "waga: ";
       message += weightString; 
       message += " kg\r\n" ;

       //wilgotnosc wew
       message += "wilg wew: ";
      message += humidity1String; 
       message += " %\r\n" ;

       //temperatura wew
       message += "temp wew: ";
       message += temperature1String; 
       message += " C\r\n" ;

       //wilgotnosc zew
       message += "wilg zew: ";
       message += humidity2String; 
       message += " %\r\n" ;

       //temperatura zew
       message += "temp zew: ";
       message += temperature2String; 
       message += " C\r\n" ;

        //temperatura zew
       message += "aku: ";
       message += voltageString; 
       message += " V\r\n" ;
      
       message.toCharArray(smsContent,255);
       delay(100);
       //DEBUG?Serial.println(smsContent):true;
      
       bool messageSent = sim800.sendSMS(number,smsContent);
       if(messageSent)
       {
         //Serial.println("Sent");
         requestSendSms = false; //acknowledge sms sent
       }
       else
         Serial.println("Not Sent");       
  }

  void readHumTemp(){
    //Pobranie informacji o wilgotnosci
    delay(100);
    humidity1 = dht1.getHumidity();
    dtostrf(humidity1, 6, 0, humidity1String); 
    //Pobranie informacji o temperaturze
    temperature1 = dht1.getTemperature();
    dtostrf(temperature1, 6, 1, temperature1String); 
  
  if ((dht1.getStatusString() == "OK") && (DEBUG))  {
    Serial.print("Czujnik 1: ");
    Serial.print(humidity1);
    Serial.print("%RH | ");
    Serial.print(temperature1);
    Serial.println("*C");
  }
  //Odczekanie wymaganego czasugo
  delay(dht1.getMinimumSamplingPeriod());
  
  //Pobranie informacji o wilgotnosci
  humidity2 = dht2.getHumidity();
  dtostrf(humidity2, 6, 0, humidity2String); 
  //Pobranie informacji o temperaturze
  temperature2 = dht2.getTemperature();
  dtostrf(temperature2, 6, 1, temperature2String); 
  
  if ((dht2.getStatusString() == "OK")&& (DEBUG)) {
    Serial.print("Czujnik 2: ");
    Serial.print(humidity2);
    Serial.print("%RH | ");
    Serial.print(temperature2);
    Serial.println("*C");
  }
  //Odczekanie wymaganego czasugo
  delay(dht2.getMinimumSamplingPeriod());
 }

 void readBatteryStatus()
 {
    int sensorValue = analogRead(A0);
    // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
    voltage = (sensorValue * (5.0 / 1023.0))*2 ;
    dtostrf(voltage, 7, 2, voltageString);
    // DEBUG?Serial.print(voltage):true;
    // DEBUG?Serial.println("V"):true;
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
    time1 = sim800.getTime();
    time1 = time1.substring(13,15);

     //set up request for sending sms only once per hour according to programmed hours
    int hour = time1.toInt() + 2;   
    hourActual = hour;
    if (hourOld != hourActual){
      for (int i=0; i<17;i++){      
        if (programmedHours[i] == hourActual){    
          //DEBUG?Serial.println("sms request"):true;    
          requestSendSms = true;
        }
      }     
    } else {
        //DEBUG?Serial.println("already checked "):true; 
    }
    hourOld = hourActual;
    time1 = String(hour);
    //DEBUG?Serial.println("Actual hour: "):true; 
    //DEBUG?Serial.println(time1):true;    
  }

  void getLocationGps(){
     //Serial.print("Lokalizacja: ");
     location = sim800.getLocation();
     location = location.substring(2,21);
     //Serial.println(location);
 
    gpsLng = location.substring(0,9);
    //Serial.println(gpsLng);
    gpsLat = location.substring(10,21); 
    //Serial.println(gpsLat);
  }

  void software_Reset() // Restarts program from beginning but does not reset the peripherals and registers
  {
    asm volatile ("  jmp 0");  
  }  
