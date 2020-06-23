#include <Arduino.h>
#include <FastLED.h>
#include <SPI.h>
#include <bitBangedSPI.h>
#include <MAX7219.h>
#include <EEPROM.h>


#define JUP 0
#define JCENTER 1
#define JDOWN 2

#define COLOR_QUESTION 0xFF00FF // purple
#define COLOR_ANSWERING 0x0000FF // blue 
#define COLOR_JUDGEMENT 0xFFFF00 // yellow
#define COLOR_ANSWER_TRUE 0x00FF00 // green
#define COLOR_ANSWER_FALSE 0xFF0000 // red

const uint8_t PIN_BTNS[8] = {2, 3, 4, 5, 6, 7, 8, 9};
const uint8_t PIN_JBTNS[3] = {A0, A1, A2};
const uint8_t PIN_LEDSTRIP = 10;
const uint8_t PIN_MAX7219_DIN = 11;
const uint8_t PIN_MAX7219_CS = 12;
const uint8_t PIN_MAX7219_CLK = 13;

const uint8_t LEDSTRIP_LENGTH = 10;
CRGB leds[LEDSTRIP_LENGTH];

#define PC Serial

class Button {
  #ifndef digitalState
    #define portOfPin(P)\
    (((P)>=0&&(P)<8)?&PORTD:(((P)>7&&(P)<14)?&PORTB:&PORTC))
    #define ddrOfPin(P)\
      (((P)>=0&&(P)<8)?&DDRD:(((P)>7&&(P)<14)?&DDRB:&DDRC))
    #define pinOfPin(P)\
      (((P)>=0&&(P)<8)?&PIND:(((P)>7&&(P)<14)?&PINB:&PINC))
    #define pinIndex(P)((uint8_t)(P>13?P-14:P&7))
    #define pinMask(P)((uint8_t)(1<<pinIndex(P)))

    #define pinAsInput(P) *(ddrOfPin(P))&=~pinMask(P)
    #define pinAsInputPullUp(P) *(ddrOfPin(P))&=~pinMask(P);digitalHigh(P)
    #define pinAsOutput(P) *(ddrOfPin(P))|=pinMask(P)
    #define digitalLow(P) *(portOfPin(P))&=~pinMask(P)
    #define digitalHigh(P) *(portOfPin(P))|=pinMask(P)
    #define isHigh(P)((*(pinOfPin(P))& pinMask(P))>0)
    #define isLow(P)((*(pinOfPin(P))& pinMask(P))==0)
    #define digitalState(P)((uint8_t)isHigh(P))
  #endif

  public:
    uint8_t pin = 2;
    bool inversed = false;
    bool pullup = false;
    bool isPressed = false;
    bool isPressedOld = false;
    uint32_t debounceDelay = 10000; // mis
    uint32_t pressTime; // mis
    uint32_t releaseTime; // mis
    bool isPressProcessed = true;
    bool isReleaseProcessed = true;
    Button(){}
    Button(uint8_t pinIn, bool inversedIn, bool pullupIn){
      pin = pinIn;
      inversed = inversedIn;
      pullup = pullupIn;
    }
    
    bool pressProcessed() {
      if (!isPressProcessed){
        isPressProcessed = true;
        return false;
      }
      return true;
    }
    bool releaseProcessed() {
      if (!isReleaseProcessed){
        isReleaseProcessed = true;
        return false;
      }
      return true;
    }
    bool pressProcessedDebounced() {
      if (!isPressProcessed){
        isPressProcessed = true;
        if (releaseTime - pressTime > debounceDelay)
          return false;
      }
      return true;
    }
    bool releaseProcessedDebounced() {
      if (!isReleaseProcessed){
        isReleaseProcessed = true;
        if (pressTime - releaseTime > debounceDelay)
          return false;
      }
      return true;
    }

    void check() {
      fastRead();
      calculate();
    }

    void fastRead() {
      isPressed = digitalState(pin);
    }

    void calculate() {
      if (inversed)
        isPressed = !isPressed;

      if (isPressed && !isPressedOld) {
        pressTime = micros();
        isReleaseProcessed = false;
      } else
      if (!isPressed && isPressedOld) {
        releaseTime = micros();
        isPressProcessed = false;
      }
      isPressedOld = isPressed;
    }

