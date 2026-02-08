#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "wifi";
const char* password = "password";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 32400);

int pinGND[4] = {16, 19, 28, 21};
int pinH[7] = {10, 14, 13, 11, 9, 12, 15};
int pinM[7] = {12, 15, 11, 9, 13, 10, 14};
int IOpin[12][7] = {
    {1, 1, 1, 1, 1, 1, 0}, // 0
    {0, 1, 1, 0, 0, 0, 0}, // 1
    {1, 1, 0, 1, 1, 0, 1}, // 2
    {1, 1, 1, 1, 0, 0, 1}, // 3
    {0, 1, 1, 0, 0, 1, 1}, // 4
    {1, 0, 1, 1, 0, 1, 1}, // 5
    {1, 0, 1, 1, 1, 1, 1}, // 6
    {1, 1, 1, 0, 0, 1, 0}, // 7
    {1, 1, 1, 1, 1, 1, 1}, // 8
    {1, 1, 1, 1, 0, 1, 1}, // 9
    {0, 0, 0, 0, 0, 0, 1},
    {0, 0, 0, 0, 0, 0, 0}};
int currentTime[4] = {10, 10, 10, 10};
int mode = 0;
int lgt = 0;

int f = 1;
void setup() {
    for (int pin = 9; pin <= 25; pin++) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    pinMode(1, INPUT_PULLUP);
    pinMode(0, INPUT_PULLUP);
    pinMode(28, OUTPUT);
    pinMode(22, OUTPUT);
}

void loop() {
    for (int pin = 0; pin < 4; pin++) {
        digitalWrite(pinGND[pin], LOW);
    }

    for (int i = 0; i < 4; i++) {
        for (int pin = 0; pin < 4; pin++) {
            digitalWrite(pinGND[pin], LOW);
        }
        for (int pin = 0; pin < 7; pin++) {
            digitalWrite(pinM[pin], LOW);
        }
        if (mode != 0 && mode <= 25) delayMicroseconds(5000 - (mode * 200));
        if (mode == 0) delayMicroseconds(lgt * 5);
        digitalWrite(pinGND[i], HIGH);
        for (int j = 0; j < 7; j++) {
            if (i == 1 || i == 0) digitalWrite(pinM[j], IOpin[currentTime[i]][j]);
            if (i == 2 || i == 3) digitalWrite(pinH[j], IOpin[currentTime[i]][j]);
        }
        if (mode != 0) delayMicroseconds(mode * 200);
        if (mode == 0) delayMicroseconds(abs(5000 - (lgt * 5)));
    }
}

void setup1() {
    Serial.begin(115200);
    // --- Wi-Fi接続 ---
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        i++;
        if (i > 10) {
            bz(1000, 300, 22);
        }
    }
    Serial.println("\nWiFi connected");
    pinMode(27, INPUT);

    timeClient.begin();
    timeClient.setUpdateInterval(5 * 60 * 1000);
}
void loop1() {
    long time = millis();
    long dely = 0;
    while (true) {
        lgt = analogRead(27);
        if (digitalRead(1) == LOW) {
            mode++;
            if (mode > 25) mode = 0;
            currentTime[3] = 11;
            currentTime[2] = 11;
            if ((mode / 10) == 0) currentTime[1] = 11;
            currentTime[1] = mode / 10;
            currentTime[0] = mode % 10;
            delay(500);
        }
        if (digitalRead(0) == LOW && mode == 0) {
            mode += 10;
            delay(4000);
            mode -= 10;
        }
        dely = millis() - time;
        if (dely > 999) break;
    }
    Serial.printf("%d\n", lgt);
    timeClient.update();
    int currentHour = timeClient.getHours();
    int currentMinute = timeClient.getMinutes();
    currentTime[3] = currentHour / 10;
    currentTime[2] = currentHour % 10;
    currentTime[1] = currentMinute / 10;
    currentTime[0] = currentMinute % 10;
    if (currentMinute == 0 && f == 1) {
        if (mode != 0) bz(500, 500, 22); // 音鳴らしてみる(将来的にはアラーム機能つけたい)
        f = 0;
    } else if (currentMinute != 0) {
        f = 1;
    }
}

void bz(int hz, int t, int pin) {
    int t0 = millis(), dt = 0;
    while (dt < t) {
        digitalWrite(pin, HIGH);
        delayMicroseconds(500000 / hz);
        digitalWrite(pin, LOW);
        delayMicroseconds(500000 / hz);
        dt = millis() - t0;
    }
}