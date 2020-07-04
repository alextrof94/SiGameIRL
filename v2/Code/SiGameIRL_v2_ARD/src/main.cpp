#include <Arduino.h>
#include <FastLED.h>
#include <SPI.h>
#include <bitBangedSPI.h>
#include <MAX7219.h>
#include <Smoothed.h>

const uint16_t lightSensorDark = 400;
const uint16_t lightSensorLight = 700;
Smoothed <uint16_t> smoother; 

const uint8_t PIN_LIGHT_SENSOR = A1;
const uint8_t PIN_LEDSTRIP = A0;
const uint8_t PIN_MAX7219_DIN = 11;
const uint8_t PIN_MAX7219_CS = 12;
const uint8_t PIN_MAX7219_CLK = 13;
const uint8_t PIN_PLAYERS[8] = {6, 2, 3, 4, 7, 8, 9, 5};
//const uint8_t PIN_PLAYERS[8] = {_3, 4, 5, 6, 7, 8, 9, 2};

const uint8_t LEDSTRIP_LENGTH = 14;
CRGB leds[LEDSTRIP_LENGTH];

MAX7219 display (1, PIN_MAX7219_CS, PIN_MAX7219_DIN, PIN_MAX7219_CLK);  // ChipsCount / LOAD / DIN / CLK

const uint32_t colors[10] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF, 0x6600FF, 0xFFFF00, 0x111111, 0x555555, 0x999999, 0xBBBBBB};

#define MODE_INITIALIZATION 100
#define MODE_GAME 0
uint8_t mode = MODE_INITIALIZATION;
uint8_t playerState[8]; // 0 - off, 1 - on, 2 - blink, 3 - blink fast

uint32_t blinkTimer;
const uint32_t blinkDelay = 500; 
uint32_t blinkFastTimer;
const uint32_t blinkFastDelay = 50;
void playerWork() {
  for (uint8_t i = 0; i < 8; i++) {
    if (playerState[i] == 0)
      digitalWrite(PIN_PLAYERS[i], 0);
    else if (playerState[i] == 1)
      digitalWrite(PIN_PLAYERS[i], 1);
  }

  if (millis() > blinkTimer){
    blinkTimer = millis() + blinkDelay;
    for (uint8_t i = 0; i < 8; i++)
      if (playerState[i] == 2)
        digitalWrite(PIN_PLAYERS[i], !digitalRead(PIN_PLAYERS[i]));
  }
  if (millis() > blinkFastTimer){
    blinkFastTimer = millis() + blinkFastDelay;
    for (uint8_t i = 0; i < 8; i++)
      if (playerState[i] == 3)
        digitalWrite(PIN_PLAYERS[i], !digitalRead(PIN_PLAYERS[i]));
  }
}

uint16_t brightness;
uint16_t readBrightness() {
    brightness = analogRead(PIN_LIGHT_SENSOR);
    if (brightness < lightSensorDark)
      brightness = lightSensorDark;
    if (brightness > lightSensorLight)
      brightness = lightSensorLight;     
    brightness = map(brightness, lightSensorDark, lightSensorLight, 3, 255);
    smoother.add(brightness);
    return smoother.get();
}

uint32_t initSendTimer = 0;
const uint32_t initSendDelay = 1000;
void modeInitialization() {
  if (millis() > initSendTimer) {
    initSendTimer = millis() + initSendDelay;
    // sending ledstrip length
    Serial.print('L');
    uint8_t len = LEDSTRIP_LENGTH / 2;
    if (len < 100)
      Serial.print('0');
    else
      Serial.print(len / 100);
    
    if (len < 10)
      Serial.print('0');
    else
      Serial.print(len / 10 % 10);

    if (len < 1)
      Serial.print('0');
    else
      Serial.print(len % 10);
  }
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'K') {
      mode = MODE_GAME;
    }  
  }
}

