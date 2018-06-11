#include "HYF290B.h"
//#include <etstimer.h>
#include <esplibs/libmain.h>
#include <espressif/esp_system.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// How response the LED "off" detection is. If the timer goes this many milliseconds
// without being reset by the LED lines being properly set, the LED will be detected
// as being OFF.
#define LED_RESPONSIVENESS 100

#define ON_OFF_CHECK_INTERVAL 400000

#define OSCILLATION_CHECK_INTERVAL 400000


// A motor control line will have this number of pulses for each speed. After The
// indicated number of pulses, a pulse will be skipped.

// If we go this long without seeing a negative edge on a motor line, then we've
// counted all of the pulses
#define PULSE_LENGTH_US 15000

// Total number of speeds on the fan
#define NUM_SPEEDS 8.0

// Number of pulses before one is skipped on motor line M
#define SPD1_NUM_PULSES 6
#define SPD2_NUM_PULSES 9
#define SPD3_NUM_PULSES 13
// No "skips"
#define SPD4_NUM_PULSES 0

// Number of pulses before one is skipped on motor line H
#define SPD5_NUM_PULSES 4
#define SPD6_NUM_PULSES 6
#define SPD7_NUM_PULSES 9
// No "skips"
#define SPD8_NUM_PULSES 0
#define SPD_MAX_PULSES 14

typedef enum {
  MOTOR_MEDIUM,
  MOTOR_HI,
} MOTOR_SPEED_TYPE_ENUM_t;

static struct {
  ETSTimer speed_timer;
  ETSTimer oscillation_timer;
  ETSTimer power_timer;
  bool power;
  bool oscillate;
  uint8_t hi_pin;
  uint8_t med_pin;
  uint8_t oscillation_pin;
  uint8_t npulses;
  uint8_t last_npulses;
  uint8_t last_mode;
  float speed;
  uint8_t int_speed;

  uint8_t power_btn;
  uint8_t speed_btn;
  uint8_t oscillate_btn;
  fan_speed_cb_t callback;
  on_off_state_cb_t power_callback;
  on_off_state_cb_t oscillate_callback;
  uint32_t last_activity_timer;
} g_motor_config;


static void motor_pin_cb(uint8_t gpio_num);

static void report_speed(uint8_t speed);

static void power_timer_cb(void *arg);

static void oscillation_pin_cb(uint8_t gpio_num);

static void button_pusher_init(uint8_t btn_gpio);
static void push_button(uint8_t btn_gpio);

typedef struct {
  uint32_t ts;
  uint8_t gpio_source;
} motor_evt_t;

QueueHandle_t g_oscillation_evt_q;
void oscillation_monitor_task(void *pvParameters) {
  uint32_t ts = 0;
  BaseType_t ret;

  g_oscillation_evt_q = xQueueCreate(2, sizeof(uint32_t));
  gpio_enable(g_motor_config.oscillation_pin, GPIO_INPUT);
  gpio_set_interrupt(g_motor_config.oscillation_pin, GPIO_INTTYPE_EDGE_NEG, oscillation_pin_cb);

  g_motor_config.oscillate_callback(g_motor_config.oscillate);
  while(1) {
    ret = xQueueReceive(g_oscillation_evt_q, &ts, 25);
    xQueueReset(g_oscillation_evt_q);
    if (ret == pdTRUE) {
      if (g_motor_config.oscillate == false) {
          g_motor_config.oscillate_callback(true);
      }
      g_motor_config.oscillate = true;
    } else {
      if (g_motor_config.oscillate == true) {
        g_motor_config.oscillate_callback(false);
      }
      g_motor_config.oscillate = false;
    }
  }
}

void power_monitor_task(void *pvParameters) {
  while(1) {
    vTaskDelay(25);
    if ((sdk_system_get_time() - g_motor_config.last_activity_timer) > ON_OFF_CHECK_INTERVAL) {
      if (g_motor_config.power == true) {
        g_motor_config.power_callback(false);
      }
      g_motor_config.power = false;
    } else {
      if (g_motor_config.power == false) {
        g_motor_config.power_callback(true);
      }
      g_motor_config.power = true;
    }
  }
}

