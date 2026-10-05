#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_BME280.h>
#include <SPI.h>
#include <LoRa.h>
#include <RTClib.h>
#include "HX711.h"

// Configuración de la Pantalla OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

// Presión a nivel del mar para el BME280
#define SEALEVELPRESSURE_HPA (1013.25)

// Pines I2C
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 20

// Pines SPI para el Módulo LoRa
#define SPI_SCK  18   
#define SPI_MISO 17   
#define SPI_MOSI 16   
#define LORA_NSS   5
#define LORA_RST  6   
#define LORA_DIO0 7   

// Pines para el módulo HX711 (Báscula)
#define HX711_DT  1
#define HX711_SCK 2

// Instancias de los periféricos
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SCD30 scd30;
Adafruit_BME280 bme;
RTC_DS3231 rtc;
HX711 bascula;

// Variables para almacenar las lecturas actuales
float tempActual = 0.0;
float humActual = 0.0;
float co2Actual = 0.0;
float altitudActual = 0.0;
float pesoActual = 0.0;
float factor_calibracion = -22580.0;

void setup() {
  Serial.begin(115200);

  // Inicializar Buses de comunicación
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, LORA_NSS);

  // Inicializar Pantalla OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Fallo OLED"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE); 
  display.setTextSize(1);
  display.setCursor(0, 0); 
  display.println("Iniciando Nodo..."); 
  display.display();
  Serial.println("OLED OK!");

  // Inicializar RTC DS3231
  if (!rtc.begin()) {
    Serial.println("No se encontró el módulo RTC");
    while (1);
  }
  Serial.println("RTC OK!");

  // Ajustar hora si se perdió la alimentación
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Inicializar SCD30
  if (!scd30.begin()) {
    Serial.println("¡Error con SCD30!");
    while (1) { delay(10); }
  }
  Serial.println("SCD30 OK!");

  // Inicializar BME280
  unsigned status = bme.begin(0x76); 
  if (!status) {
    status = bme.begin(0x77); 
    if (!status) {
      Serial.println("¡Error: No se encontró el BME280! Revisa cables o dirección.");
      while (1) { delay(10); }
    }
  }
  Serial.println("BME280 OK!");

  // Inicializar y calibrar HX711
  bascula.begin(HX711_DT, HX711_SCK);
  bascula.set_scale(factor_calibracion);
  bascula.tare();
  Serial.println("HX711 OK!");

  // Inicializar LoRa
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("Error LoRa");
    while (1);
  }
  Serial.println("LoRa OK!");
}

void imprimirPantalla() {
  display.clearDisplay();

  // Temperatura y Humedad
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Temp: ");
  display.print(tempActual, 1);
  display.print(" C");

  display.setCursor(68, 0);
  display.print("Hum: ");
  display.print(humActual, 1);
  display.print("%");

  // CO2
  display.setCursor(0, 14);
  display.print("CO2:  ");
  display.print(co2Actual, 0);
  display.print(" ppm");

  // Altitud
  display.setCursor(0, 26);
  display.print("Alt:  ");
  display.print(altitudActual, 1);
  display.print(" m");

  // Peso
  display.setCursor(0, 38);
  display.print("Peso: ");
  display.print(pesoActual, 2);
  display.print(" Kg");

  // Hora actual del RTC
  DateTime ahora = rtc.now();
  display.setCursor(0, 50);
  display.printf("Hora: %02d:%02d:%02d", ahora.hour(), ahora.minute(), ahora.second());

  display.display();
}

void loop() {
  // Leer SCD30
  if (scd30.dataReady()) {
    if (scd30.read()) {
      co2Actual = scd30.CO2;
      tempActual = scd30.temperature;
      humActual = scd30.relative_humidity;
    }
  }

  // Leer BME280
  altitudActual = bme.readAltitude(SEALEVELPRESSURE_HPA);

  // Leer peso del HX711
  if (bascula.is_ready()) {
    pesoActual = bascula.get_units(1);
  }

  // Actualizar pantalla OLED
  imprimirPantalla();

  // Obtener fecha y hora del RTC DS3231
  DateTime ahora = rtc.now();
  String fechaHora = String(ahora.year()) + "-" + 
                     String(ahora.month()) + "-" + 
                     String(ahora.day()) + " " + 
                     String(ahora.hour()) + ":" + 
                     String(ahora.minute()) + ":" + 
                     String(ahora.second());

  // Construir trama JSON
  String json = "{";
  json += "\"fecha\":\"" + fechaHora + "\",";
  json += "\"co2\":" + String(co2Actual, 0) + ",";
  json += "\"temperatura\":" + String(tempActual, 1) + ",";
  json += "\"humedad\":" + String(humActual, 1) + ",";
  json += "\"altitud\":" + String(altitudActual, 1) + ",";
  json += "\"peso\":" + String(pesoActual, 2);
  json += "}";

  // Enviar JSON vía LoRa
  LoRa.beginPacket();
  LoRa.print(json);
  LoRa.endPacket();

  delay(2000);
}