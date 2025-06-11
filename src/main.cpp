#include<Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI(); // Instanțierea obiectului TFT_eSPI

#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false

int lastMinute = -1;
int lastDay = -1;
int lastMonth = -1;
int lastYear = -1;

const char *ssid = "FBI5G";
const char *password = "3.14159265";
bool buttonStates_Incalzire[] = {false, false, false}; // Variabilă pentru a verifica dacă butonul a fost apăsat
bool buttonStates_Umidificare[] = {false, false, false}; // Variabilă pentru a verifica dacă butonul a fost apăsat

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 60000); // GMT+2 pentru România

void drawDreptunghi(int pozX, int pozY, int lungimeL, int latimel, int nrCamera){
    int margine = 1;
    char camera[10];
    // Desenează chenarul alb
    tft.fillRect(pozX - margine, pozY - margine, lungimeL + 2 * margine, latimel + 2 * margine, TFT_BROWN);
    // Desenează dreptunghiul albastru
    tft.fillRect(pozX, pozY, lungimeL, latimel, TFT_DARKGREY);
    // Afișează textul "Camera X" în interiorul dreptunghiului
    tft.setTextColor(TFT_ORANGE);
    tft.setTextSize(1);
    tft.setCursor((pozX + lungimeL) - (lungimeL / 2) - 22, pozY - 10); // Poziționare corectă a textului
    sprintf(camera, "Camera %d", nrCamera);
    tft.print(camera);
    tft.setCursor(pozX + 5, pozY + 5); // Poziționare text în interiorul dreptunghiului
    tft.setTextColor(TFT_WHITE);
    tft.print("Temp-act:");
    tft.setCursor(pozX + 5, pozY + 15); // Poziționare text în interiorul dreptunghiului
    tft.print("Rh-act:");
    tft.setTextColor(TFT_MAGENTA);
    tft.setCursor(pozX + 5, pozY + 30); // Poziționare text în interiorul dreptunghiului
    tft.print("Temp-set:");
    tft.setCursor(pozX + 5, pozY + 40); // Poziționare text în interiorul dreptunghiului
    tft.print("Rh-set:");

    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(((pozX+5+65)-65/2)-27,pozY+latimel-29);
    tft.print("Incalzire");

    tft.setTextColor(TFT_YELLOW);
    tft.setTextSize(1);
    tft.setCursor(((pozX+5+65)-65/2)-27+68,pozY+latimel-29);
    tft.print("Umidificare");
}

