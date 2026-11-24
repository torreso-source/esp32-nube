#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// Pines del ESP32-CAM AI-Thinker
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

WebServer server(80);
Preferences prefs;
String lineaSerial = "";

bool iniciarCamara() {
  camera_config_t config = {};
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
  config.frame_size = FRAMESIZE_QVGA;   // 320x240, ligero para empezar
  config.jpeg_quality = 12;
  if (psramFound()) {
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error al iniciar la camara: 0x%x\n", err);
    return false;
  }
  return true;
}

void conectarWifi() {
  String ssid = prefs.getString("ssid", "");
  String clave = prefs.getString("clave", "");
  if (ssid == "") {
    Serial.println("Sin WiFi guardado. Escribe: WIFI nombre|clave");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), clave.c_str());
  Serial.print("Conectando a ");
  Serial.println(ssid);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Listo. Abre en Safari: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("No se pudo conectar. Revisa nombre y clave (red de 2.4 GHz).");
  }
}

void paginaInicio() {
  server.send(200, "text/html",
    "<html><body style='margin:0;background:#000'>"
    "<img src='/stream' style='width:100%'></body></html>");
}

void tomarFoto() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Error de camara");
    return;
  }
  server.sendHeader("Content-Disposition", "inline; filename=foto.jpg");
  server.send_P(200, "image/jpeg", (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void transmitirVideo() {
  WiFiClient cliente = server.client();
  cliente.print("HTTP/1.1 200 OK\r\n"
                "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n");
  while (cliente.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) break;
    cliente.print("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ");
    cliente.print(fb->len);
    cliente.print("\r\n\r\n");
    cliente.write(fb->buf, fb->len);
    cliente.print("\r\n");
    esp_camera_fb_return(fb);
  }
}

void leerSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      lineaSerial.trim();
      if (lineaSerial.startsWith("WIFI ")) {
        String datos = lineaSerial.substring(5);
        int sep = datos.indexOf('|');
        if (sep > 0) {
          prefs.putString("ssid", datos.substring(0, sep));
          prefs.putString("clave", datos.substring(sep + 1));
          conectarWifi();
        } else {
          Serial.println("Formato: WIFI nombre|clave");
        }
      }
      lineaSerial = "";
    } else {
      lineaSerial += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  if (!iniciarCamara()) {
    Serial.println("Revisa el cable plano de la camara y la alimentacion.");
  }
  prefs.begin("wifi", false);
  server.on("/", paginaInicio);
  server.on("/stream", transmitirVideo);
  server.on("/foto", tomarFoto);
  server.begin();
  conectarWifi();
}

void loop() {
  leerSerial();
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
}

