//----------------------------------------------
//-------------- LIBS & DEFINITIONS ------------
//----------------------------------------------

#include "HX711.h" //przetwornik tensometru
#include <SoftwareSerial.h> //komunikacja szeregowa
#include <DHT.h> //czujniki temperatury i wilgotności
#include "LowPower.h" //oszczędzanie energii
#include <BareBoneSim800.h> // 

#define DEBUG 1
#define INIT_TEST 0
#define USE_GSM 1
#define USE_HTTP 1
#define USE_DHT 0
#define USE_SAVE_ENERGY 0
#define USE_SCALE 0

#define PHONE_NUMBER "+48723491046"

#define PERIPH_MOSFET_PIN 10 //pin tranzystora mosfet odcinającego zasilanie układu

#define calibration_factor 14850.0 //Wartość współczynnika skalowania dla tensometru (dostosowanego w programie kalibracyjnym)
#define zero_factor 8457924 //Wartość współczynnika zera (potrzebne jeśli na wadze cały czas znajduje się obciążenie)
#define DOUT  5 //układ HX711
#define CLK  4  //układ HX711

#define DHT11_DEVICE1_PIN 2
#define DHT11_DEVICE2_PIN 3


//----------------------------------------------
//--------- VARIABLES & OBJECTS ----------------
//----------------------------------------------
BareBoneSim800 sim800("internet"); //moduł GSM
DHT dht1;
DHT dht2;
HX711 scale(DOUT, CLK);

float weight;
char weightString[10] = "";

//stan akumulatora
float voltage = 0.0;
char voltageString[10] = "";

//czujniki na zewnątrz
int wilgotnosc1 = 0;
char wilgotnosc1String[10] = "";
int temperatura1 = 0;
char temperatura1String[10] = "";

//czujniki w środku ula
int wilgotnosc2 = 0;
char wilgotnosc2String[10] = "";
int temperatura2 = 0;
char temperatura2String[10] = "";

//zmienne dla pobrania czasu i lokalizacji
 String time1 = "";
 String location = "";
 String gpsLng = "";
 String gpsLat = "";

bool deviceAttached;
 
//zmienna do nieblokującej obsługi
unsigned long millisActual = 0;

//maszyna stanów do sekwencji
enum machineState {
    startProgram,
    wakeUpArduino,
    turnOnSupply,
    connectSIM800,
    getTimeSIM800,
    getLocationSIM800,
    measureWeight,
    measureDHT11,
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
  pinMode(PERIPH_MOSFET_PIN, OUTPUT);
  Serial.begin(9600);//inicjalizacja dla debugowania w konsoli
  while(!Serial);//oczekiwanie na koniec inicjalizacji

  //Inicjalizacja czujników temperatury i wilgotności
  dht1.setup(DHT11_DEVICE1_PIN);
  dht2.setup(DHT11_DEVICE2_PIN);
  delay(100);
  sim800.begin();
  delay(100);
  //Inicjalizacja HX711
  scale.set_scale(calibration_factor);                //This value is obtained by using the SparkFun_HX711_Calibration sketch
  scale.set_offset(zero_factor);                      //Zero out the scale using a previously known zero_factor
  thisMachine = turnOnSupply;
  // DEBUG?Serial.println("KROK : Startujemy..."):true;
}
//----------------------------------------------
//--------------- L O O P ----------------------
//----------------------------------------------

void loop() {
  millisActual = millis();
  switch (thisMachine){
    case turnOnSupply:
      // DEBUG?Serial.println("KROK :Zasilanie peryferiów uruchamianie modułów..."):true;
      digitalWrite(PERIPH_MOSFET_PIN, HIGH);
      delay(5000);
      // DEBUG?Serial.println("KROK :Zasilanie peryferiów włączone"):true;
      thisMachine = measureWeight;
    break;
    
    case startProgram:
    break;
    
    case wakeUpArduino:
    break;
    
    case measureWeight:
      DEBUG?Serial.println("KROK :Pomiar wagi"):true;
      wagaPomiar();
      thisMachine = measureDHT11;
    break;
    
    case measureDHT11:
      DEBUG?Serial.println("KROK : DHT11"):true;
      readHumTemp(); 
      thisMachine = measureBattery;
    break;
    
    case measureBattery:
      DEBUG?Serial.println("KROK : STAN AKUMULATORA"):true;
      readBatteryStatus(); 
      thisMachine = connectSIM800;
    break;
    
    case connectSIM800:
      deviceAttached = sim800.isAttached();
      if (deviceAttached)
        DEBUG?Serial.println("SIM800 OK"):true;
      else
        DEBUG?Serial.println("SIM800 ERROR"):true;
      thisMachine = getTimeSIM800;
    break;
        
    case getTimeSIM800:
      getTimeAndCheckSchedule();
      thisMachine = getLocationSIM800;
    break;
    
    case getLocationSIM800:
      getLocationGps();
      thisMachine = sendSmsSIM800;
    break;
    
    case sendSmsSIM800:
      DEBUG?Serial.println("KROK : SIM800L"):true;
      sendSms(); 
      thisMachine = turnOffSupply;
    break;
    
    case turnOffSupply:
      digitalWrite(PERIPH_MOSFET_PIN, LOW);
      DEBUG?Serial.println("KROK :Zasilanie peryferiów wyłączone"):true;      
      thisMachine = goSleep;
    break;
    
    case goSleep:
      DEBUG?Serial.println("KROK : Usypianie..."):true;  
      goSleep8s(3);   
      thisMachine = turnOnSupply;
    break;
    default:
    DEBUG?Serial.println("ERROR"):true;
    }

    
}

