#include "esp_camera.h"
#include "Arduino.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


const char* asciiChars = " .,-:;=!*#$@#&%$@";

const char* ssid = "***********";
const char* password = "*****";

void setup() {
  Serial.begin(115200);

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;

  config.frame_size = FRAMESIZE_VGA;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Erro ao inicializar a câmera");
    return;
  }

  Serial.println("Câmera inicializada com sucesso!");
}

void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Erro ao capturar a imagem");
    return;
  }

  int width = fb->width;
  int height = fb->height;
  uint8_t *img = fb->buf;

  Serial.println();

  
  for (int y = 0; y < height; y++) {  
    for (int x = 0; x < width; x++) { 
      int pixelIndex = y * width + x;
      uint8_t pixelValue = img[pixelIndex];

      
      int asciiIndex = map(pixelValue, 0, 255, 0, strlen(asciiChars) - 1); 
      Serial.print(asciiChars[asciiIndex]);
    }
    Serial.println();
  }

  esp_camera_fb_return(fb);
  delay(500); 
}

