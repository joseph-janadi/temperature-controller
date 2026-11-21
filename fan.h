#include <stdint.h>

/**
 * @brief Enable PWM to fan
 *
 * @return Status: 0 on success, -1 on failure
 */
int fan_init(const uint32_t pwm_period_ns);

void fan_cleanup(void);

int fan_set_duty(uint32_t duty_cycle_ns);

int fan_get_duty(uint8_t *duty_cycle);
