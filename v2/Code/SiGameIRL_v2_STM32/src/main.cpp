#include <Arduino.h>

const uint8_t PIN_PLAYERS[8] = {PB0, PA1, PA2, PA3, PB4, PB5, PB6, PA7}; // !!! SETUP THIS WITH YOUR SOLDERED BUTTONS (MUST BE IN PB0, PA1, PA2, PA3, PB4, PB5, PB6, PA7)

const uint8_t PIN_JUDGE_MASTER = PA15; // !!! SETUP THIS WITH YOUR SOLDERED BUTTONS (MUST BE IN PB10, PB11, PA15)
const uint8_t PIN_JUDGE_SPECIAL = PB10; // !!! SETUP THIS WITH YOUR SOLDERED BUTTONS (MUST BE IN PB10, PB11, PA15)
const uint8_t PIN_JUDGE_RESERVED = PB11; // !!! SETUP THIS WITH YOUR SOLDERED BUTTONS (MUST BE IN PB10, PB11, PA15)
const uint8_t PIN_JUDGE_A_T_F = PA6; // !!! SETUP THIS WITH YOUR SOLDERED BUTTONS (MUST BE IN PA4, PA5, PA6)
const uint8_t PIN_JUDGE_A_P_1_4 = PA4; // !!! SETUP THIS WITH YOUR SOLDERED BUTTONS (MUST BE IN PA4, PA5, PA6)
const uint8_t PIN_JUDGE_A_P_5_8 = PA5; // !!! SETUP THIS WITH YOUR SOLDERED BUTTONS (MUST BE IN PA4, PA5, PA6)

#define PC Serial
const uint32_t PC_BAUDTRATE = 115200;
#define ARD Serial1
const uint32_t ARD_BAUDTRATE = 115200;

#define COLOR_QUESTION 0x6600FF // purple
#define COLOR_ANSWERING 0x0000FF // blue 
#define COLOR_JUDGEMENT 0xFFFF00 // yellow
#define COLOR_ANSWER_TRUE 0x00FF00 // green
#define COLOR_ANSWER_FALSE 0xFF0000 // red

#define MODE_INITIALIZATION 100
#define MODE_SETTINGS 0
#define MODE_READY 1
#define MODE_QUESTION_NORMAL 2
#define MODE_QUESTION_CAT_SELECT_PLAYER 3
#define MODE_ANSWERING 6
#define MODE_JUDGEMENT 7
#define MODE_ROUND_END 8
#define MODE_RANDOM_SELECTING_PLAYERS 20
#define MODE_RANDOM_DO 21
uint8_t mode = MODE_INITIALIZATION;
bool isCatQuestion = false;
uint8_t modeOld = 255;
int answeringPlayer = -1;
int answeringPlayerOld = -1;

#define ROUND_END_STATUS_TIME_IS_UP 0
#define ROUND_END_STATUS_SKIPPED 1
#define ROUND_END_STATUS_ANSWERED 2
#define ROUND_END_STATUS_CAT_FALSE 3
uint8_t roundEndStatus = ROUND_END_STATUS_TIME_IS_UP;

class Button {
  private:
    bool isPressedOld = false;
  public:
    uint8_t pin;
    bool isPressed = false;
    bool isClicked = false;
    uint32_t pressTime = 0;
    uint32_t releaseTime = 0;
    uint32_t debounceTime = 50000; // 50 ms

    Button () {}
    Button(uint8_t inPin) {
      pin = inPin;
    };

    void readState() {
      isPressed = !digitalRead(pin);

      if (isPressed && !isPressedOld) {
        pressTime = micros();
      } else {
        if (!isPressed && isPressedOld) {
          releaseTime = micros();
          if (releaseTime - pressTime > debounceTime)
            isClicked = true;
        }
      }
      
      isPressedOld = isPressed;
    };

    bool clickNotProcessed() {
      if (isClicked) {
        isClicked = false;
        return true;
      } else
        return false;      
    };
};

