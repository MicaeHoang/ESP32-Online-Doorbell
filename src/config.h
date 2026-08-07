#ifndef CONFIG_H
#define CONFIG_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Preferences.h>
#include "Audio.h"
#include <LittleFS.h>

// === DYNAMIC CONFIGURATION VARIABLES ===
extern Preferences preferences;
extern char botToken[60];
extern char chatID[20];

extern WiFiClientSecure clientTCP;
extern UniversalTelegramBot *bot;
extern Audio audio;

extern unsigned long lastTimeBotScan;
const unsigned long botRequestInterval = 1000;

// === GPIO CONFIGURATION ===
#define BUTTON_CAM_PIN   1   // button 1: Take the photo or reset the wifi setting when start
#define BUTTON_VOICE_PIN 3   // button 2: Record the voice
#define LED_PIN          2   // LED

// I2S Micro INMP441
#define I2S_MIC_WS       42
#define I2S_MIC_SCK      41
#define I2S_MIC_SD       38

// I2S Speaker MAX98357A
#define I2S_SPEAKER_LRC  19
#define I2S_SPEAKER_BCLK 20
#define I2S_SPEAKER_DIN  47

// Audio recording parameters
#define SAMPLE_RATE      16000
#define RECORD_TIME_SEC  5

// Freenove S3 Camera Mount
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM      16
#define Y8_GPIO_NUM      17
#define Y7_GPIO_NUM      18
#define Y6_GPIO_NUM      12
#define Y5_GPIO_NUM      10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM      11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM    13

// Structure WAV Header
struct WAVHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t chunkSize;
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4]  = {'f', 'm', 't', ' '};
  uint32_t subchunk1Size = 16;
  uint16_t audioFormat   = 1;
  uint16_t numChannels   = 1;
  uint32_t sampleRate    = SAMPLE_RATE;
  uint32_t byteRate      = SAMPLE_RATE * 2;
  uint16_t blockAlign    = 2;
  uint16_t bitsPerSample = 16;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t subchunk2Size;
};

#endif