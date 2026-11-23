/* =====================================================================
   BRAÇO ROBÓTICO — VERSÃO FINAL HÍBRIDA (Local + Remoto)
   ===================================================================== */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// ==================================================================
//                       [CALIBRAR] SERVOS GERAIS
// ==================================================================
#define FREQ        50
#define SERVO_MIN   130
#define SERVO_MAX   490
#define ANG_MIN     0
#define ANG_MAX     180

// ==================================================================
//              [CALIBRAR] LIMITES ESTENDIDOS (A1 e A2)
// ==================================================================
#define SERVO_MIN_EXT 110
#define SERVO_MAX_EXT 510

#define GRIP_FECHADA  90
#define GRIP_ABERTA   150

// ==================================================================
//                       PCA9685
// ==================================================================
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// ==================================================================
//                       PINOS
// ==================================================================
const int potBase     = A0;
const int potShoulder = A1;
const int potElbow    = A2;
const int potWrist    = A3;

const int btnMode = 8;
const int btnGrip = 9;
const int btnStop = 10;

// ==================================================================
//                       CANAIS PCA9685
// ==================================================================
const int CH_BASE     = 0;
const int CH_SHOULDER = 1;
const int CH_ELBOW    = 2;
const int CH_WRIST    = 3;
const int CH_GRIP     = 4;

// ==================================================================
//                       MODOS E CONTROLE REMOTO
// ==================================================================
enum Mode { MANUAL, RECORD, PLAY, RETORNANDO };
Mode mode = MANUAL;

bool pausado = false;
bool aproximando = false;

bool modoRemoto = false; 
int alvoRemoto[4] = {90, 90, 90, 90};
bool gripRemotoClosed = true;

// ==================================================================
//                       ESTADO DO PLAYBACK
// ==================================================================
enum EstadoPlay { INICIAR_PASSO, MOVENDO, ESPERANDO };
EstadoPlay estadoPlay = INICIAR_PASSO;
int          passoAtual = 0;
unsigned long tInicio = 0;

float dirPlay[4]      = {0, 0, 0, 0};
float dirGripPlay     = 0;
int   passosMovPlay   = 0;
int   passoAtualMicro = 0;
int   delMicroPlay    = 1000;

// ==================================================================
//                       GRAVAÇÃO
// ==================================================================
#define MAX_STEPS 120
byte  joint0[MAX_STEPS];
byte  joint1[MAX_STEPS];
byte  joint2[MAX_STEPS];
byte  joint3[MAX_STEPS];
byte  jointGrip[MAX_STEPS];
byte  tempoPasso[MAX_STEPS];
int   arrayStep = 0;
int   arrayMax  = 0;

unsigned long lastRecordMillis = 0;

// ==================================================================
//                       ESTADO DOS SERVOS
// ==================================================================
float ist[4]     = {90, 90, 90, 90};
float istGrip    = GRIP_FECHADA;
int   sol[4]     = {90, 90, 90, 90};
int   solGrip    = GRIP_FECHADA;

int   stepsMax = 0;
int   steps    = 0;
unsigned long lastStepMicros = 0;

bool gripClosed = true;

// ==================================================================
//                       BOTÕES FÍSICOS
// ==================================================================
bool lastModeBtn = HIGH;
bool lastGripBtn = HIGH;
bool lastStopBtn = HIGH;

bool          modeBtnPendente = false;
unsigned long lastModePress   = 0;
const unsigned long JANELA_DUPLO = 350;

bool          stopBtnPendente = false;
unsigned long lastStopPress   = 0;
const unsigned long JANELA_DUPLO_STOP = 400;

// ==================================================================
//                       HELPERS
// ==================================================================
int anguloParaTicksNormal(int ang) {
  ang = constrain(ang, ANG_MIN, ANG_MAX);
  return map(ang, ANG_MIN, ANG_MAX, SERVO_MIN, SERVO_MAX);
}

int anguloParaTicksEstendido(int ang) {
  ang = constrain(ang, ANG_MIN, ANG_MAX);
  return map(ang, ANG_MIN, ANG_MAX, SERVO_MIN_EXT, SERVO_MAX_EXT);
}