class ButtonAnalog {
  private:
    uint16_t buf;
    bool isPressedOld = false;
  public:
    uint8_t pin;
    uint16_t analogMin;
    uint16_t analogMax;
    bool isPressed = false;
    bool isClicked = false;
    uint32_t pressTime = 0;
    uint32_t releaseTime = 0;
    uint32_t debounceTime = 50000; // 100 ms

    ButtonAnalog(uint8_t inPin, uint16_t inValueMin, uint16_t inValueMax) {
      pin = inPin;
      analogMin = inValueMin;
      analogMax = inValueMax;
    };

    void readState() {
      buf = analogRead(pin);
      if (buf >= analogMin && buf <= analogMax)
        isPressed = true;
      else
        isPressed = false;

      if (isPressed && !isPressedOld) {
        pressTime = micros();
      } else {
        if (!isPressed && isPressedOld) {
          releaseTime = micros();
          if (releaseTime - pressTime > debounceTime)
            isClicked = true;
        }
      }
      
      isPressedOld = isPressed;
    };

    bool clickNotProcessed() {
      if (isClicked) {
        isClicked = false;
        return true;
      } else
        return false;      
    };
};

class Player {
  public:
    Button button;
    uint32_t falseStartPenaltyTimer = 0;
    bool falseStartPenalty = false;
    bool canAnswer = false;
    uint8_t playerIndex = 255;
    uint8_t playerActive = false;

    Player() {};

    void reset() {
      canAnswer = true;
      falseStartPenalty = false;
    }
};
Player players[8];
uint8_t activePlayerCount = 0;

Button jbtnMaster = Button(PIN_JUDGE_MASTER);
Button jbtnSpecial = Button(PIN_JUDGE_SPECIAL);
Button jbtnReserved = Button(PIN_JUDGE_RESERVED);
ButtonAnalog jbtnTrue = ButtonAnalog(PIN_JUDGE_A_T_F, 400, 620);
ButtonAnalog jbtnFalse = ButtonAnalog(PIN_JUDGE_A_T_F, 900, 1023);
ButtonAnalog jbtnPS[8] = {
  ButtonAnalog(PIN_JUDGE_A_P_1_4, 200, 290),
  ButtonAnalog(PIN_JUDGE_A_P_1_4, 290, 480),
  ButtonAnalog(PIN_JUDGE_A_P_1_4, 480, 700),
  ButtonAnalog(PIN_JUDGE_A_P_1_4, 700, 1023),
  ButtonAnalog(PIN_JUDGE_A_P_5_8, 200, 290),
  ButtonAnalog(PIN_JUDGE_A_P_5_8, 290, 480),
  ButtonAnalog(PIN_JUDGE_A_P_5_8, 480, 700),
  ButtonAnalog(PIN_JUDGE_A_P_5_8, 700, 1023)
};


const uint32_t questionSecondsMax = 120;
uint32_t questionSeconds = 10;
uint32_t questionSecondsLeft;
uint32_t questionMsTimer;
uint32_t questionMsTimerLeft;

const uint32_t answerSecondsMax = 120;
uint32_t answerSeconds = 30;
uint32_t answerSecondsLeft;
uint32_t answerMsTimer;

const uint32_t judgeSecondsMax = 120;
uint32_t judgeSeconds = 120;
uint32_t judgeSecondsLeft;
uint32_t judgeMsTimer;

const uint32_t falseStartPenaltySecondsMax = 5;
uint32_t falseStartPenaltySeconds = 2;
bool falseStartPenaltyEnabled = true;
uint32_t falseStartPenaltyDelay = 2000;

