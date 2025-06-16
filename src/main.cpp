#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <FS.h>
#include <SPIFFS.h>
#include <EEPROM.h>
#include <esp_now.h>
//MAC-ul acestui esp-D4:8A:FC:A4:89:90
uint8_t receiverMac[] = {0x3C, 0x8A, 0x1F, 0xB9, 0xDC, 0xCC};//Adresa MAC a esp32 care controloeaza încălzirea și umidificarea

TFT_eSPI tft = TFT_eSPI();
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 60000);

#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false  // Pune pe true dacă vrei să recalculezi touchscreen-ul
#define TEMP_MIN 10.0 // Temperatura minimă
#define TEMP_MAX 80.0 // Temperatura maximă
#define RH_MIN 30 // Umiditatea minimă
#define RH_MAX 80 // Umiditatea maximă

bool IN_SETTINGS_MENU=false; // Pune pe true dacă vrei să intri în meni
int lastMinute = -1;
int lastDay = -1;
int lastMonth = -1;
int lastYear = -1;
unsigned long lastTouchTime = 0;
const unsigned long debounceInterval = 200; //ms
const char *ssid = "FBI5G";
const char *password = "3.14159265";
bool buttonStates_Incalzire[] = {false, false, false};
bool buttonStates_Umidificare[] = {false, false, false};
float default_temp_set[] = {20.0, 20.0, 20.0}; // Temperatura setată pentru fiecare cameră
int default_rh_set[] = {50, 50, 50}; // Umiditatea setată pentru fiecare cameră
 static char message_length[16];

void decodare_date_primite() {
}

void trimite_mesaj_la_regulator(uint8_t cameraNR) {
    String mesaj = "cam" + String(cameraNR) + "-" + String(default_temp_set[cameraNR - 1],1) + "-" + String(default_rh_set[cameraNR - 1],0);
    const char* mesaj_cstr = mesaj.c_str();
    esp_err_t result = esp_now_send(receiverMac, (const uint8_t*)mesaj_cstr, strlen(mesaj_cstr));
    if (result == ESP_OK) {
        Serial.println("✅ Mesaj trimis cu succes!");
    } else {
        Serial.println("❌ Eroare la trimiterea mesajului!");
    }
}

void saveSettingsToEEPROM() {
    EEPROM.begin(512); // Alocăm spațiu suficient
    // 🔹 Salvează temperaturile și umiditatea
    for (int i = 0; i < 3; i++) {
        EEPROM.put(i * sizeof(float), default_temp_set[i]); // Salvăm temperaturile
        EEPROM.put(12 + i * sizeof(int), default_rh_set[i]); // Salvăm umiditatea
    }
    EEPROM.commit(); // 🔹 Confirmă modificările (esențial!)
    Serial.println("Setările au fost salvate în EEPROM.");
}

void loadSettingsFromEEPROM() {
    EEPROM.begin(512); // Alocăm spațiu suficient

    for (int i = 0; i < 3; i++) {
        EEPROM.get(i * 4, default_temp_set[i]);
        EEPROM.get(12 + i * 4, default_rh_set[i]);

        // 🔹 Verificăm dacă valoarea citită este validă (pentru evitarea coruperii datelor)
        if (default_temp_set[i] < 10.0 || default_temp_set[i] > 35.0) {
            default_temp_set[i] = 20.0; // Resetăm la valoarea default dacă este coruptă
        }

        if (default_rh_set[i] < 30 || default_rh_set[i] > 90) {
            default_rh_set[i] = 50; // Resetăm umiditatea la default dacă este coruptă
        }
    }
    EEPROM.end(); // 🔹 Evităm coruperea memoriei
    Serial.println("Setările au fost încărcate corect din EEPROM.");
}

void clearEEPROM() {
    EEPROM.begin(512); // Alocăm spațiu suficient
    for (int i = 0; i < 512; i++) {
        EEPROM.write(i, 0xFF); // Rescriem fiecare locație cu valoarea default (0xFF)
    }
    EEPROM.commit(); // 🔹 Confirmăm ștergerea
    Serial.println("EEPROM a fost curățat.");
}

