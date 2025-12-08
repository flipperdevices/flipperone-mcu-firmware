/**
 * @brief Регистры микросхемы INA219
 *
 * Перечисление адресов регистров INA219 (0x00–0x05) и краткое назначение каждого из них.
 *
 * - ina219_reg_config (0x00)
 *   Конфигурационный регистр (16 бит, RW). Содержит биты сброса, выбор диапазона шунта (BRNG),
 *   коэффициент усиления (PGA), режимы и разрешение/усреднение АЦП для шунта и шины.
 *   Используется для задания режима работы и параметров измерений.
 *
 * - ina219_reg_shunt_voltage (0x01)
 *   Регистр напряжения на шунте (16 бит, R). Знаковое (two's complement) значение падения
 *   напряжения на шунтовом резисторе. Типичное LSB = 10 µV (реальное разрешение определяется даташитом).
 *   Для получения тока измеренное значение делится на сопротивление шунта.
 *
 * - ina219_reg_bus_voltage (0x02)
 *   Регистр напряжения шины (16 бит, R). Содержит измеренное напряжение шины в старших битах
 *   (LSB = 4 mV в типичном режиме). Низкоуровневые биты включают флаги статуса (например,
 *   Conversion Ready, Math Overflow и т.п.). Формат и флаги описаны в даташите.
 *
 * - ina219_reg_power (0x03)
 *   Регистр мощности (16 бит, R). Беззнаковое значение активной мощности, вычисляемое внутренне
 *   микросхемой на основе калибровки и измеренных тока/напряжения. LSB мощности зависит от
 *   установленного значения в регистре калибровки (обычно = 20 * Current_LSB).
 *
 * - ina219_reg_current (0x04)
 *   Регистр тока (16 бит, R). Знаковое значение тока; LSB (Current_LSB) определяется значением
 *   регистра калибровки. Для корректных значений перед чтением требуется установить соответствующую
 *   калибровку в регистре calibration.
 *
 * - ina219_reg_calibration (0x05)
 *   Регистр калибровки (16 бит, RW). Значение, используемое для масштабирования вычислений тока и мощности.
 *   Определяет Current_LSB и Power_LSB и тем самым диапазон/разрешение измерений; требует расчёта
 *   в соответствии с выбранным шунтом и требуемой точностью.
 *
 * Примечания:
 * - Перед измерениями тока и мощности убедитесь, что регистра calibration установлена корректно.
 * - Подробные форматы битов, расчёты LSB и поведение флагов — в официальном даташите INA219.
 */
#pragma once

#include <stdint.h>
//https://ti.com/lit/ds/symlink/ina219.pdf?ts=1764919130911&ref_url=https%253A%252F%252Feu.mouser.com%252F

/* clang-format off */




typedef enum {
    Ina219RegConfig         = 0x00,     /**< Configuration Register */
    Ina219RegShuntVoltage   = 0x01,     /**< Shunt Voltage Register */
    Ina219RegBusVoltage     = 0x02,     /**< Bus Voltage Register */
    Ina219RegPower          = 0x03,     /**< Power Register */
    Ina219RegCurrent        = 0x04,     /**< Current Register */
    Ina219RegCalibration    = 0x05      /**< Calibration Register */
} Ina219Reg;

typedef struct {
    uint8_t mode            : 3;        // Operating Mode (0b000 - Power-Down; 0b001 - Shunt Voltage triggered; 
                                        // 0b010 - Bus Voltage, triggered; 0b011 - Shunt and Bus, Voltage triggered; 0b100 - ADC off (disabled); 
                                        // 0b101 - Shunt Voltage, continuous; 0b110 - Bus Voltage, continuous; 0b111 - Shunt and Bus Voltage, continuous)
    uint8_t sadc            : 4;        // SADC Shunt ADC Resolution/Averaging (0b0000 - 9-bit, 84us; 0b0001 - 10-bit, 148us; 0b0010 - 11-bit, 276us; 0b0011 - 12-bit, 532us; 
                                        // 0b01000 - 12-bit, 532us;  0b1001 - 2 samples averaged, 1.06ms; 0b1010 - 4 samples averaged, 2.13ms; 0b1011 - 8 samples averaged, 4.26ms;
                                        // 0b1100 - 16 samples averaged, 8.51ms; 0b1101 - 32 samples averaged, 17.02ms; 0b1110 - 64 samples averaged, 34.05ms; 0b1111 - 128 samples averaged, 68.10ms)
    uint8_t badc            : 4;        // These bits adjust the Bus ADC resolution (9-; 10-; 11-; or 12-bit) or set the number of samples used when
                                        // averaging results for the Bus Voltage Register (02h). Same encoding as SADC.
    uint8_t pg              : 2;        // Programmable Gain Amplifier Configuration (0b00 - Gain 1, 40mV Range; 0b01 - Gain /2, 80mV Range; 0b10 - Gain /4, 160mV Range; 0b11 - Gain /8, 320mV Range)
    uint8_t brng            : 1;        // Bus Voltage Range (0 = 16V; 1 = 32V(default value))
    uint8_t reserved        : 1;        // Reserved
    uint8_t rst             : 1;        // Reset Bit (1 = Reset INA219, all configuration registers are set to default values)
} Ina219ConfigRegBits;

typedef struct {
    uint8_t ovf             : 1;        // Math Overflow Flag (1 = Overflow occurred)
    uint8_t cnvr            : 1;        // Conversion Ready Flag (1 = Conversion complete)
    uint8_t reserved        : 1;        // Reserved
    uint16_t bus_voltage    : 13;       // Bus Voltage Value (LSB = 4mV)
} Ina219BusVoltageRegBits;

/* clang-format on */