void processPlayer(uint8_t playerIndex) {
  players[playerIndex].button.isPressed = !digitalRead(PIN_PLAYERS[playerIndex]);
  if (players[playerIndex].button.isPressed) {
    players[playerIndex].button.pressTime = micros();
    if (mode == MODE_READY) {
      if (falseStartPenaltyEnabled) {
        players[playerIndex].falseStartPenalty = true;
        players[playerIndex].falseStartPenaltyTimer = millis() + falseStartPenaltyDelay;
      }
    } else
    if (mode == MODE_QUESTION_NORMAL) {
      if (answeringPlayer == -1) {
        if (!players[playerIndex].falseStartPenalty && players[playerIndex].canAnswer)
          answeringPlayer = playerIndex; // attention
      }
    } else
    if (mode == MODE_ANSWERING || mode == MODE_JUDGEMENT)
    {
      if (falseStartPenaltyEnabled && answeringPlayer != playerIndex) { // attention
        players[playerIndex].falseStartPenalty = true;
        players[playerIndex].falseStartPenaltyTimer = millis() + falseStartPenaltyDelay;
      }
    }    
  }
  else {
    players[playerIndex].button.releaseTime = micros();
    players[playerIndex].button.isClicked = true;
  }
}

void P0_INT() {
  processPlayer(0);
}
void P1_INT() {
  processPlayer(1);
}
void P2_INT() {
  processPlayer(2);
}
void P3_INT() {
  processPlayer(3);
}
void P4_INT() {
  processPlayer(4);
}
void P5_INT() {
  processPlayer(5);
}
void P6_INT() {
  processPlayer(6);
}
void P7_INT() {
  processPlayer(7);
}

void JMASTER_INT() {
  jbtnMaster.readState();
}
void JSPECIAL_INT() {
  jbtnSpecial.readState();
}
void JRESERVED_INT() {
  jbtnReserved.readState();
}

void jbuttonsCheck() {
  jbtnTrue.readState();
  jbtnFalse.readState();
  for (uint8_t i = 0; i < 8; i++)
    jbtnPS[i].readState();
}

void sendToLeds(uint8_t needToLight, uint32_t color) {
  ARD.print('L');
  ARD.print((char)needToLight);
  ARD.print((char)((color >> 16) & 0xFF));
  ARD.print((char)((color >> 8) & 0xFF));
  ARD.println((char)(color & 0xFF));
}
void sendText(String str) {
  ARD.print('T');
  ARD.println(str);
}
void sendReset() {
  ARD.println('R');
}
void sendPlayer(uint8_t p, char state){
  ARD.print('P');
  ARD.print((char)p);
  ARD.println(state);
}


bool rEnable = true;
uint32_t displayUpdateTimer;
const uint32_t displayUpdateDelay = 1000; // ms
uint32_t ledsUpdateTimer;
uint32_t ledsUpdateDelay;

uint8_t ledstripLength = 0;
uint8_t initNeed = 1;
void modeInitialization() {
  if (ARD.available() > 3) {
    char cmd = ARD.read();
    if (cmd == 'L') {
      uint8_t k = 100;
      for (uint8_t i = 0; i < 3; i++) {
        ledstripLength += (ARD.read() - '0') * k;
        k /= 10;
      }
      initNeed--;
      ARD.print('K');
      PC.print("Log: ledstrip length "); PC.println(ledstripLength);
    }  
  }
  if (initNeed == 0) {
    mode = MODE_SETTINGS;
    digitalWrite(PC13, 0);
  }
}

