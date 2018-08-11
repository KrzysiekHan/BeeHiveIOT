/*
Biblioteka dla modułu HX711 : https://github.com/bogde/HX711

Połączenie Arduino i modułu HX711
 2 -> HX711 CLK
 3 -> DOUT


*/

#include "HX711.h"

#define calibration_factor 14850.0 //Wartość współczynnika skalowania dla tensometru (dostosowanego w programie kalibracyjnym)
#define zero_factor 8457924 //Wartość współczynnika zera (potrzebne jeśli na wadze cały czas znajduje się obciążenie)

#define DOUT  3
#define CLK  2

HX711 scale(DOUT, CLK);

void setup() {
  
  Serial.begin(9600);//inicjalizacja dla debugowania w konsoli
  scale.set_scale(calibration_factor); //This value is obtained by using the SparkFun_HX711_Calibration sketch
  scale.set_offset(zero_factor); //Zero out the scale using a previously known zero_factor

}

void loop() {
  Serial.print("Odczyt: ");
  Serial.print(scale.get_units(), 1); //scale.get_units() returns a float
  Serial.print(" kg"); //You can change to kg but you'll need to change the calibration_factor
  Serial.println();
  delay(10000);
}