    void setInput() {
      pinMode(pin, pullup ? INPUT_PULLUP : INPUT);
    }
    void setOutput() {
      pinMode(pin, OUTPUT);
      digitalLow(pin);
    }
};

struct Player {
  Button btn;
  uint32_t falseStartPenaltyTimer = 0;
  bool falseStartPenalty = false;
  bool canAnswer = false;
};

const uint8_t playersCount = 8;
Player players[playersCount];
const uint8_t jbtnsCount = 3;
Button jbtns[jbtnsCount];

void buttonsCheck() {
   for (uint8_t i = 0; i < playersCount; i++) {
    players[i].btn.fastRead();
   }
   for (uint8_t i = 0; i < playersCount; i++) {
    players[i].btn.calculate();
  }
}

void jbuttonsCheck() {
  for (uint8_t i = 0; i < jbtnsCount; i++) {
    jbtns[i].check();
  }
}


MAX7219 display (1, PIN_MAX7219_CS, PIN_MAX7219_DIN, PIN_MAX7219_CLK);  // Chips / LOAD / DIN / CLK

#define MODE_SETTINGS 0
#define MODE_READY 1
#define MODE_QUESTION_COUNTDOWN 2
#define MODE_ANSWERING 3
#define MODE_JUDGEMENT 4
#define MODE_ROUND_END 5
uint8_t mode = MODE_SETTINGS;

#define ROUND_END_STATUS_TIME_IS_UP 0
#define ROUND_END_STATUS_SKIPPED 1
#define ROUND_END_STATUS_ANSWERED 2
uint8_t roundEndStatus = ROUND_END_STATUS_TIME_IS_UP;

#define ANSWER_IS_FALSE 0
#define ANSWER_IS_TRUE 1
uint8_t answerStatus = ANSWER_IS_FALSE;

const uint32_t questionSecondsMax = 120;
uint32_t questionSeconds = 5;
uint32_t questionSecondsLeft;
uint32_t questionMsTimer;
uint32_t questionMsTimerLeft;

const uint32_t answerSecondsMax = 120;
uint32_t answerSeconds = 30;
uint32_t answerSecondsLeft;
uint32_t answerMsTimer;
uint8_t answeringPlayer = playersCount;

const uint32_t judgeSecondsMax = 120;
uint32_t judgeSeconds = 120;
uint32_t judgeSecondsLeft;
uint32_t judgeMsTimer;

const uint32_t falseStartPenaltySecondsMax = 5;
uint32_t falseStartPenaltySeconds = 2;
bool falseStartPenaltyEnabled = true;

uint32_t displayUpdateTimer;
uint32_t displayUpdateDelay = 1000; // ms
uint32_t ledsUpdateTimer;
uint32_t ledsUpdateDelay;

