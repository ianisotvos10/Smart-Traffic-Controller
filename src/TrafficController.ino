#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ================= 1. DEFINIRE PINI =================

// Semafor S1 (Banda Principala)
const int S1_VERDE = 2;
const int S1_ROSU = 3;    
const int S1_GALBEN = 4;  

// Semafor S2 (Banda Laterala)
const int S2_ROSU_PIN = A1;  
const int S2_VERDE_PIN = A2; 

// Semafor Pietoni
const int PED_VERDE = 12;
const int PED_ROSU = 13;

// Radar IR (Start/Stop)
const int RADAR_IR_START = 8; 
const int RADAR_IR_STOP = 9;  

// Radar Ultrasonic S1 (Inversat: 6 si 5)
const int TRIG_S1 = 6;
const int ECHO_S1 = 5;

// Radar Ultrasonic S2 (Inversat: 10 si 11)
const int TRIG_S2 = 10;
const int ECHO_S2 = 11;

// Altele
const int PIN_TOUCH = 7;
const int PIN_BUZZER = A3;

LiquidCrystal_I2C lcd(0x27, 16, 2); 

// ================= 2. SETARI & CALIBRARE (AICI MODIFICI!) =================

// --- CALIBRARE IR (Foarte Important!) ---
// Masoara cu rigla distanta dintre senzorul Start si Stop
const float DISTANTA_FIZICA_IR = 0.05; // 0.05 inseamna 5 cm. MODIFICA DACA E ALTFEL!

// --- SCALARE (Multiplicatori de viteza pt macheta) ---
const int SCARA_ULTRASONIC = 12; // Scara mai mica pt stabilitate
const int SCARA_IR = 40;         // Scara pt IR (daca distanta e corecta, nu trebe imensa)

// --- LIMITARE DISTANTA (Ca sa nu vada peretii) ---
const int DIST_MAX_S1 = 20; // cm
const int DIST_MAX_S2 = 20; // cm

// --- FILTRE ---
const float LIMITA_VITEZA = 30.0;     
const int PRAG_MINIM_MISCARE_CM = 4; // Ignoram tremuratul sub 4cm

// --- TIMPI ---
const unsigned long INTERVAL_SCHIMBARE_FAZA = 10000; 
const int INTERVAL_CHECK = 100; 

// ================= 3. VARIABILE SISTEM =================
unsigned long timerCiclu = 0;   
bool s1EsteVerde = true;        
bool cicluUrgentaActiv = false; 

// Variabile Senzori
unsigned long startMicro = 0;
bool masurareIrActiva = false;

long distVecheS1 = 0, distVecheS2 = 0;
unsigned long timpVechiS1 = 0, timpVechiS2 = 0;

// Variabile Afisaj
int vitezometruS1_IR = 0; 
int vitezometruS1_US = 0; 
int vitezometruS2 = 0;    

unsigned long timpUltimaMiscareS1_IR = 0;
unsigned long timpUltimaMiscareS1_US = 0;
unsigned long timpUltimaMiscareS2 = 0;