void reset_to_default_settings() {
    clearEEPROM(); // Curățăm EEPROM-ul
    for (int i = 0; i < 3; i++) {
        default_temp_set[i] = 20.0; // Resetăm temperatura la 20.0
        default_rh_set[i] = 50; // Resetăm umiditatea la 50%
        buttonStates_Incalzire[i] = false; // Resetăm starea butonului de încălzire
        buttonStates_Umidificare[i] = false; // Resetăm starea butonului de umidificare
    }
    IN_SETTINGS_MENU = false; // Ieșim din meniul de setări
    Serial.println("Setările au fost resetate la valorile implicite.");
    saveSettingsToEEPROM(); // Salvăm setările implicite în EEPROM
    delay(500); // Așteptăm 1 secundă pentru a permite utilizatorului să vadă mesajul
    ESP.restart(); // Repornește ESP pentru a aplica setările implicite
}

void draw_button_incalzire(uint16_t pozX, uint16_t pozY, uint16_t lungimeL, uint16_t latimel, uint8_t nrCamera) {
    if (buttonStates_Incalzire[nrCamera - 1]) {
        tft.fillRect(pozX + 5, pozY + latimel - 20, 65, 15, TFT_GREEN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(pozX + 31, pozY + latimel - 16);
        tft.print("ON");
    } else {
        tft.fillRect(pozX + 5, pozY + latimel - 20, 65, 15, TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(pozX + 29, pozY + latimel - 16);
        tft.print("OFF");
    }
}

void draw_button_umidificare(uint16_t pozX, uint16_t pozY, uint16_t lungimeL, uint16_t latimel, uint8_t nrCamera) {
    if (buttonStates_Umidificare[nrCamera - 1]) {
        tft.fillRect(pozX + 79, pozY + latimel - 20, 65, 15, TFT_GREEN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(pozX + 105, pozY + latimel - 16);
        tft.print("ON");
    } else {
        tft.fillRect(pozX + 79, pozY + latimel - 20, 65, 15, TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(pozX + 103, pozY + latimel - 16);
        tft.print("OFF");
    }
}

void draw_buttons(uint16_t pozX, uint16_t pozY, uint16_t lungimeL, u16_t latimel, uint8_t nrCamera) {
    draw_button_incalzire(pozX, pozY, lungimeL, latimel, nrCamera);
    draw_button_umidificare(pozX, pozY, lungimeL, latimel, nrCamera);
}

void draw_button_minus(uint16_t yOffset, uint8_t i, bool isTemp) {
    if (isTemp) {
        // 🔹 Buton "-" pentru rosu
        tft.fillRect(200, yOffset - 5, 18, 18, TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(207, yOffset);
        tft.print("-");
        // 🔹 Afișează temperatura
        tft.fillRect(222, yOffset - 5, 40, 20, TFT_DARKCYAN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(230, yOffset);
        tft.print(default_temp_set[i], 1);
        // 🔹 Buton "-" inapoi transparent
        delay(200);
        tft.fillRect(200, yOffset - 5, 18, 18, TFT_DARKCYAN);
        tft.setTextColor(TFT_RED);
        tft.setCursor(207, yOffset);
        tft.print("-");
    } else {
        // 🔹 Buton "-" pentru rosu
        tft.fillRect(200, yOffset +15, 18, 18, TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(207, yOffset+20);
        tft.print("-");
        // 🔹 Afișează umiditatea
        tft.fillRect(222, yOffset +20, 40, 20, TFT_DARKCYAN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(230, yOffset+20);
        tft.print(default_rh_set[i], 1);
        // 🔹 Buton "-" inapoi transparent
        delay(150);
        tft.fillRect(200, yOffset +15, 18, 18, TFT_DARKCYAN);
        tft.setTextColor(TFT_RED);
        tft.setCursor(207, yOffset+20);
        tft.print("-");
    }
}

void draw_button_plus(uint16_t yOffset, uint8_t i, bool isTemp) {
    if (isTemp) {
        // 🔹 Buton "-" pentru rosu
        tft.fillRect(265, yOffset - 5, 18, 18, TFT_GREEN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(272, yOffset);
        tft.print("+");
        // 🔹 Afișează temperatura
        tft.fillRect(222, yOffset - 5, 40, 20, TFT_DARKCYAN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(230, yOffset);
        tft.print(default_temp_set[i], 1);
        // 🔹 Buton "-" inapoi transparent
        delay(200);
        tft.fillRect(265, yOffset - 5, 18, 18, TFT_DARKCYAN);
        tft.setTextColor(TFT_GREEN);
        tft.setCursor(272, yOffset);
        tft.print("+");
    } else {
        // 🔹 Buton "+" pentru rosu
        tft.fillRect(265, yOffset +15, 18, 18, TFT_GREEN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(272, yOffset+20);
        tft.print("+");
        // 🔹 Afișează umiditatea
        tft.fillRect(222, yOffset +20, 40, 20, TFT_DARKCYAN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(230, yOffset+20);
        tft.print(default_rh_set[i], 1);
        // 🔹 Buton "+" inapoi transparent
        delay(150);
        tft.fillRect(265, yOffset +15, 18, 18, TFT_DARKCYAN);
        tft.setTextColor(TFT_GREEN);
        tft.setCursor(272, yOffset+20);
        tft.print("+");
    }
}

void drawDreptunghi(uint8_t pozX, uint8_t pozY, uint8_t lungimeL, uint8_t latimel, uint8_t nrCamera) {
    int margine = 1;
    char camera[10];
    tft.fillRect(pozX - margine, pozY - margine, lungimeL + 2 * margine, latimel + 2 * margine, TFT_BROWN);
    tft.fillRect(pozX, pozY, lungimeL, latimel, TFT_DARKGREY);
    tft.setTextColor(TFT_ORANGE);
    tft.setTextSize(1);
    tft.setCursor((pozX + lungimeL) - (lungimeL / 2) - 22, pozY - 10);
    sprintf(camera, "Camera %d", nrCamera);
    tft.print(camera);

    tft.setCursor(pozX + 5, pozY + 5);
    tft.setTextColor(TFT_WHITE);
    tft.print("Temp-act:");
    tft.setCursor(pozX + 5, pozY + 15);
    tft.print("Humi-act:");
    tft.setTextColor(TFT_MAGENTA);
    tft.setCursor(pozX + 5, pozY + 30);
    tft.print("Temp-set:");
    tft.setCursor(pozX + 5, pozY + 40);
    tft.print("Humi-set:");

    // 🔹 Afișează temperatura setată
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(pozX + 70, pozY + 30);
    tft.print(default_temp_set[nrCamera - 1], 1); // Afisează temperatura setată cu o zecimală

    // 🔹 Afișează umiditatea setată
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(pozX + 70, pozY + 40);
    tft.print(default_rh_set[nrCamera - 1]); // Afisează umiditatea setată fără zecimale

    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(((pozX + 5 + 65) - 65 / 2) - 27, pozY + latimel - 29);
    tft.print("Incalzire");

    tft.setCursor(((pozX + 5 + 65) - 65 / 2) - 27 + 68, pozY + latimel - 29);
    tft.print("Umidificare");
    draw_buttons(pozX, pozY, lungimeL, latimel, nrCamera);
}

void draw_meniu_setari() {
    tft.setTextSize(2);
    tft.fillRect(0, 0, 320, 240, TFT_DARKCYAN);
    tft.setTextColor(TFT_RED);
    tft.setCursor(125, 2);
    tft.print("Setari");

    // 🔹 Buton "Salvare setari"
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(272, 3);
    tft.setTextSize(1);
    tft.print("Salvare");
    tft.setCursor(275, 13);
    tft.print("setari");

    // 🔹 Buton "Inapoi"
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(5, 5);
    tft.print("Inapoi");

    // 🔹 Buton de resetare la setari initiale
    tft.fillRect(240, 200, 70, 30, TFT_WHITE);
    tft.setTextColor(TFT_RED);
    tft.setCursor(250, 205);
    tft.print("Resetare");
    tft.setCursor(255, 215);
    tft.print("setari");


    // 🔹 Afișare temperaturi și umiditate pentru fiecare cameră + butoane de modificare
    for (int i = 0; i < 3; i++) {
        int yOffset = 35 + i * 45; // Ajustare poziție verticală

        // 🔹 Temperatură
        tft.setTextColor(TFT_ORANGE);
        tft.setCursor(5, yOffset);
        tft.print("Temperatura setata Camera ");
        tft.print(i + 1);
        tft.print(": ");

        // Buton "-"
        //tft.fillRect(200, yOffset - 5, 18, 18, TFT_RED);
        tft.setTextColor(TFT_RED);
        tft.setCursor(207, yOffset);
        tft.print("-");

        // Valoare
        //tft.fillRect(222, yOffset - 5, 40, 20, TFT_DARKCYAN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(230, yOffset);
        tft.print(default_temp_set[i], 1);

        // Buton "+"
        //tft.fillRect(265, yOffset - 5, 18, 18, TFT_GREEN);
        tft.setTextColor(TFT_GREEN);
        tft.setCursor(272, yOffset);
        tft.print("+");

        // 🔹 Umiditate
        yOffset += 20;
        tft.setTextColor(TFT_ORANGE);
        tft.setCursor(5, yOffset);
        tft.print("Umiditate setata Camera ");
        tft.print(i + 1);
        tft.print(": ");

        // Buton "-"
        tft.setTextColor(TFT_RED);
        tft.setCursor(207, yOffset);
        tft.print("-");

        // Valoare
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(230, yOffset);
        tft.print(default_rh_set[i]);

        // Buton "+"
        tft.setTextColor(TFT_GREEN);
        tft.setCursor(272, yOffset);
        tft.print("+");
    }
}

void print_data_and_time() {
    timeClient.update();
    time_t rawTime = timeClient.getEpochTime();
    struct tm *timeInfo = localtime(&rawTime);
    int currentMinute = timeInfo->tm_min;
    int currentHour = (timeInfo->tm_hour + 1) % 24;
    int currentDay = timeInfo->tm_mday;
    int currentMonth = timeInfo->tm_mon + 1;
    int currentYear = timeInfo->tm_year + 1900;
    char formattedTime[10], formattedDate[12];

    if (!IN_SETTINGS_MENU){
        if (currentMinute != lastMinute){
            lastMinute = currentMinute;
            sprintf(formattedTime, "%02d:%02d", currentHour, currentMinute);
            tft.fillRect(157, 10, 35, 10, TFT_BLACK);
            tft.setTextColor(TFT_WHITE);
            tft.setCursor(157, 10);
            tft.print(formattedTime);
        }
        if (currentDay != lastDay || currentMonth != lastMonth || currentYear != lastYear){
            lastDay = currentDay;
            lastMonth = currentMonth;
            lastYear = currentYear;
            sprintf(formattedDate, "%02d/%02d/%04d", currentDay, currentMonth, currentYear);
            tft.fillRect(147, 0, 60, 10, TFT_BLACK);
            tft.setTextColor(TFT_WHITE);
            tft.setCursor(147, 0);
            tft.print(formattedDate);
        }
    }
}

void reset_date_and_time() {
    lastMinute = -1;
    lastDay = -1;
    lastMonth = -1;
    lastYear = -1;
}

void print_temperature_and_humidity(uint8_t camera,float temp,uint8_t umiditate) {
    char tempStr[10], umidStr[10];
    sprintf(tempStr, "%.1f", temp);
    sprintf(umidStr, "%d", umiditate);
    if (camera == 1) {
        tft.fillRect(80, 5, 50, 10, TFT_BLACK);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(80, 5);
        tft.print(tempStr);
        tft.fillRect(80, 15, 50, 10, TFT_BLACK);
        tft.setCursor(80, 15);
        tft.print(umidStr);
    } else if (camera == 2) {
        tft.fillRect(240, 5, 50, 10, TFT_BLACK);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(240, 5);
        tft.print(tempStr);
        tft.fillRect(240, 15, 50, 10, TFT_BLACK);
        tft.setCursor(240, 15);
        tft.print(umidStr);
    } else if (camera == 3) {
        tft.fillRect(160, 105, 50, 10, TFT_BLACK);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(160, 105);
        tft.print(tempStr);
        tft.fillRect(160, 115, 50, 10, TFT_BLACK);
        tft.setCursor(160, 115);
        tft.print(umidStr);
    }
    else if (camera==0){
        tft.fillRect(80, 5, 50, 10, TFT_BLACK);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(80, 5);
        tft.print(tempStr);
        tft.fillRect(80, 15, 50, 10, TFT_BLACK);
        tft.setCursor(80, 15);
        tft.print(umidStr);
    }  
}

void drawUI() {
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1);

    tft.setTextColor(TFT_GREEN);
    tft.setCursor(0, 0);
    tft.print("Temp-ext:");

    tft.setCursor(0, 10);
    tft.print("Humi-ext:");

    tft.setTextColor(TFT_WHITE);
    tft.setCursor(117, 0);
    tft.print("Data: ");

    tft.setCursor(132, 10);
    tft.print("Ora: ");

    tft.setTextColor(TFT_RED);
    tft.setCursor(275, 7);
    tft.print("Setari");

    drawDreptunghi(5, 45, 150, 85, 1); 
    drawDreptunghi(165, 45, 150, 85, 2);
    drawDreptunghi(85, 145, 150, 85, 3);
}

void handleTouch(uint16_t x, uint16_t y) {
    //pentru meniul principal
    if (!IN_SETTINGS_MENU){
        // pentru camera 1
        if (x >= 10 && x <= 75 && y >= 110 && y <= 130){
            buttonStates_Incalzire[0] = !buttonStates_Incalzire[0];
            draw_button_incalzire(5, 45, 150, 85, 1);
        }
        else if (x >= 84 && x <= 149 && y >= 110 && y <= 130){
            buttonStates_Umidificare[0] = !buttonStates_Umidificare[0];
            draw_button_umidificare(5, 45, 150, 85, 1);
        }
        // pentru camera 2
        else if (x >= 170 && x <= 235 && y >= 110 && y <= 130){
            buttonStates_Incalzire[1] = !buttonStates_Incalzire[1];
            draw_button_incalzire(165, 45, 150, 85, 2);
        }
        else if (x >= 244 && x <= 309 && y >= 110 && y <= 130){
            buttonStates_Umidificare[1] = !buttonStates_Umidificare[1];
            draw_button_umidificare(165, 45, 150, 85, 2);
        }
        // pentru camera 3
        else if (x >= 90 && x <= 155 && y >= 210 && y <= 230){
            buttonStates_Incalzire[2] = !buttonStates_Incalzire[2];
            draw_button_incalzire(85, 145, 150, 85, 3);
        }
        else if (x >= 164 && x <= 229 && y >= 210 && y <= 230){
            buttonStates_Umidificare[2] = !buttonStates_Umidificare[2];
            draw_button_umidificare(85, 145, 150, 85, 3);
        }
        else if (x >= 270 && x <= 320 && y >= 0 && y <= 20){
            IN_SETTINGS_MENU = true;
            draw_meniu_setari();
        }
    }else{ //meniul de setări
        if (x >= 5 && x <= 25 && y >= 5 && y <= 20) {
            IN_SETTINGS_MENU = false;
            reset_date_and_time();
            drawUI();
            print_data_and_time();
        }
        else if (x >= 272 && x <= 320 && y >= 0 && y <= 13){
            //Serial.println("Salvare setari...");
            saveSettingsToEEPROM();
            tft.setCursor(80, 165);
            tft.setTextSize(1);
            tft.setTextColor(TFT_GREEN);
            tft.print("! Setarile au fost salvate !");
            delay(1500);
            tft.fillRect(80, 165, 165, 15, TFT_DARKCYAN);
        }
        // 🔹 Buton "Resetare setari" (dreapta jos)
        else if (x >= 240 && x <= 310 && y >= 200 && y <= 230)
        {
            Serial.println("Ține apăsat pentru reset...");

            unsigned long startTime = millis();

            // 🔹 Așteptăm 5 secunde pentru apăsare lungă
            while (millis() - startTime < 5000)
            {
                uint16_t xTemp, yTemp;
                if (!tft.getTouch(&xTemp, &yTemp))
                {
                    Serial.println("Reset anulat.");
                    tft.fillRect(240, 200, 70, 30, TFT_WHITE);
                    tft.setTextColor(TFT_RED);
                    tft.setCursor(250, 205);
                    tft.print("Resetare");
                    tft.setCursor(255, 215);
                    tft.print("setari");
                    return; // Ieșim dacă nu mai este apăsat
                }else{
                    tft.fillRect(240, 200, 70, 30, TFT_RED);
                    tft.setTextColor(TFT_WHITE);
                    tft.setCursor(250, 205);
                    tft.print("Resetare");
                    tft.setCursor(257, 215);
                    tft.print("setari");
                }
            }
            // 🔹 Resetăm toate setările și repornim ESP
            reset_to_default_settings();
        }

        for (int i = 0; i < 3; i++) {
            int yTemp = 35 + i * 45;
            int yRh = yTemp + 20;

            // Temperatura –
            if (x >= 207 && x <= 220 && y >= yTemp && y <= yTemp + 12) {
                default_temp_set[i] -= 0.5;
                if (default_temp_set[i] > TEMP_MIN && default_temp_set[i] < TEMP_MAX) {
                    draw_button_minus(yTemp, i,true);
                }
                else {
                    default_temp_set[i] = TEMP_MIN; // Asigură-te că nu scade sub minim
                    draw_button_minus(yTemp, i,true);
                }
            }

            // Temperatura +
            if (x >= 272 && x <= 285 && y >= yTemp && y <= yTemp + 12) {
                default_temp_set[i] += 0.5;
                if (default_temp_set[i] > TEMP_MIN && default_temp_set[i] < TEMP_MAX) {
                    draw_button_plus(yTemp, i,true);
                }
                else {
                    default_temp_set[i] = TEMP_MAX; // Asigură-te că nu depășește maximul
                    draw_button_plus(yTemp, i,true);
                }
            }

            // Umiditate –
            if (x >= 207 && x <= 220 && y >= yRh && y <= yRh + 12) {
                default_rh_set[i] -= 1;
                if (default_rh_set[i] >= RH_MIN && default_rh_set[i] <= RH_MAX) {
                    draw_button_minus(yTemp, i,false);
                }
                else {
                    default_rh_set[i] = RH_MIN; // Asigură-te că nu scade sub minim
                    draw_button_minus(yTemp, i,false);
                }
            }

            // Umiditate +
            if (x >= 272 && x <= 285 && y >= yRh && y <= yRh + 12) {
                default_rh_set[i] += 1;
                if (default_rh_set[i] >= RH_MIN && default_rh_set[i] <= RH_MAX) {
                    draw_button_plus(yTemp, i,false);
                }
                else {
                    default_rh_set[i] = RH_MAX; // Asigură-te că nu depășește maximul
                    draw_button_plus(yTemp, i,false);
                }
            }
    }
    }
}

void checkTouch() {
    uint16_t x, y;
    if (tft.getTouch(&x, &y)) {
        unsigned long currentTime = millis();
        if (currentTime - lastTouchTime > debounceInterval) {
            handleTouch(x, y);
            lastTouchTime = currentTime;
        }
    }
}

void touch_calibrate() {
    uint16_t calData[5];
    uint8_t calDataOK = 0;

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    if (SPIFFS.exists(CALIBRATION_FILE)) {
        if (REPEAT_CAL) {
            SPIFFS.remove(CALIBRATION_FILE);
        } else {
            fs::File f = SPIFFS.open(CALIBRATION_FILE, "r");
            if (f) {
                if (f.readBytes((char *)calData, 14) == 14) {
                    tft.setTouch(calData);
                    calDataOK = 1;
                    Serial.println("Touch calibration loaded.");
                }
                f.close();
            }
        }
    }

    if (!calDataOK || REPEAT_CAL) {
        tft.setRotation(3); // 🔹 Setează orientarea pe orizontală
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(20, 10);
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.println("Touch corners as indicated");

        tft.setTextFont(1);
        tft.println();

        // 🔹 Calibrare touchscreen în modul orizontal
        tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
        tft.setTouch(calData);

        fs::File f = SPIFFS.open(CALIBRATION_FILE, "w");
        if (f) {
            f.write((const unsigned char *)calData, 14);
            f.close();
            Serial.println("Calibration saved.");
        }
    }
}

void setup() {
    Serial.begin(115200);
    // 🔹 1️⃣ Conectare la Wi-Fi pentru update de timp
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    Serial.println("\nWi-Fi conectat!");

    timeClient.begin(); // 🔹 Pornim sincronizarea NTP
    tft.init();
    touch_calibrate();
    loadSettingsFromEEPROM();
    drawUI();
    print_data_and_time();
}

void loop() {
    checkTouch();
    print_data_and_time();
    delay(10);
}