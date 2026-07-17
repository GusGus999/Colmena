#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SCD30.h>

// --- CONFIGURACIÓN WI-FI ---
const char* ssid = "Pichishouse_EXT";
const char* password = "Pichi1970";

// --- CONFIGURACIÓN OLED E I2C ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SCD30 scd30;
AsyncWebServer server(80);

// Variables para almacenar la última lectura
float tempActual = 0.0;
float humActual = 0.0;
float co2Actual = 0.0;

// Interfaz Web Almacenada en la memoria FLASH
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Monitoreo Ambiental IoT</title>
  <style>
    body { font-family: 'Segoe UI', sans-serif; background: #121212; color: #e0e0e0; text-align: center; margin: 0; padding: 20px; }
    h1 { color: #ffb300; margin-bottom: 30px; }
    .grid { display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; max-width: 800px; margin: 0 auto; }
    .card { background: #1e1e1e; border-radius: 12px; padding: 20px; min-width: 200px; flex: 1; box-shadow: 0 4px 15px rgba(0,0,0,0.5); border-left: 5px solid #ffb300; }
    .card.co2 { border-left-color: #ff5252; }
    .card.hum { border-left-color: #40c4ff; }
    .label { font-size: 1.1rem; color: #9e9e9e; }
    .value { font-size: 2.5rem; font-weight: bold; margin: 10px 0; }
    .unit { font-size: 1rem; color: #757575; }
  </style>
</head>
<body>
  <h1>Panel de Monitoreo Ambiental</h1>
  <div class="grid">
    <div class="card co2">
      <div class="label">Dióxido de Carbono</div>
      <div class="value" id="co2">--</div>
      <div class="unit">ppm</div>
    </div>
    <div class="card">
      <div class="label">Temperatura</div>
      <div class="value" id="temp">--</div>
      <div class="unit">&deg;C</div>
    </div>
    <div class="card hum">
      <div class="label">Humedad Relativa</div>
      <div class="value" id="hum">--</div>
      <div class="unit">%</div>
    </div>
  </div>
<script>
  function obtenerDatos() {
    fetch('/api/datos')
      .then(response => response.json())
      .then(data => {
        document.getElementById("co2").innerText = Math.round(data.co2);
        document.getElementById("temp").innerText = data.temperatura.toFixed(1);
        document.getElementById("hum").innerText = data.humedad.toFixed(1);
      })
      .catch(err => console.error("Error obteniendo datos: ", err));
  }
  setInterval(obtenerDatos, 2000); // Consultar cada 2 segundos sin recargar
  obtenerDatos();
</script>
</body>
</html>)rawliteral";

void setup() {
  Serial.begin(115200);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Fallo OLED"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Conectando Wi-Fi...");
  display.display();

  // Conectar Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n¡Wi-Fi Conectado!");
  Serial.print("Direccion IP: ");
  Serial.println(WiFi.localIP());

  // Mostrar IP en OLED
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Wi-Fi OK!");
  display.setCursor(0, 20);
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();
  delay(2000);

  if (!scd30.begin()) {
    Serial.println("¡Error con SCD30!");
    while (1) { delay(10); }
  }

  // Configurar Servidor Asíncrono
  // Ruta principal (Servir página HTML)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Ruta API JSON para actualización en segundo plano
  server.on("/api/datos", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"co2\":" + String(co2Actual, 0) + ",";
    json += "\"temperatura\":" + String(tempActual, 1) + ",";
    json += "\"humedad\":" + String(humActual, 1);
    json += "}";
    request->send(200, "application/json", json);
  });

  server.begin();
  Serial.println("Servidor Web Asíncrono iniciado.");
}

void loop() {
  if (scd30.dataReady()) {
    
    if (scd30.read()) {
      co2Actual = scd30.CO2;
      tempActual = scd30.temperature;
      humActual = scd30.relative_humidity;

      display.clearDisplay();

      display.setTextSize(2);
      display.setCursor(0, 0);
      display.print("Ambiente");

      display.setTextSize(1);
      display.setCursor(0, 20);
      display.print("CO2:  ");
      display.print(scd30.CO2, 0);
      display.setTextSize(1);
      display.print(" ppm");

      display.setCursor(0, 35);
      display.print("Temp: ");
      display.print(scd30.temperature, 1); 
      display.print(" C");

      display.setCursor(0, 50);
      display.print("Hum:  ");
      display.print(scd30.relative_humidity, 1); 
      display.print(" %");

      display.display();
    }
  }

  delay(500);
}