void escreveServo(int canal, float ang) {
  if (canal == CH_SHOULDER || canal == CH_ELBOW) {
    pwm.setPWM(canal, 0, anguloParaTicksEstendido((int)ang));
  } else {
    pwm.setPWM(canal, 0, anguloParaTicksNormal((int)ang));
  }
}

// ==================================================================
//                       LEITURA DOS POTENCIÔMETROS (Híbrida)
// ==================================================================
void lePotenciometros(int saida[4]) {
  if (modoRemoto) {
    saida[0] = alvoRemoto[0];
    saida[1] = alvoRemoto[1];
    saida[2] = alvoRemoto[2];
    saida[3] = alvoRemoto[3];
    gripClosed = gripRemotoClosed;
  } else {
    saida[0] = constrain(map(analogRead(potBase),     162, 842, ANG_MIN, ANG_MAX), ANG_MIN, ANG_MAX);
    saida[1] = constrain(map(analogRead(potShoulder), 175, 847, ANG_MIN, ANG_MAX), ANG_MIN, ANG_MAX);
    saida[2] = constrain(map(analogRead(potElbow),    150, 873, ANG_MIN, ANG_MAX), ANG_MIN, ANG_MAX);
    saida[3] = constrain(map(analogRead(potWrist),    862, 118, ANG_MIN, ANG_MAX), ANG_MIN, ANG_MAX);
  }
}

// ==================================================================
//                       RESET DO ESTADO DO PLAYBACK
// ==================================================================
void resetEstadoPlayback() {
  estadoPlay = INICIAR_PASSO;
  passoAtual = 0;
  tInicio = 0;
  for (int i = 0; i < 4; i++) dirPlay[i] = 0;
  dirGripPlay = 0;
  passosMovPlay = 0;
  passoAtualMicro = 0;
  delMicroPlay = 1000;
}

// ==================================================================
//            LEITURA DE COMANDOS DO NODE-RED
// ==================================================================
void checaSerialRemota() {
  if (Serial.available() > 0) {
    String comando = Serial.readStringUntil('\n');
    if (comando.length() > 20) { comando = ""; } 
    comando.trim();

    if (comando == "REMOTO_ON") { 
      modoRemoto = true; 
      Serial.println(F(">>> Modo: REMOTO (Controle pelo Node-RED)")); 
    }
    else if (comando == "REMOTO_OFF") { 
      modoRemoto = false; 
      Serial.println(F(">>> Modo: LOCAL (Controle pela Miniatura)")); 
    }
    
    // Controles remotos de gravação/play
    else if (comando == "GRAVAR" && modoRemoto && mode != PLAY) {
      mode = RECORD; arrayMax = 0; arrayStep = 0; lastRecordMillis = millis();
      Serial.println(F(">>> Comando Remoto: GRAVAR"));
    }
    else if (comando == "PLAY" && modoRemoto && arrayMax > 0) {
      mode = PLAY; pausado = false; aproximando = true; resetEstadoPlayback(); arrayStep = 0; steps = 0;
      Serial.println(F(">>> Comando Remoto: PLAY"));
    }
    else if (comando == "PAUSE" && modoRemoto && mode == PLAY && !aproximando) {
      pausado = !pausado;
      Serial.println(pausado ? F(">>> Comando Remoto: PAUSADO") : F(">>> Comando Remoto: RETOMADO"));
    }
    else if (comando == "STOP" && modoRemoto) {
      if (mode == PLAY || mode == RECORD) { mode = RETORNANDO; pausado = false; aproximando = false; }
      Serial.println(F(">>> Comando Remoto: STOP"));
    }
    
    // Sliders
    else if (comando.startsWith("B:")) alvoRemoto[0] = constrain(comando.substring(2).toInt(), 0, 180);
    else if (comando.startsWith("O:")) alvoRemoto[1] = constrain(comando.substring(2).toInt(), 0, 180);
    else if (comando.startsWith("C:")) alvoRemoto[2] = constrain(comando.substring(2).toInt(), 0, 180);
    else if (comando.startsWith("P:")) alvoRemoto[3] = constrain(comando.substring(2).toInt(), 0, 180);
    else if (comando.startsWith("G:")) gripRemotoClosed = (comando.substring(2).toInt() < 135);
  }
}

