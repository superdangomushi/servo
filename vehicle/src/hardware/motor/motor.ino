#include "vehicle.h"
#define RAD 28.5
#define WID 100.0
#define USEC 1000000.0
#define ACCLIMIT 10.0
int state[2] = {0, 0};
const int pinA[4] = {9, 8, 10, 11};
const int pinB[4] = {5, 4, 2, 3};
float v[5] = {0, 0, 0, 0, 0};
char words[64];
int i = 0, mode = 0;
float v0R = 0.0, v0L = 0.0;

enum mode {
    circle,
    straight,
    stop
};

int list[8][4] = {
    {1, 0, 1, 0},
    {1, 0, 0, 0},
    {1, 0, 0, 1},
    {0, 0, 0, 1},
    {0, 1, 0, 1},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0}};

void setup() {
    for (int i = 2; i < 13; i++) {
        pinMode(i, OUTPUT);
    }
    digitalWrite(12, HIGH);
    digitalWrite(6, HIGH);
    Serial.begin(115200);
}

void loop() {
    enum mode mode;
    while (Serial.available()) {
        char c = Serial.read();
        if (c == ';') {
            words[i] = 0;
            i = 0;
            char *p = words;
            for (int j = 0; j < 5; j++) {
                if (mode != circle) {
                    v[j] = atof(p) / RAD;
                } else if (mode == circle && (j == 0 || j == 1)) {
                    v[j] = atof(p) / RAD;
                } else {
                    v[j] = atof(p);
                }
                p = strchr(p, ',');
                if (p) p++;
            }
            Serial.flush();
        } else if (c == 's')
            mode = stop;
        else if (c == 'r')
            mode = circle;
        else if (c == 'l')
            mode = straight;
        else if (i < sizeof(words) - 1)
            words[i++] = c;
    }

    switch (mode) {
        case stop:
            v[0] = 0;
            v[1] = 0;
            v[2] = 0;
            v[3] = 0;
            v0R = 0.0;
            v0L = 0.0;
            break;
        case straight:
            straight(v[0], v[1], v[2]);
            v[0] = 0;
            v[1] = 0;
            v[2] = 0;
            v[3] = 0;
            break;
        case circle:
            circle(v[0], v[1], v[2], v[3], v[4]);
            break;
        default:
            accel(v[0], v[1], v[2], v[3], state);
    }
}

void straight(float V, float L, float acc) {
    long time = USEC * ((L / fabs(V)) - (fabs(V) / acc));
    Serial.println(acc);
    accel(acc, acc, V, V, state);
    move(V, V, time, state);
    accel(acc, acc, 0, 0, state);
}

void circle(float acc, float V, float R, float theta, float RL) {
    theta = theta * 3.14 / 180.0;
    float vmin, vmax, dVmin, dVmax, T_theta;
    if (R == 0) {
        vmin = -V, vmax = V, dVmin = ACCLIMIT, dVmax = ACCLIMIT;
        T_theta = USEC * (((WID * theta) / fabs(V * RAD)) - (fabs(V * RAD) / (acc * RAD)));
    } else {
        vmin = V * ((R - WID) / R);
        vmax = V * ((R + WID) / R);
        dVmin = dV * ((R - WID) / R);
        dVmax = dV * ((R + WID) / R);
        T_theta = USEC * ((R * theta / fabs(V * RAD)) - (fabs(V * RAD) / (acc * RAD)));
    }
    if (RL == 0) {
        accel(dVmin, dVmax, vmin, vmax, state);
        move(vmin, vmax, T_theta, state);
        accel(dVmin, dVmax, 0, 0, state);
    } else {
        accel(dVmax, dVmin, vmax, vmin, state);
        move(vmax, vmin, T_theta, state);
        accel(dVmax, dVmin, 0, 0, state);
    }
    v[2] = 0;
    v[3] = 0;
}
