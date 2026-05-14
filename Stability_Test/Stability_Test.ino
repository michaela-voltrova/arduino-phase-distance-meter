/*================================================================================= 
  KOMPLETNÍ TEST STABILITY
  Hardware: Arduino MEGA, AD9850 (DDS), AD8302 (Fázový detektor), ADS1115 (16-bit ADC)
  Metoda: Pro každou frekvenci (1.5 MHz až 30 MHz) provést:
       50x Restart generátoru -> 100x Měření napětí
  ================================================================================= */

#include <AD9850.h>
#include <Adafruit_ADS1X15.h> // Knihovna pro přesný 16bit převodník na MEGA
#include <Wire.h>

// --- PINY ---
#define W_CLK_PIN 7
#define FU_UD_PIN 8
#define DATA_PIN 9
#define RESET_PIN 10

// --- NASTAVENÍ TESTU ---
const int NUM_CYCLES = 50;           // Kolikrát restartovat generátor
const int SAMPLES_PER_CYCLE = 100;   // Kolik měření v jednom cyklu

// --- TABULKA FREKVENCÍ (ROZŠÍŘENÁ) ---
const int NUM_FREQS = 8;
double freq_table[NUM_FREQS] = {
  1500000.0,  // 1.5 MHz
  5000000.0,  // 5.0 MHz
  8000000.0,  // 8.0 MHz
  12000000.0, // 12.0 MHz
  16000000.0, // 16.0 MHz
  20000000.0, // 20.0 MHz
  25000000.0, // 25.0 MHz 
  30000000.0  // 30.0 MHz 
};

// Instance
AD9850 myDDS;
Adafruit_ADS1115 ads; 

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Inicializace převodníku
  if (!ads.begin()) {
    Serial.println(F("Převodník ADS1115 nenalezen! Zkontroluj I2C piny (SDA=20, SCL=21)."));
    while (1); 
  }
  ads.setGain(GAIN_ONE); // Rozsah +- 4.096 V
  
  myDDS.begin(W_CLK_PIN, FU_UD_PIN, DATA_PIN, RESET_PIN);
  myDDS.calibrate(124999500); // Taktovací frekvence krystalu generátoru - PO JEHO URČENÍ POMOCÍ SPEKTRÁLNÍHO ANALYZÁTORU

  Serial.println(F("=== START KOMPLETNIHO TESTU STABILITY ==="));
  Serial.println(F("Format: Frekvence_MHz;Cyklus_Restartu;Cislo_Vzorku;Napeti_V"));

  // Spustíme velký test
  runMultiFreqTest();
}

void loop() {
  // Prázdné, test jede jen jednou
}

void runMultiFreqTest() {
  for (int f = 0; f < NUM_FREQS; f++) {
    double currentFreq = freq_table[f];
    
    Serial.print(F(">>> TESTUJI FREKVENCI: "));
    if (currentFreq < 1000000.0) {
      Serial.print(currentFreq / 1000.0, 0);
      Serial.println(F(" kHz <<<"));
    } else {
      Serial.print(currentFreq / 1e6, 1);
      Serial.println(F(" MHz <<<"));
    }
    
    for (int cycle = 1; cycle <= NUM_CYCLES; cycle++) {
      
      // A) HARDWAROVÝ RESET
      digitalWrite(RESET_PIN, HIGH);
      delay(5);
      digitalWrite(RESET_PIN, LOW);
      delay(5);
      
      // B) NASTAVENÍ FREKVENCE
      myDDS.setfreq((long)currentFreq, 0);
      delay(50); 
      
      // C) MĚŘENÍ
      for (int sample = 1; sample <= SAMPLES_PER_CYCLE; sample++) {
        
        double voltage = ads.computeVolts(ads.readADC_SingleEnded(0));

        // Výpis dat
        Serial.print(currentFreq / 1e6, 4); 
        Serial.print(";");
        Serial.print(cycle);               
        Serial.print(";");
        Serial.print(sample);              
        Serial.print(";");
        Serial.println(voltage, 5);        
        
        delay(2); 
      }
    }
    Serial.println("------------------------------------------------");
  }

  Serial.println(F("=== VSECHNY TESTY DOKONCENY ==="));
}