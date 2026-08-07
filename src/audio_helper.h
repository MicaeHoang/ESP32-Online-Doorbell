#ifndef AUDIO_HELPER_H
#define AUDIO_HELPER_H

#include <driver/i2s_std.h>
#include "config.h"
#include <HTTPClient.h>

inline bool downloadVoiceToLittleFS(const String &url, const char *path) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("❌ HTTPClient begin failed!");
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("❌ HTTP GET error: %d\n", httpCode);
    http.end();
    return false;
  }

  // Delete the old file if it exists to avoid playing the previous file by mistake.
  if (LittleFS.exists(path)) LittleFS.remove(path);

  File f = LittleFS.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("❌ Unable to open LittleFS file for writing.!");
    http.end();
    return false;
  }

  int len = http.getSize();
  uint8_t buff[512];
  WiFiClient *stream = http.getStreamPtr();
  int totalWritten = 0;

  while (http.connected() && (len > 0 || len == -1)) {
    size_t availableSize = stream->available();
    if (availableSize) {
      int c = stream->readBytes(buff, min((size_t)sizeof(buff), availableSize));
      f.write(buff, c);
      totalWritten += c;
      if (len > 0) len -= c;
    }
    delay(1);
  }

  f.close();
  http.end();
  Serial.printf("✅ Downloaded %d bytes to %s\n", totalWritten, path);
  return totalWritten > 0;
}

inline void recordAndSendVoice() {
  Serial.println("🎙️ Preparing to record (5 seconds)...");
  
  // 1. Pause speaker playback
  audio.stopSong();

  size_t wavDataSize = SAMPLE_RATE * 2 * RECORD_TIME_SEC; // 16000 * 2 * 5 = 160,000 bytes
  size_t totalFileSize = 44 + wavDataSize;

  // Safe RAM allocation (Prioritize PSRAM if available)
  uint8_t *wavBuffer = NULL;
  if (psramFound()) {
    wavBuffer = (uint8_t *)ps_malloc(totalFileSize);
  } else {
    wavBuffer = (uint8_t *)malloc(totalFileSize);
  }

  if (!wavBuffer) {
    Serial.println("❌ Không đủ RAM!");
    return;
  }

  // 2. I2S Rx Configuration (Audio Recording)
  i2s_chan_handle_t rx_handle = NULL;
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  
  if (i2s_new_channel(&chan_cfg, NULL, &rx_handle) != ESP_OK) {
    Serial.println("❌ Error creating I2S channel!");
    free(wavBuffer);
    return;
  }

  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_MIC_SCK,
      .ws   = (gpio_num_t)I2S_MIC_WS,
      .dout = I2S_GPIO_UNUSED,
      .din  = (gpio_num_t)I2S_MIC_SD,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv   = false,
      },
    },
  };
  
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

  if (i2s_channel_init_std_mode(rx_handle, &std_cfg) != ESP_OK) {
    Serial.println("❌ I2S STD Mode initialization error!");
    i2s_del_channel(rx_handle);
    free(wavBuffer);
    return;
  }

  i2s_channel_enable(rx_handle);

  // 3. Create WAV Header
  WAVHeader header;
  header.chunkSize = totalFileSize - 8;
  header.subchunk2Size = wavDataSize;
  memcpy(wavBuffer, &header, 44);

  // 4. Read data from the microcontroller (32-bit) and bit-shift it to 16-bit before writing to the WAV file.
  size_t totalSamplesNeeded = wavDataSize / 2;   // wavDataSize calculated based on 16-bit (2 bytes/sample)
  size_t samplesWritten = 0;
  int16_t *dataPtr16 = (int16_t *)(wavBuffer + 44);

  const size_t CHUNK_SAMPLES = 256;
  int32_t rawBuf[CHUNK_SAMPLES];
  size_t bytesRead = 0;

  long startTime = millis();
  while (samplesWritten < totalSamplesNeeded && (millis() - startTime < (RECORD_TIME_SEC * 1000 + 500))) {
    size_t samplesToRead = (totalSamplesNeeded - samplesWritten > CHUNK_SAMPLES) ? CHUNK_SAMPLES : (totalSamplesNeeded - samplesWritten);
    size_t bytesToRead = samplesToRead * sizeof(int32_t);

    if (i2s_channel_read(rx_handle, rawBuf, bytesToRead, &bytesRead, pdMS_TO_TICKS(1000)) == ESP_OK) {
      size_t samplesRead = bytesRead / sizeof(int32_t);
      for (size_t i = 0; i < samplesRead; i++) {
        dataPtr16[samplesWritten + i] = (int16_t)(rawBuf[i] >> 14);   // 24-bit -> 16-bit
      }
      samplesWritten += samplesRead;
    }
  }

  // 5. Tắt I2S Rx
  i2s_channel_disable(rx_handle);
  i2s_del_channel(rx_handle);

  Serial.println("✅ Sending voice message to Telegram...");

  // 6. Send data via Telegram
  String head = "--FreenoveESP32\r\nContent-Disposition: form-data; name=\"voice\"; filename=\"voice.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
  String tail = "\r\n--FreenoveESP32--\r\n";
  uint32_t totalLen = head.length() + totalFileSize + tail.length();

  if (clientTCP.connect("api.telegram.org", 443)) {
    clientTCP.println("POST /bot" + String(botToken) + "/sendVoice?chat_id=" + String(chatID) + " HTTP/1.1");
    clientTCP.println("Host: api.telegram.org");
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=FreenoveESP32");
    clientTCP.println();
    clientTCP.print(head);

    for (size_t i = 0; i < totalFileSize; i += 1024) {
      size_t chunkSize = (i + 1024 < totalFileSize) ? 1024 : (totalFileSize - i);
      clientTCP.write(wavBuffer + i, chunkSize);
    }
    clientTCP.print(tail);

    int timeout = 5000;
    long startMillis = millis();
    while (clientTCP.connected() && (millis() - startMillis < timeout)) {
      while (clientTCP.available()) clientTCP.read();
    }
    clientTCP.stop();
    Serial.println("🚀 Voice message sent successfully!");
  }

  // Safely free memory
  free(wavBuffer);
}

#endif