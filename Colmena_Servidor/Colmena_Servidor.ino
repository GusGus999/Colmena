#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <LoRa.h>

const char* ssid = "Pichishouse_EXT";
const char* password = "Pichi1970";
//const char* ssid = "TP-Link_7D88";
//const char* password = "19663043";

AsyncWebServer server(80);

#define LORA_NSS 5
#define LORA_RST 15
#define LORA_DIO0 2

// Variable global que almacena el JSON asíncrono
String ultimoPaqueteJSON = "{\"co2\":0,\"temperatura\":0,\"humedad\":0,\"altitud\":0,\"frecuencia\":0,\"peso\":0}";

// Tu interfaz Web
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
    <div class="card co2"><div class="label">Dióxido de Carbono</div><div class="value" id="co2">--</div><div class="unit">ppm</div></div>
    <div class="card"><div class="label">Temperatura</div><div class="value" id="temp">--</div><div class="unit">&deg;C</div></div>
    <div class="card hum"><div class="label">Humedad Relativa</div><div class="value" id="hum">--</div><div class="unit">%</div></div>
    <div class="card alt"><div class="label">Altitud</div><div class="value" id="alt">--</div><div class="unit">m s.n.m.</div></div>
    <div class="card frec"><div class="label">Frecuencia</div><div class="value" id="frec">--</div><div class="unit">Hz</div></div>
    <div class="card peso"><div class="label">Peso</div><div class="value" id="peso">--</div><div class="unit">Kg</div></div>
  </div>
<script>
  function obtenerDatos() {
    fetch('/api/datos')
      .then(response => response.json())
      .then(data => {
        document.getElementById("co2").innerText = Math.round(data.co2);
        document.getElementById("temp").innerText = data.temperatura.toFixed(1);
        document.getElementById("hum").innerText = data.humedad.toFixed(1);
        document.getElementById("alt").innerText = data.altitud.toFixed(1);
        document.getElementById("frec").innerText = Math.round(data.frecuencia);
        document.getElementById("peso").innerText = data.peso.toFixed(2);
      }).catch(err => console.error(err));
  }
  setInterval(obtenerDatos, 2000);
  obtenerDatos();
</script>
</body>
</html>)rawliteral";

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Conectando Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) { delay(500); 
  Serial.print("."); }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/api/datos", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", ultimoPaqueteJSON);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });

  server.begin();
  
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Error iniciando LoRa Receptor");
    while (1);
  }
  Serial.println("LoRa y Wi-Fi OK!");
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
  }
}