uint8_t modeSettingsStage = 0;
bool modeSettingsUpdateDisplay = true;
uint8_t playerIndex = 0;
void modeSettings() {
  jbuttonsCheck();
  switch (modeSettingsStage) {
    case 0: 
      if (modeSettingsUpdateDisplay) {
        modeSettingsUpdateDisplay = false;
        String str = "Cntdn   ";
        str[5] = (questionSeconds / 100)+'0';
        str[6] = ((questionSeconds / 10) % 10)+'0';
        str[7] = (questionSeconds % 10)+'0';
        sendText(str);
      }
      if (jbtnTrue.clickNotProcessed()) {
        questionSeconds += 5;
        if (questionSeconds > questionSecondsMax)
          questionSeconds = questionSecondsMax;
        modeSettingsUpdateDisplay = true;
      }
      if (jbtnFalse.clickNotProcessed()) {
        questionSeconds -= 5;
        if (questionSeconds < 5)
          questionSeconds = 5;
        modeSettingsUpdateDisplay = true;
      }
      if (jbtnMaster.clickNotProcessed()) {
        modeSettingsStage++;
        modeSettingsUpdateDisplay = true;
      }
      break;
    case 1: 
      if (modeSettingsUpdateDisplay) {
        modeSettingsUpdateDisplay = false;
        String str = "AnS     ";
        str[5] = (answerSeconds / 100)+'0';
        str[6] = ((answerSeconds / 10) % 10)+'0';
        str[7] = (answerSeconds % 10)+'0';
        sendText(str);
      }
      if (jbtnTrue.clickNotProcessed()) {
        answerSeconds += 5;
        if (answerSeconds > answerSecondsMax)
          answerSeconds = answerSecondsMax;
        modeSettingsUpdateDisplay = true;
      }
      if (jbtnFalse.clickNotProcessed()) {
        answerSeconds -= 5;
        if (answerSeconds < 5)
          answerSeconds = 5;
        modeSettingsUpdateDisplay = true;
      }
      if (jbtnMaster.clickNotProcessed()) {
        modeSettingsStage++;
        modeSettingsUpdateDisplay = true;
      }
      break;
    case 2: 
      if (modeSettingsUpdateDisplay) {
        modeSettingsUpdateDisplay = false;
        String str = "JudgE   ";
        str[5] = (judgeSeconds / 100)+'0';
        str[6] = ((judgeSeconds / 10) % 10)+'0';
        str[7] = (judgeSeconds % 10)+'0';
        sendText(str);
      }
      if (jbtnTrue.clickNotProcessed()) {
        judgeSeconds += 5;
        if (judgeSeconds > judgeSecondsMax)
          judgeSeconds = judgeSecondsMax;
        modeSettingsUpdateDisplay = true;
      }
      if (jbtnFalse.clickNotProcessed()) {
        judgeSeconds -= 5;
        if (judgeSeconds < 5)
          judgeSeconds = 5;
        modeSettingsUpdateDisplay = true;
      }
      if (jbtnMaster.clickNotProcessed()) {
        modeSettingsStage++;
        modeSettingsUpdateDisplay = true;
      }
      break;
    case 3: 
      if (modeSettingsUpdateDisplay) {
        modeSettingsUpdateDisplay = false;
        String str = "PEn     ";
        str[7] = falseStartPenaltySeconds +'0';
        sendText(str);
      }
      if (jbtnTrue.clickNotProcessed()) {
        falseStartPenaltySeconds += 1;
        if (falseStartPenaltySeconds > falseStartPenaltySecondsMax)
          falseStartPenaltySeconds = falseStartPenaltySecondsMax;
        modeSettingsUpdateDisplay = true;
      }
      if (jbtnFalse.clickNotProcessed()) {
        if (falseStartPenaltySeconds > 0)
          falseStartPenaltySeconds -= 1;
        falseStartPenaltyEnabled = !(falseStartPenaltySeconds == 0);
        modeSettingsUpdateDisplay = true;
      }
      if (jbtnMaster.clickNotProcessed()) {
        modeSettingsStage++;
        modeSettingsUpdateDisplay = true;
      }
      break;
    case 4:
      if (modeSettingsUpdateDisplay) {
        modeSettingsUpdateDisplay = false;
        String str = "PLAYErS. ";
        str[8] = playerIndex +'0';
        sendText(str);
      }
      for (uint8_t i = 0; i < 8; i++) {
        if (!players[i].playerActive && players[i].button.clickNotProcessed()) {
          players[i].playerActive = true;
          players[i].playerIndex = playerIndex;
          sendPlayer(i, 'F');
          playerIndex++;
          modeSettingsUpdateDisplay = true;
        }
      }
      if (jbtnMaster.clickNotProcessed()) {
        for (uint8_t i = 0; i < 8; i++){
          if (players[i].playerActive)
            activePlayerCount++;
        }
        if (activePlayerCount == 0) {
          activePlayerCount = 8;
          for (uint8_t i = 0; i < 8; i++) {
            players[i].playerActive = true;
            players[i].playerIndex = playerIndex;
            playerIndex++;
          }
        }
        for (uint8_t i = 0; i < 8; i++) {
          if (!players[i].playerActive) {
            players[i].playerIndex = playerIndex;
            playerIndex++;
          }
        }
        randomSeed(micros());
        modeSettingsStage++;
        modeSettingsUpdateDisplay = true;
      }
      break;
  }
  if (modeSettingsStage == 5) {
    modeSettingsStage = 0;
    mode = MODE_READY;
    rEnable = true;
  }
}

