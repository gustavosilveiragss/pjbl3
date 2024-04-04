#include "esp32-hal-gpio.h"
#include "hal/gpio_types.h"
#include <Arduino.h>
#include <array>

constexpr std::array<int, 3> password_order{ 1, 0, 2 };
constexpr std::array<gpio_num_t, 3> buttons{ GPIO_NUM_34, GPIO_NUM_35, GPIO_NUM_32 };

void setup() {
    Serial.begin(115200);
    for (auto i : buttons)
        pinMode(i, INPUT_PULLUP);

    pinMode(GPIO_NUM_14, OUTPUT);
}

void loop() {
    tone(GPIO_NUM_14, 1000);

    static auto tick = 0;

    if (millis() - tick >= 1000) {
        for (auto b : buttons)
            Serial.println(digitalRead(b));

        tick = millis();
    }
}