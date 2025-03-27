#include <WiFi.h>
#include <WebServer.h>
#include "esp_camera.h"
#include "Arduino.h"
#include <EEPROM.h>

// Configurações da câmera
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

const char* DEFAULT_SSID = "Filippi";
const char* DEFAULT_PASSWORD = "compiuter";
const char* asciiChars = " .,-:;=!*#$@#&%$@";
//const char* asciiChars = "@$%&#@$#*!=;:-,. ";
const int EEPROM_SIZE = 512;
const int SSID_ADDR = 0;
const int PASS_ADDR = 64;
const int MAX_CRED_LENGTH = 63;


WebServer server(80);

String wifiSSID = "";
String wifiPassword = "";
unsigned long lastAsciiOutput = 0;
const long asciiInterval = 10000;  
int black_level = 0;    
int white_level = 100;  
int gamma_correction = 50;
int brightness = 0;      
int contrast = 0;      
int threshold = 0;       

void setupCamera();
void initStorage();
void processImage(uint8_t* img, int width, int height);
void loadWiFiCredentials();
void saveWiFiCredentials(String ssid, String password);
void connectToWiFi();
void generateAsciiOutput(camera_fb_t *fb, bool serialOutput);
void handleStream();
void handleConfigPage();
void handleSaveWifi();
void handleRestart();
void setupWebServer();
void handleSerialCommands();

void setupCamera() {
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
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE; 
  config.frame_size = FRAMESIZE_QVGA;  
  config.jpeg_quality = 10;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Falha na inicialização da câmera!");
    delay(1000);
    ESP.restart();
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_contrast(s, 2);
  s->set_brightness(s, 0);
  s->set_vflip(s, 1);  //girar
}

void initStorage() {
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Falha ao iniciar EEPROM");
    delay(1000);
    ESP.restart();
  }
}

void processImage(uint8_t* img, int width, int height) {
  for (int i = 0; i < width * height; i++) {
    int pixelValue = img[i];
    
    pixelValue = constrain(pixelValue + brightness * 255 / 100, 0, 255);
    
    if (contrast != 0) {
      float factor = (259.0 * (contrast + 255.0)) / (255.0 * (259.0 - contrast));
      pixelValue = constrain(factor * (pixelValue - 128) + 128, 0, 255);
    }
    
    if (threshold > 0) {
      pixelValue = (pixelValue > threshold) ? 255 : 0;
    }
    
    img[i] = pixelValue;
  }
}

void loadWiFiCredentials() {
  wifiSSID = "";
  wifiPassword = "";
  
  char c = EEPROM.read(SSID_ADDR);
  for (int addr = SSID_ADDR; c != '\0' && addr < SSID_ADDR + MAX_CRED_LENGTH && c != 255; addr++) {
    wifiSSID += c;
    c = EEPROM.read(addr + 1);
  }
  
  c = EEPROM.read(PASS_ADDR);
  for (int addr = PASS_ADDR; c != '\0' && addr < PASS_ADDR + MAX_CRED_LENGTH && c != 255; addr++) {
    wifiPassword += c;
    c = EEPROM.read(addr + 1);
  }

  if (wifiSSID.length() == 0 || wifiPassword.length() == 0) {
    wifiSSID = DEFAULT_SSID;
    wifiPassword = DEFAULT_PASSWORD;
    Serial.println("Usando credenciais WiFi padrão");
  }
}

void saveWiFiCredentials(String ssid, String password) {
  for (int i = 0; i < 128; i++) {
    EEPROM.write(i, 0);
  }

  for (int i = 0; i < ssid.length() && i < MAX_CRED_LENGTH; i++) {
    EEPROM.write(SSID_ADDR + i, ssid[i]);
  }
  EEPROM.write(SSID_ADDR + ssid.length(), '\0'); 

  for (int i = 0; i < password.length() && i < MAX_CRED_LENGTH; i++) {
    EEPROM.write(PASS_ADDR + i, password[i]);
  }
  EEPROM.write(PASS_ADDR + password.length(), '\0'); 

  if (!EEPROM.commit()) {
    Serial.println("Falha ao salvar na EEPROM!");
  } else {
    Serial.println("Credenciais salvas com sucesso!");
  }
}


