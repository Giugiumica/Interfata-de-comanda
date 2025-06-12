#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <FS.h>
#include <SPIFFS.h>
#include <EEPROM.h>


TFT_eSPI tft = TFT_eSPI();

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
const unsigned long debounceInterval = 300; //ms

const char *ssid = "FBI5G";
const char *password = "3.14159265";

bool buttonStates_Incalzire[] = {false, false, false};
bool buttonStates_Umidificare[] = {false, false, false};

float default_temp_set[] = {20.0, 20.0, 20.0}; // Temperatura setată pentru fiecare cameră
int default_rh_set[] = {50, 50, 50}; // Umiditatea setată pentru fiecare cameră



WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 60000);

void saveSettingsToEEPROM() {
    EEPROM.begin(12); // Alocăm spațiu (3 x temperaturi + 3 x umiditate)
    for (int i = 0; i < 3; i++) {
        EEPROM.put(i * 4, default_temp_set[i]); // Salvează temperaturile
        EEPROM.put(12 + i * 4, default_rh_set[i]); // Salvează umiditatea
    }
    EEPROM.commit(); // Confirma modificările
    Serial.println("Setările au fost salvate în EEPROM.");
}


void draw_button_incalzire(int pozX, int pozY, int lungimeL, int latimel, int nrCamera) {
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

void draw_button_umidificare(int pozX, int pozY, int lungimeL, int latimel, int nrCamera) {
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

void draw_buttons(int pozX, int pozY, int lungimeL, int latimel, int nrCamera) {
    draw_button_incalzire(pozX, pozY, lungimeL, latimel, nrCamera);
    draw_button_umidificare(pozX, pozY, lungimeL, latimel, nrCamera);
}

void draw_button_minus(int yOffset, int i, bool isTemp) {
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

void draw_button_plus(int yOffset, int i, bool isTemp) {
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

void drawDreptunghi(int pozX, int pozY, int lungimeL, int latimel, int nrCamera) {
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
    tft.print("Rh-act:");
    tft.setTextColor(TFT_MAGENTA);
    tft.setCursor(pozX + 5, pozY + 30);
    tft.print("Temp-set:");
    tft.setCursor(pozX + 5, pozY + 40);
    tft.print("Rh-set:");

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

void drawUI() {
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1);

    tft.setTextColor(TFT_GREEN);
    tft.setCursor(0, 0);
    tft.print("Rh-ext:");

    tft.setCursor(0, 10);
    tft.print("Temp-ext:");

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

void handleTouch(int x, int y) {
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
            handleTouch(x, y); // Apelează funcția ta logică
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
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi...");
    }

    timeClient.begin();
    tft.init();
    touch_calibrate();  // FOARTE IMPORTANT
    drawUI();
}

void loop() {
    print_data_and_time();
    checkTouch();
    delay(10);
}