void falsestartWork() {
  if (falseStartPenaltyEnabled) {
    for (uint8_t i = 0; i < 8; i++) {
      if (players[i].falseStartPenalty && millis() > players[i].falseStartPenaltyTimer) {
        players[i].falseStartPenalty = false;
      }
    }
  }
}

void blinking(uint32_t color) {
  for (uint8_t j = 0; j < 5; j++) {
    sendToLeds(ledstripLength, color);
    delay(100);
    sendToLeds(0, 0);
    delay(100);
  }
}

uint32_t buttonBattleTimer;
const uint32_t buttonBattleDelay = 100;
void modeReady() {
  if (rEnable) {
    rEnable = false;
    sendReset();
    if (answeringPlayerOld == -1) {
      bool rndGreat = false;
      while (!rndGreat){
        answeringPlayerOld = random(0, 7);
        if (players[answeringPlayerOld].playerActive)
          rndGreat = true;
      }
    }
    sendPlayer(answeringPlayerOld, 'B');
    jbtnMaster.clickNotProcessed();
    jbtnSpecial.clickNotProcessed();
    jbtnReserved.clickNotProcessed();
    String str = "rEAdY   ";
    str[7] = players[answeringPlayerOld].playerIndex +'1';
    sendText(str);
    for (uint8_t i = 0; i < 8; i++) {
      players[i].reset();
    }
  }
  if (jbtnMaster.clickNotProcessed()) {
    mode = MODE_QUESTION_NORMAL;
    isCatQuestion = false;
    rEnable = true;
    questionMsTimer = questionSeconds * 1000 + millis() - 1;
    ledsUpdateDelay = (questionSeconds * 1000) / ledstripLength;
    buttonBattleTimer = millis() + buttonBattleDelay;
    sendReset();
  }
  if (jbtnSpecial.clickNotProcessed()) {
    mode = MODE_QUESTION_CAT_SELECT_PLAYER;
    isCatQuestion = true;
    rEnable = true;
    sendReset();
  }
  if (jbtnReserved.clickNotProcessed()){
    if (jbtnReserved.releaseTime - jbtnReserved.pressTime > 1000000) {
      mode = MODE_RANDOM_SELECTING_PLAYERS;
      rEnable = true;
    }
  }
  falsestartWork();
}

void modeQuestionNormal() {
  // leds work
  if (millis() >= ledsUpdateTimer) {
    ledsUpdateTimer = millis() + ledsUpdateDelay;
    uint8_t ledsNeedLight = ((questionMsTimer - millis()) / (questionSeconds * 10) * ledstripLength) / 100 + 1;
    if (ledsNeedLight > ledstripLength)
      ledsNeedLight = ledstripLength;
    sendToLeds(ledsNeedLight, COLOR_QUESTION);
  }
  // display work
  if (millis() >= displayUpdateTimer) {
    displayUpdateTimer = millis() + displayUpdateDelay;
    questionSecondsLeft = (questionMsTimer - millis()) / 1000;
    String str = "Cnt.dn.   ";
    str[7] = (questionSecondsLeft / 100)+'0';
    str[8] = ((questionSecondsLeft / 10) % 10)+'0';
    str[9] = (questionSecondsLeft % 10)+'0';
    sendText(str);
  }
  // time is up work
  if (millis() > questionMsTimer) {
    mode = MODE_ROUND_END;
    rEnable = true;
    roundEndStatus = ROUND_END_STATUS_TIME_IS_UP;
    sendReset();
  }
  // skip work
  if (jbtnReserved.clickNotProcessed()){
    mode = MODE_ROUND_END;
    rEnable = true;
    roundEndStatus = ROUND_END_STATUS_SKIPPED;
    sendReset();
  }  
  // check answering player
  if (answeringPlayer != -1) {
    mode = MODE_ANSWERING;
    jbtnFalse.clickNotProcessed();
    jbtnTrue.clickNotProcessed();
    sendPlayer(answeringPlayer, 'S');      
    displayUpdateTimer = 0;
    ledsUpdateTimer = 0;
    ledsUpdateDelay = (answerSeconds * 1000) / ledstripLength;
    questionMsTimerLeft = questionMsTimer - millis();
    answerMsTimer = answerSeconds * 1000 + millis() - 1;
    rEnable = true;
  }
  // check all bad
  uint8_t cantAnswerPlayerCount = 0;
  for (uint8_t i = 0; i < 8; i++)
    if (!players[i].canAnswer)
      cantAnswerPlayerCount++;
  if (cantAnswerPlayerCount == activePlayerCount){
    mode = MODE_ROUND_END;
    rEnable = true;
    roundEndStatus = ROUND_END_STATUS_SKIPPED;
    sendReset();
  }
  // false start work
  falsestartWork();
}