void connectToWiFi() {
  if (wifiSSID.length() == 0) loadWiFiCredentials();

  Serial.print("Tentando conectar à rede salva: ");
  Serial.println(wifiSSID);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) { // 20 segundos de tentativa
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFalha na rede salva! Tentando rede padrão...");
    WiFi.begin(DEFAULT_SSID, DEFAULT_PASSWORD);
    
    startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
      delay(500);
      Serial.print(".");
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nTodas as redes falharam! Iniciando Access Point...");
    WiFi.softAP("ESP32-CAM", NULL); // Rede aberta
    Serial.print("Access Point criado! Conecte-se a: ESP32-CAM\nIP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("\nConectado! IP: " + WiFi.localIP().toString());
  }
}

void generateAsciiOutput(camera_fb_t *fb, bool serialOutput) {
  if (!fb || fb->format != PIXFORMAT_GRAYSCALE) return;

  processImage(fb->buf, fb->width, fb->height);
  
  int width = fb->width;
  int height = fb->height;
  uint8_t *img = fb->buf;

  if (serialOutput) {
    Serial.write(27);      
    Serial.print("[2J");   
    Serial.write(27);      
    Serial.print("[H");     
  }

  const int skipX = 2; 
  const int skipY = 3; 
  
  for (int y = 0; y < height; y += skipY) {
    for(int x = 0; x < width; x += skipX) {
      int pixelIndex = y * width + x;
      uint8_t pixelValue = img[pixelIndex];
      int asciiIndex = map(pixelValue, 0, 255, 0, strlen(asciiChars) - 1);
      Serial.print(asciiChars[asciiIndex]);
    }
    Serial.println();
  }
  
  if (serialOutput) {
    Serial.println("\n=== ESP32 Camera ASCII ===");
    Serial.println("IP: " + WiFi.localIP().toString());
    Serial.println(String("Black level: ") + black_level + " | White level: " + white_level + " | Gamma: " + gamma_correction);
  }
}

void handleStream() {
  WiFiClient client = server.client();
  
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println();
  
  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb || fb->format != PIXFORMAT_GRAYSCALE) {
      if(fb) esp_camera_fb_return(fb);
      delay(100);
      continue;
    }

    uint8_t *modified_buf = (uint8_t *)malloc(fb->len);
    if (!modified_buf) {
      esp_camera_fb_return(fb);
      continue;
    }
    memcpy(modified_buf, fb->buf, fb->len);

    for(int i = 0; i < fb->width * fb->height; i++) {
      float pixel = modified_buf[i];
      
      pixel = map(pixel, 0, 255, black_level * 2.55, white_level * 2.55);
      
      float gamma_val = 1.0 + (gamma_correction / 50.0);
      pixel = 255 * pow(pixel / 255.0, gamma_val);
      
      modified_buf[i] = constrain(pixel, 0, 255);
    }

    uint8_t *out_buf = NULL;
    size_t out_len = 0;
    bool jpeg_converted = frame2jpg(fb, 80, &out_buf, &out_len);
    
    if(jpeg_converted && out_len > 0) {
      client.println("--frame");
      client.println("Content-Type: image/jpeg");
      client.println("Content-Length: " + String(out_len));
      client.println();
      client.write(out_buf, out_len);
      client.println();
      free(out_buf);
    }
    
    free(modified_buf);
    esp_camera_fb_return(fb);
    delay(50);
  }
}

void handleConfigPage() {
  String html = "<html><head><meta charset='UTF-8'>";
  html += "<title>Configuração da Câmera</title>";
  html += "<style>body{font-family:Arial;margin:20px;}";
  html += "form{margin:20px 0;padding:20px;background:#f0f0f0;border-radius:5px;}";
  html += "input[type='submit']{background:#4CAF50;color:white;border:none;padding:10px 15px;border-radius:3px;}</style>";
  html += "</head><body>";
  html += "<h1>Configuração da Câmera ESP32</h1>";
  
  html += "<form action='/savewifi' method='post'>";
  html += "<h2>Configuração WiFi</h2>";
  html += "<label>SSID:</label><br>";
  html += "<input type='text' name='ssid' value='" + wifiSSID + "' required><br><br>";
  html += "<label>Senha:</label><br>";
  html += "<input type='password' name='password' required><br><br>";
  html += "<input type='submit' value='Salvar Configurações WiFi'>";
  html += "</form>";
  
  html += "<form action='/restart' method='get'>";
  html += "<h2>Reiniciar Sistema</h2>";
  html += "<p>Após salvar novas configurações WiFi, reinicie para aplicar.</p>";
  html += "<input type='submit' value='Reiniciar Agora'>";
  html += "</form>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleSaveWifi() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    wifiSSID = server.arg("ssid");
    wifiPassword = server.arg("password");
    
    saveWiFiCredentials(wifiSSID, wifiPassword);
    
    String message = "<html><body><h1>Configurações Salvas</h1>";
    message += "<p>SSID: " + wifiSSID + "</p>";
    message += "<p>Reinicie o dispositivo para aplicar as novas configurações.</p>";
    message += "<p><a href='/config'>Voltar</a> | <a href='/restart'>Reiniciar Agora</a></p>";
    message += "</body></html>";
    
    server.send(200, "text/html", message);
  } else {
    server.send(400, "text/plain", "Parâmetros faltando: ssid e password são necessários");
  }
}

