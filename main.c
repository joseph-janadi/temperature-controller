#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <syslog.h>
#include <signal.h>
#include "temp_sensor.h"
#include "heater.h"

#define PROGRAM_NAME "temperature-controller"
#define NUM_SIGNALS 3

#define CLOCKID     CLOCK_MONOTONIC
#define INTERVAL_MS 1000UL
#define NS_PER_S    1000000000LL
#define NS_PER_MS   1000000ULL

const uint32_t HEATER_PWM_PERIOD_NS = 1000000;
const uint8_t HEATER_PWM_DUTY_CYCLE = 50;
const uint32_t HEATER_PWM_DUTY_CYCLE_NS = (uint64_t)HEATER_PWM_PERIOD_NS *
    HEATER_PWM_DUTY_CYCLE / 100;

static volatile sig_atomic_t g_stop = 0;

static int setup_signal_handlers(void);
static int wait_for_next_tick(struct timespec *next_time);

int main(void)
{
    int status;
    int32_t temp;
    struct timespec next_time;
    
    status = setup_signal_handlers();
    if (status) {
        exit(EXIT_FAILURE);
    }

    openlog(PROGRAM_NAME, 0, LOG_USER);
    syslog(LOG_INFO, "Program started");

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

    while (!g_stop) {
        // Get temperature
        status = get_temp(&temp);
        if (status < 0) {
            exit(EXIT_FAILURE);
        }

        // Log temperature value
        syslog(LOG_INFO, "temp = %d.%d C", temp/100, temp%100);

        // Wait until next scheduled time
        status = wait_for_next_tick(&next_time);
        if (status) {
            exit(EXIT_FAILURE);
        }
    }

    // Clean up temp_sensor (I2C) and heater (PWM)
    temp_sensor_cleanup();
    heater_cleanup();

    syslog(LOG_INFO, "Program stopped");

    return 0;
}

void handler(int signo)
{
    (void)signo;
    g_stop = 1;
}

static int setup_signal_handlers(void)
{
    int status;
    struct sigaction act = { 0 };
    int signals[NUM_SIGNALS] = {SIGINT, SIGTERM, SIGHUP};

    act.sa_handler = handler;
    for (int i = 0; i < NUM_SIGNALS; i++) {
        status = sigaction(signals[i], &act, NULL);
        if (status) {
            syslog(LOG_ERR, "Failed to change signal actions: %s",
                    strerror(errno));
            return -1;
        }
    }

    return 0;
}

static int wait_for_next_tick(struct timespec *next_time)
{
    int status;
    int64_t next_nsec;

    // Increment current time by interval
    next_nsec = next_time->tv_nsec + (INTERVAL_MS * NS_PER_MS);
    next_time->tv_sec += next_nsec / NS_PER_S;
    next_time->tv_nsec = next_nsec % NS_PER_S ;

    // Sleep until next scheduled time
    // Ignore interrupts unless g_stop is set
    do {
        status = clock_nanosleep(CLOCKID, TIMER_ABSTIME, next_time, NULL);
    } while (status == EINTR && !g_stop);
    if (status != 0 && status != EINTR) {
        syslog(LOG_ERR, "Failed clock_nanosleep(): %s", strerror(status));
        return -1;
    }

    return 0;
}