uint8_t modeSettingsStage = 0;
bool modeSettingsUpdateDisplay = true;
void modeSettings() {
  jbuttonsCheck();
  switch (modeSettingsStage) {
    case 0: 
      if (modeSettingsUpdateDisplay) {
        modeSettingsUpdateDisplay = false;
        display.sendString("Cnt.dn.   "); 
        display.sendChar(5, (questionSeconds / 100)+'0', false);
        display.sendChar(6, ((questionSeconds / 10) % 10)+'0', false);
        display.sendChar(7, (questionSeconds % 10)+'0', false);
        PC.print("Question countdown: ");
        PC.println(questionSeconds);
      }
      if (!jbtns[JUP].pressProcessedDebounced()) {
        questionSeconds += 5;
        if (questionSeconds > questionSecondsMax)
          questionSeconds = questionSecondsMax;
        modeSettingsUpdateDisplay = true;
      }
      if (!jbtns[JDOWN].pressProcessedDebounced()) {
        questionSeconds -= 5;
        if (questionSeconds < 5)
          questionSeconds = 5;
        modeSettingsUpdateDisplay = true;
      }
      if (!jbtns[JCENTER].pressProcessedDebounced()) {
        modeSettingsStage++;
        modeSettingsUpdateDisplay = true;
      }
      break;
    case 1: 
      if (modeSettingsUpdateDisplay) {
        modeSettingsUpdateDisplay = false;
        display.sendString("AnS.     "); 
        display.sendChar(5, (answerSeconds / 100)+'0', false);
        display.sendChar(6, ((answerSeconds / 10) % 10)+'0', false);
        display.sendChar(7, (answerSeconds % 10)+'0', false);
        PC.print("Answering countdown: ");
        PC.println(answerSeconds);
      }
      if (!jbtns[JUP].pressProcessedDebounced()) {
        answerSeconds += 5;
        if (answerSeconds > answerSecondsMax)
          answerSeconds = answerSecondsMax;
        modeSettingsUpdateDisplay = true;
      }
      if (!jbtns[JDOWN].pressProcessedDebounced()) {
        answerSeconds -= 5;
        if (answerSeconds < 5)
          answerSeconds = 5;
        modeSettingsUpdateDisplay = true;
      }
      if (!jbtns[JCENTER].pressProcessedDebounced()) {
        modeSettingsStage++;
        modeSettingsUpdateDisplay = true;
      }
      break;
    case 2: 
      if (modeSettingsUpdateDisplay) {
        modeSettingsUpdateDisplay = false;
        display.sendString("JudgE   "); 
        display.sendChar(5, (judgeSeconds / 100)+'0', false);
        display.sendChar(6, ((judgeSeconds / 10) % 10)+'0', false);
        display.sendChar(7, (judgeSeconds % 10)+'0', false);
        PC.print("Judgement countdown: ");
        PC.println(judgeSeconds);
      }
      if (!jbtns[JUP].pressProcessedDebounced()) {
        judgeSeconds += 5;
        if (judgeSeconds > judgeSecondsMax)
          judgeSeconds = judgeSecondsMax;
        modeSettingsUpdateDisplay = true;
      }
      if (!jbtns[JDOWN].pressProcessedDebounced()) {
        judgeSeconds -= 5;
        if (judgeSeconds < 5)
          judgeSeconds = 5;
        modeSettingsUpdateDisplay = true;
      }
      if (!jbtns[JCENTER].pressProcessedDebounced()) {
        modeSettingsStage++;
        modeSettingsUpdateDisplay = true;
      }
      break;
    case 3: 
      if (modeSettingsUpdateDisplay) {
        modeSettingsUpdateDisplay = false;
        display.sendString("PEn.   "); 
        display.sendChar(7, falseStartPenaltySeconds +'0', false);
        PC.print("FalseStart Penalty seconds: ");
        PC.println(falseStartPenaltySeconds);
      }
      if (!jbtns[JUP].pressProcessedDebounced()) {
        falseStartPenaltySeconds += 1;
        if (falseStartPenaltySeconds > falseStartPenaltySecondsMax)
          falseStartPenaltySeconds = falseStartPenaltySecondsMax;
        modeSettingsUpdateDisplay = true;
      }
      if (!jbtns[JDOWN].pressProcessedDebounced()) {
        if (falseStartPenaltySeconds > 0)
          falseStartPenaltySeconds -= 1;
        falseStartPenaltyEnabled = !(falseStartPenaltySeconds == 0);
        modeSettingsUpdateDisplay = true;
      }
      if (!jbtns[JCENTER].pressProcessedDebounced()) {
        modeSettingsStage++;
        modeSettingsUpdateDisplay = true;
      }
      break;
  }
  if (modeSettingsStage == 4) {
    modeSettingsStage = 0;
    mode = MODE_READY;
    display.sendString("rEAdY"); 
    PC.println("READY!");
    EEPROM.put(10, questionSeconds);
    EEPROM.put(20, answerSeconds);
    EEPROM.put(30, judgeSeconds);
    EEPROM.put(40, falseStartPenaltySeconds);
    jbtns[JCENTER].debounceDelay = 100000;
  }
}