bool cmdComplite =  false;
char cmdbuf[20];
uint8_t cmdIndex = 0;
void modeGame() {
  playerWork();
  if (Serial.available() > 0) {
    cmdbuf[cmdIndex] = Serial.read();
    if (cmdbuf[cmdIndex] == '\r' || cmdbuf[cmdIndex] == '\n') {
      cmdComplite = true;
    }
    cmdIndex++;
  }
  if (cmdComplite) {
    cmdComplite = false;
    cmdIndex = 0;

    if (cmdbuf[0] == 'P') {
      uint8_t p = (uint8_t)cmdbuf[1];
      char subcmd = cmdbuf[2];
      if (subcmd == 'S'){
        playerState[p] = 1;
      } else if (subcmd == 'B') {
        playerState[p] = 2;
      } else if (subcmd == 'F') {
        playerState[p] = 3;
      } else if (subcmd == 'R') {
        playerState[p] = 0;
      }
    } else
    if (cmdbuf[0] == 'L') {
      for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
        leds[i] = 0;
      uint8_t needToLight = (uint8_t)cmdbuf[1];
      uint8_t colorR = (uint8_t)cmdbuf[2];
      uint8_t colorG = (uint8_t)cmdbuf[3];
      uint8_t colorB = (uint8_t)cmdbuf[4];

      if (needToLight > LEDSTRIP_LENGTH / 2)
        needToLight = LEDSTRIP_LENGTH / 2;
      uint8_t lM = LEDSTRIP_LENGTH / 2;
      for (uint8_t i = lM - needToLight; i < lM; i++)
        leds[i] = CRGB(colorR, colorG, colorB);
      for (uint8_t i = lM; i < needToLight + lM; i++)
        leds[i] = CRGB(colorR, colorG, colorB);

      FastLED.setBrightness(readBrightness());
      FastLED.show();
    } else
    if (cmdbuf[0] == 'T') {
      display.sendString("        ");
      display.setIntensity(readBrightness() / 18);
      uint8_t dispPos = 0;
      uint8_t charIndex = 1;
      bool reading = true;
      char charNow;
      char charNext;
      while (reading) {
        if (dispPos == 0) {
          charNow = cmdbuf[charIndex++];
          charNext = cmdbuf[charIndex++]; 
          if (charNext == '.') {
            display.sendChar(dispPos, charNow, true);
            charNext = cmdbuf[charIndex++];
            dispPos++;
          } else {
            display.sendChar(dispPos, charNow, false);
            dispPos++;
          }
        } else {
          charNow = charNext;
          charNext = cmdbuf[charIndex++]; 
          if (charNext == '.') {
            display.sendChar(dispPos, charNow, true);
            charNext = cmdbuf[charIndex++]; 
            dispPos++;
          } else {
            display.sendChar(dispPos, charNow, false);
            dispPos++;
          }
        }
        if (dispPos == 8)
          reading = false;
        if (charNext == '\n' || charNext == '\r')
          reading = false;
      }
    } else
    if (cmdbuf[0] == 'R') {
      display.sendString("        "); 
      for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++) {
        leds[i] = 0;
      }
      FastLED.show();
      for (uint8_t i = 0; i < 8; i++) {
        playerState[i] = 0;
      }
    }
  }  
}

void setup() {
  pinMode(PIN_LIGHT_SENSOR, INPUT);
  for (uint8_t i = 0; i < 8; i++)
    pinMode(PIN_PLAYERS[i], OUTPUT);
  Serial.begin(115200);
  FastLED.addLeds<WS2812B, PIN_LEDSTRIP, GRB>(leds, LEDSTRIP_LENGTH);
  display.begin();
  display.setIntensity(5); // 15 max
  // test
  for (uint8_t i = 0; i < 8; i++)
    digitalWrite(PIN_PLAYERS[i], 1);
  for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
    leds[i] = 0x11111111;
  FastLED.show();
  for (uint8_t j = 0; j < 10; j++) {
    for (uint8_t i = 0; i < 8; i++) {
      display.sendChar(i, j+'0', true);
    }
    delay(50);
  }
  for (uint8_t i = 0; i < 8; i++) {
    display.sendChar(i, '8', true);
  }
  delay(500);
  for (uint8_t i = 0; i < LEDSTRIP_LENGTH; i++)
    leds[i] = 0;
  FastLED.show();
  display.sendString("        ");
  for (uint8_t i = 0; i < 8; i++)
    digitalWrite(PIN_PLAYERS[i], 0);
  // test end
	smoother.begin(SMOOTHED_EXPONENTIAL, 10);
  smoother.clear();
}

void loop() {
  switch (mode) {
    case MODE_INITIALIZATION: modeInitialization(); break;
    case MODE_GAME: modeGame(); break;
  }
}