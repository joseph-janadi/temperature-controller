#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include "temp_sensor.h"

#define PROGRAM_NAME "temperature-controller"
#define CLOCKID CLOCK_MONOTONIC
#define INTERVAL_MS 1000
#define NS_PER_S 1000000000L
#define NS_PER_MS 1000000L

int wait(void);

int main(void)
{
    int status;
    int32_t temp;

    openlog(PROGRAM_NAME, 0, LOG_USER);
    syslog(LOG_INFO, "Program has started");

    // Initialize temperature sensor
    status = temp_sensor_init();
    if (status) {
        exit(EXIT_FAILURE);
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
        status = wait();
        if (status) {
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

int wait(void)
{
    int status;
    struct timespec next_time;

    // Get current time
    status = clock_gettime(CLOCKID, &next_time);
    if (status) {
        syslog(LOG_ERR, "Failed clock_gettime");
        return -1;
    }

    // Increment current time by interval
    next_time.tv_nsec += INTERVAL_MS * NS_PER_MS;
    if (next_time.tv_nsec >= NS_PER_S) {
        next_time.tv_sec++;
        next_time.tv_nsec -= NS_PER_S;
    }

    // Sleep until next scheduled time
    status = clock_nanosleep(CLOCKID, TIMER_ABSTIME, &next_time, NULL);
    if (status) {
        syslog(LOG_ERR, "Failed clock_nanosleep(): %s\n", strerror(status));
        return -1;
    }

    return 0;
}
