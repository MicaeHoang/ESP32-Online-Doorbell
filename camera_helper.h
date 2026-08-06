#ifndef CAMERA_HELPER_H
#define CAMERA_HELPER_H

#include "esp_camera.h"
#include "config.h"

inline void configCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera initialization error: 0x%x\n", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 0);
}

inline String sendPhotoTelegram() {
  for (int i = 0; i < 2; i++) {
    camera_fb_t * fb_old = esp_camera_fb_get();
    if (fb_old) esp_camera_fb_return(fb_old);
  }

  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) return "Failed";

  if (clientTCP.connect("api.telegram.org", 443)) {
    String head = "--FreenoveESP32\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + String(chatID) + "\r\n--FreenoveESP32\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--FreenoveESP32--\r\n";

    uint32_t totalLen = fb->len + head.length() + tail.length();

    clientTCP.println("POST /bot" + String(botToken) + "/sendPhoto HTTP/1.1");
    clientTCP.println("Host: api.telegram.org");
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=FreenoveESP32");
    clientTCP.println();
    clientTCP.print(head);

    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n = 0; n < fbLen; n += 1024) {
      if (n + 1024 < fbLen) clientTCP.write(fbBuf + n, 1024);
      else clientTCP.write(fbBuf + n, fbLen - n);
    }
    clientTCP.print(tail);
    esp_camera_fb_return(fb);

    int timeout = 5000;
    long startMillis = millis();
    while (!clientTCP.available() && (millis() - startMillis < timeout)) delay(10);
    while (clientTCP.available()) clientTCP.read();
    
    clientTCP.stop();
    return "OK";
  }
  
  esp_camera_fb_return(fb);
  return "Failed";
}

#endif