/* ================================================================================= 
   FÁZOVÝ DÁLKOMĚR
   Hardware: Arduino MEGA, AD9850 (DDS), AD8302 (Fázový detektor), ADS1115 (16-bit ADC)
   Metoda: Ideální hodnoty AD8302 (10 mV/stupeň) + Zjištění ofsetu HW pro každou frekvenci
  ================================================================================= */

#include <AD9850.h>           
#include <EEPROM.h>           
#include <Adafruit_ADS1X15.h> 
#include <math.h>             

// --- DEFINICE PINŮ PRO KOMUNIKACI S DDS GENERÁTOREM ---
#define W_CLK_PIN 7     
#define FU_UD_PIN 8     
#define DATA_PIN 9      
#define RESET_PIN 10    

// --- FYZIKÁLNÍ KONSTANTY VEDENÍ ---
const double trimFreq = 124999500;  
const double c0 = 299792458.0;      
const double VF = 0.82;             

// =================================================================================
// --- IDEÁLNÍ TEORETICKÉ HODNOTY Z DATASHEETU AD8302 ---
// Neřešíme V0 ani citlivost pro každou frekvenci, důvěřujeme čisté teorii.
const double THEO_V0 = 0.000;       // Ideální napětí při 0° fázovém posunu
const double THEO_SENS = 0.010;     // Ideální citlivost 10 mV / stupeň
// =================================================================================

// Kabel pro zjištění HW ofsetu
const double LEN_REF_m = 0.200;   

// --- FREKVENČNÍ TABULKA ---
const int NUM_FREQS = 8;              
double freq_table[NUM_FREQS] = {      
  1500000.0,  
  5000000.0,  // PEVNÁ KOTVA
  8000000.0,  
  12000000.0, 
  16000000.0, 
  20000000.0, 
  25000000.0, 
  30000000.0  
}; 

// Pole pro uložení HW ofsetu pro každou frekvenci zvlášť
float HW_OFFSET_TABLE[NUM_FREQS]; 
bool calibrationValid = false;        
bool paused = true;                   

Adafruit_ADS1115 ads;                 
AD9850 myDDS;                         

// Magické číslo změněno pro vynucení nové kalibrace
const uint32_t EEPROM_MAGIC = 0xAD9850E2; 

// ================= FUNKCE PRO PRÁCI S EEPROM =================
// Nyní ukládáme pole 8 hodnot (HW ofset pro každou frekvenci)
void writeFloat(int addr, float val) {
  uint8_t *p = (uint8_t*)&val;        
  for(int i=0; i<4; i++) EEPROM.write(addr+i, p[i]); 
}

float readFloat(int addr) {
  float val; uint8_t *p = (uint8_t*)&val;
  for(int i=0; i<4; i++) p[i] = EEPROM.read(addr+i); 
  return val;                                        
}

void saveCal() {
  uint32_t mag = EEPROM_MAGIC; uint8_t *p = (uint8_t*)&mag;
  for(int i=0; i<4; i++) EEPROM.write(i, p[i]);      
  // Uložení ofsetů pro všechny frekvence (od adresy 4, každá hodnota 4 bajty)
  for(int i=0; i<NUM_FREQS; i++) {
     writeFloat(4 + (i * 4), HW_OFFSET_TABLE[i]);
  }
}

bool loadCal() {
  uint32_t mag; uint8_t *p = (uint8_t*)&mag;
  for(int i=0; i<4; i++) p[i] = EEPROM.read(i);      
  if(mag != EEPROM_MAGIC) return false;             
  // Načtení ofsetů pro všechny frekvence
  for(int i=0; i<NUM_FREQS; i++) {
     HW_OFFSET_TABLE[i] = readFloat(4 + (i * 4));
  }
  return true; 
}

void flushSerial() { while(Serial.available()) Serial.read(); }

void setFreq(double MHz) {
  myDDS.setfreq((long)(MHz * 1e6), 0); 
  delay(100); 
}

double readAverageVoltage() {
  double sum = 0;
  for(int i=0; i<100; i++) { 
    sum += ads.computeVolts(ads.readADC_SingleEnded(0)); 
    delay(2); 
  }
  return sum / 100.0;                                      
}

