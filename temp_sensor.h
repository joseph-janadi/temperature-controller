/**
 * @brief Initializes BM280 sensor
 *
 * Configures the sensor for temperature measurement only, in forced mode.
 *
 * @return Status: 0 on success, -1 on error
 */
int temp_sensor_init(void);

/**
 * @brief Reads the temperature from BM280 sensor
 *
 * @param[out] temp Pointer to store the temperature in degrees-C as a scaled
 *                  integer (e.g. 2550 is 25.50 degrees-C).
 * @return Status: 0 on success, -1 on error
 */
int get_temp(int32_t *temp);
