/*
 * ADEGA AGNELLO - Monitor de ambiente para adega
 *
 * Mede temperatura, umidade e luminosidade (média de 5 s), exibe no LCD,
 * sinaliza com LEDs/buzzer e registra ocorrências críticas na EEPROM.
 *
 * Bibliotecas: DHT sensor library, Adafruit Unified Sensor,
 *              LiquidCrystal I2C, RTClib
 *
 * Botão (D6):
 *   clique curto -> troca de tela
 *   clique longo -> setup (fuso UTC, unidade, idioma)
 *                   no setup: curto muda o valor, longo confirma
 *
 * Serial (115200): 'l' lista os logs, 'c' apaga os logs
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <DHT.h>
#include <EEPROM.h>

// ---------------- PINOS ----------------
const int pinoDHT     = 2;
const int pinoBotao   = 6;
const int pinoBuzzer  = 7;
const int ledVerde    = 9;
const int ledAmarelo  = 10;
const int ledVermelho = 11;
const int pinoLDR     = A1;

#define TIPO_DHT DHT22  // DHT11 em hardware real

// No módulo LDR usado, mais escuro = leitura maior
const bool LDR_INVERTIDO = true;

// ---------------- LIMITES ----------------
const float TEMP_OK_MIN = 10.0;   // °C
const float TEMP_OK_MAX = 16.0;
const float TEMP_MARGEM = 3.0;    // além da margem = crítico

const float UMID_OK_MIN = 60.0;   // %
const float UMID_OK_MAX = 80.0;
const float UMID_MARGEM = 10.0;

const int LUZ_ESCURO_MAX = 20;    // % (acima de LUZ_MEIA_MAX = crítico)
const int LUZ_MEIA_MAX   = 50;

// ---------------- TEMPOS (ms) ----------------
const unsigned long JANELA_MEDIA_MS   = 5000;  // período da média
const unsigned long INTERVALO_LEITURA = 1000;  // intervalo entre amostras
const unsigned long TROCA_TELA_MS     = 3000;
const unsigned long CLIQUE_LONGO_MS   = 1500;
const unsigned long CALIBRACAO_LDR_MS = 5000;

// ---------------- EEPROM ----------------
// 0 marca | 1 fuso (+12) | 2 unidade | 3 idioma | 4 posição do log | 5 qtd de logs
// A partir do endereço 16: registros de 8 bytes (buffer circular)
const int END_MARCA = 0, END_UTC = 1, END_UNIDADE = 2,
          END_IDIOMA = 3, END_LOG_POS = 4, END_LOG_QTD = 5;
const int END_LOGS = 16;
const byte MARCA_EEPROM = 0xA7;

struct RegistroLog {
  uint32_t ts;      // UTC (segundos desde 1970)
  int16_t  temp10;  // temperatura x10
  uint8_t  umid;
  uint8_t  luz;
};
const int MAX_LOGS = (1024 - END_LOGS) / sizeof(RegistroLog);

// ---------------- OBJETOS ----------------
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;  // DS3231: trocar por RTC_DS3231
DHT dht(pinoDHT, TIPO_DHT);

// ---------------- ESTADO ----------------
// Configurações (salvas na EEPROM)
int8_t  utcOffset = -3;
uint8_t unidade   = 0;   // 0 = °C, 1 = °F
uint8_t idioma    = 0;   // 0 = PT, 1 = EN, 2 = ES

uint8_t logPos = 0, logQtd = 0;
uint8_t mascaraCriticaAnterior = 0;

// Calibração do LDR
int ldrMin = 1023, ldrMax = 0;

// Acumuladores da janela de média
float tempSoma = 0, umidSoma = 0;
unsigned long ldrSoma = 0;
int dhtAmostras = 0, ldrAmostras = 0;
unsigned long inicioJanela = 0, ultimaLeitura = 0;

// Últimas médias
float tempMedia = 0, umidMedia = 0;
int luzPct = 0;
bool temMedicao = false;

// Níveis: 0 = OK, 1 = atenção, 2 = crítico
uint8_t nivelTemp = 0, nivelUmid = 0, nivelLuz = 0, nivelGeral = 0;

// Tendência em relação à média anterior: 1 sobe, -1 desce, 0 estável
int8_t tendTemp = 0, tendUmid = 0;

uint8_t telaAtual = 0;  // 0 resumo, 1 temp, 2 umidade, 3 luz, 4 data/hora
unsigned long ultimaTroca = 0;
bool buzzerLigado = false;

// Seleciona o texto conforme o idioma (F() mantém a string na flash)
#define TR(pt, en, es) (idioma == 0 ? F(pt) : (idioma == 1 ? F(en) : F(es)))

// ============================================================
//  ANIMAÇÃO DA GARRAFA
// ============================================================
byte image01_f1[] = { B00000, B11111, B11111, B01110, B00100, B00100, B00100, B01110 };
byte image01_f2[] = { B00000, B00000, B11000, B00100, B01111, B11100, B11000, B00000 };
byte image02_f2[] = { B00000, B00000, B00000, B01000, B11000, B01000, B00000, B00000 };
byte image03_f2[] = { B00000, B00000, B00000, B00000, B00000, B00000, B00000, B00001 };
byte image04_f2[] = { B00011, B00010, B00000, B00010, B00000, B00000, B00000, B00000 };
byte image01_f3[] = { B00000, B00000, B11000, B00100, B00011, B00100, B11000, B00000 };
byte image04_f3[] = { B00001, B00011, B00011, B00110, B00010, B00000, B00010, B00000 };
byte image01_f4[] = { B00000, B00001, B00000, B00001, B00010, B00001, B00011, B00110 };
byte image02_f4[] = { B00010, B00000, B00000, B00000, B00000, B00000, B00000, B00000 };
byte image01_f5[] = { B00010, B00100, B00100, B01100, B11000, B01001, B11011, B01110 };
byte image02_f5[] = { B00000, B00000, B00000, B10100, B11000, B10000, B10000, B00000 };
byte image02_f6[] = { B00000, B00000, B00000, B10110, B11001, B10000, B10000, B10000 };
byte image03_f6[] = { B00000, B00000, B00000, B01110, B10001, B01111, B11001, B01111 };
byte image04_f6[] = { B00001, B10001, B01110, B00000, B00000, B00000, B00000, B00000 };
byte image01_f7[] = { B00000, B00000, B00000, B01110, B11001, B01101, B11111, B01110 };
byte image05_f7[] = { B00000, B00000, B00000, B10110, B11001, B10001, B11111, B10000 };
byte image06_f7[] = { B10000, B10000, B10000, B00000, B00000, B00000, B00000, B00000 };
byte image07_f7[] = { B00000, B00000, B00000, B01110, B10001, B11111, B10000, B01111 };

// Cada frame carrega os caracteres customizados e os posiciona no LCD
void frame1() {
  lcd.clear(); lcd.createChar(0, image01_f1);
  lcd.setCursor(7, 0); lcd.write(byte(0));
}
void frame2() {
  lcd.clear(); lcd.createChar(0, image01_f2); lcd.createChar(1, image02_f2); lcd.createChar(2, image03_f2); lcd.createChar(3, image04_f2);
  lcd.setCursor(7, 0); lcd.write(byte(0)); lcd.setCursor(8, 0); lcd.write(byte(1)); lcd.setCursor(6, 0); lcd.write(byte(2)); lcd.setCursor(6, 1); lcd.write(byte(3));
}
void frame3() {
  lcd.clear(); lcd.createChar(0, image01_f3); lcd.createChar(1, image02_f2); lcd.createChar(2, image03_f2); lcd.createChar(3, image04_f3);
  lcd.setCursor(7, 0); lcd.write(byte(0)); lcd.setCursor(8, 0); lcd.write(byte(1)); lcd.setCursor(6, 0); lcd.write(byte(2)); lcd.setCursor(6, 1); lcd.write(byte(3));
}
void frame4() {
  lcd.clear(); lcd.createChar(0, image01_f4); lcd.createChar(1, image02_f4);
  lcd.setCursor(6, 0); lcd.write(byte(0)); lcd.setCursor(6, 1); lcd.write(byte(1));
}
void frame5() {
  lcd.clear(); lcd.createChar(0, image01_f5); lcd.createChar(1, image02_f5);
  lcd.setCursor(6, 0); lcd.write(byte(0)); lcd.setCursor(7, 0); lcd.write(byte(1));
}
void frame6() {
  lcd.clear(); lcd.createChar(0, image01_f5); lcd.createChar(1, image02_f6); lcd.createChar(2, image03_f6); lcd.createChar(3, image04_f6);
  lcd.setCursor(6, 0); lcd.write(byte(0)); lcd.setCursor(7, 0); lcd.write(byte(1)); lcd.setCursor(8, 0); lcd.write(byte(2)); lcd.setCursor(6, 1); lcd.write(byte(3));
}
void frame7() {
  lcd.clear(); lcd.createChar(0, image01_f7); lcd.createChar(1, image02_f6); lcd.createChar(2, image03_f6); lcd.createChar(3, image04_f6); lcd.createChar(4, image05_f7); lcd.createChar(5, image06_f7); lcd.createChar(6, image07_f7);
  lcd.setCursor(6, 0); lcd.write(byte(0)); lcd.setCursor(7, 0); lcd.write(byte(1)); lcd.setCursor(8, 0); lcd.write(byte(2)); lcd.setCursor(6, 1); lcd.write(byte(3));
  lcd.setCursor(9, 0); lcd.write(byte(4)); lcd.setCursor(9, 1); lcd.write(byte(5)); lcd.setCursor(10, 0); lcd.write(byte(6));
}

// ============================================================
//  ÍCONES DA TELA-RESUMO
// ============================================================
byte iconeTermometro[] = { B00100, B01010, B01010, B01010, B01110, B11111, B11111, B01110 };
byte iconeGota[]       = { B00100, B00100, B01010, B01010, B10001, B10001, B10001, B01110 };
byte iconeSol[]        = { B00100, B10101, B01110, B11111, B01110, B10101, B00100, B00000 };
byte iconeAlerta[]     = { B00100, B00100, B00100, B00100, B00100, B00000, B00100, B00000 };
byte iconeSetaCima[]   = { B00100, B01110, B10101, B00100, B00100, B00100, B00000, B00000 };
byte iconeSetaBaixo[]  = { B00000, B00000, B00100, B00100, B00100, B10101, B01110, B00100 };

// Substitui os caracteres da animação na CGRAM
void carregarIcones() {
  lcd.createChar(0, iconeTermometro);
  lcd.createChar(1, iconeGota);
  lcd.createChar(2, iconeSol);
  lcd.createChar(3, iconeAlerta);
  lcd.createChar(4, iconeSetaCima);
  lcd.createChar(5, iconeSetaBaixo);
}

byte setaTendencia(int8_t tendencia) {
  if (tendencia > 0) return 4;
  if (tendencia < 0) return 5;
  return 0x7E;  // "→" da ROM do LCD
}

// Barra que enche conforme a janela de média avança
void desenhaBarraProgresso(uint8_t linha, uint8_t colInicio, uint8_t largura) {
  unsigned long decorrido = millis() - inicioJanela;
  uint8_t cheias = (decorrido * largura) / JANELA_MEDIA_MS;
  if (cheias > largura) cheias = largura;

  lcd.setCursor(colInicio, linha);
  for (uint8_t i = 0; i < largura; i++) {
    lcd.write((byte)(i < cheias ? 255 : ' '));
  }
}

// ============================================================
//  AUXILIARES DE DISPLAY
// ============================================================

// Escreve a linha completa (16 colunas) sem precisar de lcd.clear()
void escreveLinha(uint8_t linha, const char *txt) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", txt);
  lcd.setCursor(0, linha);
  lcd.print(buf);
}

void escreveLinhaF(uint8_t linha, const __FlashStringHelper *txt) {
  char buf[17];
  strncpy_P(buf, (PGM_P)txt, 16);
  buf[16] = '\0';
  escreveLinha(linha, buf);
}

void copiaF(char *destino, const __FlashStringHelper *origem) {
  strcpy_P(destino, (PGM_P)origem);
}

// ============================================================
//  EEPROM: CONFIGURAÇÕES E LOGS
// ============================================================
void salvarConfig() {
  EEPROM.update(END_UTC, (uint8_t)(utcOffset + 12));
  EEPROM.update(END_UNIDADE, unidade);
  EEPROM.update(END_IDIOMA, idioma);
}

void carregarConfig() {
  // Primeira execução: grava valores padrão
  if (EEPROM.read(END_MARCA) != MARCA_EEPROM) {
    EEPROM.update(END_MARCA, MARCA_EEPROM);
    EEPROM.update(END_LOG_POS, 0);
    EEPROM.update(END_LOG_QTD, 0);
    salvarConfig();
  }

  utcOffset = (int8_t)EEPROM.read(END_UTC) - 12;
  unidade   = EEPROM.read(END_UNIDADE);
  idioma    = EEPROM.read(END_IDIOMA);
  logPos    = EEPROM.read(END_LOG_POS);
  logQtd    = EEPROM.read(END_LOG_QTD);

  // Valida os valores lidos
  if (utcOffset < -12 || utcOffset > 14) utcOffset = -3;
  if (unidade > 1) unidade = 0;
  if (idioma > 2) idioma = 0;
  if (logPos >= MAX_LOGS) logPos = 0;
  if (logQtd > MAX_LOGS) logQtd = 0;
}

// Grava um registro; ao chegar no fim da área, sobrescreve o mais antigo
void gravarLog() {
  RegistroLog r;
  r.ts     = rtc.now().unixtime();
  r.temp10 = (int16_t)(tempMedia * 10);
  r.umid   = (uint8_t)umidMedia;
  r.luz    = (uint8_t)luzPct;

  EEPROM.put(END_LOGS + logPos * sizeof(RegistroLog), r);

  logPos = (logPos + 1) % MAX_LOGS;
  if (logQtd < MAX_LOGS) logQtd++;
  EEPROM.update(END_LOG_POS, logPos);
  EEPROM.update(END_LOG_QTD, logQtd);
}

// Imprime os logs no Serial, do mais antigo ao mais recente
void imprimirLogs() {
  Serial.println(F("--- LOGS (data UTC; temp C; umidade %; luz %) ---"));
  int inicio = (logPos - logQtd + MAX_LOGS) % MAX_LOGS;
  for (int i = 0; i < logQtd; i++) {
    RegistroLog r;
    EEPROM.get(END_LOGS + ((inicio + i) % MAX_LOGS) * sizeof(RegistroLog), r);
    DateTime d(r.ts);
    char linha[24];
    snprintf(linha, sizeof(linha), "%02d/%02d/%04d %02d:%02d:%02d",
             d.day(), d.month(), d.year(), d.hour(), d.minute(), d.second());
    Serial.print(linha);
    Serial.print(F("; ")); Serial.print(r.temp10 / 10.0, 1);
    Serial.print(F("; ")); Serial.print(r.umid);
    Serial.print(F("; ")); Serial.println(r.luz);
  }
  Serial.print(F("Total: ")); Serial.println(logQtd);
}

void limparLogs() {
  logPos = 0;
  logQtd = 0;
  EEPROM.update(END_LOG_POS, 0);
  EEPROM.update(END_LOG_QTD, 0);
  Serial.println(F("Logs apagados."));
}

void tratarSerial() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'l' || c == 'L') imprimirLogs();
    else if (c == 'c' || c == 'C') limparLogs();
  }
}

// ============================================================
//  BOTÃO
// ============================================================
// Retorna 0 = nada, 1 = clique curto, 2 = clique longo (ao soltar o botão)
uint8_t lerBotao() {
  static bool ultimoEstado = HIGH;
  static unsigned long tPress = 0;
  static unsigned long tMudanca = 0;

  bool leitura = digitalRead(pinoBotao);
  unsigned long agora = millis();

  // Debounce de 40 ms
  if (leitura != ultimoEstado && (agora - tMudanca) > 40) {
    tMudanca = agora;
    ultimoEstado = leitura;
    if (leitura == LOW) {
      tPress = agora;
    } else {
      return ((agora - tPress) >= CLIQUE_LONGO_MS) ? 2 : 1;
    }
  }
  return 0;
}

// ============================================================
//  SETUP PELO BOTÃO (fuso, unidade, idioma)
// ============================================================
void executarSetup() {
  noTone(pinoBuzzer);
  buzzerLigado = false;
  lcd.backlight();
  digitalWrite(ledVerde, LOW);
  digitalWrite(ledAmarelo, LOW);
  digitalWrite(ledVermelho, LOW);

  for (uint8_t passo = 0; passo < 3; passo++) {
    bool confirmado = false;
    bool redesenhar = true;

    while (!confirmado) {
      if (redesenhar) {
        redesenhar = false;
        char valor[17];

        if (passo == 0) {
          escreveLinhaF(0, TR("Fuso horario", "Time zone", "Zona horaria"));
          snprintf(valor, sizeof(valor), "UTC%+d", utcOffset);
        } else if (passo == 1) {
          escreveLinhaF(0, TR("Unidade temp.", "Temp. unit", "Unidad temp."));
          strcpy(valor, unidade == 0 ? "Celsius (C)" : "Fahrenheit (F)");
        } else {
          escreveLinhaF(0, TR("Idioma", "Language", "Idioma"));
          if (idioma == 0) strcpy(valor, "Portugues");
          else if (idioma == 1) strcpy(valor, "English");
          else strcpy(valor, "Espanol");
        }
        escreveLinha(1, valor);
      }

      uint8_t b = lerBotao();
      if (b == 1) {  // curto: altera o valor
        if (passo == 0) { utcOffset++; if (utcOffset > 14) utcOffset = -12; }
        else if (passo == 1) unidade = !unidade;
        else idioma = (idioma + 1) % 3;
        redesenhar = true;
      } else if (b == 2) {  // longo: confirma
        confirmado = true;
      }
    }
  }

  salvarConfig();
  lcd.clear();
  escreveLinhaF(0, TR("Configuracao", "Settings", "Configuracion"));
  escreveLinhaF(1, TR("salva!", "saved!", "guardada!"));
  delay(1200);
  lcd.clear();
}

// ============================================================
//  LDR: CALIBRAÇÃO E CONVERSÃO
// ============================================================
void atualizaLimitesLDR(int leitura) {
  if (leitura < ldrMin) ldrMin = leitura;
  if (leitura > ldrMax) ldrMax = leitura;
}

// Calibração inicial: observa o ambiente para definir mínimo e máximo
void calibrarLDR() {
  ldrMin = 1023;
  ldrMax = 0;

  lcd.clear();
  escreveLinhaF(0, TR("Calibrando LDR..", "Calibrating LDR", "Calibrando LDR.."));

  unsigned long inicio = millis();
  int ultimoSeg = -1;

  while (millis() - inicio < CALIBRACAO_LDR_MS) {
    atualizaLimitesLDR(analogRead(pinoLDR));

    int resta = (int)((CALIBRACAO_LDR_MS - (millis() - inicio)) / 1000) + 1;
    if (resta != ultimoSeg) {
      ultimoSeg = resta;
      char b[17];
      snprintf(b, sizeof(b), "%d s", resta);
      escreveLinha(1, b);
    }
    delay(20);
  }

  // Faixa muito estreita: usa a escala completa
  if (ldrMax - ldrMin < 100) {
    ldrMin = 0;
    ldrMax = 1023;
  }
  lcd.clear();
}

// Converte a leitura bruta (0-1023) em porcentagem de luz (0-100)
int ldrParaPorcentagem(long mediaBruta) {
  int pct = map(mediaBruta, ldrMin, ldrMax, 0, 100);
  pct = constrain(pct, 0, 100);
  if (LDR_INVERTIDO) pct = 100 - pct;
  return pct;
}

// ============================================================
//  CLASSIFICAÇÃO
// ============================================================
uint8_t classificaTemp(float t) {
  if (t >= TEMP_OK_MIN && t <= TEMP_OK_MAX) return 0;
  if (t >= TEMP_OK_MIN - TEMP_MARGEM && t <= TEMP_OK_MAX + TEMP_MARGEM) return 1;
  return 2;
}

uint8_t classificaUmid(float u) {
  if (u >= UMID_OK_MIN && u <= UMID_OK_MAX) return 0;
  if (u >= UMID_OK_MIN - UMID_MARGEM && u <= UMID_OK_MAX + UMID_MARGEM) return 1;
  return 2;
}

uint8_t classificaLuz(int pct) {
  if (pct <= LUZ_ESCURO_MAX) return 0;
  if (pct <= LUZ_MEIA_MAX) return 1;
  return 2;
}

// ============================================================
//  MEDIÇÃO
// ============================================================
void reiniciarJanela() {
  tempSoma = 0; umidSoma = 0; dhtAmostras = 0;
  ldrSoma = 0; ldrAmostras = 0;
  inicioJanela = millis();
}

// Acumula uma amostra por intervalo, sem bloquear o loop
void coletarAmostras() {
  unsigned long agora = millis();
  if (agora - ultimaLeitura < INTERVALO_LEITURA) return;
  ultimaLeitura = agora;

  int leitura = analogRead(pinoLDR);
  atualizaLimitesLDR(leitura);  // calibração contínua
  ldrSoma += leitura;
  ldrAmostras++;

  float t = dht.readTemperature();
  float u = dht.readHumidity();
  if (!isnan(t) && !isnan(u)) {
    tempSoma += t;
    umidSoma += u;
    dhtAmostras++;
  } else {
    Serial.println(F("Falha ao ler o DHT"));
  }
}

// Fecha a janela: calcula médias, classifica e registra alertas críticos
void fecharJanela() {
  if (dhtAmostras > 0) {
    float novaTemp = tempSoma / dhtAmostras;
    float novaUmid = umidSoma / dhtAmostras;

    // Tendência: só considera variação acima de 0,3 °C ou 2 %
    if (temMedicao) {
      float dT = novaTemp - tempMedia;
      float dU = novaUmid - umidMedia;
      tendTemp = (dT > 0.3) ? 1 : (dT < -0.3) ? -1 : 0;
      tendUmid = (dU > 2.0) ? 1 : (dU < -2.0) ? -1 : 0;
    }

    tempMedia = novaTemp;
    umidMedia = novaUmid;
    temMedicao = true;
  }
  if (ldrAmostras > 0) {
    luzPct = ldrParaPorcentagem(ldrSoma / ldrAmostras);
  }

  if (temMedicao) {
    nivelTemp = classificaTemp(tempMedia);
    nivelUmid = classificaUmid(umidMedia);
    nivelLuz  = classificaLuz(luzPct);
    nivelGeral = max(nivelTemp, max(nivelUmid, nivelLuz));

    // Bits: 1 = temp, 2 = umidade, 4 = luz em nível crítico
    uint8_t mascara = 0;
    if (nivelTemp == 2) mascara |= 1;
    if (nivelUmid == 2) mascara |= 2;
    if (nivelLuz  == 2) mascara |= 4;

    // Grava só quando surge um alerta crítico novo
    if (mascara != 0 && mascara != mascaraCriticaAnterior) gravarLog();
    mascaraCriticaAnterior = mascara;

    Serial.print(F("Media -> T: ")); Serial.print(tempMedia, 1);
    Serial.print(F(" C | U: ")); Serial.print(umidMedia, 0);
    Serial.print(F(" % | Luz: ")); Serial.print(luzPct);
    Serial.println(F(" %"));
  }

  reiniciarJanela();
}

// ============================================================
//  SAÍDAS: LEDS E BUZZER
// ============================================================
void atualizarSaidas() {
  if (!temMedicao) {
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledAmarelo, LOW);
    digitalWrite(ledVermelho, LOW);
    return;
  }

  digitalWrite(ledVerde,    nivelGeral == 0);
  digitalWrite(ledAmarelo,  nivelGeral == 1);
  digitalWrite(ledVermelho, nivelGeral == 2);

  // Crítico: bipa 200 ms a cada 1 s; o backlight apaga durante o bip
  bool deveSoar = (nivelGeral == 2) && ((millis() % 1000) < 200);
  if (deveSoar != buzzerLigado) {
    buzzerLigado = deveSoar;
    if (deveSoar) {
      tone(pinoBuzzer, 1000);
      lcd.noBacklight();
    } else {
      noTone(pinoBuzzer);
      lcd.backlight();
    }
  }
}

// ============================================================
//  DISPLAY
// ============================================================
const __FlashStringHelper* textoTemp() {
  if (nivelTemp == 0) return TR("Temperatura OK", "Temperature OK", "Temperatura OK");
  if (tempMedia > TEMP_OK_MAX) return TR("Temp. ALTA", "Temp. HIGH", "Temp. ALTA");
  return TR("Temp. BAIXA", "Temp. LOW", "Temp. BAJA");
}

const __FlashStringHelper* textoUmid() {
  if (nivelUmid == 0) return TR("Umidade OK", "Humidity OK", "Humedad OK");
  if (umidMedia > UMID_OK_MAX) return TR("Umidade ALTA", "Humidity HIGH", "Humedad ALTA");
  return TR("Umidade BAIXA", "Humidity LOW", "Humedad BAJA");
}

void atualizarDisplay() {
  static unsigned long ultimoDesenho = 0;
  unsigned long agora = millis();

  if (agora - ultimoDesenho < 500) return;
  ultimoDesenho = agora;

  // Rotação automática das telas
  if (agora - ultimaTroca >= TROCA_TELA_MS) {
    telaAtual = (telaAtual + 1) % 5;
    ultimaTroca = agora;
  }

  char l1[24], l2[24], num[12];

  // Antes da primeira média, só o relógio está disponível
  if (!temMedicao && telaAtual != 4) {
    escreveLinhaF(0, TR("Medindo...", "Measuring...", "Midiendo..."));
    desenhaBarraProgresso(1, 0, 16);
    return;
  }

  switch (telaAtual) {
    case 0: {  // Resumo
      // Escrita direta no LCD porque os ícones usam o código 0
      float t = (unidade == 0) ? tempMedia : (tempMedia * 1.8 + 32.0);
      uint8_t col = 0;

      lcd.setCursor(0, 0);
      lcd.write(byte(0)); col++;
      dtostrf(t, 1, 1, num);
      col += lcd.print(num);
      col += lcd.print(unidade == 0 ? 'C' : 'F');
      lcd.write(setaTendencia(tendTemp)); col++;
      col += lcd.print(' ');
      lcd.write(byte(1)); col++;
      dtostrf(umidMedia, 1, 0, num);
      col += lcd.print(num);
      col += lcd.print('%');
      lcd.write(setaTendencia(tendUmid)); col++;
      while (col < 16) { lcd.print(' '); col++; }

      lcd.setCursor(0, 1);
      lcd.write(byte(2));
      snprintf(l2, sizeof(l2), "%3d%% ", luzPct);
      lcd.print(l2);
      desenhaBarraProgresso(1, 6, 9);
      lcd.setCursor(15, 1);
      if (nivelGeral == 2) lcd.write(byte(3));
      else lcd.print(' ');
      return;
    }
    case 1: {  // Temperatura
      copiaF(l1, textoTemp());
      float t = (unidade == 0) ? tempMedia : (tempMedia * 1.8 + 32.0);
      dtostrf(t, 1, 1, num);
      snprintf(l2, sizeof(l2), "Temp. = %s%c", num, unidade == 0 ? 'C' : 'F');
      break;
    }
    case 2: {  // Umidade
      copiaF(l1, textoUmid());
      copiaF(l2, TR("Umidade = ", "Humidity = ", "Humedad = "));
      dtostrf(umidMedia, 1, 0, num);
      strcat(l2, num);
      strcat(l2, "%");
      break;
    }
    case 3: {  // Luminosidade
      if (nivelLuz == 0) {
        copiaF(l1, TR("Ambiente", "Environment", "Ambiente"));
        copiaF(l2, TR("ESCURO", "DARK", "OSCURO"));
      } else if (nivelLuz == 1) {
        copiaF(l1, TR("Ambiente a meia", "Ambient is", "Ambiente en"));
        copiaF(l2, TR("luz", "dimly lit", "penumbra"));
      } else {
        copiaF(l1, TR("Ambiente muito", "Environment too", "Ambiente muy"));
        copiaF(l2, TR("CLARO", "BRIGHT", "CLARO"));
      }
      size_t n = strlen(l2);
      snprintf(l2 + n, sizeof(l2) - n, " %d%%", luzPct);
      break;
    }
    default: {  // Data e hora no fuso configurado
      DateTime utc = rtc.now();
      DateTime local(utc.unixtime() + (int32_t)utcOffset * 3600L);
      snprintf(l1, sizeof(l1), "%02d/%02d/%04d UTC%+d",
               local.day(), local.month(), local.year(), utcOffset);
      snprintf(l2, sizeof(l2), "%02d:%02d:%02d",
               local.hour(), local.minute(), local.second());
      break;
    }
  }

  escreveLinha(0, l1);
  escreveLinha(1, l2);
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println(F("INICIALIZANDO"));

  pinMode(ledVermelho, OUTPUT);
  pinMode(ledAmarelo, OUTPUT);
  pinMode(ledVerde, OUTPUT);
  pinMode(pinoBuzzer, OUTPUT);
  pinMode(pinoBotao, INPUT_PULLUP);

  dht.begin();
  carregarConfig();

  lcd.init();
  lcd.backlight();
  lcd.clear();

  // RTC guardado em UTC. Se estiver parado, acerta pela data de compilação
  // (horário de Brasília, UTC-3) somando 3 h
  if (!rtc.begin()) {
    Serial.println(F("RTC nao encontrado!"));
  } else if (!rtc.isrunning()) {
    DateTime compilacao(F(__DATE__), F(__TIME__));
    rtc.adjust(DateTime(compilacao.unixtime() + 3 * 3600L));
  }

  // Animação da garrafa (executa uma única vez)
  frame1(); delay(800);
  frame2(); delay(600);
  frame3(); delay(500);
  frame4(); delay(700);
  frame5(); delay(400);
  frame6(); delay(400);
  frame7(); delay(1350);

  // Mensagem de boas-vindas
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(TR("Bem-vindo(a) a", "Welcome to", "Bienvenido a"));
  lcd.setCursor(0, 1);
  lcd.print("Adega Agnello!");
  delay(1500);

  // Letreiro rolando para a direita
  for (int x = 0; x < 16; x++) {
    lcd.scrollDisplayRight();
    delay(400);
  }

  lcd.clear();  // também desfaz o deslocamento do letreiro
  carregarIcones();

  calibrarLDR();
  reiniciarJanela();
  ultimaTroca = millis();
}

// ============================================================
//  LOOP
// ============================================================
// Sem delay(): o tempo é controlado por millis()
void loop() {
  tratarSerial();

  uint8_t b = lerBotao();
  if (b == 2) {
    executarSetup();
    reiniciarJanela();
    ultimaTroca = millis();
    return;
  } else if (b == 1) {
    telaAtual = (telaAtual + 1) % 5;
    ultimaTroca = millis();
  }

  coletarAmostras();

  if (millis() - inicioJanela >= JANELA_MEDIA_MS) {
    fecharJanela();
  }

  atualizarSaidas();
  atualizarDisplay();
}