//----------------------------------------------
//----------- F U N K C J E --------------------
//----------------------------------------------

void wagaPomiar(){
      weight = scale.get_units();
      if(weight<0){ weight = 0;}
      dtostrf(weight, 6, 1, weightString); 
      // DEBUG?Serial.print(weight):true;
      // DEBUG?Serial.println("kg"):true;
  }
  
void sendSms(){
  if (USE_GSM)
  {  
       const char* number = PHONE_NUMBER;
       char smsContent[255];
       String message = "";
       //waga
       message += "waga: ";
       message += weightString; 
       message += " kg \r\n" ;

       //wilgotnosc wew
       message += "wilg wew: ";
       message += wilgotnosc1String; 
       message += " % \r\n" ;

       //temperatura wew
       message += "temp wew: ";
       message += temperatura1String; 
       message += " C \r\n" ;

       //wilgotnosc zew
       message += "wilg zew: ";
       message += wilgotnosc2String; 
       message += " % \r\n" ;

       //temperatura zew
       message += "temp zew: ";
       message += temperatura2String; 
       message += " C \r\n" ;

        //temperatura zew
       message += "aku: ";
       message += voltageString; 
       message += " V \r\n" ;
       
       message.toCharArray(smsContent,255);
       // DEBUG?Serial.println(smsContent):true;
      
       bool messageSent = sim800.sendSMS(number,smsContent);
       if(messageSent)
         Serial.println("Sent");
       else
         Serial.println("Not Sent");       
  } else
  {
    DEBUG?Serial.println("GSM wyłączony USE_GSM = 0 "):true;
  };

  }

  void readHumTemp(){
    //Pobranie informacji o wilgotnosci
    delay(100);
    wilgotnosc1 = dht1.getHumidity();
    dtostrf(wilgotnosc1, 6, 0, wilgotnosc1String); 
    //Pobranie informacji o temperaturze
    temperatura1 = dht1.getTemperature();
    dtostrf(temperatura1, 6, 1, temperatura1String); 
  
  if ((dht1.getStatusString() == "OK") && (DEBUG))  {
    Serial.print("Czujnik 1: ");
    Serial.print(wilgotnosc1);
    Serial.print("%RH | ");
    Serial.print(temperatura1);
    Serial.println("*C");
  }
  //Odczekanie wymaganego czasugo
  delay(dht1.getMinimumSamplingPeriod());
  
  //Pobranie informacji o wilgotnosci
  wilgotnosc2 = dht2.getHumidity();
  dtostrf(wilgotnosc2, 6, 0, wilgotnosc2String); 
  //Pobranie informacji o temperaturze
  temperatura2 = dht2.getTemperature();
  dtostrf(temperatura2, 6, 1, temperatura2String); 
  
  if ((dht2.getStatusString() == "OK")&& (DEBUG)) {
    Serial.print("Czujnik 2: ");
    Serial.print(wilgotnosc2);
    Serial.print("%RH | ");
    Serial.print(temperatura2);
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
    DEBUG?Serial.print("odpoczywam: "):true; 
    DEBUG?Serial.println(i*8):true; 
    //LowPower.idle(SLEEP_8S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_OFF);
    delay(50);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);  
    delay(50);                
    }
  }
  
 void getTimeAndCheckSchedule(){
    Serial.print("Aktualna godzina: ");
    time1 = sim800.getTime();
    time1 = time1.substring(13,15);
    int hour = time1.toInt() + 2;
    time1 = String(hour);
    Serial.println(time1);
  }

  void getLocationGps(){
     Serial.print("Lokalizacja: ");
     location = sim800.getLocation();
     location = location.substring(2,21);
    Serial.println(location);
 
    gpsLng = location.substring(0,9);
    //a = gpsLng.toFloat();
    Serial.println(gpsLng);
    gpsLat = location.substring(10,21); 
    //b = gpsLat.toFloat();
    Serial.println(gpsLat);
  }
    
