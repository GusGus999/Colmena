#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_BME280.h>
#include <SPI.h>
#include <LoRa.h>
#include <RTClib.h>

// Configuración de la Pantalla OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

// Presión a nivel del mar para el BME280
#define SEALEVELPRESSURE_HPA (1013.25)

// Pines I2C
#define I2C_SDA_PIN 2 // 21
#define I2C_SCL_PIN 4 //20

// Pines SPI para el Módulo LoRa
#define SPI_SCK  25 // 18   
#define SPI_MISO 26 // 17   
#define SPI_MOSI 27 // 16   
#define LORA_NSS  12 // 5
#define LORA_RST  13 // 6   
#define LORA_DIO0 14 // 7   

// Instancias de los periféricos
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SCD30 scd30;
Adafruit_BME280 bme;
RTC_DS3231 rtc;

// Variables para almacenar las lecturas actuales
float tempActual = 0.0;
float humActual = 0.0;
float co2Actual = 0.0;
float altitudActual = 0.0;

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

  // Si el RTC perdió la batería, ajusta a la hora de compilación
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

  // Temperatura
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(tempActual, 1); 
  display.setTextSize(1);
  display.print("C");

  // Humedad
  display.setTextSize(2);
  display.setCursor(64, 0);
  display.print(humActual, 1);
  display.setTextSize(1);
  display.print("%");

  // CO2
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.print("CO2: ");
  display.print(co2Actual, 0);
  display.println(" ppm");

  // Altitud
  display.setCursor(0, 38);
  display.print("Alt: ");
  display.print(altitudActual, 1);
  display.println(" m");

  // Hora actual del RTC
  DateTime ahora = rtc.now();
  display.setCursor(0, 52);
  display.printf("Hora: %02d:%02d:%02d", ahora.hour(), ahora.minute(), ahora.second());

  display.display();
}

void loop() {
  // Leer temperatura, humedad y CO2 del SCD30
  if (scd30.dataReady()) {
    if (scd30.read()) {
      co2Actual = scd30.CO2;
      tempActual = scd30.temperature;
      humActual = scd30.relative_humidity;
    }
  }

  // Leer altitud del BME280
  altitudActual = bme.readAltitude(SEALEVELPRESSURE_HPA);

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

  // Construir trama JSON solo con las variables activas
  String json = "{";
  json += "\"fecha\":\"" + fechaHora + "\",";
  json += "\"co2\":" + String(co2Actual, 0) + ",";
  json += "\"temperatura\":" + String(tempActual, 1) + ",";
  json += "\"humedad\":" + String(humActual, 1) + ",";
  json += "\"altitud\":" + String(altitudActual, 1);
  json += "}";

  // Enviar JSON vía LoRa
  LoRa.beginPacket();
  LoRa.print(json);
  LoRa.endPacket();

  delay(2000);
}