// ==================================================================
//                       SETUP
// ==================================================================
void setup() {
  Serial.begin(115200);

  pinMode(btnMode, INPUT_PULLUP);
  pinMode(btnGrip, INPUT_PULLUP);
  pinMode(btnStop, INPUT_PULLUP);

  pwm.begin();
  pwm.setPWMFreq(FREQ);
  delay(10);

  for (int i = 0; i < 4; i++) escreveServo(i, ist[i]);
  escreveServo(CH_GRIP, istGrip);

  // --- MENSAGEM DE SAUDAÇÃO ---
  Serial.println(F("====================================="));
  Serial.println(F("  BRACO ROBOTICO - VERSAO HIBRIDA"));
  Serial.println(F("====================================="));
  Serial.println(F("  BOTOES FISICOS (LOCAL):"));
  Serial.println(F("  MODE  1x = GRAVAR"));
  Serial.println(F("  MODE  2x = PLAY"));
  Serial.println(F("  GRIP  1x = abre/fecha garra"));
  Serial.println(F("  STOP  1x = PAUSE (no PLAY)"));
  Serial.println(F("  STOP  2x = volta ao MANUAL"));
  Serial.println(F("-------------------------------------"));
  Serial.println(F("  CONTROLE REMOTO (NODE-RED):"));
  Serial.println(F("  Ative o botao REMOTO ON no painel."));
  Serial.println(F("====================================="));
  Serial.println();
}

// ==================================================================
//                       LOOP
// ==================================================================
void loop() {
  checaSerialRemota();
  trataBotoes();

  switch (mode) {
    case MANUAL:     modoManual();     break;
    case RECORD:     modoGravacao();   break;
    case PLAY:       modoPlayback();   break;
    case RETORNANDO: modoRetornando(); break;
  }
}

// ==================================================================
//                       BOTÕES FÍSICOS
// ==================================================================
void trataBotoes() {
  unsigned long now = millis();

  // BOTÃO MODE
  bool m = digitalRead(btnMode);
  if (m == LOW && lastModeBtn == HIGH) {
    delay(30);
    if (digitalRead(btnMode) == LOW) {
      if (modeBtnPendente && (now - lastModePress < JANELA_DUPLO)) {
        modeBtnPendente = false;
        if (arrayMax > 0) {
          mode = PLAY; pausado = false; aproximando = true; resetEstadoPlayback();
          arrayStep = 0; steps = 0; lastStepMicros = micros();
          Serial.println(F(">>> Modo: PLAY"));
        }
      } else {
        modeBtnPendente = true; lastModePress = now;
      }
    }
  }
  lastModeBtn = m;

  if (modeBtnPendente && (now - lastModePress > JANELA_DUPLO)) {
    modeBtnPendente = false; mode = RECORD; pausado = false; aproximando = false;
    arrayMax = 0; arrayStep = 0; lastRecordMillis = millis();
    Serial.println(F(">>> Modo: GRAVACAO"));
  }

  // BOTÃO STOP
  bool s = digitalRead(btnStop);
  if (s == LOW && lastStopBtn == HIGH) {
    delay(30);
    if (digitalRead(btnStop) == LOW) {
      if (stopBtnPendente && (now - lastStopPress < JANELA_DUPLO_STOP)) {
        stopBtnPendente = false;
        if (mode == PLAY || mode == RECORD) {
          mode = RETORNANDO; pausado = false; aproximando = false;
          Serial.println(F(">>> STOP -> retornando ao MANUAL"));
        }
      } else {
        stopBtnPendente = true; lastStopPress = now;
      }
    }
  }
  lastStopBtn = s;

  if (stopBtnPendente && (now - lastStopPress > JANELA_DUPLO_STOP)) {
    stopBtnPendente = false;
    if (mode == PLAY && !aproximando) { 
      pausado = !pausado; 
      Serial.println(pausado ? F(">>> PLAY pausado") : F(">>> PLAY retomado"));
    }
  }

  // BOTÃO GRIP
  bool g = digitalRead(btnGrip);
  if (g == LOW && lastGripBtn == HIGH) {
    delay(30);
    if (digitalRead(btnGrip) == LOW) { 
      gripClosed = !gripClosed; 
      Serial.println(gripClosed ? F("Garra: FECHADA") : F("Garra: ABERTA"));
    }
  }
  lastGripBtn = g;
}

