#define I2C_SDA 14
#define I2C_SCL 13
#define ENS160_EN_PIN 12
//#define DEBUG

#ifdef DEBUG
  #include "esp_mac.h" 
  String getDefaultMacAddress() {

  String mac = "";

  unsigned char mac_base[6] = {0};

  if (esp_efuse_mac_get_default(mac_base) == ESP_OK) {
    char buffer[18];  // 6*2 characters for hex + 5 characters for colons + 1 character for null terminator
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac_base[0], mac_base[1], mac_base[2], mac_base[3], mac_base[4], mac_base[5]);
    mac = buffer;
  }

  return mac;
}

String getInterfaceMacAddress(esp_mac_type_t interface) {

  String mac = "";

  unsigned char mac_base[6] = {0};

  if (esp_read_mac(mac_base, interface) == ESP_OK) {
    char buffer[18];  // 6*2 characters for hex + 5 characters for colons + 1 character for null terminator
    sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X", mac_base[0], mac_base[1], mac_base[2], mac_base[3], mac_base[4], mac_base[5]);
    mac = buffer;
  }

  return mac;
}
#endif

#include <Wire.h>

#include <Adafruit_AHTX0.h>
Adafruit_AHTX0 aht;

#include "ScioSense_ENS160.h"
ScioSense_ENS160 ens160(ENS160_I2CADDR_1); //0x53..ENS160+AHT21


#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

#define COMPANY_ID 0xFFFF

uint8_t crc8(uint8_t *data, uint8_t len) {
  uint8_t crc = 0x00;
  for (uint8_t i = 0; i < len; i++) crc ^= data[i];
  return crc;
}

#define RGB_BRIGHTNESS 5
#ifdef RGB_BUILTIN

  enum LedColor {
    RED = 0,
    GREEN = 1,
    BLUE = 2
  };
  void blink(uint8_t number, LedColor color);

  void blink(uint8_t number, LedColor color){
    for(uint8_t i = 0; i<number;i++){
      switch(color) {
        case RED:
          rgbLedWrite(RGB_BUILTIN, RGB_BRIGHTNESS, 0, 0);
          break;
        case GREEN:
          rgbLedWrite(RGB_BUILTIN, 0, RGB_BRIGHTNESS, 0);
          break;
        case BLUE:
          rgbLedWrite(RGB_BUILTIN, 0, 0, RGB_BRIGHTNESS);
          break;
      }
      delay(50);
      rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
      delay(200);
    }
    rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
    delay(500);
  }
#endif


#include <esp_sleep.h>
#define SLEEP_TIME_SEC 2   // tiempo de sleep


void setup() {
  pinMode(ENS160_EN_PIN, OUTPUT);
  digitalWrite(ENS160_EN_PIN, HIGH);
  
  #ifdef DEBUG
    Serial.begin(9600);  
    delay(100); // tiempo de estabilización
    #ifdef RGB_BUILTIN
      while (!Serial) {
        blink(1, RED)
      }
    #endif
     Serial.print("Wi-Fi Station (using 'esp_efuse_mac_get_default')\t");
    Serial.println(getDefaultMacAddress());

    Serial.print("WiFi Station (using 'esp_read_mac')\t\t\t");
    Serial.println(getInterfaceMacAddress(ESP_MAC_WIFI_STA));

    Serial.print("WiFi Soft-AP (using 'esp_read_mac')\t\t\t");
    Serial.println(getInterfaceMacAddress(ESP_MAC_WIFI_SOFTAP));

    Serial.print("Bluetooth (using 'esp_read_mac')\t\t\t");
    Serial.println(getInterfaceMacAddress(ESP_MAC_BT));

    Serial.print("Ethernet (using 'esp_read_mac')\t\t\t\t");
    Serial.println(getInterfaceMacAddress(ESP_MAC_ETH));
  #endif

  delay(1000);

  ens160.setI2C(I2C_SDA, I2C_SCL);
  ens160.begin();
  auto ret = ens160.setMode(ENS160_OPMODE_STD);
  #ifdef DEBUG
    Serial.printf("ENS160: ");
    Serial.println(ens160.available() and ret ? "done." : "failed!");
  #endif
  ret = aht.begin();
  
  #ifdef DEBUG
    Serial.printf("AHT20: ");
    Serial.println(ret ? "done." : "failed!");
  #endif

  // Inicializar el MAX17043
  //if (!max17043.begin()) {
  //  Serial.println("No se pudo inicializar el MAX17043");
  //  while (1); // Detener el programa si no se puede inicializar
  //}

  sensors_event_t h, t;
  aht.getEvent(&h, &t);
  int16_t temp = t.temperature * 100;
  uint16_t hum = h.relative_humidity * 100;

  uint8_t aqi = 0;
  uint16_t tvoc = 0, eco2 = 0;
  if (ens160.available()) {
  // Give values to Air Quality Sensor.
    ens160.set_envdata(t.temperature, h.relative_humidity);
    ens160.measure(true);
    ens160.measureRaw(true);
    aqi = ens160.getAQI();
    tvoc = ens160.getTVOC();
    eco2 = ens160.geteCO2();
  }
  uint16_t batt = 0;

  uint8_t mfgData[14];   // 2 bytesID + 12 bytes payload
    mfgData[0] = COMPANY_ID & 0xFF;        // LSB
    mfgData[1] = (COMPANY_ID >> 8) & 0xFF; 
  
    mfgData[2]  = temp >> 8;
    mfgData[3]  = temp & 0xFF;
    mfgData[4]  = hum >> 8;
    mfgData[5]  = hum & 0xFF;
    mfgData[6]  = aqi;
    mfgData[7]  = tvoc >> 8;
    mfgData[8]  = tvoc & 0xFF;
    mfgData[9]  = eco2 >> 8;
    mfgData[10]  = eco2 & 0xFF;
    mfgData[11]  = batt >> 8;
    mfgData[12] = batt & 0xFF;
    mfgData[13] = crc8(mfgData, 13);
  
     // ---- BLE Advertising ----
    BLEDevice::init("ESP_AIR_DS");
    BLEAdvertising *adv = BLEDevice::getAdvertising();
    BLEAdvertisementData advData;
  
    adv->setScanResponse(false);
    adv->setMinPreferred(0x12);
  
    advData.setFlags(0x06);
    String mfgString;
    mfgString.reserve(sizeof(mfgData));
    for (size_t i = 0; i < sizeof(mfgData); i++) {
      mfgString += (char)mfgData[i];
    }
    advData.setManufacturerData(mfgString);
  
    adv->setAdvertisementData(advData);
  
    BLEDevice::startAdvertising();
    #ifdef DEBUG
      Serial.println("========== BLE PACKET ==========");
      Serial.print("Temperature : "); Serial.print((float)temp/100, 2); Serial.println(" °C");
      Serial.print("Humidity    : "); Serial.print((float)hum/100, 2); Serial.println(" %");
      Serial.print("AQI         : "); Serial.println(aqi);
      Serial.print("TVOC        : "); Serial.print(tvoc); Serial.println(" ppb");
      Serial.print("eCO2        : "); Serial.print(eco2); Serial.println(" ppm");
      Serial.print("Battery     : "); Serial.print(batt, 2); Serial.println(" V");
      Serial.println("================================");
    #endif
  
    delay(200);
    BLEDevice::stopAdvertising();
  
      // ---- Deep Sleep ----
    esp_sleep_enable_timer_wakeup(SLEEP_TIME_SEC * 1000000ULL);
    esp_deep_sleep_start();
  
    delay(1000);
  
  }
  
  void loop() {}
