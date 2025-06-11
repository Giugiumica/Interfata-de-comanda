#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI(); // Instanțierea obiectului TFT_eSPI

#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false

const char *ssid = "FBI5G";
const char *password = "3.14159265";
bool buttonStates[] = {false, false, false}; // Variabilă pentru a verifica dacă butonul a fost apăsat

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 60000); // GMT+2 pentru România

void drawDreptunghi(int pozX, int pozY, int lungimeL, int latimel, int nrCamera)
{
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

    /* int buttonX = pozX + 30;
    int buttonY = pozY + latimel - 20;
    int buttonW = 70;
    int buttonH = 15;
    if (buttonStates[nrCamera - 1])
    {
        tft.fillRect(pozX+60, buttonY, buttonW, buttonH, TFT_GREEN);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(buttonX + 35, buttonY + 3);
        tft.print("ON");
    }
    else
    {
        tft.fillRect(pozX+60, buttonY, buttonW, buttonH, TFT_RED);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(buttonX + 35, buttonY + 3);
        tft.print("OFF");
    } */
}

void print_data_and_time(){
    timeClient.update();
    time_t rawTime = timeClient.getEpochTime();
    struct tm *timeInfo = localtime(&rawTime);
    char formattedTime[10], formattedDate[12];
    sprintf(formattedTime, "%02d:%02d", timeInfo->tm_hour+1, timeInfo->tm_min);
    sprintf(formattedDate, "%02d/%02d/%04d", timeInfo->tm_mday, timeInfo->tm_mon + 1, timeInfo->tm_year + 1900);
    tft.fillRect(147, 0, 60, 10, TFT_BLACK); // Curățare zona pentru data
    tft.fillRect(156, 10, 35, 10, TFT_BLACK); // Curățare zona pentru ora
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(147, 0);
    tft.print(formattedDate);
    tft.setCursor(157, 10);
    tft.print(formattedTime);
}

void drawUI()
{
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

void touch_calibrate()
{
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

void setup()
{
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print("Error conectare WiFi, încerc din nou...");
    }
    timeClient.begin();
    tft.init();
    tft.setRotation(0);
    //touch_calibrate();

    drawUI(); // desenare UI
}

void loop()
{
    /*
    uint16_t t_x = 0, t_y = 0;
    bool pressed = tft.getTouch(&t_x, &t_y);
    if (pressed){
    }
    */
    print_data_and_time();
    delay(1000);
}