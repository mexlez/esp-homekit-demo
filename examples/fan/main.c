#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"

#include "HYF290B.h"


// Buttons are active low for at least 10ms to "press"
#define BTN_POWER 5
// Only include this for completeness; the light is needed to monitor state
#define BTN_LIGHT 0
#define BTN_OSCILLATE 4
#define BTN_SPEED 14
#define BTN_TIMER 0

// LED high lines; used along with LED common lines to infer states

// Lines connected to the motor BJTs, labelled "H" and "M" on the PCB
#define MOTOR_HI_PIN 13
#define MOTOR_MED_PIN 2

// Oscillation motor control line; drives a stepper motor driver w/square wave
#define LINE_OSC 12

static void wifi_init() {
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}

void fan_identify(homekit_value_t _value) {
  // called to identify the accessory
  // spin the fan up and down a couple times
    printf("Fan identify\n");
}

homekit_value_t fan_active_get(void) {
  return HOMEKIT_UINT8(HYF290B_power_get());
}

void fan_active_set(homekit_value_t value) {
  HYF290B_power_set(value.int_value);
}

void rotation_speed_set(homekit_value_t value) {
  HYF290B_speed_set(value.float_value);
}

homekit_value_t rotation_speed_get(void) {
  return HOMEKIT_FLOAT(HYF290B_speed_get());
}

void swing_mode_set(homekit_value_t value) {
  HYF290B_oscillation_set(value.int_value);
}

homekit_value_t swing_mode_get(void) {
  return HOMEKIT_UINT8(HYF290B_oscillation_get());
}

// Initially (physically) turned off
homekit_characteristic_t fan_active = HOMEKIT_CHARACTERISTIC_(
        ACTIVE,
        0,
        .getter=fan_active_get,
        .setter=fan_active_set
    );

// Initially not spinning
homekit_characteristic_t rotation_speed = HOMEKIT_CHARACTERISTIC_(
        ROTATION_SPEED,
        0.0,
        .getter=rotation_speed_get,
        .setter=rotation_speed_set
    );

// Initially not swinging (oscillating)
homekit_characteristic_t swing_mode = HOMEKIT_CHARACTERISTIC_(
        SWING_MODE,
        0,
        .getter=swing_mode_get,
        .setter=swing_mode_set
    );


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_fan,
        .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(
                ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "Fart-Wafter 9000"),
                    HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Honeywell"),
                    HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0012345"),
                    HOMEKIT_CHARACTERISTIC(MODEL, "HYF290B"),
                    HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, fan_identify),
                    NULL
                },
            ),
            HOMEKIT_SERVICE(
                FAN2,
                .primary=true,
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "Fart-Wafter 9000"),
                    &fan_active,
                    &rotation_speed,
                    &swing_mode,
                    NULL
                },
            ),
            NULL
        },
    ),
    NULL
};

void fan_speed_changed_cb(float speed) {
  printf("Fan speed: %f\n", speed);
  homekit_characteristic_notify(&rotation_speed, HOMEKIT_FLOAT(speed));
}

void power_state_changed_cb(bool on_off) {
  printf("Power state: %i\n", on_off);
  homekit_characteristic_notify(&fan_active, HOMEKIT_UINT8(on_off));
}

void oscillation_state_changed_cb(bool on_off) {
  printf("Oscillation state: %i\n", on_off);
  homekit_characteristic_notify(&swing_mode, HOMEKIT_UINT8(on_off));
}

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void user_init(void) {
    uart_set_baud(0, 115200);
    wifi_init();
    HYF290B_init( MOTOR_HI_PIN,
                  MOTOR_MED_PIN,
                  LINE_OSC,
                  fan_speed_changed_cb,
                  power_state_changed_cb,
                  oscillation_state_changed_cb,
                  BTN_POWER,
                  BTN_SPEED,
                  BTN_OSCILLATE
                );

    homekit_server_init(&config);
    HYF290B_start();
}