QueueHandle_t g_motor_evt_q;
void motor_monitor_task(void *pvParameters) {
  uint32_t last_ts = 0;
  uint8_t npulses = 0;
  uint8_t last_npulses = 0;
  uint8_t new_speed = 0;
  BaseType_t ret;

  g_motor_config.last_activity_timer = 0;
  g_motor_evt_q = xQueueCreate(10, sizeof(motor_evt_t));
  gpio_enable(g_motor_config.hi_pin, GPIO_INPUT);
  gpio_enable(g_motor_config.med_pin, GPIO_INPUT);
  gpio_set_interrupt(g_motor_config.hi_pin, GPIO_INTTYPE_EDGE_NEG, motor_pin_cb);
  gpio_set_interrupt(g_motor_config.med_pin, GPIO_INTTYPE_EDGE_NEG, motor_pin_cb);
  report_speed(g_motor_config.int_speed);
  while(1) {
    new_speed = 0;
    motor_evt_t evt;
    ret = xQueueReceive(g_motor_evt_q, &evt, portMAX_DELAY);
    if (ret != pdTRUE) {
      continue;
    }
    g_motor_config.last_activity_timer = evt.ts;
    npulses++;
    // printf("%i\n", evt.ts - last_ts);
    if (evt.ts - last_ts > PULSE_LENGTH_US) {
      if (evt.gpio_source == g_motor_config.med_pin) {
        // printf("%i\n", evt.ts - last_ts);
        // printf("npulses: %i last_npulses: %i\n", npulses, last_npulses);
        if (last_npulses == npulses) {
          continue;
        }
        last_npulses = npulses;
        switch(npulses) {
          case 5:
          case 6:
            new_speed = 1;
            break;
          case 9:
            new_speed = 2;
            break;
          case 13:
            new_speed = 3;
            break;
          default:
            break;
        }
      } else if (evt.gpio_source == g_motor_config.hi_pin) {
        switch(npulses) {
          case 4:
            new_speed = 5;
            break;
          case 5:
            break;
          case 6:
            new_speed = 6;
            break;
          case 9:
            new_speed = 7;
            break;
          default:
            break;
        }
      }
      npulses = 0;
    } else if (npulses >= SPD_MAX_PULSES) {
      if (evt.gpio_source == g_motor_config.med_pin) {
        new_speed = 4;
      } else if (evt.gpio_source == g_motor_config.hi_pin) {
        new_speed = 8;
      }
      npulses = 0;
    }
    if (new_speed != 0 && new_speed != g_motor_config.int_speed) {
      g_motor_config.int_speed = new_speed;
      report_speed(new_speed);
    }
    last_ts = evt.ts;
  }
}



void HYF290B_init(uint8_t motor_hi_pin,
                  uint8_t motor_med_pin,
                  uint8_t oscillation_pin,
                  fan_speed_cb_t speed_changed_callback,
                  on_off_state_cb_t power_state_changed_callback,
                  on_off_state_cb_t oscillation_state_changed_callback,
                  uint8_t power_btn_pin,
                  uint8_t speed_btn_pin,
                  uint8_t oscillate_btn_pin) {
  printf("Starting HYF290B driver...\n");
  g_motor_config.hi_pin = motor_hi_pin;
  g_motor_config.med_pin = motor_med_pin;
  g_motor_config.oscillation_pin = oscillation_pin;
  g_motor_config.callback = speed_changed_callback;
  g_motor_config.power_callback = power_state_changed_callback;
  g_motor_config.oscillate_callback = oscillation_state_changed_callback;
  g_motor_config.power_btn = power_btn_pin;
  g_motor_config.speed_btn = speed_btn_pin;
  g_motor_config.oscillate_btn = oscillate_btn_pin;

  printf("HYF290B driver running!\n");
}

