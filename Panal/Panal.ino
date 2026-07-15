#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SCD30.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SCD30 scd30;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("Inicializando sistema de monitoreo...");

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Fallo al inicializar la pantalla SSD1306 OLED"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("Iniciando SCD30...");
  display.display();

  if (!scd30.begin()) {
    Serial.println("¡No se pudo encontrar el sensor SCD30! Revisa las conexiones.");
    display.clearDisplay();
    display.setCursor(0, 20);
    display.println("Error: Sensor SCD30");
    display.println("no encontrado.");
    display.display();
    while (1) { delay(10); }
  }

  // scd30.setMeasurementInterval(2); // Intervalo de medición en segundos (2 por defecto)
  
  display.clearDisplay();
  display.setCursor(0, 20);
  display.println("¡Todo listo!");
  display.display();
  delay(1500);
}

void loop() {
  if (scd30.dataReady()) {
    
    if (!scd30.read()) {
      Serial.println("Error al leer datos del SCD30");
      return;
    }

    Serial.print("CO2: ");
    Serial.print(scd30.CO2, 0);
    Serial.print(" ppm | Temp: ");
    Serial.print(scd30.temperature, 1);
    Serial.print(" C | Humedad: ");
    Serial.print(scd30.relative_humidity, 1);
    Serial.println(" %");

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

  delay(500);
}