uint8_t selectedPlayerIndex = 255;
uint8_t selectedPlayer = 255;
void modeQuestionCatSelectPlayer() {
  if (rEnable) {
    rEnable = false;
    selectedPlayerIndex = 255;
    selectedPlayer = 255;
    String str = "CAt.Sel.P.";
    sendText(str);
  }
  jbuttonsCheck();
  for (uint8_t i = 0; i < 8; i++) {
    if (jbtnPS[i].clickNotProcessed()) {
      selectedPlayerIndex = i;
    }
  }
  for (uint8_t i = 0; i < 8; i++) {
    if (players[i].playerIndex == selectedPlayerIndex) {
      if (selectedPlayer != i){
        if (selectedPlayer != 255)
          sendPlayer(selectedPlayer, 'R');
        selectedPlayer = i;
        sendPlayer(i, 'S');
        String str = "CAt.Sel.P. ";
        str[10] = selectedPlayerIndex + '1';
        sendText(str);
      }
    }
  }
  if (selectedPlayer != 255) {
    if (jbtnMaster.clickNotProcessed()){
      mode = MODE_ANSWERING;
      answeringPlayer = selectedPlayer;
      answeringPlayerOld = selectedPlayer;
      jbtnFalse.clickNotProcessed();
      jbtnTrue.clickNotProcessed();
      sendPlayer(answeringPlayer, 'S');
      displayUpdateTimer = 0;
      ledsUpdateTimer = 0;
      ledsUpdateDelay = (answerSeconds * 1000) / ledstripLength;
      questionMsTimerLeft = questionMsTimer - millis();
      answerMsTimer = answerSeconds * 1000 + millis();
      rEnable = true;
    }
  }
}

void answeredFalse() {
  players[answeringPlayer].canAnswer = false;
  sendReset();
  blinking(COLOR_ANSWER_FALSE);
  questionMsTimer = millis() + questionMsTimerLeft;
  displayUpdateTimer = 0;
  ledsUpdateTimer = 0;
  ledsUpdateDelay = (questionSeconds * 1000) / ledstripLength;
  answeringPlayer = -1;
  if (!isCatQuestion)
    mode = MODE_QUESTION_NORMAL;
  else {
    mode = MODE_ROUND_END;
    roundEndStatus = ROUND_END_STATUS_CAT_FALSE;
  }  
}

void answeredTrue() {
  mode = MODE_ROUND_END;
  answeringPlayerOld = answeringPlayer;
  answeringPlayer = -1;
  roundEndStatus = ROUND_END_STATUS_ANSWERED;
}

