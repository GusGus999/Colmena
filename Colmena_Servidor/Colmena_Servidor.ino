#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h> 
#include "credentials.h"

unsigned long ultimoEnvio = 0;
const unsigned long INTERVALO_MINIMO = 6000; 
long ultimaLecturaId = -1;

AsyncWebServer server(80);
Preferences preferences; // Objeto para almacenar SSID y clave

#define LORA_DIO0 2
#define LORA_SS 5
#define LORA_RST 15
#define TFT_CS 21
#define TFT_DC 4
#define TFT_RST 22

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// Variable global que almacena el JSON asíncrono
String ultimoPaqueteJSON = "{\"fecha\":\"--\",\"co2\":0,\"temperatura\":0,\"humedad\":0,\"altitud\":0,\"frecuencia\":0,\"peso\":0}";
String htmlOpcionesRedes = "";

// Función para escanear redes, mostrarlas en la TFT y levantar el portal de configuración
void iniciarPortalConfiguracion() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Buscando redes...");

  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();

  // Mostrar lista de redes en pantalla TFT (Orientación vertical 240x320)
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(5, 5);
  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(2);
  tft.println("Redes Detectadas:");
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);

  htmlOpcionesRedes = "";
  int y = 30;

  for (int i = 0; i < n && i < 8; ++i) { // Muestra hasta 8 redes en pantalla
    String ssidRed = WiFi.SSID(i);
    if (ssidRed.length() > 22) ssidRed = ssidRed.substring(0, 19) + "...";
    String redInfo = String(i + 1) + ". " + ssidRed + " (" + String(WiFi.RSSI(i)) + "dBm)";
    tft.setCursor(5, y);
    tft.println(redInfo);
    y += 14;

    htmlOpcionesRedes += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
  }

  // Iniciar red propia temporal
  WiFi.softAP("ESP32_Config_WiFi");

  // Instrucciones en TFT
  tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(5, y + 10);
  tft.println("--------------------------------");
  tft.println("1. Conectate al WiFi:");
  tft.setTextColor(ILI9341_YELLOW);
  tft.println("   SSID: ESP32_Config_WiFi");
  tft.setTextColor(ILI9341_CYAN);
  tft.println("2. Abre en navegador:");
  tft.setTextColor(ILI9341_YELLOW);
  tft.println("   http://192.168.4.1");

  // Web Endpoint para mostrar el formulario HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>body{font-family:Arial;margin:20px;} select,input{width:100%;padding:10px;margin:8px 0;box-sizing:border-box;}</style></head>"
                  "<body><h2>Configurar WiFi Estacion</h2>"
                  "<form action='/guardar' method='POST'>"
                  "<label>Selecciona tu Red:</label>"
                  "<select name='ssid'>" + htmlOpcionesRedes + "</select>"
                  "<label>Contrasena:</label>"
                  "<input type='password' name='pass' placeholder='Clave Wi-Fi'>"
                  "<input type='submit' value='Guardar y Conectar' style='background:#04AA6D;color:white;border:none;border-radius:4px;'>"
                  "</form></body></html>";
    request->send(200, "text/html", html);
  });

  // Web Endpoint para recibir y guardar las credenciales ingresadas
  server.on("/guardar", HTTP_POST, [](AsyncWebServerRequest *request){
    String reqSSID = "";
    String reqPass = "";
    if (request->hasParam("ssid", true)) reqSSID = request->getParam("ssid", true)->value();
    if (request->hasParam("pass", true)) reqPass = request->getParam("pass", true)->value();

    if (reqSSID.length() > 0) {
      preferences.putString("ssid", reqSSID);
      preferences.putString("pass", reqPass);
      request->send(200, "text/html", "<h2>Datos guardados correctamente. Reiniciando servidor...</h2>");
      delay(2000);
      ESP.restart(); // Reinicia el ESP32 para conectarse a la nueva red
    } else {
      request->send(400, "text/html", "Error: Selecciona una red valida.");
    }
  });

  server.begin();

  // Pausa la ejecución aquí hasta que la persona guarde los datos y el ESP32 se reinicie
  while (true) {
    delay(500);
  }
}

