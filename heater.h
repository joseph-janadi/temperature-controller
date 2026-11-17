#include <stdint.h>

/**
 * @brief Start PWM to heating element
 *
 * @param[in] pwm_period_ns PWM period in nanoseconds.
 * @param[in] pwm_duty_cycle_ns PWM duty cycle in nanoseconds; must be less than
 * or equal to pwm_period_ns.
 * @return Status: 0 on success, -1 on failure
 */
int heater_init(const uint32_t pwm_period_ns, const uint32_t pwm_duty_cycle_ns);