void modeAnswering() {
  if (rEnable){
    sendPlayer(answeringPlayer, 'S'); 
  }
  // display work
  if (millis() > displayUpdateTimer) {
    displayUpdateTimer = millis() + displayUpdateDelay;
    answerSecondsLeft = (answerMsTimer - millis()) / 1000;
    String str = "An.P     ";
    str[4] = players[answeringPlayer].playerIndex + '1';
    str[6] = (answerSecondsLeft / 100)+'0';
    str[7] = ((answerSecondsLeft / 10) % 10)+'0';
    str[8] = (answerSecondsLeft % 10)+'0';
    sendText(str);
  }
  // leds work
  if (millis() > ledsUpdateTimer) {
    ledsUpdateTimer = millis() + ledsUpdateDelay;
    uint8_t ledsNeedLight = ((((answerMsTimer - millis()) / (answerSeconds * 10)) * ledstripLength) / 100) + 1;
    sendToLeds(ledsNeedLight, COLOR_ANSWERING);
  }
  // buttons work
  jbuttonsCheck();
  if (jbtnTrue.clickNotProcessed()) {
    answeredTrue();
  }
  if (jbtnFalse.clickNotProcessed()) {
    answeredFalse();
  }
  // time is up work
  if (millis() > answerMsTimer) {
    sendReset();
    mode = MODE_JUDGEMENT;
    judgeMsTimer = millis() + (judgeSeconds * 1000) - 1;
    displayUpdateTimer = 0;
    ledsUpdateTimer = 0;
    ledsUpdateDelay = (judgeSeconds * 1000) / ledstripLength;
  }
  // false start work
  falsestartWork();
}

void modeJudgement() {
  // display work
  if (millis() > displayUpdateTimer) {
    displayUpdateTimer = millis() + displayUpdateDelay;
    judgeSecondsLeft = (judgeMsTimer - millis()) / 1000;
    String str = "Jd.P     ";
    str[4] = players[answeringPlayer].playerIndex + '1';
    str[6] = (judgeSecondsLeft / 100)+'0';
    str[7] = ((judgeSecondsLeft / 10) % 10)+'0';
    str[8] = (judgeSecondsLeft % 10)+'0';
    sendText(str);
  }
  // leds work
  if (millis() > ledsUpdateTimer) {
    ledsUpdateTimer = millis() + ledsUpdateDelay;
    uint8_t ledsNeedLight = ((judgeMsTimer - millis()) / (judgeSeconds * 10) * ledstripLength) / 100 + 1;
    sendToLeds(ledsNeedLight, COLOR_JUDGEMENT);
  }
  // buttons work
  jbuttonsCheck();
  if (jbtnTrue.clickNotProcessed()) {
    answeredTrue();
  }
  if (jbtnFalse.clickNotProcessed()) {
    answeredFalse();
  }
  // time is up work
  if (millis() > judgeMsTimer) {
    answeredFalse();
  }
  // false start work
  falsestartWork();
}

void modeRoundEnd() {
  switch (roundEndStatus) {
    case ROUND_END_STATUS_TIME_IS_UP:
      blinking(COLOR_ANSWER_FALSE);
      break;
    case ROUND_END_STATUS_SKIPPED:
      blinking(COLOR_JUDGEMENT);
      break;
    case ROUND_END_STATUS_ANSWERED:
      blinking(COLOR_ANSWER_TRUE);
      break;
    case ROUND_END_STATUS_CAT_FALSE:
      blinking(COLOR_ANSWER_FALSE);
      break;
  }
  mode = MODE_READY;
  rEnable = true;
}

bool playerRandomEnabled[8];
void modeRandomSelectingPlayers() {
  if (rEnable) {
    rEnable = false;
    sendReset();
    String str = "rnd.sel.p ";
    sendText(str);
    for (uint8_t i = 0; i < 8; i++) {
      playerRandomEnabled[i] = false;
    }
  }
  jbuttonsCheck();
  for (uint8_t i = 0; i < 8; i++) {
    if (jbtnPS[i].clickNotProcessed()) {
      playerRandomEnabled[i] = !playerRandomEnabled[i];
      for (uint8_t j = 0; j < 8; j++) {
        if (players[j].playerIndex == i) {
          sendPlayer(j, playerRandomEnabled[i] ? 'B' : 'R');
        }
      }
    }
  }
  if (jbtnMaster.clickNotProcessed()){
    bool canContinue = false;
    for (uint8_t i = 0; i < 8; i++) {
      if (playerRandomEnabled[i]) {
        canContinue = true;
      }
    }
    if (canContinue){
      mode = MODE_RANDOM_DO;
      rEnable = true;
    } else {
      mode = MODE_READY;
      rEnable = true;
    }
  }
}

