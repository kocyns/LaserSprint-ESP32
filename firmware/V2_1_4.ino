#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_mac.h"
#include "esp_system.h"

// --- KONFIGURACE NATIVNÍCH PINŮ PRO ESP32-C3 ---
#define POWER_PIN   6   
#define BUTTON_PIN  1   
#define PIXEL_PIN   0   
#define MCU_RX_PIN  5   
#define MCU_TX_PIN  4   
#define BAT_PIN     3   

Adafruit_NeoPixel pixel(1, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

// --- BLE PROMĚNNÉ ---
uint8_t gateIdHex = 0x00;  
bool bleIsActive = false; 
int currentBatDigit = 9;   
unsigned long lastBatCheck = 0;

// Proměnné pro časový offset
unsigned long triggerTime = 0; 

// --- PROMĚNNÉ PRO SENZOR A ALGORITMUS ---
unsigned long triggerActiveUntil = 0;
float dist_old = 0;          
unsigned long buttonPressStart = 0;

// --- PROMĚNNÉ PRO UART PARSER (TF-LUNA) ---
uint8_t uartBuffer[9];
uint8_t uartIndex = 0;

uint32_t currentPixelColor = 0xFFFFFFFF; 

void setPixelColorIfChanged(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t newColor = (r << 16) | (g << 8) | b;
    if (newColor != currentPixelColor) {
        currentPixelColor = newColor;
        pixel.setPixelColor(0, r, g, b);
        pixel.show();
    }
}

void updateBLEServiceUUID(bool activeState, int batDigit) {
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->stop(); 
    
    uint16_t interval = activeState ? 0x20 : 0x140;
    pAdvertising->setMinInterval(interval);
    pAdvertising->setMaxInterval(interval);

    BLEAdvertisementData advData;
    advData.setFlags(0x06);
    advData.setName("LS_GATE");
    
    int offsetMs = 0;
    if (activeState && triggerTime > 0) {
        unsigned long elapsed = millis() - triggerTime;
        if (elapsed > 999) elapsed = 999;
        offsetMs = (int)elapsed;
    }

    int offsetTens = offsetMs / 10;
    if (offsetTens > 99) offsetTens = 99;

    char uuidStr[37];
    snprintf(uuidStr, sizeof(uuidStr), "11111111-2222-3333-4444-555555%02x%d%02d%d", 
             gateIdHex,
             activeState ? 1 : 0, 
             offsetTens, 
             batDigit);
    
    advData.setCompleteServices(BLEUUID(uuidStr));
    pAdvertising->setAdvertisementData(advData);
    pAdvertising->start(); 
}

void readBattery() {
    uint32_t pinMilliVolts = analogReadMilliVolts(BAT_PIN);
    float batteryVoltage = (pinMilliVolts * (100.0 + 220.0) / 220.0) / 1000.0;
    
    int pct = (batteryVoltage - 3.2) / (4.2 - 3.2) * 100;
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
    
    int digit = pct / 10;
    if (digit > 9) digit = 9;
    if (digit < 0) digit = 0;
    
    if (digit != currentBatDigit) {
        currentBatDigit = digit;
        updateBLEServiceUUID(bleIsActive, currentBatDigit);
    }
}

void setup() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    gateIdHex = mac[5]; 

    pixel.begin();
    pixel.setBrightness(50);

    // Zapnutí napájení 5V pro LED
    pinMode(POWER_PIN, OUTPUT);
    digitalWrite(POWER_PIN, HIGH);
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    bool powerOnConfirmed = false;
    esp_reset_reason_t resetReason = esp_reset_reason();

    // Pokud start NEBYL vyvolán probuzením ze spánku (HW reset nebo připojení napájení)
    if (resetReason != ESP_RST_DEEPSLEEP) {
        powerOnConfirmed = true; 
        pixel.setPixelColor(0, 0, 255, 0); // Zelené potvrzení zapnutí
        pixel.show();
    } else {
        // Probuzení ze spánku -> vyžaduje 2s držení
        unsigned long startHoldTime = millis();

        while (digitalRead(BUTTON_PIN) == LOW) {
            if (millis() - startHoldTime >= 2000) {
                powerOnConfirmed = true;
                pixel.setPixelColor(0, 0, 255, 0); 
                pixel.show();
                break;
            }
            delay(10);
        }
    }

    if (!powerOnConfirmed) {
        pixel.setPixelColor(0, 0, 0, 0);
        pixel.show();
        digitalWrite(POWER_PIN, LOW);
        esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
        esp_deep_sleep_start();
    }

    while (digitalRead(BUTTON_PIN) == LOW) {
        delay(10);
    }

    analogSetAttenuation(ADC_11db);
    Serial1.begin(115200, SERIAL_8N1, MCU_RX_PIN, MCU_TX_PIN);
    
    BLEDevice::init(""); 
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setScanResponse(false);
    
    readBattery();
    updateBLEServiceUUID(false, currentBatDigit);
}

