// 追加のボードマネージャ(https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json)
#define AnalogPin 27
#define TEST_DEBUG // テスト用
int batteryVoltage = 0, batteryLevel = 0;
void setup() {
    pinMode(AnalogPin, INPUT);
}
void setup1() {
    Serial.begin(115200);
}

void loop() {
    if(Serial.available() > 0) {
        char command = Serial.read();
        if(command == 's') {
            Serial.printf("b:%d::\n", batteryLevel);
        }
    }
    delay(1);
}

void loop1() {
    #ifndef TEST_DEBUG
    batteryVoltage = analogRead(AnalogPin);
    batteryLevel = map(batteryVoltage, 0, 4095, 0, 3300);
    #endif
    #ifdef TEST_DEBUG
    batteryLevel = random(0, 4095);
    #endif
}
