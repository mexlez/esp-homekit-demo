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
#include "button.h"


// Low-sides shared by all LEDs
#define LED_COMMON_1 0
#define LED_COMMON_2 0

// Buttons are actibe low for at least 10ms to "press"
#define BTN_POWER 0
// Only include this for completeness; the light is needed to monitor state
#define BTN_LIGHT 0
#define BTN_OSCILLATE 0
#define BTN_SPEED 0
#define BTN_TIMER 0

// LED high lines; used along with LED common lines to infer states

// Speed LED lines
#define STAT_SP1_2 0
#define STAT_SP3_4 0
#define STAT_SP5_6 0
#define STAT_SP7_8 0

// Timer LED lines
#define STAT_T1_2 0
#define STAT_T3_4 0

// Oscillation motor control line; drives a stepper motor driver w/square wave
#define LINE_OSC 0

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

uint8_t g_fan_active = 0;
homekit_value_t fan_active_get(void) {
  return HOMEKIT_UINT8(g_fan_active);
}

void fan_active_set(homekit_value_t value) {
  // Press BTN_POWER
  printf("ONOFF:: %i\n", value.int_value);
  g_fan_active = value.int_value;
  switch(value.int_value) {
      case 0:
        // Press BTN_POWER until the fan is off
        printf("Fan OFF\n");
        break;
      case 1:
        // Press BTN_POWER until the fan is on
        printf("Fan ON\n");
        break;
      default:
        // This should never happen
        printf("Unknown command received in HomeKit 'Active' callback: %i\n", value.int_value);
  }
}

float g_fan_speed = 0.0;
void rotation_speed_set(homekit_value_t value) {
  printf("Fan speed set to: %f\n", value.float_value);
  g_fan_speed = value.float_value;
  if (value.float_value <= 0.0) {
    printf("Fan set to OFF\n");
  } else if (value.float_value <= 11.1) {
    printf("Fan set to speed 1\n");
  } else if (value.float_value <= 22.2) {
    printf("Fan set to speed 2\n");
  } else if (value.float_value <= 33.3) {
    printf("Fan set to speed 3\n");
  } else if (value.float_value <= 44.4) {
    printf("Fan set to speed 4\n");
  } else if (value.float_value <= 55.5) {
    printf("Fan set to speed 5\n");
  } else if (value.float_value <= 66.6) {
    printf("Fan set to speed 6\n");
  } else if (value.float_value <= 77.7) {
    printf("Fan set to speed 7\n");
  } else if (value.float_value <= 88.8) {
    printf("Fan set to speed 8\n");
  }
  // Press BTN_SPEED until the speed matches the set speed
}
homekit_value_t rotation_speed_get(void) {
  return HOMEKIT_FLOAT(g_fan_speed);
}

int g_swing_mode = 0;
void swing_mode_set(homekit_value_t value) {
  // Press BTN_OSCILLATE
  printf("OSCILLATION:: %i\n", value.int_value);
  g_swing_mode = value.int_value;
  switch(value.int_value) {
    case 0:
      // press BTN_OSCILLATE until oscillation is OFF
      printf("Oscillation OFF\n");
      break;
    case 1:
      // Press BTN_OSCILLATE until oscillation is ON
      printf("Oscilaltion ON\n");
      break;
    default:
      // This should never happen
      printf("Unknown command received in HomeKit 'Swing Mode' callback: %i\n", value.int_value);
  }
}
homekit_value_t swing_mode_get(void) {
  return HOMEKIT_UINT8(g_swing_mode);
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


void button_callback(uint8_t gpio, button_event_t event) {
    switch (event) {
        case button_event_single_press:
            printf("single press\n");
            // homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(0));
            break;
        case button_event_double_press:
            printf("double press\n");
            // homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(1));
            break;
        case button_event_long_press:
            printf("long press\n");
            // homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(2));
            break;
        default:
            printf("unknown button event: %d\n", event);
    }
}


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


homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};


void user_init(void) {
    uart_set_baud(0, 115200);

    wifi_init();
    /*
    if (button_create(BUTTON_PIN, button_callback)) {
        printf("Failed to initialize button\n");
    }
    */
    homekit_server_init(&config);
}