// ================= EXPERIMENTÁLNÍ KALIBRACE HW OFSETŮ =================
void calibrateAll() {
  Serial.println(F("\n=== ZJIŠŤOVÁNÍ HW OFSETŮ PRO VŠECHNY FREKVENCE ==="));
  Serial.print(F("Připoj referenční kabel (")); Serial.print(LEN_REF_m); Serial.println(F(" m) a stiskni ENTER."));
  while(!Serial.available()); flushSerial(); 
  Serial.println(F("Měřím napětí a odvozuji parazitní HW ofsety z datasheetových hodnot..."));

  for (int i=0; i<NUM_FREQS; i++) {
    setFreq(freq_table[i]/1e6);              // nastavení frekvence generátoru
    double V_meas = readAverageVoltage();    // průměr 100 vzorků z ADS1115
    
    // převod napětí na fázový posun pomocí datasheetových konstant
    double phase = abs(THEO_V0 - V_meas) / THEO_SENS;  // [°]
    if (phase > 180.0) phase = 180.0;                  // naměřený fázový rozdíl nemůže být větší než jsou limity AD8302

    // výpočet délky odpovídající naměřené fázi
    double lambda = c0 / freq_table[i];
    double L_raw = (phase / 360.0) * (lambda * VF); // [m]
    
    // HW ofset = rozdíl vypočtené délky od skutečné délky kal. kabelu
    HW_OFFSET_TABLE[i] = L_raw - LEN_REF_m;

    //výpis výsledků 
    Serial.print(F("[")); Serial.print(freq_table[i]/1e6, 1); Serial.print(F(" MHz] "));
    Serial.print(F("Napětí = ")); Serial.print(V_meas, 4); Serial.print(F(" V | "));
    Serial.print(F("Délka z datasheetu = ")); Serial.print(L_raw, 3); Serial.print(F(" m | "));
    Serial.print(F("=> HW OFSET = ")); Serial.print(HW_OFFSET_TABLE[i], 3); Serial.println(F(" m"));
  }
  // zápis ofsetů do EEPROM
  saveCal(); 
  calibrationValid = true; 
  Serial.println(F("\n Uloženo do EEPROM. Napiš 'MERENI'"));
}

void setup() {
  Serial.begin(115200); 
  Serial.println(F("Startuji systém (MEGA Edition + FREKVENČNÍ HW OFSETY EXPERIMENT)..."));
  
  if (!ads.begin()) { Serial.println(F(" ADS1115 nenalezen!")); while (1); }
  ads.setGain(GAIN_ONE); 
  
  myDDS.begin(W_CLK_PIN, FU_UD_PIN, DATA_PIN, RESET_PIN); 
  myDDS.calibrate(trimFreq); 
  myDDS.up();
  
  if(loadCal()) { 
      calibrationValid = true; 
      Serial.println(F(" HW OFSETY úspěšně načteny. Napiš 'MERENI'.")); 
  } else { 
      Serial.println(F(" Paměť prázdná nebo změna formátu! Napiš 'KALIBRACE'")); 
  }
}

