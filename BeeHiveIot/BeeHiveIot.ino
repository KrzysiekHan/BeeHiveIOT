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

#define PERIPH_MOSFET_PIN 10 //pin tranzystora mosfet odcinającego zasilanie układu

#define calibration_factor 14850.0 //Wartość współczynnika skalowania dla tensometru (dostosowanego w programie kalibracyjnym)
#define zero_factor 8457924 //Wartość współczynnika zera (potrzebne jeśli na wadze cały czas znajduje się obciążenie)
#define DOUT  5 //układ HX711
#define CLK  4  //układ HX711

#define DHT11_DEVICE1_PIN 2
#define DHT11_DEVICE2_PIN 3

#define SIM800_TX_PIN 8 //SIM800 TX <-> Arduino D8
#define SIM800_RX_PIN 7 //SIM800 RX <-> Arduino D7
SoftwareSerial serialSIM800(SIM800_TX_PIN,SIM800_RX_PIN);

//----------------------------------------------
//--------- VARIABLES & OBJECTS ----------------
//----------------------------------------------
BareBoneSim800 sim800; //moduł GSM
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

//zmienna do nieblokującej obsługi
unsigned long millisActual = 0;

//maszyna stanów do sekwencji
enum machineState {
    startProgram,
    wakeUpArduino,
    turnOnSupply,
    measureWeight,
    measureDHT11,
    measureBattery,
    sim800send,
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

  dht1.setup(DHT11_DEVICE1_PIN);
  dht2.setup(DHT11_DEVICE2_PIN);
  delay(100);
  //Inicjalizacja modułu GSM
  serialSIM800.begin(9600);
  delay(100); 
  DEBUG?Serial.println("Inicjalizacja HX711..."):true;
  scale.set_scale(calibration_factor);                //This value is obtained by using the SparkFun_HX711_Calibration sketch
  scale.set_offset(zero_factor);                      //Zero out the scale using a previously known zero_factor
  
  DEBUG?Serial.println("Zakończono inicjalizację HX711"):true;
  if(INIT_TEST){
      wagaPomiar();  
      sendSms();
    }; 
  thisMachine = turnOnSupply;
  delay(2000);
  DEBUG?Serial.println("KROK : Startujemy..."):true;
}
//----------------------------------------------
//--------------- L O O P ----------------------
//----------------------------------------------

void loop() {
  millisActual = millis();

    //Sekwencja pracy programu
    if (thisMachine == turnOnSupply )
    {
      DEBUG?Serial.println("KROK :Zasilanie peryferiów uruchamianie modułów..."):true;
      digitalWrite(PERIPH_MOSFET_PIN, HIGH);
      delay(5000);
      DEBUG?Serial.println("KROK :Zasilanie peryferiów włączone"):true;
      thisMachine = measureWeight;
    }
    
    if (thisMachine == measureWeight )
    {
      DEBUG?Serial.println("KROK :Pomiar wagi"):true;
      wagaPomiar();
      thisMachine = measureDHT11;
    }
    
    if (thisMachine == measureDHT11 )
    {
      DEBUG?Serial.println("KROK : DHT11"):true;
      readHumTemp(); 
      thisMachine = measureBattery;
    }

    if (thisMachine == measureBattery )
    {
      DEBUG?Serial.println("KROK : STAN AKUMULATORA"):true;
      readBatteryStatus(); 
      thisMachine = sim800send;
    }
    
    if (thisMachine == sim800send )
    {
      DEBUG?Serial.println("KROK : SIM800L"):true;
      sendSms(); 
      thisMachine = turnOffSupply;
    }

    if (thisMachine == turnOffSupply )
    {
      digitalWrite(PERIPH_MOSFET_PIN, LOW);
      DEBUG?Serial.println("KROK :Zasilanie peryferiów wyłączone"):true;      
      thisMachine = goSleep;
    }
  
    if (thisMachine == goSleep )
    {
      DEBUG?Serial.println("KROK : Usypianie..."):true;  
      goSleep8s(3);   
 
      thisMachine = turnOnSupply;
    }
}

//----------------------------------------------
//----------- F U N K C J E --------------------
//----------------------------------------------

void wagaPomiar(){
      weight = scale.get_units();
      if(weight<0){ weight = 0;}
      dtostrf(weight, 6, 1, weightString); 
      DEBUG?Serial.print(weight):true;
      DEBUG?Serial.println("kg"):true;
  }
  
void sendSms(){
  if (USE_GSM)
  {
          //Ustawienie formatu SMS na ASCII
        serialSIM800.write("AT+CMGF=1\r\n");
        delay(200);
        serialSIM800.write("AT+CMGS=\"723491046\"\r\n");
        delay(200);
        serialSIM800.println(">> UL GSM <<");
        serialSIM800.print("waga:");
        serialSIM800.print(weightString);
        serialSIM800.print(" kg");      
        serialSIM800.println();
        
        serialSIM800.print("wilgotnosc wew:");
        serialSIM800.print(wilgotnosc1String);
        serialSIM800.print(" %");
        serialSIM800.println();

        serialSIM800.print("temperatura wew:");
        serialSIM800.print(temperatura1String);
        serialSIM800.print(" C");
        serialSIM800.println();

        serialSIM800.print("wilgotnosc zew:");
        serialSIM800.print(wilgotnosc2String);
        serialSIM800.print(" %");
        serialSIM800.println();

        serialSIM800.print("temperatura zew:");
        serialSIM800.print(temperatura2String);
        serialSIM800.print(" C");
        serialSIM800.println();

        serialSIM800.print("Akumulator:");
        serialSIM800.print(voltageString);
        serialSIM800.print(" V");
        serialSIM800.println();

        if (voltage < 5.8){
          serialSIM800.println("KONIECZNE LADOWANIE !");
        }

        delay(200);
        //Znak końca wiadomości sms
        serialSIM800.write((char)26);
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
  DEBUG?Serial.print(voltage):true;
  DEBUG?Serial.println("V"):true;
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
