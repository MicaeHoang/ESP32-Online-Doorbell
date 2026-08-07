#include <WiFiManager.h>
#include "esp_pm.h"
#include "esp_wifi.h"

// Include the defined helper files
#include "config.h"
#include "camera_helper.h"
#include "audio_helper.h"

// === config.h VARIABLE DEFINITIONS ===
Preferences preferences;
char botToken[60] = "";
char chatID[20]   = "";
WiFiClientSecure clientTCP;
UniversalTelegramBot *bot;
Audio audio;
unsigned long lastTimeBotScan = 0;

// === OTHER PROCESSING FUNCTIONS ===

// Wi-Fi reset function upon ESP32 startup
void setupWiFiAndPortal() {
  if (digitalRead(BUTTON_CAM_PIN) == LOW) {
    WiFiManager wm;
    wm.resetSettings();
    preferences.begin("telegram_cfg", false);
    preferences.clear();
    preferences.end();
    delay(2000);
    ESP.restart();
  }

  preferences.begin("telegram_cfg", false);
  preferences.getString("token", "").toCharArray(botToken, 60);
  preferences.getString("chat_id", "").toCharArray(chatID, 20);

  WiFiManager wm;
  WiFiManagerParameter custom_bot_token("bot_token", "Telegram Bot Token", botToken, 60);
  WiFiManagerParameter custom_chat_id("chat_id", "Telegram Chat ID", chatID, 20);

  wm.addParameter(&custom_bot_token);
  wm.addParameter(&custom_chat_id);
  wm.setConfigPortalTimeout(180);

  if (!wm.autoConnect("ESP32-S3-CAM-Setup")) {
    delay(3000);
    ESP.restart();
  }

  strcpy(botToken, custom_bot_token.getValue());
  strcpy(chatID, custom_chat_id.getValue());
  preferences.putString("token", botToken);
  preferences.putString("chat_id", chatID);
  preferences.end();
}

// function to set low-power/battery-saving mode
void setupLowPower() {
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  #if CONFIG_PM_ENABLE
    esp_pm_config_esp32s3_t pm_config = {
      .max_freq_mhz = 240, .min_freq_mhz = 80, .light_sleep_enable = true
    };
    esp_pm_configure(&pm_config);
  #endif
}

// Function to check incoming messages; is "check" a command or a voice input?
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String msg_chat_id = String(bot->messages[i].chat_id);
    if (msg_chat_id != String(chatID)) continue;

    String text = bot->messages[i].text;
    String type = bot->messages[i].type;
    String file_path = bot->messages[i].file_path; // Get file_path from the struct

    Serial.println("text: " + text);
    Serial.println("type: " + type);
    Serial.println("file_path: " + file_path);

    // Check if it is a voice/audio message or contains a file_path
    if (type == "voice" || type == "audio" && file_path.length() > 0) {
      bot->sendMessage(msg_chat_id, "🔊 Loading and playing voice message...", "");

      if (file_path.length() > 0) {
        bot->sendMessage(msg_chat_id, "⏬ Loading audio file...", "");
        String download_url = file_path;

        if (downloadVoiceToLittleFS(download_url, "/voice.oga")) {
          bot->sendMessage(msg_chat_id, "🔊 Playing...", "");
          audio.connecttoFS(LittleFS, "/voice.oga");
        } else {
          bot->sendMessage(msg_chat_id, "❌ File download failed; cannot play.", "");
        }
      }
    } 
    else {
      // Xử lý các lệnh dạng Text
      if (text == "/start") {
        bot->sendMessage(msg_chat_id, "Command:\n/led_on    -turn on light\n/led_off    -turn off light\n/capture  -take photo", "");
      }
      else if (text == "/led_on") {
        digitalWrite(LED_PIN, HIGH);
        bot->sendMessage(msg_chat_id, "💡 Turn on the light!", "");
      }
      else if (text == "/led_off") {
        digitalWrite(LED_PIN, LOW);
        bot->sendMessage(msg_chat_id, "🌑 Turn off the lights!", "");
      }
      else if (text == "/capture") {
        bot->sendMessage(msg_chat_id, "📸 Taking a photo...", "");
        sendPhotoTelegram();
      }
    }
  }
}

// check buttons functions
void checkButtons() {
  // BUTTON_VOICE_PIN: record the voice from ESP32
  if (digitalRead(BUTTON_VOICE_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_VOICE_PIN) == LOW) {
      recordAndSendVoice();
      while (digitalRead(BUTTON_VOICE_PIN) == LOW);
    }
  }
  // BUTTON_CAM_PIN: take the photo from ESP32
  if (digitalRead(BUTTON_CAM_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_CAM_PIN) == LOW) {
      bot->sendMessage(chatID, "🔔 Someone is taking a photo!", "");
      sendPhotoTelegram();
      while (digitalRead(BUTTON_CAM_PIN) == LOW);
    }
  }
}

// function used to check for and process new messages sent to the Telegram Bot at regular intervals
void checkTelegramMessages() {
  if (millis() - lastTimeBotScan > botRequestInterval) {
    int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot->getUpdates(bot->last_message_received + 1);
    }
    lastTimeBotScan = millis();
  }
}

// === MAIN SETUP & LOOP ===
void setup() {
  Serial.begin(115200);
  
  pinMode(BUTTON_CAM_PIN, INPUT_PULLUP);
  pinMode(BUTTON_VOICE_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  if (!LittleFS.begin(true)) {   // true = auto-format if no filesystem exists
    Serial.println("❌ LittleFS mount error!");
  }

  setupWiFiAndPortal();

  clientTCP.setInsecure();
  bot = new UniversalTelegramBot(botToken, clientTCP);

  configCamera();
  audio.setPinout(I2S_SPEAKER_BCLK, I2S_SPEAKER_LRC, I2S_SPEAKER_DIN);
  audio.setVolume(50);

  setupLowPower();

  bot->sendMessage(chatID, "🤖 ESP32-S3 Online Doorbell!", "");
}

void loop() {
  audio.loop();
  checkButtons();
  checkTelegramMessages();
}