void setup() {
  Serial.begin(115200);

  tft.begin();
  tft.setRotation(0); // 0 = Orientación vertical (240x320)
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Iniciando Sistema..");

  // Leer credenciales guardadas en la memoria interna
  preferences.begin("wifi-config", false);
  String ssidGuardado = preferences.getString("ssid", "");
  String passGuardado = preferences.getString("pass", "");

  bool conectado = false;

  if (ssidGuardado.length() > 0) {
    tft.setCursor(10, 40);
    tft.println("Conectando WiFi:");
    tft.setTextColor(ILI9341_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(10, 65);
    tft.println(ssidGuardado);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssidGuardado.c_str(), passGuardado.c_str());

    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 15) {
      delay(500);
      Serial.print(".");
      intentos++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      conectado = true;
    }
  }

  // Si no hay credenciales guardadas o falló la conexión, iniciar portal de escaneo
  if (!conectado) {
    iniciarPortalConfiguracion();
  }

  server.on("/api/datos", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", ultimoPaqueteJSON);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });
  server.begin();

  // Iniciar Bus SPI compartido y Módulo LoRa
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

  tft.setCursor(10, 305); tft.print("Act:");
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

    // Dibuja un rectángulo negro para limpiar el espacio donde se imprimirá la fecha.
    tft.fillRect(70, 305, 170, 20, ILI9341_BLACK); 
    tft.setTextSize(1); 
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK); 
    tft.setCursor(70, 310); 
    tft.print(doc["fecha"].as<String>());
  }
}

void enviarExtra(String variable, String valor, String unidad) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, API_URL_EXTRA);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-device-uid", DEVICE_UID);
  http.addHeader("x-api-key", API_KEY);

  StaticJsonDocument<200> doc;
  doc["lectura_iot_id"] = ultimaLecturaId;
  doc["variable"] = variable;
  doc["valor"] = valor;
  doc["unidad"] = unidad;
  doc["tipo_dato"] = "numerico";

  String payload;
  serializeJson(doc, payload);

  int codigoExtra = http.POST(payload);
  Serial.print("Extra ");
  Serial.print(variable);
  Serial.print(" -> HTTP ");
  Serial.println(codigoExtra);

  http.end();
}

void enviarAPlataforma(JsonDocument& doc) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - ultimoEnvio < INTERVALO_MINIMO) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, API_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-device-uid", DEVICE_UID);
  http.addHeader("x-api-key", API_KEY);

  StaticJsonDocument<256> envio;
  envio["temperatura"] = doc["temperatura"];
  envio["humedad"]     = doc["humedad"];
  envio["peso"]        = doc["peso"];
  envio["origen_comunicacion"] = "radiofrecuencia";

  String payload;
  serializeJson(envio, payload);

  int codigo = http.POST(payload);
  Serial.print("Lectura principal -> HTTP ");
  Serial.println(codigo);

  if (codigo == 201) {
    String respuesta = http.getString();
    StaticJsonDocument<128> respDoc;
    deserializeJson(respDoc, respuesta);
    ultimaLecturaId = respDoc["id"] | -1;
    ultimoEnvio = millis();
    http.end();

    if (ultimaLecturaId > 0) {
      enviarExtra("co2", doc["co2"].as<String>(), "ppm");
      enviarExtra("altitud", doc["altitud"].as<String>(), "m");
      enviarExtra("frecuencia", doc["frecuencia"].as<String>(), "Hz");
    }
  } else {
    Serial.println(http.getString());
    http.end();
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

    StaticJsonDocument<256> docLectura;
    if (!deserializeJson(docLectura, ultimoPaqueteJSON)) {
      enviarAPlataforma(docLectura);
    }
  }
}