// ==================================================================
//                       MODO MANUAL
// ==================================================================
void modoManual() {
  static unsigned long last = 0;
  if (millis() - last < 20) return;
  last = millis();

  int alvo[4];
  lePotenciometros(alvo);

  for (int i = 0; i < 4; i++) {
    ist[i] += (alvo[i] - ist[i]) * 0.15;
    escreveServo(i, ist[i]);
  }

  float gripAlvo = gripClosed ? GRIP_FECHADA : GRIP_ABERTA;
  istGrip += (gripAlvo - istGrip) * 0.15;
  escreveServo(CH_GRIP, istGrip);
}

// ==================================================================
//                       MODO RETORNANDO
// ==================================================================
void modoRetornando() {
  static unsigned long last = 0;
  if (millis() - last < 20) return;
  last = millis();

  int alvo[4];
  lePotenciometros(alvo);

  for (int i = 0; i < 4; i++) {
    ist[i] += (alvo[i] - ist[i]) * 0.08;
    escreveServo(i, ist[i]);
  }

  float gripAlvo = gripClosed ? GRIP_FECHADA : GRIP_ABERTA;
  istGrip += (gripAlvo - istGrip) * 0.08;
  escreveServo(CH_GRIP, istGrip);

  bool chegou = true;
  for (int i = 0; i < 4; i++) {
    if (fabs(alvo[i] - ist[i]) > 2.0) { chegou = false; break; }
  }
  if (fabs(gripAlvo - istGrip) > 2.0) chegou = false;

  if (chegou) { 
    mode = MANUAL; 
    Serial.println(F(">>> MANUAL (transicao concluida)"));
  }
}

// ==================================================================
//                       MODO GRAVAÇÃO
// ==================================================================
void modoGravacao() {
  static unsigned long last = 0;
  if (millis() - last < 30) return;
  last = millis();

  int alvo[4];
  lePotenciometros(alvo);
  for (int i = 0; i < 4; i++) {
    ist[i] = alvo[i];
    escreveServo(i, ist[i]);
  }

  float gripAlvo = gripClosed ? GRIP_FECHADA : GRIP_ABERTA;
  istGrip += (gripAlvo - istGrip) * 0.15;
  escreveServo(CH_GRIP, istGrip);

  bool gravouAgora = false;
  unsigned long agora = millis();
  unsigned long deltaT = agora - lastRecordMillis;

  if (deltaT >= 200) gravouAgora = true;

  if (gravouAgora && arrayStep < MAX_STEPS) {
    byte t = (deltaT > 255) ? 255 : (byte)deltaT;
    tempoPasso[arrayStep] = t;

    joint0[arrayStep]    = (byte)constrain((int)ist[0], 0, 255);
    joint1[arrayStep]    = (byte)constrain((int)ist[1], 0, 255);
    joint2[arrayStep]    = (byte)constrain((int)ist[2], 0, 255);
    joint3[arrayStep]    = (byte)constrain((int)ist[3], 0, 255);
    jointGrip[arrayStep] = (byte)constrain((int)istGrip, 0, 255);

    arrayStep++;
    arrayMax = arrayStep;
    lastRecordMillis = agora;

    Serial.print(F("Passo ")); Serial.print(arrayStep);
    Serial.print(F(" (dt=")); Serial.print(t);
    Serial.print(F("ms, garra=")); Serial.print((int)istGrip);
    Serial.println(F(")"));
  }
}

