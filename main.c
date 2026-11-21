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
#include "fan.h"

#define PROGRAM_NAME "temperature-controller"
#define NUM_SIGNALS 3

#define CLOCKID     CLOCK_MONOTONIC
#define INTERVAL_MS 1000UL
#define NS_PER_S    1000000000LL
#define NS_PER_MS   1000000ULL

#define HEATER_PWM_PERIOD_NS        1000000UL
#define HEATER_PWM_DUTY_CYCLE       50U
#define HEATER_PWM_DUTY_CYCLE_NS    ((uint32_t)((uint64_t)(HEATER_PWM_PERIOD_NS) * \
                                    (HEATER_PWM_DUTY_CYCLE) / 100))

#define TEMP_THRESHOLD_LOWER    20L
#define TEMP_THRESHOLD_UPPER    25L
#define FAN_PWM_PERIOD_NS       1000000UL
#define FAN_DUTY_CYCLE_MIN      20
#define FAN_DUTY_CYCLE_MAX      100

static volatile sig_atomic_t g_stop = 0;

static int setup_signal_handlers(void);
uint8_t duty_from_temp(int32_t temp);
static int wait_for_next_tick(struct timespec *next_time);

int main(void)
{
    int status;
    int32_t temp;
    uint8_t fan_duty_target, fan_duty_actual;
    struct timespec next_time;
    
    status = setup_signal_handlers();
    if (status) {
        exit(EXIT_FAILURE);
    }

    openlog(PROGRAM_NAME, 0, LOG_USER);
    syslog(LOG_INFO, "Program started");

    // Initialize temperature sensor, heater, and fan
    status = temp_sensor_init();
    if (status) {
        exit(EXIT_FAILURE);
    }
    status = heater_init(HEATER_PWM_PERIOD_NS, HEATER_PWM_DUTY_CYCLE_NS);
    if (status) {
        exit(EXIT_FAILURE);
    }
    status = fan_init(FAN_PWM_PERIOD_NS);
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
        // Get temperature and log it
        status = get_temp(&temp);
        if (status < 0) {
            break;
        }
        syslog(LOG_INFO, "temp = %d.%d C", temp/100, temp%100);

        // Calcuate fan duty cycle target from temperature
        fan_duty_target = duty_from_temp(temp/100);

        // Set fan duty cycle to maintain temperature
        status = fan_set_duty((uint64_t)fan_duty_target * FAN_PWM_PERIOD_NS / 100);
        if (status) {
            break;
        }

        // Get actual fan duty cycle and log it
        status = fan_get_duty(&fan_duty_actual);
        if (status) {
            break;
        }

        // Wait until next scheduled time
        status = wait_for_next_tick(&next_time);
        if (status) {
            break;
        }
    }

    // Clean up temp_sensor (I2C) and heater (PWM)
    temp_sensor_cleanup();
    heater_cleanup();
    fan_cleanup();

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

uint8_t duty_from_temp(int32_t temp)
{
    if (temp < TEMP_THRESHOLD_LOWER) {
        return 0;
    }
    else if (temp >= TEMP_THRESHOLD_UPPER) {
        return 100;
    }

    return (uint8_t)(FAN_DUTY_CYCLE_MIN + (temp - TEMP_THRESHOLD_LOWER) *
            (FAN_DUTY_CYCLE_MAX - FAN_DUTY_CYCLE_MIN) / (TEMP_THRESHOLD_UPPER -
                TEMP_THRESHOLD_LOWER));
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
