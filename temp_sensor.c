#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <syslog.h>

#define I2C_DEV_FILE "/dev/i2c-1"
#define BME280_ADDR 0x77
#define DIG_T_REG 0x88
#define DIG_T_LEN 6
#define CTRL_MEAS_REG 0xF4
#define TEMP_OVERSAMPLE_X1 0x1
#define TEMP_OVERSAMPLE TEMP_OVERSAMPLE_X1
#define TEMP_OVERSAMPLE_SHIFT 5
#define CTRL_MEAS_MODE_FORCED 0x1
#define CTRL_MEAS_MODE CTRL_MEAS_MODE_FORCED
#define CTRL_MEAS_MODE_SHIFT 0
#define CTRL_MEAS_OPTIONS ((TEMP_OVERSAMPLE << TEMP_OVERSAMPLE_SHIFT) | \
        (CTRL_MEAS_MODE << CTRL_MEAS_MODE_SHIFT))
#define TEMP_REG 0xFA
#define TEMP_REG_LEN 3

static int i2c_fd;
// Temperature trimming parameters for conversion calculation
static uint16_t dig_T1;
static int16_t dig_T2, dig_T3;

static int32_t adc_to_degc(int32_t adc_T);

int temp_sensor_init(void)
{
    int status;
    int32_t num_bytes_read;
    uint8_t dig_T_arr[DIG_T_LEN];

    // Open i2c file
    i2c_fd = open(I2C_DEV_FILE, O_RDWR);
    if (i2c_fd < 0) {
        syslog(LOG_ERR, "Failed to open i2c file");
        return -1;
    }

    // Select BME280 sensor to communicate with over I2C
    status = ioctl(i2c_fd, I2C_SLAVE, BME280_ADDR);
    if (status < 0) {
        syslog(LOG_ERR, "Failed to select sensor as I2C slave: %s",
                strerror(errno));
    }

    // Get temperature trimming parameters
    num_bytes_read = i2c_smbus_read_i2c_block_data(i2c_fd, DIG_T_REG,
            DIG_T_LEN, dig_T_arr);
    if (num_bytes_read != DIG_T_LEN) {
        syslog(LOG_ERR, "Failed to get dig_T_arr: read %d bytes instead of %d",
                num_bytes_read, DIG_T_LEN);
        return -1;
    }
    dig_T1 = (dig_T_arr[1] << 8) | dig_T_arr[0];
    dig_T2 = (dig_T_arr[3] << 8) | dig_T_arr[2];
    dig_T3 = (dig_T_arr[5] << 8) | dig_T_arr[4];

    return 0;
}

int get_temp(int32_t *temp)
{
    int status;
    int32_t adc_val, num_bytes_read;
    uint8_t adc_val_arr[TEMP_REG_LEN];

    // Configure sensor for temperature measurement only, forced mode.
    // Required to bring out of sleep each time
    status = i2c_smbus_write_byte_data(i2c_fd, CTRL_MEAS_REG,
            CTRL_MEAS_OPTIONS);
    if (status) {
        syslog(LOG_ERR, "Failed to write ctrl_meas options");
        return -1;
    }

    // Read raw ADC value from temperature registers
    num_bytes_read = i2c_smbus_read_i2c_block_data(i2c_fd, TEMP_REG,
            TEMP_REG_LEN, adc_val_arr);
    if (num_bytes_read != TEMP_REG_LEN) {
        syslog(LOG_ERR, "Failed to get temperature ADC value");
        return -1;
    }
    adc_val = (adc_val_arr[0] << 12) | (adc_val_arr[1] << 4) |
        (adc_val_arr[2] >> 4);

    // Convert raw ADC value to degrees-C
    *temp = adc_to_degc(adc_val);

    return 0;
}

// Adapted from BME280 datasheet
static int32_t adc_to_degc(int32_t adc_T)
{
    int32_t var1, var2, T, t_fine;

    var1 = ((((adc_T>>3) - ((int32_t)dig_T1<<1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T>>4) - ((int32_t)dig_T1)) * ((adc_T>>4) - ((int32_t)dig_T1))) >> 12)
            * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;

    return T;
}
