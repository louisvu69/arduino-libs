/*******************************************************************************
 * Copyright (c) 2023 Renesas Electronics Corporation
 * All Rights Reserved.
 *
 * This code is proprietary to Renesas, and is license pursuant to the terms and
 * conditions that may be accessed at:
 * https://www.renesas.com/eu/en/document/msc/renesas-software-license-terms-gas-sensor-software
 *
 ******************************************************************************/

/**
 * @file    iaq_2nd_gen.ino
 * @brief   This is an Arduino example for the ZMOD4410 gas sensor using the iaq_2nd_gen library.
 * @version 3.2.0
 * @author Renesas Electronics Corporation
 */


#include <zmod4410_config_iaq2.h>
#include <zmod4xxx.h>
#include <zmod4xxx_hal.h>
#include <iaq_2nd_gen.h>


void error_handle();

zmod4xxx_dev_t dev;

/* Sensor specific variables */
uint8_t zmod4xxx_status;
uint8_t prod_data[ZMOD4410_PROD_DATA_LEN];
uint8_t adc_result[ZMOD4410_ADC_DATA_LEN] = { 0 };
uint8_t track_number[ZMOD4XXX_LEN_TRACKING];
iaq_2nd_gen_handle_t algo_handle;
iaq_2nd_gen_results_t algo_results;
iaq_2nd_gen_inputs_t algo_input;


