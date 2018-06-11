#include <stdint.h>
#include <stdbool.h>
typedef void (*fan_speed_cb_t)(float speed);
typedef void (*on_off_state_cb_t)(bool on_off);
void HYF290B_init(uint8_t motor_hi_pin,
                  uint8_t motor_med_pin,
                  uint8_t oscillation_pin,
                  fan_speed_cb_t speed_changed_callback,
                  on_off_state_cb_t power_state_changed_callback,
                  on_off_state_cb_t oscillation_state_changed_callback,
                  uint8_t power_btn_pin,
                  uint8_t speed_btn_pin,
                  uint8_t oscillate_btn_pin);
void HYF290B_start(void);
void HYF290B_speed_set(float speed);
float HYF290B_speed_get(void);
void HYF290B_power_set(bool on_off);
bool HYF290B_power_get(void);
void HYF290B_oscillation_set(bool on_off);
bool HYF290B_oscillation_get(void);
