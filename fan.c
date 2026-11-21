#include "fan.h"
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#define PWM_CHIP_NUM    "0"
#define PWM_CHIP_DIR    "/sys/class/pwm/pwmchip" PWM_CHIP_NUM "/"
#define PWM_CHAN_NUM    "1"
#define PWM_CHAN_DIR    PWM_CHIP_DIR "pwm" PWM_CHAN_NUM "/"
#define MAX_BUF_SIZE    16

static int write_to_file(const char *file, const char *val);

int fan_init(const uint32_t pwm_period_ns)
{
    int status;
    char pwm_period_ns_str[MAX_BUF_SIZE];

    // Convert PWM period to string
    status = snprintf(pwm_period_ns_str, MAX_BUF_SIZE, "%u", pwm_period_ns);
    if (status < 0) {
        syslog(LOG_ERR, "Failed to convert fan pwm_period_ns to string");
        return -1;
    }

    // Export PWM channel for use with sysfs
    status = write_to_file(PWM_CHIP_DIR "export", PWM_CHAN_NUM);
    if (status) {
        syslog(LOG_ERR, "Failed to export PWM channel %s", PWM_CHAN_NUM);
        return -1;
    }

    // Set PWM period
    status = write_to_file(PWM_CHAN_DIR "period", pwm_period_ns_str);
    if (status) {
        syslog(LOG_ERR, "Failed to set channel %s period", PWM_CHAN_NUM);
        return -1;
    }

    // Enable PWM
    status = write_to_file(PWM_CHAN_DIR "enable", "1");
    if (status) {
        syslog(LOG_ERR, "Failed to enable PWM channel %s", PWM_CHAN_NUM);
        return -1;
    }

    return 0;
}

void fan_cleanup(void)
{
    int status;

    // Disable PWM
    status = write_to_file(PWM_CHAN_DIR "enable", "0");
    if (status) {
        syslog(LOG_ERR, "Failed to disable PWM channel %s", PWM_CHAN_NUM);
    }

    // Unexport PWM channel
    status = write_to_file(PWM_CHIP_DIR "unexport", PWM_CHAN_NUM);
    if (status) {
        syslog(LOG_ERR, "Failed to unexport PWM channel %s", PWM_CHAN_NUM);
    }
}

int fan_set_duty(uint32_t duty_cycle_ns)
{
    int status;
    char pwm_duty_cycle_ns_str[MAX_BUF_SIZE];

    // Convert duty cycle to string
    status = snprintf(pwm_duty_cycle_ns_str, MAX_BUF_SIZE, "%u", duty_cycle_ns);
    if (status < 0) {
        syslog(LOG_ERR, "Failed to convert fan duty_cycle_ns to string");
        return -1;
    }

    // Set duty cycle
    status = write_to_file(PWM_CHAN_DIR "duty_cycle", pwm_duty_cycle_ns_str);
    if (status) {
        syslog(LOG_ERR, "Failed to set channel %s duty cycle", PWM_CHAN_NUM);
        return -1;
    }

    return 0;
}

int fan_get_duty(uint8_t *duty_cycle)
{
    (void)duty_cycle;

    return 0;
}

static int write_to_file(const char *file, const char *val)
{
    int fd, status;
    size_t num_bytes_to_write;
    ssize_t num_bytes_written;

    fd = open(file, O_WRONLY);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to open file %s: %s", file, strerror(errno));
        return -1;
    }

    num_bytes_to_write = strlen(val);
    num_bytes_written = write(fd, val, num_bytes_to_write);
    if (num_bytes_written < 0) {
        syslog(LOG_ERR, "Failed to write %s to file %s: %s", val, file,
                strerror(errno));
        return -1;
    }
    if ((size_t)num_bytes_written != num_bytes_to_write) {
        syslog(LOG_ERR, "Failed to write %s to file %s: %zd of %zu bytes written",
                val, file, num_bytes_written, num_bytes_to_write);
        return -1;
    }

    status = close(fd);
    if (status) {
        syslog(LOG_ERR, "Failed to close file %s: %s", file, strerror(errno));
        return -1;
    }

    return 0;
}