void setup() {
  int8_t lib_ret;
  zmod4xxx_err api_ret;

  Serial.begin(9600);

  /*
     * Additional delay is required to wait until system is ready.
     * It is used for MKRZERO platform.
     */
  delay(2000);

  Serial.println(F("Starting the Sensor!"));
  /**** TARGET SPECIFIC FUNCTION ****/
  /*
     * To allow the example running on customer-specific hardware, the init_hardware
     * function must be adapted accordingly. The mandatory funtion pointers *read,
     * *write and *delay require to be passed to "dev" (reference files located
     * in "dependencies/zmod4xxx_api/HAL" directory). For more information, read
     * the Datasheet, section "I2C Interface and Data Transmission Protocol".
     */
  api_ret = init_hardware(&dev);
  if (api_ret) {
    Serial.print(F("Error "));
    Serial.print(api_ret);
    Serial.println(F(" during init hardware, exiting program!\n"));
    error_handle();
  }
  /**** TARGET SPECIFIC FUNCTION ****/

  /* Sensor related data */
  dev.i2c_addr = ZMOD4410_I2C_ADDR;
  dev.pid = ZMOD4410_PID;
  dev.init_conf = &zmod_iaq2_sensor_cfg[INIT];
  dev.meas_conf = &zmod_iaq2_sensor_cfg[MEASUREMENT];
  dev.prod_data = prod_data;

  /* Read product ID and configuration parameters. */
  api_ret = zmod4xxx_read_sensor_info(&dev);
  if (api_ret) {
    Serial.print(F("Error "));
    Serial.print(api_ret);
    Serial.println(
      F(" during reading sensor information, exiting program!\n"));
    error_handle();
  }

  /*
     * Retrieve sensors unique tracking number and individual trimming information.
     * Provide this information when requesting support from Renesas.
     */

  api_ret = zmod4xxx_read_tracking_number(&dev, track_number);
  if (api_ret) {
    Serial.print(F("Error "));
    Serial.print(api_ret);
    Serial.println(F(" during preparation of the sensor, exiting program!\n"));
    error_handle();
  }
  Serial.print(F("Sensor tracking number: x0000"));
  for (uint8_t i = 0; i < sizeof(track_number); i++) {
    Serial.print(track_number[i], HEX);
  }
  Serial.println("");
  Serial.print(F("Sensor trimming data:"));
  for (uint8_t i = 0; i < sizeof(prod_data); i++) {
    Serial.print(prod_data[i]);
  }
  Serial.println("");

  /* Determine calibration parameters and configure measurement. */
  api_ret = zmod4xxx_prepare_sensor(&dev);
  if (api_ret) {
    Serial.print(F("Error "));
    Serial.print(api_ret);
    Serial.println(F(" during preparation of the sensor, exiting program!\n"));
    error_handle();
  }

  /*
     * One-time initialization of the algorithm. Handle passed to calculation
     * function.
     */
  lib_ret = init_iaq_2nd_gen(&algo_handle);
  if (lib_ret) {
    Serial.println(F("Error "));
    Serial.print(lib_ret);
    Serial.println(F(" during initializing algorithm, exiting program!"));
    error_handle();
  }
}
int count = 0;
void loop() {
  int8_t lib_ret;
  zmod4xxx_err api_ret;

  /* Start a measurement. */
  api_ret = zmod4xxx_start_measurement(&dev);
  if (api_ret) {
    Serial.print(F("Error "));
    Serial.print(api_ret);
    Serial.println(F(" during starting measurement, exiting program!\n"));
    error_handle();
  }

  /*
     * Perform delay. Required to keep proper measurement timing and keep algorithm accuracy.
     * For more information, read the Programming Manual, section
     * "Interrupt Usage and Measurement Timing".
     */
  dev.delay_ms(ZMOD4410_IAQ2_SAMPLE_TIME);

  /* Verify completion of measurement sequence. */
  api_ret = zmod4xxx_read_status(&dev, &zmod4xxx_status);
  if (api_ret) {
    Serial.print(F("Error "));
    Serial.print(api_ret);
    Serial.println(F(" during read of sensor status, exiting program!\n"));
    error_handle();
  }

  /* Check if measurement is running. */
  if (zmod4xxx_status & STATUS_SEQUENCER_RUNNING_MASK) {
    /*
         * Check if reset during measurement occured. For more information,
         * read the Programming Manual, section "Error Codes".
         */
    api_ret = zmod4xxx_check_error_event(&dev);
    switch (api_ret) {
      case ERROR_POR_EVENT:
        Serial.print(F("Error "));
        Serial.print(api_ret);
        Serial.println(F("Measurement completion fault. Unexpected sensor reset.\n"));
        break;
      case ZMOD4XXX_OK:
        Serial.print(F("Error "));
        Serial.print(api_ret);
        Serial.println(F("Measurement completion fault. Wrong sensor setup.\n"));
        break;
      default:
        Serial.print(F("Error "));
        Serial.print(api_ret);
        Serial.println(F("Error during reading status register "));
        break;
    }
    error_handle();
  }

  /* Read sensor ADC output. */
  api_ret = zmod4xxx_read_adc_result(&dev, adc_result);
  if (api_ret) {
    Serial.print(F("Error "));
    Serial.print(api_ret);
    Serial.println(F("Error %d during read of ADC results, exiting program!\n"));
    error_handle();
  }

  /*
     * Check validity of the ADC results. For more information, read the
     * Programming Manual, section "Error Codes".
     */
  api_ret = zmod4xxx_check_error_event(&dev);
  if (api_ret) {
    Serial.print(F("Error "));
    Serial.print(api_ret);
    Serial.println(F("during reading status register, exiting program!\n"));
    error_handle();
  }


  /*
     * Assign algorithm inputs: raw sensor data and ambient conditions.
     * Production code should use measured temperature and humidity values.
     */
  algo_input.adc_result = adc_result;
  algo_input.humidity_pct = 50.0;
  algo_input.temperature_degc = 20.0;
  /* Calculate algorithm results */
  lib_ret = calc_iaq_2nd_gen(&algo_handle, &dev, &algo_input, &algo_results);

  // if ((lib_ret != IAQ_2ND_GEN_OK) && (lib_ret != IAQ_2ND_GEN_STABILIZATION)) {
  //   Serial.println(F("Error when calculating algorithm, exiting program!"));
  // } else {
    Serial.println(F("*********** Measurements ***********"));
    for (int i = 0; i < 13; i++) {
      Serial.print(F(" Rmox["));
      Serial.print(i);
      Serial.print(F("] = "));
      Serial.print(algo_results.rmox[i] / 1e3);
      Serial.println(F(" kOhm"));
    }
    Serial.print(F(" Rcda = "));
    Serial.print(pow(10, algo_results.log_rcda) / 1e3);
    Serial.println(F(" kOhm"));
    Serial.print(F(" EtOH = "));
    Serial.print(algo_results.etoh);
    Serial.println(F(" ppm"));
    Serial.print(F(" TVOC = "));
    Serial.print(algo_results.tvoc);
    Serial.println(F(" mg/m^3"));
    Serial.print(F(" eCO2 = "));
    Serial.print(algo_results.eco2);
    Serial.println(F(" ppm"));
    Serial.print(F(" IAQ  = "));
    Serial.println(algo_results.iaq);
    Serial.println(count++);         // print out the count variable to see how many times has the loop ran
    switch (lib_ret) {
      case IAQ_2ND_GEN_STABILIZATION:
        /* The sensor should run for at least 100 cycles to stabilize.
             * Algorithm results obtained during this period SHOULD NOT be
             * considered as valid outputs! */
        Serial.println(F("Warm-Up!"));
        break;
      case IAQ_2ND_GEN_OK:
        Serial.println(F("Valid!"));
        break;
      case IAQ_2ND_GEN_DAMAGE:
        Serial.println(F("Error: Sensor probably damaged. Algorithm results may be incorrect."));
        break;
      default: /* other errors */
        Serial.println(F("Unexpected Error during algorithm calculation: Exiting Program."));
        error_handle();
    // }
  }
}

void error_handle() {
  while (1)
    ;
}
