# arduino-phase-distance-meter
Repository for the bachelor's thesis "Phase Distance Meter Model on the Arduino Platform" (CTU in Prague). Contains source codes and oscilloscope data analysis.
# OBSAH PROJEKTU

## arduino_phase_distance_meter – Hlavní kód pro měření vzdálenosti

Hardware: Arduino MEGA, AD9850 (DDS), AD8302 (Fázový detektor), ADS1115 (16-bit ADC)

### **Návod k obsluze** 
Pro ovládání dálkoměru se využívá Sériový monitor v Arduino IDE. Je nutné nastavit komunikační rychlost na 115200 baudů.  
Systém po spuštění zkontroluje přítomnost kalibračních dat v paměti EEPROM. Pokud jsou data nalezena, je dálkoměr okamžitě připraven k měření. V opačném případě vás systém vyzve k prvotní kalibraci. Kalibraci je vhodné provádět před každým měřením kvůli změnám okolního prostředí.

Ovládání pomocí příkazů přes Sériový monitor:

*KALIBRACE*

Tento příkaz spustí sekvenci pro zjištění hardwarových ofsetů měřicího vedení. Před jeho odesláním je nutné připojit kalibrační kabel o přesné délce 0,2 m. Systém změří ofsety pro všech 8 frekvencí a hodnoty trvale uloží do EEPROM paměti.

*MERENI (nebo stisk klávesy ENTER)*

Příkaz pro spuštění hlavního měřicího algoritmu. Systém nejprve změří absolutní vzdálenost pomocí kotevní frekvence 5 MHz a následně pomocí rozbalování fáze dohledá přesnou délku. Výsledek je spřesněn robustní shlukovou analýzou (s tolerancí 15 cm) pro eliminaci chyb a odrazů. Na výstupu Sériového monitoru se následně zobrazí finální přesná délka připojeného kabelu.

## Vysledky_mereni - Výsledky měření
Tato složka obsahuje složku s naměřenými daty před optimalizací, a složku a naměřenými daty po optimalizaci systému 
### **mereni_pred_optimalizaci** 
1 × kalibrace.txt

7 × kabel_*_m.txt - měření daného kabelu

### **mereni_po_optimalizaci** 
1 × kalibrace.txt

7 × kabel_*_m.txt - měření daného kabelu


## Stability_Test - Kód pro zištění stability systému

Hardware: Arduino MEGA, AD9850 (DDS), AD8302 (Fázový detektor), ADS1115 (16-bit ADC)

Kód automaticky projde 8 frekvencí v rozsahu od 1,5 MHz do 30 MHz. Pro každou frekvenci je provedeno 50 hardwarových restartů generátoru, po každém restartu je naměřeno 100 vzorků napětí.

## Osciloskop - Výsledky z analýzy generátoru osciloskopem

Obsahuje výsledky z analýzy generátoru.

6 × Analyza_Trojgraf_zatez_*Hz.png - Měřený signál se zátěží 50 Ω 

8 × Analyza_Trojgraf_*Hz.png -  Měřený signál bez zátěže