void modeReady() {
  jbuttonsCheck();
  if (!jbtns[JCENTER].pressProcessedDebounced()) {
    PC.println("Question!");
    mode = MODE_QUESTION_COUNTDOWN;
    questionMsTimer = questionSeconds * 1000 + millis();
    ledsUpdateDelay = (questionSeconds * 1000) / LEDSTRIP_LENGTH;
    jbtns[JCENTER].debounceDelay = 1000000;
    for (uint8_t i = 0; i < playersCount; i++) {
      players[i].canAnswer = true;
    }
  }

  if (falseStartPenaltyEnabled) {
    buttonsCheck();
    for (uint8_t i = 0; i < playersCount; i++) {
      if (players[i].btn.isPressed) {
        players[i].falseStartPenalty = true;
        players[i].falseStartPenaltyTimer = millis() + (falseStartPenaltySeconds * 1000);
        PC.print("FALSE START for player: ");
        PC.println(i);
        PC.print("for seconds: ");
        PC.println(falseStartPenaltySeconds);
        PC.print("before: ");
        PC.println(players[i].falseStartPenaltyTimer);
      }
    }
    for (uint8_t i = 0; i < playersCount; i++) {
      if (players[i].falseStartPenalty && millis() > players[i].falseStartPenaltyTimer) {
        players[i].falseStartPenalty = false;
        PC.print("FALSE START END for player: ");
        PC.println(i);
      }
    }
  }
}

void modeQuestionCountdown() {
  // display work
  if (millis() > displayUpdateTimer) {
    displayUpdateTimer = millis() + displayUpdateDelay;
    questionSecondsLeft = (questionMsTimer - millis()) / 1000;
    display.sendString("Cnt.dn."); 
    display.sendChar(5, (questionSecondsLeft / 100)+'0', false);
    display.sendChar(6, ((questionSecondsLeft / 10) % 10)+'0', false);
    display.sendChar(7, (questionSecondsLeft % 10)+'0', false);
  }
  // leds work
  if (millis() > ledsUpdateTimer) {
    ledsUpdateTimer = millis() + ledsUpdateDelay;
    uint8_t ledsNeedLight = ((questionMsTimer - millis()) / (questionSeconds * 10) * LEDSTRIP_LENGTH) / 100 + 1;
    for (uint8_t i = 0; i < ledsNeedLight; i++)
      leds[i] = COLOR_QUESTION;
    for (uint8_t i = ledsNeedLight; i < LEDSTRIP_LENGTH; i++)
      leds[i] = 0;
    FastLED.show();
  }
  // buttons work
  buttonsCheck();
  for (uint8_t i = 0; i < playersCount; i++) {
    if (players[i].canAnswer && !players[i].falseStartPenalty && players[i].btn.isPressed) {
      players[i].btn.setOutput();
      mode = MODE_ANSWERING;
      answerMsTimer = answerSeconds * 1000 + millis();
      answeringPlayer = i;
      PC.print("Answering player "); 
      PC.println(i); 
      displayUpdateTimer = 0;
      ledsUpdateTimer = 0;
      ledsUpdateDelay = (answerSeconds * 1000) / LEDSTRIP_LENGTH;
      questionMsTimerLeft = questionMsTimer - millis();
    }
  }
  // skip question work
  jbuttonsCheck();
  if (!jbtns[JCENTER].pressProcessedDebounced()) {
    mode = MODE_ROUND_END;
    roundEndStatus = ROUND_END_STATUS_SKIPPED;
    PC.println("SKIPPED");
  }
  // time is up work
  if (millis() > questionMsTimer) {
    mode = MODE_ROUND_END;
    roundEndStatus = ROUND_END_STATUS_TIME_IS_UP;
    PC.println("TIME IS UP");
  }
  // penalty work
  if (falseStartPenaltyEnabled) {
    for (uint8_t i = 0; i < playersCount; i++) {
      if (players[i].falseStartPenalty && millis() > players[i].falseStartPenaltyTimer) {
        players[i].falseStartPenalty = false;
        PC.print("FALSE START END for player: ");
        PC.println(i);
      }
    }
  }
}

