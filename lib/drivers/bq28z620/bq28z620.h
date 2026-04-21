#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>
#include "bq28z620_reg.h"

#define BQ28Z620_ADDRESS 0x55

typedef struct Bq28z620 Bq28z620;

typedef enum {
    Bq28z620StatusOk = 0,
    Bq28z620StatusRxEmpty,
    Bq28z620StatusTxEmpty,
    Bq28z620StatusError = -1,
    Bq28z620StatusTimeout = -2,
    Bq28z620StatusUnknown = -3,
} Bq28z620Status;

#ifdef __cplusplus
extern "C" {
#endif

/**
    * @brief Initialize the Bq28z620 instance with the given I2C bus handle and device address.
    * @param i2c_handle Pointer to the I2C bus handle to use for communication.
    * @param address I2C address of the Bq28z620 device (default is 0x55).
    * @return Pointer to the initialized Bq28z620 instance, or NULL if initialization failed.
    */
Bq28z620* bq28z620_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address);

/**
    * @brief Deinitialize the Bq28z620 instance and free any allocated resources.
    * @param instance Pointer to the Bq28z620 instance to deinitialize.
    */
void bq28z620_deinit(Bq28z620* instance);

/**
    * @brief Get the control status of the battery, including flags like Qmax, VOK, R_DIS, LDMD, 
    *          checksum valid, authcalm and security mode.
    * @param instance Pointer to the Bq28z620 instance.
    * @param control_status Pointer to a Bq28z620StdCmdControlStatusRegBits structure to store the control status.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_control_status(Bq28z620* instance, Bq28z620StdCmdControlStatusRegBits* control_status);

/**
    * @brief Get the estimated time to empty of the battery in minutes.
    * @param instance Pointer to the Bq28z620 instance.
    * @param time_to_empty Pointer to a uint16_t variable to store the estimated time to empty.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_time_to_empty(Bq28z620* instance, uint16_t* time_to_empty);

/**
    * @brief Get the temperature of the battery in degrees Celsius.
    * @param instance Pointer to the Bq28z620 instance.
    * @param temperature Pointer to a float variable to store the temperature.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_temperature(Bq28z620* instance, float* temperature);

/**
    * @brief Get the voltage of the battery in volts.
    * @param instance Pointer to the Bq28z620 instance.
    * @param voltage Pointer to a float variable to store the voltage.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_voltage(Bq28z620* instance, float* voltage);

/**
    * @brief Get the battery status, including error code, fully discharged, fully charged, discharging,
                initialization, remaining time alarm, remaining capacity alarm, 
                terminate discharge alarm, overtemperature alarm, terminate charge alarm and overcharged alarm.
    * @param instance Pointer to the Bq28z620 instance.
    * @param battery_status Pointer to a Bq28z620StdCmdBatteryStatusRegBits structure to store the battery status.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_battery_status(Bq28z620* instance, Bq28z620StdCmdBatteryStatusRegBits* battery_status);

/**
    * @brief Get the current of the battery in mA. Positive for charge, negative for discharge.
    * @param instance Pointer to the Bq28z620 instance.
    * @param current Pointer to an int16_t variable to store the current.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_current(Bq28z620* instance, int16_t* current);

/**
    * @brief Get the remaining capacity of the battery in mAh.
    * @param instance Pointer to the Bq28z620 instance.
    * @param remaining_capacity Pointer to a uint16_t variable to store the remaining capacity.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_remaining_capacity(Bq28z620* instance, uint16_t* remaining_capacity);

/**
    * @brief Get the full charge capacity of the battery in mAh.
    * @param instance Pointer to the Bq28z620 instance.
    * @param full_charge_capacity Pointer to a uint16_t variable to store the full charge capacity.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_full_charge_capacity(Bq28z620* instance, uint16_t* full_charge_capacity);

/**
    * @brief Get the average current of the battery in mA. Positive for charge, negative for discharge.
    * @param instance Pointer to the Bq28z620 instance.
    * @param average_current Pointer to an int16_t variable to store the average current.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_average_current(Bq28z620* instance, int16_t* average_current);

/**
    * @brief Get the average time to empty of the battery in minutes.
    * @param instance Pointer to the Bq28z620 instance.
    * @param average_time_to_empty Pointer to a uint16_t variable to store the average time to empty.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_average_time_to_empty(Bq28z620* instance, uint16_t* average_time_to_empty);

/**
    * @brief Get the average time to full charge of the battery in minutes.
    * @param instance Pointer to the Bq28z620 instance.
    * @param average_time_to_full Pointer to a uint16_t variable to store the average time to full charge.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_average_time_to_full(Bq28z620* instance, uint16_t* average_time_to_full);

/**
    * @brief Get the standby current of the battery in mA. Positive for charge, negative for discharge.
    * @param instance Pointer to the Bq28z620 instance.
    * @param standby_current Pointer to an int16_t variable to store the standby current.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_standby_current(Bq28z620* instance, int16_t* standby_current);

/**
    * @brief Get the standby time to empty of the battery in minutes.
    * @param instance Pointer to the Bq28z620 instance.
    * @param standby_time_to_empty Pointer to a uint16_t variable to store the standby time to empty.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_standby_time_to_empty(Bq28z620* instance, uint16_t* standby_time_to_empty);

/**
    * @brief Get the max load current of the battery in mA. Positive for charge, negative for discharge.
    * @param instance Pointer to the Bq28z620 instance.
    * @param max_load_current Pointer to an int16_t variable to store the max load current.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_max_load_current(Bq28z620* instance, int16_t* max_load_current);

/**
    * @brief Get the max load time to empty of the battery in minutes.
    * @param instance Pointer to the Bq28z620 instance.
    * @param max_load_time_to_empty Pointer to a uint16_t variable to store the max load time to empty.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_max_load_time_to_empty(Bq28z620* instance, uint16_t* max_load_time_to_empty);

/**
    * @brief Get the average power of the battery in mW. Positive for charge, negative for discharge.
    * @param instance Pointer to the Bq28z620 instance.
    * @param average_power Pointer to an int16_t variable to store the average power.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_average_power(Bq28z620* instance, int16_t* average_power);

/**
    * @brief Get the internal temperature of the battery in degrees Celsius.
    * @param instance Pointer to the Bq28z620 instance.
    * @param internal_temperature Pointer to a float variable to store the internal temperature.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_internal_temperature(Bq28z620* instance, float* internal_temperature);

/**
    * @brief Get the cycle count of the battery.
    * @param instance Pointer to the Bq28z620 instance.
    * @param cycle_count Pointer to a uint16_t variable to store the cycle count.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_cycle_count(Bq28z620* instance, uint16_t* cycle_count);

/**
    * @brief Get the relative state of charge of the battery in percentage.
    * @param instance Pointer to the Bq28z620 instance.
    * @param relative_state_of_charge Pointer to a uint8_t variable to store the relative state of charge.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_relative_state_of_charge(Bq28z620* instance, uint8_t* relative_state_of_charge);

/**
    * @brief Get the state of health of the battery in percentage.
    * @param instance Pointer to the Bq28z620 instance.
    * @param state_of_health Pointer to a uint8_t variable to store the state of health.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_state_of_health(Bq28z620* instance, uint8_t* state_of_health);

/**
    * @brief Get the charging voltage of the battery in volts.
    * @param instance Pointer to the Bq28z620 instance.
    * @param charging_voltage Pointer to a float variable to store the charging voltage.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_charging_voltage(Bq28z620* instance, float* charging_voltage);

/**
    * @brief Get the charging current of the battery in mA. Positive for charge, negative for discharge.
    * @param instance Pointer to the Bq28z620 instance.
    * @param charging_current Pointer to an int16_t variable to store the charging current.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_charging_current(Bq28z620* instance, int16_t* charging_current);

/**
    * @brief Get the design capacity of the battery in mAh.
    * @param instance Pointer to the Bq28z620 instance.
    * @param design_capacity Pointer to a uint16_t variable to store the design capacity.
    * @return Bq28z620Status indicating the result of the operation.
    */
Bq28z620Status bq28z620_get_design_capacity(Bq28z620* instance, uint16_t* design_capacity);

#ifdef __cplusplus
}
#endif