// ==================================================================
//                       MODO PLAYBACK
// ==================================================================
void modoPlayback() {
  if (arrayMax == 0) { mode = MANUAL; return; }
  if (pausado) { lastStepMicros = micros(); return; }

  if (aproximando) {
    static unsigned long lastAprox = 0;
    if (millis() - lastAprox < 20) return;
    lastAprox = millis();

    int alvoAprox[4] = {joint0[0], joint1[0], joint2[0], joint3[0]};
    int alvoGripAprox = jointGrip[0];

    for (int i = 0; i < 4; i++) {
      ist[i] += (alvoAprox[i] - ist[i]) * 0.08;
      escreveServo(i, ist[i]);
    }
    istGrip += (alvoGripAprox - istGrip) * 0.08;
    escreveServo(CH_GRIP, istGrip);

    bool chegou = true;
    for (int i = 0; i < 4; i++) {
      if (fabs(alvoAprox[i] - ist[i]) > 2.0) { chegou = false; break; }
    }
    if (fabs(alvoGripAprox - istGrip) > 2.0) chegou = false;

    if (chegou) {
      aproximando = false; resetEstadoPlayback(); arrayStep = 0; steps = 0; lastStepMicros = micros();
      Serial.println(F(">>> PLAY iniciando"));
    }
    return;
  }

  unsigned long agoraMs = millis();
  unsigned long agoraUs = micros();

  switch (estadoPlay) {
    case INICIAR_PASSO: {
      if (passoAtual >= arrayMax) { passoAtual = 0; Serial.println(F("-- loop --")); }
      int alvo[4] = {joint0[passoAtual], joint1[passoAtual], joint2[passoAtual], joint3[passoAtual]};
      int alvoGrip = jointGrip[passoAtual];

      float maiorDist = 0;
      for (int i = 0; i < 4; i++) {
        float d = fabs(alvo[i] - ist[i]);
        if (d > maiorDist) maiorDist = d;
      }
      float dGrip = fabs(alvoGrip - istGrip);
      if (dGrip > maiorDist) maiorDist = dGrip;

      if (maiorDist < 1.0) { tInicio = agoraMs; estadoPlay = ESPERANDO; break; }

      if (maiorDist < 5)       passosMovPlay = 20;
      else if (maiorDist < 20) passosMovPlay = 50;
      else                     passosMovPlay = 100;

      for (int i = 0; i < 4; i++) dirPlay[i] = (alvo[i] - ist[i]) / (float)passosMovPlay;
      dirGripPlay = (alvoGrip - istGrip) / (float)passosMovPlay;

      int t = tempoPasso[passoAtual];
      if (t < 50) t = 50;
      delMicroPlay = (t * 1000) / passosMovPlay;
      if (delMicroPlay < 500)   delMicroPlay = 500;
      if (delMicroPlay > 20000) delMicroPlay = 20000;

      passoAtualMicro = 0; tInicio = agoraMs; estadoPlay = MOVENDO;
      break;
    }

    case MOVENDO: {
      if (agoraUs - lastStepMicros >= (unsigned long)delMicroPlay) {
        lastStepMicros = agoraUs;
        for (int i = 0; i < 4; i++) {
          ist[i] += dirPlay[i];
          escreveServo(i, ist[i]);
        }
        istGrip += dirGripPlay;
        escreveServo(CH_GRIP, istGrip);
        passoAtualMicro++;
      }

      if (passoAtualMicro >= passosMovPlay) {
        sol[0] = joint0[passoAtual]; sol[1] = joint1[passoAtual];
        sol[2] = joint2[passoAtual]; sol[3] = joint3[passoAtual];
        solGrip = jointGrip[passoAtual];

        for (int i = 0; i < 4; i++) { ist[i] = sol[i]; escreveServo(i, ist[i]); }
        istGrip = solGrip; escreveServo(CH_GRIP, istGrip);

        estadoPlay = ESPERANDO;
      }
      break;
    }

    case ESPERANDO: {
      int t = tempoPasso[passoAtual];
      if (t < 50) t = 50;
      if (agoraMs - tInicio >= (unsigned long)t) {
        passoAtual++;
        estadoPlay = INICIAR_PASSO;
      }
      break;
    }
  }
}