void modeAnswering() {
  // display work
  if (millis() > displayUpdateTimer) {
    displayUpdateTimer = millis() + displayUpdateDelay;
    answerSecondsLeft = (answerMsTimer - millis()) / 1000;
    display.sendString("An.P");
    display.sendChar(3, answeringPlayer + '1', true);
    display.sendChar(5, (answerSecondsLeft / 100)+'0', false);
    display.sendChar(6, ((answerSecondsLeft / 10) % 10)+'0', false);
    display.sendChar(7, (answerSecondsLeft % 10)+'0', false);
  }
  // leds work
  if (millis() > ledsUpdateTimer) {
    ledsUpdateTimer = millis() + ledsUpdateDelay;
    uint8_t ledsNeedLight = (((answerMsTimer - millis()) / (answerSeconds * 10)) * LEDSTRIP_LENGTH) / 100 + 1;
    for (uint8_t i = 0; i < ledsNeedLight; i++)
      leds[i] = COLOR_ANSWERING;
    for (uint8_t i = ledsNeedLight; i < LEDSTRIP_LENGTH; i++)
      leds[i] = 0;
    FastLED.show();
  }
  jbuttonsCheck();
  if (!jbtns[JUP].pressProcessedDebounced()) {
    mode = MODE_ROUND_END;
    roundEndStatus = ROUND_END_STATUS_ANSWERED;
    PC.println("ANSWERING TRUE! +++");
  }
  if (!jbtns[JDOWN].pressProcessedDebounced()) {
    mode = MODE_QUESTION_COUNTDOWN;
    PC.println("ANSWERING FALSE! ---");
    for (uint8_t j = 0; j < 5; j++) {
      for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
        leds[i] = COLOR_ANSWER_FALSE;
      FastLED.show();
      delay(100);
      for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
        leds[i] = 0;
      FastLED.show();
      delay(100);
    }
    players[answeringPlayer].canAnswer = false;
    players[answeringPlayer].btn.setInput();
    questionMsTimer = millis() + questionMsTimerLeft;
    displayUpdateTimer = 0;
    ledsUpdateTimer = 0;
    ledsUpdateDelay = (questionSeconds * 1000) / LEDSTRIP_LENGTH;
  }
  // time is up work
  if (millis() > answerMsTimer) {
    mode = MODE_JUDGEMENT;
    judgeMsTimer = millis() + (judgeSeconds * 1000) - 1;
    displayUpdateTimer = 0;
    ledsUpdateTimer = 0;
    ledsUpdateDelay = (judgeSeconds * 1000) / LEDSTRIP_LENGTH;
    PC.println("JUDGEMENT");
  }
}

void modeJudgement() {
  // display work
  if (millis() > displayUpdateTimer) {
    displayUpdateTimer = millis() + displayUpdateDelay;
    judgeSecondsLeft = (judgeMsTimer - millis()) / 1000;
    display.sendString("Jd.P");
    display.sendChar(3, answeringPlayer + '1', true);
    display.sendChar(5, (judgeSecondsLeft / 100)+'0', false);
    display.sendChar(6, ((judgeSecondsLeft / 10) % 10)+'0', false);
    display.sendChar(7, (judgeSecondsLeft % 10)+'0', false);
  }
  // leds work
  if (millis() > ledsUpdateTimer) {
    ledsUpdateTimer = millis() + ledsUpdateDelay;
    uint8_t ledsNeedLight = ((judgeMsTimer - millis()) / (judgeSeconds * 10) * LEDSTRIP_LENGTH) / 100 + 1;
    for (uint8_t i = 0; i < ledsNeedLight; i++)
      leds[i] = COLOR_JUDGEMENT;
    for (uint8_t i = ledsNeedLight; i < LEDSTRIP_LENGTH; i++)
      leds[i] = 0;
    FastLED.show();
  }
  jbuttonsCheck();
  if (!jbtns[JUP].pressProcessedDebounced()) {
    mode = MODE_ROUND_END;
    roundEndStatus = ROUND_END_STATUS_ANSWERED;
    PC.println("JUDGEMENT TRUE! +++");
  }
  if (!jbtns[JDOWN].pressProcessedDebounced()) {
    mode = MODE_QUESTION_COUNTDOWN;
    PC.println("JUDGEMENT FALSE! ---");
    for (uint8_t j = 0; j < 5; j++) {
      for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
        leds[i] = COLOR_ANSWER_FALSE;
      FastLED.show();
      delay(100);
      for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
        leds[i] = 0;
      FastLED.show();
      delay(100);
    }
    players[answeringPlayer].canAnswer = false;
    players[answeringPlayer].btn.setInput();
    questionMsTimer = millis() + questionMsTimerLeft;
    displayUpdateTimer = 0;
    ledsUpdateTimer = 0;
    ledsUpdateDelay = (questionSeconds * 1000) / LEDSTRIP_LENGTH;
  }
  // time is up work
  if (millis() > judgeMsTimer) {
    PC.println("JUDGEMENT END TIME! ---");
    mode = MODE_QUESTION_COUNTDOWN;
    for (uint8_t j = 0; j < 5; j++) {
      for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
        leds[i] = COLOR_ANSWER_FALSE;
      FastLED.show();
      delay(100);
      for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
        leds[i] = 0;
      FastLED.show();
      delay(100);
    }
    players[answeringPlayer].canAnswer = false;
    players[answeringPlayer].btn.setInput();
    questionMsTimer = millis() + questionMsTimerLeft;
  }
}

