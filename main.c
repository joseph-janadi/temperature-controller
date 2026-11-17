#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include "temp_sensor.h"
#include "heater.h"

#define PROGRAM_NAME "temperature-controller"

#define CLOCKID     CLOCK_MONOTONIC
#define INTERVAL_MS 1000UL
#define NS_PER_S    1000000000UL
#define NS_PER_MS   1000000UL

const uint32_t HEATER_PWM_PERIOD_NS = 1000000;
const uint8_t HEATER_PWM_DUTY_CYCLE = 50;
const uint32_t HEATER_PWM_DUTY_CYCLE_NS = (uint64_t)HEATER_PWM_PERIOD_NS *
    HEATER_PWM_DUTY_CYCLE / 100;

static int wait(struct timespec *next_time);

int main(void)
{
    int status;
    int32_t temp;
    struct timespec next_time;

    openlog(PROGRAM_NAME, 0, LOG_USER);
    syslog(LOG_INFO, "Program has started");

    // Initialize temperature sensor and heater
    status = temp_sensor_init();
    if (status) {
        exit(EXIT_FAILURE);
    }
    status = heater_init(HEATER_PWM_PERIOD_NS, HEATER_PWM_DUTY_CYCLE_NS);
    if (status) {
        exit(EXIT_FAILURE);
    }

    // Get current time
    status = clock_gettime(CLOCKID, &next_time);
    if (status) {
        syslog(LOG_ERR, "Failed clock_gettime");
        return -1;
    }


    while (1) {
        // Get temperature
        status = get_temp(&temp);
        if (status < 0) {
            exit(EXIT_FAILURE);
        }

        // Log temperature value
        syslog(LOG_INFO, "temp = %d.%d C", temp/100, temp%100);

        // Wait until next scheduled time
        status = wait(&next_time);
        if (status) {
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

static int wait(struct timespec *next_time)
{
    int status;

    // Increment current time by interval
    next_time->tv_nsec += INTERVAL_MS * NS_PER_MS;
    if ((uint32_t)next_time->tv_nsec >= NS_PER_S) {
        next_time->tv_sec++;
        next_time->tv_nsec -= NS_PER_S;
    }

    // Sleep until next scheduled time
    status = clock_nanosleep(CLOCKID, TIMER_ABSTIME, next_time, NULL);
    if (status) {
        syslog(LOG_ERR, "Failed clock_nanosleep(): %s\n", strerror(status));
        return -1;
    }

    return 0;
}