// ================= HLAVNÍ SMYČKA ALGORITMU PRO MĚŘENÍ =================
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n'); cmd.trim(); cmd.toUpperCase();
    if (cmd == "KALIBRACE") calibrateAll();
    if (cmd == "MERENI" || cmd == "") paused = false; 
  }
  if (paused || !calibrationValid) return;

  Serial.println(F("\n========================================================"));
  Serial.println(F("=== MĚŘENÍ: PEVNÁ KOTVA + LOKÁLNÍ HW OFSETY PŘÍVODŮ ==="));
  Serial.println(F("========================================================"));

  double namerene_delky[NUM_FREQS]; 
  double L_approx = 0; 
  
  // -------------------------------------------------------------------------
  // KROK 1: MĚŘENÍ ABSOLUTNÍ KOTVY (5.0 MHz) A ODEČTENÍ LOKÁLNÍHO OFSETU
  // -------------------------------------------------------------------------
  Serial.println(F("\n>>> KROK 1: MĚŘENÍ ABSOLUTNÍ START_KOTVY (5.0 MHz) <<<"));
  setFreq(freq_table[1]/1e6); 
  double V_meas_1 = readAverageVoltage();
  
  double phase_1 = abs(THEO_V0 - V_meas_1) / THEO_SENS;
  if (phase_1 > 180.0) phase_1 = 180.0; 
  
  double lambda_kotvy = c0 / freq_table[1];
  double delka_vlny_kotvy = lambda_kotvy * VF;
  
  double L_raw_1 = (phase_1 / 360.0) * delka_vlny_kotvy; 
  L_approx = L_raw_1 - HW_OFFSET_TABLE[1]; // Odečteme ofset uložený pro index 1 (5 MHz)

  //výpis výsledků
  Serial.print(F("  -> Napětí = ")); Serial.print(V_meas_1, 4); Serial.println(F(" V")); 
  Serial.print(F("  -> Surová délka (datasheet) = ")); Serial.println(L_raw_1, 3);
  Serial.print(F("  -> Odečten lokální HW ofset = -")); Serial.println(HW_OFFSET_TABLE[1], 3);
  Serial.print(F("  ===> FINÁLNÍ L_approx (KOTVA) = ")); Serial.print(L_approx, 3); Serial.println(F(" m"));
  
  namerene_delky[1] = L_approx;

  // -------------------------------------------------------------------------
  // KROK 2: ROZBALOVÁNÍ FÁZE A HW OFSET OSTATNÍCH FREKVENCÍ
  // -------------------------------------------------------------------------
  Serial.println(F("\n>>> KROK 2: JEMNÉ MĚŘENÍ A HLEDÁNÍ NÁSOBKŮ <<<"));
  for (int i=0; i<NUM_FREQS; i++) { 
    if (i == 1) continue; 
    double f = freq_table[i];
    double lambda = c0 / f;
    double delka_vlny_kabelu = lambda * VF; 
    double bod_zlomu = delka_vlny_kabelu / 2.0; //bod zlomu pro představu 
    
    Serial.print(F("\n[")); Serial.print(f/1e6, 1); Serial.println(F(" MHz]"));
    Serial.print(F("  -> Bod zlomu (180°): ")); Serial.print(bod_zlomu, 3); Serial.println(F(" m"));
  
    // upozornění že by se měření mohlo nacázet blízko zóny kde AD8302 měří hůře 
    double predikovana_faze = fmod((L_approx / delka_vlny_kabelu) * 360.0, 180.0);
    if (predikovana_faze < 15.0 || predikovana_faze > 165.0) {
       Serial.print(F(" UPOZORNĚNÍ: Senzor je blízko slepé zóny (U/V ohyb). Predikce fáze: ")); Serial.print(predikovana_faze, 1); Serial.println(F("°"));
    }
    //měření napětí
    setFreq(f/1e6); 
    double V_meas = readAverageVoltage(); 
    
    // používání ideálních datasheetových konstant
    double phase = abs(THEO_V0 - V_meas) / THEO_SENS;
    if (phase > 180.0) phase = 180.0; 
    //výpis naměřeného napětí a vypočteného posunu fáze pro přehlednost
    Serial.print(F("  -> Napětí = ")); Serial.print(V_meas, 4); Serial.print(F(" V | Fáze = ")); Serial.print(phase, 1); Serial.println(F("°"));

    // proměnné pro uložení vítězného výsledku 
    // (počáteční chyba nastavena vysoko, aby se vždy přepsala realitou)
    double best_L_raw = 0; double min_err_raw = 99999.0; int best_N = 0; char best_typ = 'A';
    
    for(int N=0; N<20; N++) { 
      double L_candidate_A = ((N * 360.0) + phase) / 360.0 * delka_vlny_kabelu;
      double L_candidate_B = ((N * 360.0) + (360.0 - phase)) / 360.0 * delka_vlny_kabelu;
      
      // odečtení lokálních ofset této frekvence
      double err_A = abs((L_candidate_A - HW_OFFSET_TABLE[i]) - L_approx);
      double err_B = abs((L_candidate_B - HW_OFFSET_TABLE[i]) - L_approx);

      if(err_A < min_err_raw) { min_err_raw = err_A; best_L_raw = L_candidate_A; best_N = N; best_typ = 'A'; }
      if(err_B < min_err_raw) { min_err_raw = err_B; best_L_raw = L_candidate_B; best_N = N; best_typ = 'B'; }
    }

    // finální délka
    double best_L_final = best_L_raw - HW_OFFSET_TABLE[i]; 
    //výpis výsledků
    Serial.print(F("  ===> Zvolena surová délka [N=")); Serial.print(best_N); Serial.print(F(", Svah ")); 
    Serial.print(best_typ); Serial.print(F("]: ")); Serial.println(best_L_raw, 3);
    Serial.print(F("  ===> ČISTÁ DÉLKA KABELU (surová - HW ofset z kalibrace): ")); Serial.println(best_L_final, 3);
    
    namerene_delky[i] = best_L_final; 
  }

  // -------------------------------------------------------------------------
  // KROK 3: ZÁVĚREČNÁ STATISTIKA (ROBUSTNÍ SHLUKOVÁ ANALÝZA)
  // -------------------------------------------------------------------------
  Serial.println(F("\n========================================================"));
  Serial.println(F("--- ZÁVĚREČNÁ STATISTIKA: ROBUSTNÍ SHLUKOVÁ ANALÝZA ---"));
  
  double TOLERANCE = 0.15; // Mírná tolerance pro shlukování (15 cm)
  int max_cluster_count = 0;  //pole pro počet frekvenci ve shluhu
  double best_cluster_avg = 0; //pole pro výsledný průměr
  bool final_cluster_mask[NUM_FREQS] = {false};

  for(int i=0; i<NUM_FREQS; i++) {           // Každá jedna frekvence vytvoří svůj shluk
      if(namerene_delky[i] <= 0.0) continue;
      
      double current_center = namerene_delky[i]; // Daná frekvence prohlásí svou délku za centrum shluku
      int current_count = 0;  //pole pro počítání členů
      double current_sum = 0;  //pole pro průmer délek 
      bool current_mask[NUM_FREQS] = {false};
      // porovnání se všemi ostatními pokud do 15 cm přijmout
      for(int j=0; j<NUM_FREQS; j++) {
          if(namerene_delky[j] > 0.0 && abs(namerene_delky[j] - current_center) <= TOLERANCE) {
              current_count++;
              current_sum += namerene_delky[j];
              current_mask[j] = true;
          }
      }
      
      double current_avg = current_sum / current_count; //průměr tohoto konkrétního shluku
      bool is_better = false;  // určení, zda je tento shluk lepší než dosavadní shluk
      
      if(current_count > max_cluster_count) {  // pokud je více členů je lepší
          is_better = true;
      } 
      else if (current_count == max_cluster_count && max_cluster_count > 0) { // pokud stejně členů je lepší ten co je blíže ke kotvě 
          if(abs(current_avg - L_approx) < abs(best_cluster_avg - L_approx)) {
              is_better = true;
          }
      }
      
      if(is_better) {   // zapsání shluku 
          max_cluster_count = current_count;
          best_cluster_avg = current_avg;
          for(int k=0; k<NUM_FREQS; k++) final_cluster_mask[k] = current_mask[k];
      }
  }
  // prohledání všech shluků a výpis vítěze
  if(max_cluster_count > 0) {
      Serial.print(F("-> Nalezen vítězný shluk (shoda ")); Serial.print(max_cluster_count); Serial.println(F(" frekvencí):"));
      Serial.print(F("   Použito: "));
      
      double final_sum = 0;     //pole pro finální, nejpřesnější součet
      for(int i=0; i<NUM_FREQS; i++) {
          if(final_cluster_mask[i]) { //projde final_cluster_mask
              Serial.print(namerene_delky[i], 3); Serial.print(F(" m (")); Serial.print(freq_table[i]/1e6, 1); Serial.print(F(" MHz) | "));
              final_sum += namerene_delky[i];
          }
      }
      // výpis výsledků
      Serial.println(F("\n\n========================================================"));
      Serial.print(F(" KONEČNÁ PŘESNÁ DÉLKA KABELU: ")); Serial.print(final_sum / max_cluster_count, 4); Serial.println(F(" m"));
      Serial.println(F("========================================================"));
  } else { //nenalezen žádný shluk do 15 cm
      Serial.println(F("\n CHYBA: Žádná shoda nenalezena. Data jsou masivně zarušená odrazy."));
  }
  //cekový konec měření 
  paused = true; Serial.println(F("\nStiskni ENTER pro nové měření."));
}