void setup() {
  Serial.begin(9600);
  lcd.init(); lcd.backlight();
  
  pinMode(S1_ROSU, OUTPUT); pinMode(S1_GALBEN, OUTPUT); pinMode(S1_VERDE, OUTPUT);
  pinMode(S2_ROSU_PIN, OUTPUT); pinMode(S2_VERDE_PIN, OUTPUT);
  pinMode(PED_ROSU, OUTPUT); pinMode(PED_VERDE, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  
  pinMode(TRIG_S1, OUTPUT); pinMode(ECHO_S1, INPUT);
  pinMode(TRIG_S2, OUTPUT); pinMode(ECHO_S2, INPUT);
  pinMode(RADAR_IR_START, INPUT); pinMode(RADAR_IR_STOP, INPUT);
  pinMode(PIN_TOUCH, INPUT);

  lcd.print("Sistem Calibrat");
  lcd.setCursor(0, 1); lcd.print("IR Dist: "); lcd.print((int)(DISTANTA_FIZICA_IR*100)); lcd.print("cm");
  delay(2000); lcd.clear();
  
  distVecheS1 = citesteDistanta(TRIG_S1, ECHO_S1);
  distVecheS2 = citesteDistanta(TRIG_S2, ECHO_S2);
  timpVechiS1 = millis();
  timpVechiS2 = millis();
  
  fazaS1Verde(); 
  timerCiclu = millis();
  
  updateEcranViteze(); 
}

void loop() {
  ruleazaSenzori(); // Masurare continua

  if (cicluUrgentaActiv) return; 

  unsigned long now = millis();

  // Ciclu Automat
  if (now - timerCiclu > INTERVAL_SCHIMBARE_FAZA) {
    schimbaFazaAutomata();
    timerCiclu = millis(); 
    // Resetare distante pt a preveni erori la schimbare
    distVecheS1 = citesteDistanta(TRIG_S1, ECHO_S1);
    distVecheS2 = citesteDistanta(TRIG_S2, ECHO_S2);
  }
}

// ================= 4. PROCESARE SENZORI =================

void asteaptaSiMasoara(unsigned long durata) {
  unsigned long start = millis();
  while (millis() - start < durata) {
    ruleazaSenzori(); 
  }
}

void ruleazaSenzori() {
  unsigned long now = millis();

  // --- A. SENZOR IR S1 (Corectat) ---
  if (digitalRead(RADAR_IR_START) == HIGH && !masurareIrActiva) {
      startMicro = micros(); masurareIrActiva = true;
  }
  if (digitalRead(RADAR_IR_STOP) == HIGH && masurareIrActiva) {
      unsigned long durata = micros() - startMicro;
      masurareIrActiva = false;
      
      // Filtru erori foarte mici (sub 0.05 secunde)
      if (durata > 50000) { 
        // AICI ESTE CALCULUL CORECTAT CU DISTANTA FIZICA:
        float vRaw = (DISTANTA_FIZICA_IR / (durata / 1000000.0)) * 3.6; 
        int vFinal = (int)(vRaw * SCARA_IR);
        
        vitezometruS1_IR = vFinal; 
        timpUltimaMiscareS1_IR = now;
        updateEcranViteze();    
        
        if (s1EsteVerde && vFinal > LIMITA_VITEZA) penalizareS1("! VITEZA IR S1 !");
      }
  }

  // --- B. SENZOR ULTRASONIC S1 ---
  if (!masurareIrActiva && (now - timpVechiS1 >= INTERVAL_CHECK)) {
      long distNoua = citesteDistanta(TRIG_S1, ECHO_S1);
      
      if (distNoua > 0 && distNoua < DIST_MAX_S1 && distNoua < distVecheS1) {
        long deltaD = distVecheS1 - distNoua;
        
        if (deltaD >= PRAG_MINIM_MISCARE_CM) {
          int vS1 = (int)((((float)deltaD / (now - timpVechiS1)) * 36.0) * SCARA_ULTRASONIC);
          
          if (vS1 > 5 && vS1 < 300) {
             vitezometruS1_US = vS1; 
             timpUltimaMiscareS1_US = now;
             updateEcranViteze();
             if (s1EsteVerde && vS1 > LIMITA_VITEZA) penalizareS1("! VITEZA US S1 !");
          }
        }
      }
      // Actualizam doar daca suntem in raza sau aproape
      if (distNoua < DIST_MAX_S1 + 10) distVecheS1 = distNoua; 
      timpVechiS1 = now;
  }

  // --- C. SENZOR ULTRASONIC S2 ---
  if (now - timpVechiS2 >= INTERVAL_CHECK) {
      long distNoua = citesteDistanta(TRIG_S2, ECHO_S2);
      
      if (distNoua > 0 && distNoua < DIST_MAX_S2 && distNoua < distVecheS2) {
        long deltaD = distVecheS2 - distNoua;
        
        if (deltaD >= PRAG_MINIM_MISCARE_CM) {
          int vS2 = (int)((((float)deltaD / (now - timpVechiS2)) * 36.0) * SCARA_ULTRASONIC);
          
          if (vS2 > 5 && vS2 < 300) {
             vitezometruS2 = vS2;
             timpUltimaMiscareS2 = now;
             updateEcranViteze();
             if (!s1EsteVerde && vS2 > LIMITA_VITEZA) penalizareS2("! VITEZA US S2 !");
          }
        }
      }
      if (distNoua < DIST_MAX_S2 + 10) distVecheS2 = distNoua;
      timpVechiS2 = now;
  }

  // --- RESETARE AUTOMATA (Auto-Clear) ---
  if (now - timpUltimaMiscareS1_IR > 1000 && vitezometruS1_IR != 0) {
    vitezometruS1_IR = 0; updateEcranViteze();
  }
  if (now - timpUltimaMiscareS1_US > 1000 && vitezometruS1_US != 0) {
    vitezometruS1_US = 0; updateEcranViteze();
  }
  if (now - timpUltimaMiscareS2 > 1000 && vitezometruS2 != 0) {
    vitezometruS2 = 0; updateEcranViteze();
  }

  // --- BUTON PIETONI ---
  if (digitalRead(PIN_TOUCH) == HIGH && !cicluUrgentaActiv) {
    activarePietoniExclusiv(); 
  }
}

// ================= 5. LOGICA AFISARE & ACTIUNI =================

void updateEcranViteze() {
  lcd.setCursor(0, 0); 
  lcd.print("IR:"); lcd.print(vitezometruS1_IR); 
  lcd.print(" US:"); lcd.print(vitezometruS1_US);
  lcd.print("   "); 
  
  lcd.setCursor(0, 1);
  lcd.print("S2: "); lcd.print(vitezometruS2); lcd.print(" km/h    ");
}

void activarePietoniExclusiv() {
  cicluUrgentaActiv = true;
  lcd.clear(); 
  lcd.setCursor(0, 0); lcd.print("! PIETONI !");
  lcd.setCursor(0, 1); lcd.print("STOP TRAFIC");
  tone(PIN_BUZZER, 2000, 500);
  delay(1000); 

  // Oprim pe cine era Verde
  if (s1EsteVerde) {
     digitalWrite(S1_VERDE, LOW); digitalWrite(S1_GALBEN, HIGH); 
     delay(2000); 
     digitalWrite(S1_GALBEN, LOW); digitalWrite(S1_ROSU, HIGH); 
  } else {
     controlS2(3); delay(2000);
     controlS2(1); 
  }
  
  // Asigurare: TOTUL ROSU
  digitalWrite(S1_ROSU, HIGH); controlS2(1); 

  // Afisare
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("S1:ROSU S2:ROSU");
  lcd.setCursor(0, 1); lcd.print("PIETONI: VERDE ");
  
  // Verde Pietoni
  digitalWrite(PED_ROSU, LOW); digitalWrite(PED_VERDE, HIGH);

  // Timp trecere (6 sec)
  for(int i=0; i<6; i++) { tone(PIN_BUZZER, 800, 100); delay(900); }

  // Inchidere Pietoni
  for(int i=0; i<3; i++) {
    digitalWrite(PED_VERDE, LOW); delay(300);
    digitalWrite(PED_VERDE, HIGH); delay(300);
  }
  digitalWrite(PED_VERDE, LOW); digitalWrite(PED_ROSU, HIGH);
  
  delay(1000); 
  fazaS1Verde();
  
  timerCiclu = millis();
  cicluUrgentaActiv = false;
  updateEcranViteze();
}

void schimbaFazaAutomata() {
  if (s1EsteVerde) {
    digitalWrite(S1_VERDE, LOW); digitalWrite(S1_GALBEN, HIGH); 
    asteaptaSiMasoara(2000); 
    digitalWrite(S1_GALBEN, LOW); digitalWrite(S1_ROSU, HIGH); 
    asteaptaSiMasoara(1000); 
    fazaS2Verde(); 
  } else {
    controlS2(3); asteaptaSiMasoara(2000);
    controlS2(1); asteaptaSiMasoara(1000);
    fazaS1Verde(); 
  }
}

void fazaS1Verde() {
  digitalWrite(S1_ROSU, LOW); digitalWrite(S1_GALBEN, LOW); digitalWrite(S1_VERDE, HIGH);
  controlS2(1); 
  digitalWrite(PED_VERDE, LOW); digitalWrite(PED_ROSU, HIGH);
  s1EsteVerde = true;
}

void fazaS2Verde() {
  digitalWrite(S1_VERDE, LOW); digitalWrite(S1_GALBEN, LOW); digitalWrite(S1_ROSU, HIGH);
  controlS2(2); 
  digitalWrite(PED_ROSU, LOW); digitalWrite(PED_VERDE, HIGH);
  s1EsteVerde = false;
}

void penalizareS1(String motiv) {
  cicluUrgentaActiv = true;
  lcd.clear(); lcd.print(motiv);
  tone(PIN_BUZZER, 1000, 1000); 
  delay(1000); 
  updateEcranViteze();

  digitalWrite(S1_VERDE, LOW); digitalWrite(S1_GALBEN, HIGH); delay(2000);
  digitalWrite(S1_GALBEN, LOW); digitalWrite(S1_ROSU, HIGH); delay(1000);

  fazaS2Verde();

  for(int i=0; i<5; i++) { tone(PIN_BUZZER, 500, 100); delay(900); }

  controlS2(3); delay(2000); 
  controlS2(1); delay(1000); 

  fazaS1Verde();
  timerCiclu = millis();
  cicluUrgentaActiv = false;
  updateEcranViteze();
}

void penalizareS2(String motiv) {
  cicluUrgentaActiv = true;
  lcd.clear(); lcd.print(motiv);
  tone(PIN_BUZZER, 1000, 1000); 
  delay(1000);
  updateEcranViteze();

  controlS2(3); delay(2000);
  controlS2(1); delay(1000);

  fazaS1Verde();
  delay(5000); 

  timerCiclu = millis();
  cicluUrgentaActiv = false;
  updateEcranViteze();
}

void controlS2(int culoare) {
  if (culoare == 1) { digitalWrite(S2_ROSU_PIN, HIGH); digitalWrite(S2_VERDE_PIN, LOW); }
  if (culoare == 2) { digitalWrite(S2_ROSU_PIN, LOW); digitalWrite(S2_VERDE_PIN, HIGH); }
  if (culoare == 3) { digitalWrite(S2_ROSU_PIN, HIGH); digitalWrite(S2_VERDE_PIN, HIGH); } 
}

long citesteDistanta(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2); digitalWrite(trig, HIGH); delayMicroseconds(10); digitalWrite(trig, LOW);
  long d = pulseIn(echo, HIGH, 15000); if(d == 0) return 999; return d * 0.034 / 2;
}