void handleRestart() {
  server.send(200, "text/html", "<html><body><h1>Reiniciando...</h1><p>O dispositivo está reiniciando. Aguarde alguns segundos.</p></body></html>");
  delay(1000);
  ESP.restart();
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleStream);
  server.on("/config", HTTP_GET, handleConfigPage);
  server.on("/savewifi", HTTP_POST, handleSaveWifi);
  server.on("/restart", HTTP_GET, handleRestart);
  
  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "info") {
      Serial.println("\n=== Informações do Sistema ===");
      Serial.println("SSID: " + wifiSSID);
      Serial.println("IP: " + WiFi.localIP().toString());
      Serial.println("Estado WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Conectado" : "Desconectado"));
      Serial.println("Resolução: QQVGA (160x120)");
      Serial.println("Formato: Grayscale");
      Serial.println("\nAjustes atuais:");
      Serial.println("Black level: " + String(black_level));
      Serial.println("White level: " + String(white_level));
      Serial.println("Gamma: " + String(gamma_correction));
    }
    else if (command == "restart") {
      Serial.println("Reiniciando o sistema...");
      delay(1000);
      ESP.restart();
    }
    else if (command == "config") {
      Serial.println("\nAcesse a interface web para configuração:");
      Serial.println("http://" + WiFi.localIP().toString() + "/config");
    }
    else if (command == "foto") {
      Serial.println("Capturando foto ASCII instantânea...");
      camera_fb_t *fb = esp_camera_fb_get();
      if (fb) {
        generateAsciiOutput(fb, true);
        esp_camera_fb_return(fb);
        lastAsciiOutput = millis();
      } else {
        Serial.println("Erro ao capturar frame da câmera!");
      }
    }
    else if (command.startsWith("black ")) {
      black_level = command.substring(6).toInt();
      black_level = constrain(black_level, 0, 100);
      Serial.println("Black level ajustado para: " + String(black_level));
    }
    else if (command.startsWith("white ")) {
      white_level = command.substring(6).toInt();
      white_level = constrain(white_level, 0, 100);
      Serial.println("White level ajustado para: " + String(white_level));
    }
    else if (command.startsWith("gamma ")) {
      gamma_correction = command.substring(6).toInt();
      gamma_correction = constrain(gamma_correction, 0, 100);
      Serial.println("Gamma ajustado para: " + String(gamma_correction));
    }
    else if (command == "usepadrao") {
      WiFi.begin(DEFAULT_SSID, DEFAULT_PASSWORD);
      Serial.println("Conectando à rede padrão...");
    }
    else {
      Serial.println("Comando desconhecido. Comandos disponíveis:");
      Serial.println("info - Mostra informações do sistema");
      Serial.println("restart - Reinicia o dispositivo");
      Serial.println("config - Mostra URL de configuração");
      Serial.println("foto - Captura foto ASCII instantânea");
      Serial.println("black X - Ajusta nível de preto (0-100)");
      Serial.println("white X - Ajusta nível de branco (0-100)");
      Serial.println("gamma X - Ajusta correção gamma (0-100)");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nIniciando sistema de câmera ASCII...");
  
  initStorage();
  loadWiFiCredentials();
  setupCamera();
  connectToWiFi();
  setupWebServer();
  
  Serial.println("Sistema pronto!");
  Serial.println("Acesse http://" + WiFi.localIP().toString() + "/config para configurar");
  Serial.println("Ou digite 'info', 'restart' ou 'config' no Monitor Serial");
}

void loop() {
  server.handleClient();
  handleSerialCommands();
  
  if (millis() - lastAsciiOutput > asciiInterval) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
      generateAsciiOutput(fb, false);
      esp_camera_fb_return(fb);
    }
    lastAsciiOutput = millis();
  }

  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 30000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi desconectado! Tentando reconectar...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
  }
}