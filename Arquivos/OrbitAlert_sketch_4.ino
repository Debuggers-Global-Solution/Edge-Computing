/*
 * ============================================================
 *  OrbitAlert — Edge Computing · Global Solution 2026 · FIAP
 *  Disciplina: Edge Computing & Computer Systems
 *  Aluno: Thiago Kulesza · RM 568922 · Turma 1ESW
 * ============================================================
 *
 *  BASE: CP2 – Vinheria Agnello (Debuggers)
 *
 *  FUNCIONALIDADES:
 *    · Média móvel 10 amostras (LDR + divisor 10kΩ)
 *    · 5 telas navegáveis: UP avança / DOWN volta
 *    · MENU → toggle idioma PT-BR / EN
 *    · EEPROM: salva leitura+nível a cada 60s (circular)
 *    · Animação satélite 2 frames (sem redesenho total)
 *    · 6 chars customizados na CGRAM
 *    · Buzzer: boot melódico, navegação, alertas
 *
 *  PINOS:
 *    A0  → LDR (divisor 10kΩ)        D13 → LED vermelho
 *    D12 → Buzzer passivo             A4  → LCD SDA
 *    A5  → LCD SCL                    D7  → BTN MENU
 *    D6  → BTN UP                     D5  → BTN DOWN
 *
 *  EEPROM (3 bytes/registro, 170 slots, índice em addr 510):
 *    addr+0 → ADC high byte
 *    addr+1 → ADC low byte
 *    addr+2 → nível (0–3)
 *
 *  RISCO (escala Kp/G – NOAA):
 *    ADC   0–249 → BAIXO   G0–G3
 *    ADC 250–499 → MEDIO   G4–G5
 *    ADC 500–749 → ALTO    G6–G7
 *    ADC 750–1023→ CRITICO G8–G9
 * ============================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ── Pinos ────────────────────────────────────────────────────
#define PIN_LDR        A0
#define PIN_LED        13
#define PIN_BUZZER     12
#define PIN_BTN_MENU    7
#define PIN_BTN_UP      6
#define PIN_BTN_DOWN    5

// ── LCD I2C ──────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ── Constantes ───────────────────────────────────────────────
#define NUM_AMOSTRAS       10
#define NUM_TELAS           5
#define DEBOUNCE_MS       200
#define INTERVALO_LEITURA 200
#define INTERVALO_EEPROM  60000UL

#define EEP_REG_SIZE        3
#define EEP_MAX_REG       170
#define EEP_IDX_ADDR      510

#define LIMIAR_MEDIO   250
#define LIMIAR_ALTO    500
#define LIMIAR_CRITICO 750

// ── Chars customizados CGRAM ─────────────────────────────────
//  0=Satélite A  1=Satélite B  2=Raio  3=Onda  4=OK  5=Caveira
byte charSat0[8] = { 0b00100,0b01110,0b11111,0b01110,0b00100,0b10101,0b01010,0b00000 };
byte charSat1[8] = { 0b00100,0b01110,0b11111,0b01110,0b00100,0b01010,0b10101,0b00000 };
byte charRaio[8] = { 0b00111,0b01110,0b11100,0b11111,0b00111,0b01110,0b11100,0b00000 };
byte charOnda[8] = { 0b00000,0b01010,0b10101,0b10001,0b01010,0b00100,0b00000,0b00000 };
byte charOK[8]   = { 0b00000,0b00001,0b00011,0b10110,0b11100,0b01000,0b00000,0b00000 };
byte charCav[8]  = { 0b01110,0b11111,0b10101,0b11111,0b01110,0b01110,0b00000,0b00000 };

// ── Temporização por nível [0..3] ────────────────────────────
const unsigned long intervaloLED[] = {    0, 1000,  400,  150 };
const unsigned long intervaloBuz[] = {    0,    0,  800,  300 };
const int           freqBuz[]      = {    0,    0, 1200, 2000 };
const int           duracaoBip[]   = {    0,    0,  120,  200 };

// ── Estado global ────────────────────────────────────────────
int           amostras[NUM_AMOSTRAS];
uint8_t       idxAmostra    = 0;
bool          bufferCheio   = false;
int           leituraMedia  = 0;
uint8_t       nivelAtual    = 0;
int8_t        nivelAnterior = -1;
uint8_t       telaAtual     = 0;
int8_t        telaAnterior  = -1;
bool          idiomaPT      = true;
bool          ledState      = false;
bool          frameSat      = false;
bool          redraw        = false;

unsigned long tLED   = 0, tBuz  = 0, tLeit = 0;
unsigned long tBotao = 0, tSat  = 0, tEEP  = 0;

// ── Protótipos ───────────────────────────────────────────────
void  registrarChars();
void  splash();
void  barraBootAnimada();
int   lerLDR();
uint8_t calcularNivel(int v);
uint8_t calcPct(int adc);
void  imprimePct(uint8_t pct);
void  imprimeADC(int adc);
void  imprimeBarra(int adc, uint8_t segs);
void  atualizarLCD();
void  tela0(); void tela1(); void tela2(); void tela3(); void tela4();
void  animarSatelite();
void  gerenciarLED();
void  gerenciarBuzzer();
void  verificarBotoes();
void  toggleIdioma();
void  salvarEEPROM();
void  flashMemSalva(uint16_t slot);
void  bip(int freq, int dur);
// Imprime string PROGMEM direto no LCD (sem buffer global)
void  lcdF(const __FlashStringHelper* s);

// ── Macros de texto por idioma ────────────────────────────────
// Usa F() — compilador AVR coloca em flash e nunca causa crash
#define TXT(pt, en)  (idiomaPT ? F(pt) : F(en))

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  pinMode(PIN_LED,      OUTPUT); digitalWrite(PIN_LED, LOW);
  pinMode(PIN_BTN_MENU, INPUT_PULLUP);
  pinMode(PIN_BTN_UP,   INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);

  // Pré-preenche buffer com leitura inicial
  int ini = analogRead(PIN_LDR);
  for (uint8_t i = 0; i < NUM_AMOSTRAS; i++) amostras[i] = ini;
  bufferCheio  = true;
  leituraMedia = ini;

  lcd.init();
  lcd.backlight();
  registrarChars();
  splash();

  Serial.println(F("=== OrbitAlert v2.0 ==="));
  Serial.println(F("ADC  | %  | Nivel | Kp  | T | Lang"));
  Serial.println(F("-----|----|----- -|----|---|-----"));
}

// ─────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // 1. Leitura LDR a cada 200ms
  if (now - tLeit >= INTERVALO_LEITURA) {
    tLeit         = now;
    leituraMedia  = lerLDR();
    uint8_t novoNv = calcularNivel(leituraMedia);
    if (novoNv != nivelAtual) { nivelAtual = novoNv; redraw = true; }

    uint8_t pct  = calcPct(leituraMedia);
    uint8_t kp10 = (uint8_t)(((uint32_t)leituraMedia * 90UL) / 1023UL);
    Serial.print(leituraMedia); Serial.print(F(" | "));
    Serial.print(pct);          Serial.print(F("% | "));
    // Nível sem array de ponteiros PROGMEM — switch seguro
    switch (nivelAtual) {
      case 0: Serial.print(idiomaPT ? F("BAIXO  ") : F("LOW    ")); break;
      case 1: Serial.print(idiomaPT ? F("MEDIO  ") : F("MEDIUM ")); break;
      case 2: Serial.print(idiomaPT ? F("ALTO   ") : F("HIGH   ")); break;
      case 3: Serial.print(idiomaPT ? F("CRITICO") : F("CRITIC.")); break;
    }
    Serial.print(F(" | "));
    Serial.print(kp10 / 10); Serial.print('.'); Serial.print(kp10 % 10);
    Serial.print(F(" | ")); Serial.print(telaAtual);
    Serial.print(F(" | ")); Serial.println(idiomaPT ? F("PT") : F("EN"));
  }

  // 2. Botões
  verificarBotoes();

  // 3. Redesenho condicional
  if (redraw || telaAtual != (uint8_t)telaAnterior) {
    atualizarLCD();
    nivelAnterior = nivelAtual;
    telaAnterior  = telaAtual;
    redraw        = false;
  }

  // 4. Animação satélite (só tela 0)
  if (telaAtual == 0) animarSatelite();

  // 5. LED + Buzzer
  gerenciarLED();
  gerenciarBuzzer();

  // 6. EEPROM a cada 60s
  if (now - tEEP >= INTERVALO_EEPROM) { tEEP = now; salvarEEPROM(); }
}

// ─────────────────────────────────────────────────────────────
void registrarChars() {
  lcd.createChar(0, charSat0); lcd.createChar(1, charSat1);
  lcd.createChar(2, charRaio); lcd.createChar(3, charOnda);
  lcd.createChar(4, charOK);   lcd.createChar(5, charCav);
}

// ─────────────────────────────────────────────────────────────
// Splash: identidade → satélite piscando 3x → barra de boot
// ─────────────────────────────────────────────────────────────
void splash() {
  lcd.clear(); registrarChars();
  lcd.setCursor(0,0); lcd.write(byte(0));
  lcd.print(F(" ORBITALERT")); lcd.write(byte(3));
  lcd.setCursor(0,1); lcd.print(F(" FIAP GS26 1ESW "));
  delay(1200);

  for (uint8_t i = 0; i < 3; i++) {
    lcd.setCursor(0,0); lcd.write(byte(1)); delay(280);
    lcd.setCursor(0,0); lcd.write(byte(0)); delay(280);
  }
  barraBootAnimada();
}

void barraBootAnimada() {
  lcd.clear(); registrarChars();
  lcd.setCursor(0,0);
  lcd.print(TXT("Inicializando..","Initializing..."));
  lcd.setCursor(0,1); lcd.print(F("[              ]"));

  bip(600,60); delay(100); bip(900,60); delay(100); bip(1200,60); delay(100);

  for (uint8_t i = 0; i < 14; i++) {
    lcd.setCursor(1+i, 1); lcd.print('#'); delay(70);
  }
  delay(250);

  lcd.clear(); registrarChars();
  lcd.setCursor(0,0); lcd.print(TXT("  Sistema OK!  ","  System  OK!  "));
  lcd.setCursor(0,1); lcd.print(TXT(" Monitorando.. "," Monitoring... "));
  bip(1000,80); delay(90); bip(1400,80); delay(90); bip(1800,140); delay(280);
  delay(700);
  lcd.clear(); registrarChars();
}

// ─────────────────────────────────────────────────────────────
int lerLDR() {
  amostras[idxAmostra] = analogRead(PIN_LDR);
  idxAmostra = (idxAmostra + 1) % NUM_AMOSTRAS;
  if (idxAmostra == 0) bufferCheio = true;
  int total = 0;
  uint8_t n = bufferCheio ? NUM_AMOSTRAS : (idxAmostra > 0 ? idxAmostra : 1);
  for (uint8_t i = 0; i < n; i++) total += amostras[i];
  return total / n;
}

uint8_t calcularNivel(int v) {
  if (v < LIMIAR_MEDIO)   return 0;
  if (v < LIMIAR_ALTO)    return 1;
  if (v < LIMIAR_CRITICO) return 2;
  return 3;
}

uint8_t calcPct(int adc) {
  uint8_t p = (uint8_t)(((uint32_t)adc * 100UL) / 1023UL);
  return p > 100 ? 100 : p;
}

void imprimePct(uint8_t pct) {
  if      (pct < 10)  { lcd.print(' '); lcd.print(pct); lcd.print('%'); }
  else if (pct < 100) { lcd.print(pct); lcd.print('%'); }
  else                { lcd.print(F("MAX")); }
}

void imprimeADC(int adc) {
  if (adc < 1000) lcd.print('0');
  if (adc <  100) lcd.print('0');
  if (adc <   10) lcd.print('0');
  lcd.print(adc);
}

void imprimeBarra(int adc, uint8_t segs) {
  uint8_t cheios = (uint8_t)map(adc, 0, 1023, 0, segs);
  lcd.print('[');
  for (uint8_t i = 0; i < segs; i++) lcd.print(i < cheios ? '#' : '-');
  lcd.print(']');
}

// ─────────────────────────────────────────────────────────────
void atualizarLCD() {
  lcd.clear(); registrarChars();
  switch (telaAtual) {
    case 0: tela0(); break;
    case 1: tela1(); break;
    case 2: tela2(); break;
    case 3: tela3(); break;
    case 4: tela4(); break;
  }
}

// ── TELA 0 — Risco Atual ─────────────────────────────────────
//  [sat] RISCO:CRITICO
//  [ond][#########]98%
void tela0() {
  lcd.setCursor(0,0);
  lcd.write(byte(frameSat ? 1 : 0));
  lcd.print(TXT(" RISCO:"," RISK: "));
  switch (nivelAtual) {
    case 0: lcd.print(TXT("BAIXO  ","LOW    ")); break;
    case 1: lcd.print(TXT("MEDIO  ","MEDIUM ")); break;
    case 2: lcd.print(TXT("ALTO   ","HIGH   ")); break;
    case 3: lcd.print(TXT("CRITICO","CRITIC.")); break;
  }
  lcd.setCursor(0,1);
  lcd.write(byte(3));
  imprimeBarra(leituraMedia, 9);
  imprimePct(calcPct(leituraMedia));
}

// ── TELA 1 — Barra LDR ───────────────────────────────────────
//  [⚡] ADC:0512 ( 50%)
//  [##############]
void tela1() {
  uint8_t pct = calcPct(leituraMedia);
  lcd.setCursor(0,0);
  lcd.write(byte(2));
  lcd.print(TXT(" ADC:"," ADC:"));
  imprimeADC(leituraMedia);
  lcd.print(F(" ("));
  if (pct < 10) lcd.print(' ');
  lcd.print(pct); lcd.print(F("%)"));
  lcd.setCursor(0,1);
  imprimeBarra(leituraMedia, 14);
}

// ── TELA 2 — Índice Kp Simulado ──────────────────────────────
//  [~] Kp: 7.2
//  Escala: G6-G7
void tela2() {
  uint8_t kp10 = (uint8_t)(((uint32_t)leituraMedia * 90UL) / 1023UL);
  lcd.setCursor(0,0);
  lcd.write(byte(3));
  lcd.print(TXT(" Kp: "," Kp: "));
  lcd.print(kp10 / 10); lcd.print('.'); lcd.print(kp10 % 10);
  lcd.print(F("      "));
  lcd.setCursor(0,1);
  lcd.print(TXT("Escala: ","Scale:  "));
  switch (nivelAtual) {
    case 0: lcd.print(F("G0-G3")); break;
    case 1: lcd.print(F("G4-G5")); break;
    case 2: lcd.print(F("G6-G7")); break;
    case 3: lcd.print(F("G8-G9")); break;
  }
}

// ── TELA 3 — Situação contextual ─────────────────────────────
//  SITUACAO: / SITUATION:
//  [ícone] mensagem
void tela3() {
  lcd.setCursor(0,0);
  lcd.print(TXT("  SITUACAO:   ","  SITUATION:  "));
  lcd.setCursor(0,1);
  switch (nivelAtual) {
    case 0: lcd.write(byte(4)); lcd.print(TXT(" Amb. seguro  "," Safe environ.")); break;
    case 1: lcd.write(byte(3)); lcd.print(TXT(" Atv. solar   "," Solar activ. ")); break;
    case 2: lcd.write(byte(2)); lcd.print(TXT(" GPS afetado! "," GPS affected!")); break;
    case 3: lcd.write(byte(5)); lcd.print(TXT(" PERIGO TOTAL!"," TOTAL DANGER!")); break;
  }
}

// ── TELA 4 — Sobre ───────────────────────────────────────────
//  [sat] OrbitAlert1.0
//  [~]   FIAP GS 2026
void tela4() {
  lcd.setCursor(0,0);
  lcd.write(byte(0)); lcd.print(F(" OrbitAlert1.0"));
  lcd.setCursor(0,1);
  lcd.write(byte(3)); lcd.print(F(" FIAP GS 2026 "));
}

// ─────────────────────────────────────────────────────────────
void animarSatelite() {
  if (millis() - tSat < 600) return;
  tSat    = millis();
  frameSat = !frameSat;
  lcd.setCursor(0,0);
  lcd.write(byte(frameSat ? 1 : 0));
}

// ─────────────────────────────────────────────────────────────
// Botões: MENU=toggle idioma | UP=próxima | DOWN=anterior
// ─────────────────────────────────────────────────────────────
void verificarBotoes() {
  if (millis() - tBotao < DEBOUNCE_MS) return;

  if (digitalRead(PIN_BTN_MENU) == LOW) {
    tBotao = millis(); toggleIdioma(); return;
  }
  if (digitalRead(PIN_BTN_UP) == LOW) {
    tBotao = millis();
    telaAtual = (telaAtual + 1) % NUM_TELAS;
    bip(1000, 40); atualizarLCD(); telaAnterior = telaAtual; return;
  }
  if (digitalRead(PIN_BTN_DOWN) == LOW) {
    tBotao = millis();
    telaAtual = (telaAtual - 1 + NUM_TELAS) % NUM_TELAS;
    bip(1000, 40); atualizarLCD(); telaAnterior = telaAtual;
  }
}

// ─────────────────────────────────────────────────────────────
// Toggle PT-BR ↔ EN com feedback 1s no LCD
// ─────────────────────────────────────────────────────────────
void toggleIdioma() {
  idiomaPT = !idiomaPT;
  bip(1200, 60); delay(80); bip(1600, 80);
  lcd.clear(); registrarChars();
  lcd.setCursor(0,0);
  lcd.write(byte(0));
  lcd.print(idiomaPT ? F(" Idioma: PT-BR ") : F(" Language: EN  "));
  lcd.setCursor(0,1);
  lcd.print(idiomaPT ? F(" [MENU] p/trocar") : F(" [MENU] to swap"));
  delay(1000);
  redraw = true;
}

// ─────────────────────────────────────────────────────────────
void gerenciarLED() {
  if (nivelAtual == 0) {
    digitalWrite(PIN_LED, LOW);
    if (ledState) { ledState = false; tLED = millis(); }
    return;
  }
  unsigned long now = millis();
  if (now - tLED >= intervaloLED[nivelAtual]) {
    ledState = !ledState;
    digitalWrite(PIN_LED, ledState ? HIGH : LOW);
    tLED = now;
  }
}

void gerenciarBuzzer() {
  if (nivelAtual <= 1) { noTone(PIN_BUZZER); return; }
  unsigned long now = millis();
  if (now - tBuz >= intervaloBuz[nivelAtual]) {
    tone(PIN_BUZZER, freqBuz[nivelAtual], duracaoBip[nivelAtual]);
    tBuz = now;
  }
}

// ─────────────────────────────────────────────────────────────
// EEPROM — registro circular de 170 slots
// ─────────────────────────────────────────────────────────────
void salvarEEPROM() {
  uint16_t idx;
  EEPROM.get(EEP_IDX_ADDR, idx);
  if (idx >= EEP_MAX_REG) idx = 0;

  uint16_t addr = (uint16_t)idx * EEP_REG_SIZE;
  EEPROM.write(addr,     highByte(leituraMedia));
  EEPROM.write(addr + 1, lowByte(leituraMedia));
  EEPROM.write(addr + 2, nivelAtual);

  idx = (idx + 1) % EEP_MAX_REG;
  EEPROM.put(EEP_IDX_ADDR, idx);

  Serial.print(F("[EEPROM] slot=")); Serial.print(idx);
  Serial.print(F(" adc="));         Serial.print(leituraMedia);
  Serial.print(F(" nivel="));       Serial.println(nivelAtual);

  flashMemSalva(idx);
}

void flashMemSalva(uint16_t slot) {
  lcd.clear(); registrarChars();
  lcd.setCursor(0,0);
  lcd.write(byte(4));
  lcd.print(idiomaPT ? F(" Mem. salva!   ") : F(" Mem. saved!   "));
  lcd.setCursor(0,1);
  lcd.print(F(" slot #")); lcd.print(slot);
  delay(600);
  redraw = true;
}

// ─────────────────────────────────────────────────────────────
void bip(int freq, int dur) { tone(PIN_BUZZER, freq, dur); }