void modeRoundEnd() {
  switch (roundEndStatus) {
    case ROUND_END_STATUS_TIME_IS_UP:
      PC.println("ROUND END TIME IS UP!");
      for (uint8_t j = 0; j < 5; j++) {
        for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
          leds[i] = COLOR_ANSWER_FALSE;
        FastLED.show();
        delay(100);
        for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
          leds[i] = 0;
        FastLED.show();
        delay(100);
      }
      break;
    case ROUND_END_STATUS_SKIPPED:
      PC.println("ROUND END SKIPPED!");
      for (uint8_t j = 0; j < 5; j++) {
        for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
          leds[i] = COLOR_JUDGEMENT;
        FastLED.show();
        delay(100);
        for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
          leds[i] = 0;
        FastLED.show();
        delay(100);
      }
      break;
    case ROUND_END_STATUS_ANSWERED:
      PC.println("ROUND END ANSWERED!");
      for (uint8_t j = 0; j < 5; j++) {
        for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
          leds[i] = COLOR_ANSWER_TRUE;
        FastLED.show();
        delay(100);
        for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
          leds[i] = 0;
        FastLED.show();
        delay(100);
      }
      break;
  }
  mode = MODE_READY;
  jbtns[JCENTER].debounceDelay = 100000;
  display.sendString("rEAdY"); 
  PC.println("READY!");
  for (uint8_t i = 0; i < playersCount; i++) {
    players[i].canAnswer = true;
    players[i].falseStartPenalty = false;
    players[i].btn.setInput();
  }
}

void setup() {
  // buttons
  for (uint8_t i = 0; i < playersCount; i++) {
    pinMode(PIN_BTNS[i], INPUT_PULLUP);
    players[i].btn = Button(PIN_BTNS[i], true, true);
  }
  // judge buttons
  for (uint8_t i = 0; i < jbtnsCount; i++) {
    pinMode(PIN_JBTNS[i], INPUT_PULLUP);
    jbtns[i] = Button(PIN_JBTNS[i], true, true);
  }

  PC.begin(230400);

  FastLED.addLeds<WS2812B, PIN_LEDSTRIP, GRB>(leds, LEDSTRIP_LENGTH);

  display.begin();
  display.setIntensity(5); // 15 max
  delay(100);
  for (uint8_t i = 0; i < 8; i++) {
    display.sendChar(i, '8', true);
  }
  for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
    leds[i] = 0xFFFFFF;
  FastLED.show();
  delay(1000);
  display.sendString("        ");
  for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
    leds[i] = 0;
  FastLED.show();


  uint8_t firstStart;
  EEPROM.get(0, firstStart);
  if (firstStart == 255){
    EEPROM.put(10, questionSeconds);
    EEPROM.put(20, answerSeconds);
    EEPROM.put(30, judgeSeconds);
    EEPROM.put(40, falseStartPenaltySeconds);
    EEPROM.put(0, 0);
  }
  EEPROM.get(10, questionSeconds);
  EEPROM.get(20, answerSeconds);
  EEPROM.get(30, judgeSeconds);
  EEPROM.get(40, falseStartPenaltySeconds);
}

void loop() {
  switch(mode) {
    case MODE_SETTINGS: modeSettings(); break;
    case MODE_READY: modeReady(); break;
    case MODE_QUESTION_COUNTDOWN: modeQuestionCountdown(); break;
    case MODE_ANSWERING: modeAnswering(); break;
    case MODE_JUDGEMENT: modeJudgement(); break;
    case MODE_ROUND_END: modeRoundEnd(); break;
  }
}