void HYF290B_start(void) {
    // Setup buttons
    button_pusher_init(g_motor_config.power_btn);
    button_pusher_init(g_motor_config.speed_btn);
    button_pusher_init(g_motor_config.oscillate_btn);

    xTaskCreate(motor_monitor_task, "MotorMonitorTask", 256, &g_motor_evt_q, 3, NULL);
    xTaskCreate(power_monitor_task, "PowerMonitorTask", 256, NULL, 2, NULL);
    xTaskCreate(oscillation_monitor_task, "OscillationMonitorTask", 256, g_oscillation_evt_q, 2, NULL);
}

void HYF290B_speed_set(float speed) {
  printf("fan speed set to %f\n", speed);
  uint8_t target_speed = 0;
  if (0.0 <= speed && speed <= 4.0) {
    HYF290B_power_set(false);
    return;
  } else if (4.0 < speed && speed <= 22.2) {
    target_speed = 1;
  } else if (22.2 < speed && speed <= 33.3) {
    target_speed = 2;
  } else if (33.3 < speed && speed <= 44.4) {
    target_speed = 3;
  } else if (44.4 < speed && speed <= 55.5) {
    target_speed = 4;
  } else if (55.5 < speed && speed <= 66.6) {
    target_speed = 5;
  } else if (66.6 < speed && speed <= 77.7) {
    target_speed = 6;
  } else if (77.7 < speed && speed <= 88.8) {
    target_speed = 7;
  } else {
    target_speed = 8;
  }
  if (target_speed != g_motor_config.int_speed) {
    printf("Changing speed from %i to %i\n", g_motor_config.int_speed, target_speed);
    if (!g_motor_config.power) {
      HYF290B_power_set(true);
    }
    while (g_motor_config.int_speed != target_speed) {
      push_button(g_motor_config.speed_btn);
      vTaskDelay(25);
    }
  }
}

float HYF290B_speed_get(void) {
  return g_motor_config.speed;
}


void HYF290B_oscillation_set(bool on_off) {
  printf("Oscillation set to %i", on_off);
  while (g_motor_config.oscillate != on_off) {
    push_button(g_motor_config.oscillate_btn);
    vTaskDelay(100);
  }
}

bool HYF290B_oscillation_get(void) {
  return g_motor_config.oscillate;
}

void HYF290B_power_set(bool on_off) {
  printf("Fan power %i\n", on_off);
  while (g_motor_config.power != on_off) {
    push_button(g_motor_config.power_btn);
    vTaskDelay(100);
  }
}

bool HYF290B_power_get(void) {
  return g_motor_config.power;
}

static void motor_pin_cb(uint8_t gpio) {
  motor_evt_t evt = {sdk_system_get_time(), gpio};
  xQueueSendToBackFromISR(g_motor_evt_q, &evt, NULL);
}

static void report_speed(uint8_t speed) {
  switch(speed) {
    case 1:
      g_motor_config.callback(12.5);
      break;
    case 2:
      g_motor_config.callback(25.0);
      break;
    case 3:
      g_motor_config.callback(37.5);
      break;
    case 4:
      g_motor_config.callback(50.0);
      break;
    case 5:
      g_motor_config.callback(62.5);
      break;
    case 6:
      g_motor_config.callback(75.0);
      break;
    case 7:
      g_motor_config.callback(87.5);
      break;
    case 8:
      g_motor_config.callback(100);
      break;
  }
}


static void oscillation_pin_cb(uint8_t gpio_num) {
  uint32_t ts = sdk_system_get_time();
  xQueueSendToFrontFromISR(g_oscillation_evt_q, &ts, NULL);
}

static void button_pusher_init(uint8_t btn_gpio) {
  // Setup the button as an input so we don't clobber the fan's normal button operation
  gpio_enable(btn_gpio, GPIO_INPUT);
}

static void push_button(uint8_t btn_gpio) {
  gpio_enable(btn_gpio, GPIO_OUTPUT);
  gpio_write(btn_gpio, 0);
  // Have to keep the line low long enough for the controller to read the line
  vTaskDelay(20);
  gpio_enable(btn_gpio, GPIO_INPUT);
  return;
}
