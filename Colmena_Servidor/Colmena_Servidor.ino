#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ArduinoJson.h>

const char* ssid = "Pichishouse_EXT";
const char* password = "Pichi1970";
//const char* ssid = "TP-Link_7D88";
//const char* password = "19663043";

AsyncWebServer server(80);

#define LORA_DIO0 2
#define LORA_SS 5
#define LORA_RST 15
#define TFT_CS 21
#define TFT_DC 4
#define TFT_RST 22

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Variable global que almacena el JSON asíncrono
String ultimoPaqueteJSON = "{\"co2\":0,\"temperatura\":0,\"humedad\":0,\"altitud\":0,\"frecuencia\":0,\"peso\":0}";

void setup() {
  Serial.begin(115200);

  tft.begin();
  tft.setRotation(0); // 0 = Orientación vertical
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Iniciando Sistema..");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  tft.setCursor(10, 40);
  tft.print("Conectando WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  
  server.on("/api/datos", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", ultimoPaqueteJSON);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });
  server.begin();

  // Iniciar Bus SPI compartido
  SPI.begin(18, 19, 23, LORA_SS); 
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  
  if (!LoRa.begin(433E6)) {
    tft.fillScreen(ILI9341_RED);
    tft.setCursor(10, 10);
    tft.println("Error iniciando LoRa");
    Serial.println("Error LoRa");
    while (1);
  }

  dibujarDashboardFijo();
}

void dibujarDashboardFijo() {
  tft.fillScreen(ILI9341_BLACK);

  // Encabezado
  tft.fillRect(0, 0, 240, 45, ILI9341_NAVY);
  tft.setTextColor(ILI9341_YELLOW, ILI9341_NAVY);
  tft.setTextSize(2);
  tft.setCursor(10, 12);
  tft.print("Estacion Apicola");

  // Mostrar IP
  tft.setTextColor(ILI9341_WHITE, ILI9341_NAVY);
  tft.setTextSize(1);
  tft.setCursor(10, 32);
  tft.print("IP: ");
  tft.print(WiFi.localIP());

  // Etiquetas de datos
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_LIGHTGREY, ILI9341_BLACK);

  tft.setCursor(10, 70);  tft.print("Temperatura:");
  tft.setCursor(10, 110); tft.print("Humedad:");
  tft.setCursor(10, 150); tft.print("Co2:");
  tft.setCursor(10, 190); tft.print("Masa:");
  tft.setCursor(10, 230); tft.print("Frecuencia:");
  tft.setCursor(10, 270); tft.print("Altitud:");
}

void actualizarDatosTFT(String jsonString) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (!error) {
    tft.setTextSize(2);

    // Temperatura
    tft.fillRect(150, 70, 100, 20, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.setCursor(155, 70);
    tft.printf("%.1f C", (float)doc["temperatura"]);

    // Humedad
    tft.fillRect(120, 110, 100, 20, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.setCursor(120, 110);
    tft.printf("%.1f %%", (float)doc["humedad"]);

    // CO2
    tft.fillRect(140, 150, 100, 20, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.setCursor(80, 150);
    tft.printf("%d ppm", (int)doc["co2"]);

    // Masa (peso)
    tft.fillRect(140, 190, 100, 20, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.setCursor(80, 190);
    tft.printf("%.2f Kg", (float)doc["peso"]);

    // Frecuencia
    tft.fillRect(140, 230, 100, 20, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.setCursor(150, 230);
    tft.printf("%d Hz", (int)doc["frecuencia"]);

    // Altitud
    tft.fillRect(140, 270, 100, 20, ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
    tft.setCursor(120, 270);
    tft.printf("%.1f m", (float)doc["altitud"]);
  }
}

void loop() {
  int tamanoPaquete = LoRa.parsePacket();
  
  if (tamanoPaquete) {
    String datosRecibidos = "";
    while (LoRa.available()) {
      datosRecibidos += (char)LoRa.read();
    }
    
    ultimoPaqueteJSON = datosRecibidos;
    Serial.println("Recibido: " + ultimoPaqueteJSON);
    
    actualizarDatosTFT(ultimoPaqueteJSON);
  }
}