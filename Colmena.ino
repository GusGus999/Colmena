//Acustica -
//Peso -
//Co2 -
//Fotos 
//Temperatura -
//Temperatura Relativa 
//Humedad -
//Altura -

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_BME280.h>
#include <driver/i2s.h>
#include "arduinoFFT.h"
#include "HX711.h"

const char* ssid = "Pichishouse_EXT";
const char* password = "Pichi1970";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 20
#define I2S_SCK 40
#define I2S_WS  41
#define I2S_SD  42
#define I2S_PORT I2S_NUM_0
#define SEALEVELPRESSURE_HPA (1013.25)
#define HX711_DT 1
#define HX711_SCK 2

const uint16_t muestras = 1024;           
const double frecuencia_muestreo = 16000;  

double vReal[muestras];
double vImag[muestras];

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, muestras, frecuencia_muestreo);
Adafruit_SCD30 scd30;
Adafruit_BME280 bme;
AsyncWebServer server(80);
HX711 bascula;

// Variables para almacenar la última lectura
float tempActual = 0.0;
float humActual = 0.0;
float co2Actual = 0.0;
float altitudActual = 0.0;
float frecActual = 0;
float pesoActual = 0;
float factor_calibracion = -22580.0;

int32_t muestra_audio = 0;
size_t bytes_leidos = 0;

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
    <div class="card alt">
      <div class="label">Altitud</div>
      <div class="value" id="alt">--</div>
      <div class="unit">m s.n.m.</div>
    </div>
    <div class="card frec">
      <div class="label">Frecuencia</div>
      <div class="value" id="frec">--</div>
      <div class="unit">Hz</div>
    </div>
    <div class="card peso">
      <div class="label">Peso</div>
      <div class="value" id="peso">--</div>
      <div class="unit">Kg</div>
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
        document.getElementById("alt").innerText = data.altitud.toFixed(1);
        document.getElementById("frec").innerText = Math.round(data.frecuencia);
        document.getElementById("peso").innerText = data.peso.toFixed(2);
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

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Fallo OLED"));
    for(;;);
  }
  Serial.println("OLED OK!");

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
  Serial.println("SCD30 OK!");

  //BME280
  unsigned status = bme.begin(0x76); 
  if (!status) {
    status = bme.begin(0x77); // 0x76
    if (!status) {
      Serial.println("¡Error: No se encontró el BME280! Revisa cables o dirección.");
      while (1) { delay(10); }
    }
  }
  Serial.println("BME280 OK!");

  // Configuración del protocolo I2S para el micrófono
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX), 
    .sample_rate = (uint32_t)frecuencia_muestreo,      
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,      
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,       
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,          
    .dma_buf_count = 8,                                
    .dma_buf_len = 64,                                 
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE, 
    .data_in_num = I2S_SD              
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  
  Serial.println("INMP441 OK!");

  // Inicializar y calibrar HX711
  bascula.begin(HX711_DT, HX711_SCK);
  bascula.set_scale(factor_calibracion);
  bascula.tare();
  Serial.println("HX711 OK!");

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
    json += "\"humedad\":" + String(humActual, 1) + ",";
    json += "\"altitud\":" + String(altitudActual, 1) + ",";
    json += "\"frecuencia\":" + String(frecActual, 0) + ",";
    json += "\"peso\":" + String(pesoActual, 2);
    json += "}";
    request->send(200, "application/json", json);
  });

  server.begin();
  Serial.println("Servidor Web Asíncrono iniciado.");
}

void imprimirPantalla() {
      display.clearDisplay();

      display.setTextSize(2);
      display.setCursor(0, 0);
      display.print(tempActual, 1); 
      display.setTextSize(1);
      display.print("C");

      display.setTextSize(2);
      display.setCursor(64, 0);
      display.print(humActual, 1);
      display.setTextSize(1);
      display.print("%");

      display.setTextSize(1);
      display.setCursor(0, 16);
      display.print("CO2:  ");
      display.print(co2Actual, 0);
      display.setTextSize(1);
      display.print(" ppm");

      display.setCursor(0, 26);
      display.print("Alt:  ");
      display.print(altitudActual, 1);
      display.println(" m");

      display.setCursor(0, 36);
      display.print("Frec: ");
      display.print(frecActual);
      display.println(" Hz");

      display.setCursor(0, 46);
      display.print("Peso: ");
      display.print(pesoActual);
      display.println(" Kg");

      display.display();
}

void loop() {
  if (scd30.dataReady()) {
    if (scd30.read()) {
      co2Actual = scd30.CO2;
      tempActual = scd30.temperature;
      humActual = scd30.relative_humidity;
    }
  }

  altitudActual = bme.readAltitude(SEALEVELPRESSURE_HPA);

  // Leer peso del HX711
  if (bascula.is_ready()) {
    pesoActual = bascula.get_units(1); // Realiza una lectura de la báscula
  }

  // Recolectar un paquetes de sonido
  for (int i = 0; i < muestras; i++) {
    i2s_read(I2S_PORT, &muestra_audio, sizeof(muestra_audio), &bytes_leidos, portMAX_DELAY);
    
    if (bytes_leidos > 0) {
      vReal[i] = (double)(muestra_audio >> 14); 
      vImag[i] = 0.0;
    } else {
      vReal[i] = 0.0;
      vImag[i] = 0.0;
    }
  }

  // Ejecutar la Transformada Rápida de Fourier
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();

  // Obtener la Frecuencia Dominante
  double frecuencia_dominante = FFT.majorPeak();

  // Filtro básico de ruido
  if (frecuencia_dominante > 20.0) { 
    frecActual = frecuencia_dominante;
  }

  imprimirPantalla();

  delay(500);
}