void draw_buttons(int pozX, int pozY, int lungimeL, int latimel, int nrCamera){
    if (buttonStates_Incalzire[nrCamera - 1]){
        tft.fillRect(pozX+5,pozY+latimel-20, 65, 15, TFT_GREEN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(((pozX+5+65)-65/2)-10,pozY+latimel-16);
        tft.print("ON");
    }
    else{
        tft.fillRect(pozX+5,pozY+latimel-20, 65, 15, TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(((pozX+5+65)-65/2)-10,pozY+latimel-16);
        tft.print("OFF");
    }

    if (buttonStates_Umidificare[nrCamera - 1]){
        tft.fillRect(pozX+79,pozY+latimel-20, 65, 15, TFT_GREEN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(((pozX+5+65)-65/2)-10+75,pozY+latimel-16);
        tft.print("ON");
    }
    else{
        tft.fillRect(pozX+79,pozY+latimel-20, 65, 15, TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(((pozX+5+65)-65/2)-10+75,pozY+latimel-16);
        tft.print("OFF");
    }
}

void draw_button_incalzire(int pozX, int pozY, int lungimeL, int latimel, int nrCamera){
    if (buttonStates_Incalzire[nrCamera - 1]){
        tft.fillRect(pozX+5,pozY+latimel-20, 65, 15, TFT_GREEN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(((pozX+5+65)-65/2)-10,pozY+latimel-16);
        tft.print("ON");
    }
    else{
        tft.fillRect(pozX+5,pozY+latimel-20, 65, 15, TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(((pozX+5+65)-65/2)-10,pozY+latimel-16);
        tft.print("OFF");
    }
}

void draw_button_umidificare(int pozX, int pozY, int lungimeL, int latimel, int nrCamera){
    if (buttonStates_Umidificare[nrCamera - 1]){
        tft.fillRect(pozX+79,pozY+latimel-20, 65, 15, TFT_GREEN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(((pozX+5+65)-65/2)-10+75,pozY+latimel-16);
        tft.print("ON");
    }
    else{
        tft.fillRect(pozX+79,pozY+latimel-20, 65, 15, TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(((pozX+5+65)-65/2)-10+75,pozY+latimel-16);
        tft.print("OFF");
    }
}

void handleTouch(int x, int y){
    // Verifică dacă coordonatele ating butonul de încălzire la camera 1
    if (x >= 10 && x <= 75 && y >= 45 && y <= 55) {
        buttonStates_Incalzire[0] = !buttonStates_Incalzire[0]; // Comută starea butonului
        draw_button_incalzire(5, 45, 150, 85,1);               // Re-desenare dreptunghi cu noua stare
    }else if (x>=84 && x<=110 && y>=45 && y<=55) {
        buttonStates_Umidificare[0] = !buttonStates_Umidificare[0]; // Comută starea butonului
        draw_button_umidificare(5, 45, 150, 85,1);               // Re-desenare dreptunghi cu noua stare
    }
}

void print_data_and_time(){
    timeClient.update();
    time_t rawTime = timeClient.getEpochTime();
    struct tm *timeInfo = localtime(&rawTime);
    int currentMinute = timeInfo->tm_min;
    int currentHour = (timeInfo->tm_hour + 1) % 24; // Corectare fus orar
    int currentDay = timeInfo->tm_mday+1;
    int currentMonth = timeInfo->tm_mon + 1;
    int currentYear = timeInfo->tm_year + 1900;
    char formattedTime[10], formattedDate[12];

    if (currentMinute != lastMinute) {
        lastMinute = currentMinute;
        sprintf(formattedTime, "%02d:%02d", currentHour, currentMinute);
        tft.fillRect(157, 10, 35, 10, TFT_BLACK); // Șterge doar zona orei
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(157, 10);
        tft.print(formattedTime);
    }
    // 🔹 Actualizează data doar dacă ziua/luna/anul s-a schimbat
    if (currentDay != lastDay || currentMonth != lastMonth || currentYear != lastYear) {
        lastDay = currentDay;
        lastMonth = currentMonth;
        lastYear = currentYear;
        sprintf(formattedDate, "%02d/%02d/%04d", currentDay, currentMonth, currentYear);
        tft.fillRect(147, 0, 60, 10, TFT_BLACK); // Șterge doar zona datei
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(147, 0);
        tft.print(formattedDate);
    }
}

void drawUI(){
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
    draw_buttons(5, 45, 150, 85, 1);
    drawDreptunghi(165, 45, 150, 85, 2);
    draw_buttons(165, 45, 150, 85, 2);
    drawDreptunghi(85, 145, 150, 85, 3);
    draw_buttons(85, 145, 150, 85, 3);

}

void touch_calibrate(){
    uint16_t calData[5];
    uint8_t calDataOK = 0;

    // check file system exists
    if (!SPIFFS.begin())
    {
        Serial.println("formatting file system");
        SPIFFS.format();
        SPIFFS.begin();
    }

    // check if calibration file exists and size is correct
    if (SPIFFS.exists(CALIBRATION_FILE))
    {
        if (REPEAT_CAL)
        {
            // Delete if we want to re-calibrate
            SPIFFS.remove(CALIBRATION_FILE);
        }
    }

    if (calDataOK && !REPEAT_CAL)
    {
        // calibration data valid
        tft.setTouch(calData);
    }
    else
    {
        // data not valid so recalibrate
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(20, 0);
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);

        tft.println("Touch corners as indicated");

        tft.setTextFont(1);
        tft.println();

        if (REPEAT_CAL)
        {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.println("Set REPEAT_CAL to false to stop this running again!");
        }

        tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.println("Calibration complete!");
    }
}

void setup(){
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print("Error conectare WiFi, încerc din nou...");
    }
    timeClient.begin();
    tft.init();
    drawUI(); // desenare UI
}

void loop(){
    uint16_t x, y;
    if (tft.getTouch(&x, &y)) {
        Serial.print("Touch detected at: ");
        Serial.print(x);
        Serial.print(", ");
        Serial.println(y);
        handleTouch(x, y); // gestionare atingere
    }
    print_data_and_time();
    delay(10);
}