void loop() {
    unsigned long currentTime = millis();

    // Vypnutí po 2s držení
    if (digitalRead(BUTTON_PIN) == LOW) {
        if (buttonPressStart == 0) {
            buttonPressStart = currentTime;
        } else if (currentTime - buttonPressStart >= 2000) {
            for (int i = 0; i < 3; i++) {
                pixel.setPixelColor(0, 255, 0, 0);
                pixel.show();
                delay(150);
                pixel.setPixelColor(0, 0, 0, 0);
                pixel.show();
                delay(150);
            }

            digitalWrite(POWER_PIN, LOW);
            currentPixelColor = 0;

            while (digitalRead(BUTTON_PIN) == LOW) { delay(10); }

            BLEDevice::getAdvertising()->stop();
            delay(100);

            esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
            esp_deep_sleep_start();
        }
    } else {
        buttonPressStart = 0;
    }

    if (currentTime - lastBatCheck >= 10000) {
        lastBatCheck = currentTime;
        readBattery();
    }

    if (Serial1.available() == 0) {
        delay(1);
    }

    uint16_t dist = 0;
    uint16_t strength = 0;
    bool uartSuccess = false;

    while (Serial1.available() > 0) {
        uint8_t c = Serial1.read();
        if (uartIndex == 0) {
            if (c == 0x59) { uartBuffer[0] = c; uartIndex = 1; }
        } else if (uartIndex == 1) {
            if (c == 0x59) { uartBuffer[1] = c; uartIndex = 2; }
            else { uartIndex = 0; }
        } else {
            uartBuffer[uartIndex] = c;
            uartIndex++;
            
            if (uartIndex == 9) {
                uartIndex = 0;
                uint8_t checksum = 0;
                for (int i = 0; i < 8; i++) checksum += uartBuffer[i];
                if (checksum == uartBuffer[8]) {
                    dist = uartBuffer[2] | (uartBuffer[3] << 8);       
                    strength = uartBuffer[4] | (uartBuffer[5] << 8);   
                    uartSuccess = true; 
                    break;
                }
            }
        }
    }

    if (!uartSuccess) return;

    bool validDistance = (dist > 15 && dist < 9000 && strength >= 20);
    if (dist_old == 0 && validDistance) dist_old = dist;
    if (validDistance) dist_old = (dist_old * 0.95) + (dist * 0.05);

    bool currentTrigger = false;
    if (validDistance) {
        if (dist_old > 15 && dist_old < 9000 && dist < (dist_old - 40)) currentTrigger = true;
        else if ((dist_old == 0 || dist_old >= 8000) && dist < 400) currentTrigger = true;
    }
    if (dist == 0 || dist > 9000) dist_old = 9000;

    if (currentTrigger) {
        triggerActiveUntil = currentTime + 500;
        if (!bleIsActive) {
            bleIsActive = true;
            triggerTime = currentTime;
            updateBLEServiceUUID(true, currentBatDigit);
        }
    } 
    
    if (currentTime < triggerActiveUntil) {
        setPixelColorIfChanged(255, 0, 0);
        
        static unsigned long lastActiveBleUpdate = 0;
        if (currentTime - lastActiveBleUpdate >= 25) {
            lastActiveBleUpdate = currentTime;
            updateBLEServiceUUID(true, currentBatDigit);
        }
    } else {
        if (bleIsActive) {
            bleIsActive = false;
            triggerTime = 0;
            updateBLEServiceUUID(false, currentBatDigit);
        }

        if (currentBatDigit >= 6) setPixelColorIfChanged(0, 255, 0); 
        else if (currentBatDigit >= 4) setPixelColorIfChanged(255, 200, 0); 
        else if (currentBatDigit >= 2) setPixelColorIfChanged(255, 80, 0); 
        else {
            if ((currentTime / 500) % 2 == 0) setPixelColorIfChanged(255, 0, 0);
            else setPixelColorIfChanged(0, 0, 0);
        }
    }
}