uint8_t rndResult = 255;
void modeRandomDo() {
  if (rEnable) {
    rEnable = false;
    bool rndGreat = false;
    while (!rndGreat){
      rndResult = random(0, 7);
      if (playerRandomEnabled[rndResult])
        rndGreat = true;
    }
    sendReset();
    String str = "rnd.res.  ";
    str[9] = rndResult + '1';
    sendText(str);
    for (uint8_t j = 0; j < 8; j++) {
      if (players[j].playerIndex == rndResult) {
        sendPlayer(j, 'S');
      }
    }
  }
  jbuttonsCheck();
  if (jbtnTrue.clickNotProcessed()){
    answeringPlayer = rndResult;
    answeringPlayerOld = rndResult;
    mode = MODE_READY;
    rEnable = true;
  }
  if (jbtnFalse.clickNotProcessed()) {
    mode = MODE_READY;
    rEnable = true;
  }
}

void setup() {
  ARD.begin(PC_BAUDTRATE);
  PC.begin(PC_BAUDTRATE);
  pinMode(PC13, OUTPUT);
  digitalWrite(PC13, 1);

  // players init
  for (uint8_t i = 0; i < 8; i++){
    players[i].button.pin = PIN_PLAYERS[i];
    pinMode(PIN_PLAYERS[i], INPUT_PULLUP);
  }
  attachInterrupt(PIN_PLAYERS[0], P0_INT, CHANGE);
  attachInterrupt(PIN_PLAYERS[1], P1_INT, CHANGE);
  attachInterrupt(PIN_PLAYERS[2], P2_INT, CHANGE);
  attachInterrupt(PIN_PLAYERS[3], P3_INT, CHANGE);
  attachInterrupt(PIN_PLAYERS[4], P4_INT, CHANGE);
  attachInterrupt(PIN_PLAYERS[5], P5_INT, CHANGE);
  attachInterrupt(PIN_PLAYERS[6], P6_INT, CHANGE);
  attachInterrupt(PIN_PLAYERS[7], P7_INT, CHANGE);
  
  // judge init
  pinMode(PIN_JUDGE_MASTER, INPUT_PULLUP);
  pinMode(PIN_JUDGE_SPECIAL, INPUT_PULLUP);
  pinMode(PIN_JUDGE_RESERVED, INPUT_PULLUP);
  attachInterrupt(PIN_JUDGE_MASTER, JMASTER_INT, CHANGE);
  attachInterrupt(PIN_JUDGE_SPECIAL, JSPECIAL_INT, CHANGE);
  attachInterrupt(PIN_JUDGE_RESERVED, JRESERVED_INT, CHANGE);
  pinMode(PIN_JUDGE_A_T_F, INPUT_ANALOG);
  pinMode(PIN_JUDGE_A_P_1_4, INPUT_ANALOG);
  pinMode(PIN_JUDGE_A_P_5_8, INPUT_ANALOG);
}

void loop() {
  switch (mode) {
    case MODE_INITIALIZATION: modeInitialization(); break;
    case MODE_SETTINGS: modeSettings(); break;
    case MODE_READY: modeReady(); break;
    case MODE_QUESTION_NORMAL: modeQuestionNormal(); break;
    case MODE_QUESTION_CAT_SELECT_PLAYER: modeQuestionCatSelectPlayer(); break;
    case MODE_ANSWERING: modeAnswering(); break;
    case MODE_JUDGEMENT: modeJudgement(); break;
    case MODE_ROUND_END: modeRoundEnd(); break;
    case MODE_RANDOM_SELECTING_PLAYERS: modeRandomSelectingPlayers(); break;
    case MODE_RANDOM_DO: modeRandomDo(); break;
  }
  modeOld = mode;
}