#include <Arduino.h>
#include <array>
#include <WiFi.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <cstring>
#include "Button.hpp"

constexpr auto BUZZER_PIN = GPIO_NUM_26;
constexpr auto IR_PIN = GPIO_NUM_35;
constexpr auto DHT_PIN = GPIO_NUM_14;

void connect_to_wifi(const char* ssid, const char* password) {
    Serial.print("Connecting to WiFi");

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.print("\n");
}

// configurações de conexão MQTT
WiFiClient g_wifi_client;
PubSubClient g_mqtt_client(g_wifi_client);

std::array g_password{ 0, 0, 0 };
int g_buzzer_frequency = 8000;

DHT dht(DHT_PIN, DHT22);

void setup() {
    Serial.begin(115200);

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(IR_PIN, INPUT_PULLUP);

    connect_to_wifi("iPhone de Gustavo", "senhamalucawow");
    g_mqtt_client.setServer("broker.emqx.io", 1883);

    nvs_flash_init();

    nvs_handle_t nvs_handle;
    nvs_open("storage", NVS_READONLY, &nvs_handle);

    size_t size = sizeof(g_password);
    auto err = nvs_get_blob(nvs_handle, "password", &g_password, &size);
    if (err != ESP_OK)
        Serial.printf("Error %d reading password from storage - using default\n", err);

    Serial.printf("Fetched Password: %d %d %d\n", g_password[0], g_password[1], g_password[2]);

    err = nvs_get_i32(nvs_handle, "frequency", &g_buzzer_frequency);
    if (err != ESP_OK)
        Serial.printf("Error %d reading frequency from storage - using default\n", err);

    Serial.printf("Fetched Buzzer Frequency: %d\n", g_buzzer_frequency);

    nvs_close(nvs_handle);

    dht.begin();
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    std::string_view topic_sv{ topic };

    Serial.printf("Message arrived [%s]: \"%.*s\"\n", topic, length, payload);

    if (std::strstr(reinterpret_cast<const char*>(payload), "status") != nullptr)
        return;

    nvs_handle_t nvs_handle;
    nvs_open("storage", NVS_READWRITE, &nvs_handle);

    if (topic_sv == "PASSWORD/RES/2") {
        for (size_t i = 0; i < g_password.size(); ++i)
            g_password[i] = payload[i] - '0';

        Serial.printf("New password: %d %d %d\n", g_password[0], g_password[1], g_password[2]);

        nvs_set_blob(nvs_handle, "password", g_password.data(), sizeof(g_password));
    } else if (topic_sv == "FREQUENCY/RES/2") {
        std::sscanf(reinterpret_cast<const char*>(payload), "%d", &g_buzzer_frequency);
        nvs_set_i32(nvs_handle, "frequency", g_buzzer_frequency);
        Serial.printf("New buzzer frequency: %d\n", g_buzzer_frequency);
    }

    nvs_close(nvs_handle);
}

void connect_to_mqtt_broker() {
    Serial.println("Connecting to the broker...");

    while (!g_mqtt_client.connected()) {
        if (g_mqtt_client.connect("Minimus")) {
            Serial.println("Connected!");
            g_mqtt_client.subscribe("PASSWORD/RES/#");
            g_mqtt_client.subscribe("FREQUENCY/RES/#");
            g_mqtt_client.setCallback(mqtt_callback);
        } else {
            Serial.println("Failed to connect - trying again in 5 seconds...");
            delay(5000);
        }
    }
}

void loop() {
    std::array buttons{ Button{ GPIO_NUM_25 }, Button{ GPIO_NUM_33 }, Button{ GPIO_NUM_32 } };

    std::vector<size_t> password_attempt;
    bool got_correct_password = false;

    constexpr auto LID_UPDATE_DEBOUNCE_TIME = 300;
    uint32_t lid_update_debounce = millis();
    bool is_lid_open = digitalRead(IR_PIN);

    constexpr auto DHT_READ_INTERVAL = 5000; // 5 seconds
    uint32_t last_dht_read = millis();

    while (true) {
        if (not g_mqtt_client.connected()) {
            connect_to_mqtt_broker();
            g_mqtt_client.publish("PERMISSION_STATE/REQ/2/W", "0");
            g_mqtt_client.publish("IR_STATE/REQ/2/W", is_lid_open ? "1" : "0");
        }

        if (millis() - last_dht_read > DHT_READ_INTERVAL) {
            last_dht_read = millis();

            const auto temp = std::to_string(dht.readTemperature());
            Serial.printf("Temperature: %s°C\n", temp.c_str());

            const auto hum = std::to_string(dht.readHumidity());
            Serial.printf("Humidity: %s%%\n", hum.c_str());

            g_mqtt_client.publish("TEMPERATURE/REQ/2/W", temp.c_str());
            g_mqtt_client.publish("HUMIDITY/REQ/2/W", hum.c_str());
        }

        if (not got_correct_password) {
            for (size_t i = 0; i < buttons.size(); ++i) {
                if (buttons[i].is_clicked()) {
                    Serial.printf("Button %d clicked\n", i);
                    password_attempt.push_back(i);

                    if (password_attempt.size() == g_password.size()) {
                        got_correct_password = std::equal(password_attempt.begin(), password_attempt.end(), g_password.begin());
                        Serial.println(got_correct_password ? "Correct password!" : "Wrong password!");
                        password_attempt.clear();

                        g_mqtt_client.publish("PERMISSION_STATE/REQ/2/W", got_correct_password ? "1" : "0");

                        if (got_correct_password)
                            noTone(BUZZER_PIN);
                        break;
                    }
                }
            }
        }

        if (millis() - lid_update_debounce < LID_UPDATE_DEBOUNCE_TIME)
            continue;

        lid_update_debounce = millis();

        const auto old_is_lid_open = std::exchange(is_lid_open, digitalRead(IR_PIN));
        if (is_lid_open != old_is_lid_open) {
            if (is_lid_open) {
                Serial.println("Lid opened!");
                if (not got_correct_password) {
                    Serial.println("Alarm!");
                    tone(BUZZER_PIN, g_buzzer_frequency);
                } else {
                    Serial.println("Lid opened but no alarm!");
                }
            } else {
                noTone(BUZZER_PIN);
                Serial.println("Lid closed!");

                got_correct_password = false;
                g_mqtt_client.publish("PERMISSION_STATE/REQ/2/W", "0");
            }

            g_mqtt_client.publish("IR_STATE/REQ/2/W", is_lid_open ? "1" : "0");
        }

        g_mqtt_client.loop();
    }
}
