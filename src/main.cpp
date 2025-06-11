#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <FS.h>
#include <SPIFFS.h>

TFT_eSPI tft = TFT_eSPI();

#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false  // Pune pe true dacă vrei să recalculezi touchscreen-ul

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

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 60000);

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

void handleTouch(int x, int y) {
    //pentru camera 1
    if (x >= 10 && x <= 75 && y >= 110 && y <= 130) {
        buttonStates_Incalzire[0] = !buttonStates_Incalzire[0];
        draw_button_incalzire(5, 45, 150, 85, 1);
    } else if (x >= 84 && x <= 149 && y >= 110 && y <= 130) {
        buttonStates_Umidificare[0] = !buttonStates_Umidificare[0];
        draw_button_umidificare(5, 45, 150, 85, 1);
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

void print_data_and_time() {
    timeClient.update();
    time_t rawTime = timeClient.getEpochTime();
    struct tm *timeInfo = localtime(&rawTime);
    int currentMinute = timeInfo->tm_min;
    int currentHour = (timeInfo->tm_hour + 1) % 24;
    int currentDay = timeInfo->tm_mday + 1;
    int currentMonth = timeInfo->tm_mon + 1;
    int currentYear = timeInfo->tm_year + 1900;
    char formattedTime[10], formattedDate[12];

    if (currentMinute != lastMinute) {
        lastMinute = currentMinute;
        sprintf(formattedTime, "%02d:%02d", currentHour, currentMinute);
        tft.fillRect(157, 10, 35, 10, TFT_BLACK);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(157, 10);
        tft.print(formattedTime);
    }

    if (currentDay != lastDay || currentMonth != lastMonth || currentYear != lastYear) {
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