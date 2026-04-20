#include "bq28z620.h"
#include "bq28z620_i.h"
#include <furi.h>

#include <furi_hal_i2c.h>
#define TAG "Bq28z620SubCmd"

typedef struct {
    uint16_t sub_cmd;
    uint8_t response[32];
    uint8_t crc;
    uint8_t length;
} Bq28z620SubCmdResponse;
_Static_assert(sizeof(Bq28z620SubCmdResponse) == 36, "Size check for 'Bq28z620SubCmdResponse' failed.");

static Bq28z620Status bq28z620_check_status(int stataus) {
    Bq28z620Status ret = Bq28z620StatusUnknown;
    if(stataus >= PICO_OK) {
        ret = Bq28z620StatusOk;
    } else if(stataus == PICO_ERROR_GENERIC) {
        ret = Bq28z620StatusError;
    } else if(stataus == PICO_ERROR_TIMEOUT) {
        ret = Bq28z620StatusTimeout;
    } else {
        ret = Bq28z620StatusUnknown;
    }

    return ret;
}

static uint8_t bq28z620_calculate_crc(const uint8_t* data, size_t length) {
    uint8_t crc = 0;
    for(size_t i = 0; i < length; i++) {
        crc += data[i];
    }
    return ~crc;
}

static Bq28z620Status bq28z620_sub_cmd_set(Bq28z620* instance, Bq28z620SubCmd sub_cmd) {
    furi_check(instance);

    furi_hal_i2c_acquire(instance->i2c_handle);
    uint8_t data_tx[3] = {Bq28z620StdCmdMACSubcmd, sub_cmd & 0xFF, (sub_cmd >> 8) & 0xFF};
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, data_tx, sizeof(data_tx), FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret < PICO_OK) {
        FURI_LOG_E(TAG, "Failed to send sub cmd 0x%02X", sub_cmd);
    } else {
#ifdef BQ28Z620_DEBUG_ENABLE
        BQ28Z620_DEBUG(TAG, "Sent sub cmd 0x%02X", sub_cmd);
#endif
    }
    return bq28z620_check_status(ret);
}

static Bq28z620Status bq28z620_sub_cmd_read(Bq28z620* instance, Bq28z620SubCmd sub_cmd, Bq28z620SubCmdResponse* response, uint32_t timeout_ms) {
    furi_check(instance);

    furi_hal_i2c_acquire(instance->i2c_handle);
    uint8_t data_tx[3] = {Bq28z620StdCmdMACSubcmd, sub_cmd & 0xFF, (sub_cmd >> 8) & 0xFF};
    int ret = PICO_ERROR_GENERIC;
    do {
        ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, data_tx, sizeof(data_tx), FURI_HAL_I2C_TIMEOUT_US);
        if(ret < PICO_OK) {
#ifdef BQ28Z620_DEBUG_ENABLE
            BQ28Z620_DEBUG(TAG, "Sent sub cmd 0x%02X", sub_cmd);
            BQ28Z620_DEBUG(TAG, "Response: %d bytes", sizeof(*response));
            for(size_t i = 0; i < sizeof(*response); i++) {
                BQ28Z620_DEBUG(TAG, "  %02X: %02X", i, ((uint8_t*)response)[i]);
            }
#endif
            break;
        }

        furi_delay_ms(timeout_ms); // The delay required for the chip to process the request

        uint8_t sub_cmd_reg = Bq28z620StdCmdMACSubcmd;
        ret = furi_hal_i2c_master_trx_blocking(
            instance->i2c_handle, instance->address, &sub_cmd_reg, 1, (uint8_t*)response, sizeof(*response), FURI_HAL_I2C_TIMEOUT_US);

    } while(0);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret < PICO_OK) {
        FURI_LOG_E(TAG, "Failed to send sub cmd 0x%02X", sub_cmd);
        return bq28z620_check_status(ret);
    } else {
#ifdef BQ28Z620_DEBUG_ENABLE
        BQ28Z620_DEBUG(TAG, "Sent sub cmd 0x%02X", sub_cmd);
        BQ28Z620_DEBUG(TAG, "Response: %d bytes", sizeof(*response));
        for(size_t i = 0; i < sizeof(*response); i++) {
            BQ28Z620_DEBUG(TAG, "  %02X: %02X", i, ((uint8_t*)response)[i]);
        }

#endif
        if(response->sub_cmd != sub_cmd) {
            FURI_LOG_E(TAG, "Sub cmd mismatch: sent 0x%02X, received 0x%02X", sub_cmd, response->sub_cmd);
            return Bq28z620StatusSubCmdError;
        } else {
            uint8_t calculated_crc = bq28z620_calculate_crc((uint8_t*)response, response->length - 2); // Exclude CRC and length bytes
            if(calculated_crc != response->crc) {
                FURI_LOG_E(TAG, "CRC mismatch: calculated 0x%02X, received 0x%02X", calculated_crc, response->crc);
                return Bq28z620StatusCrcError;
            }
        }
    }
    return Bq28z620StatusOk;
}

/* https://e2e.ti.com/support/power-management-group/power-management/f/power-management-forum/738029/bq28z610-data-flash-access-issues/2726302#2726302
    
    Okay it's finally working. I think TI should revise their documents since the described procedure in 12.2.45 is simply incorrect.

    For anyone else struggling with write to data flash here is the procedure:

    Write to 0x3E ( AltManufacturingAccess) the MAC address (little endian) followed by the data to write ( Note: Minimum data is 1 and maximum is 32 )
    Write to 0x60 ( MacDataChecksum ) the checksum calculated as per below
    Write to 0x61 ( MacDataLength ) the total number of bytes written including the MAC address, data bytes, checksum, and MACDataLength itself
    Read back from MAC address to verify (Write address to read to 0x3E and read back desired number of bytes)
    Special condition: If writing in chunks of 32 bytes step 1, 2, and 3 can be combined into a single write since the addresses mentioned above are contiguous. For example if writing all data to 0x3E the MAC address will write to 0x3E and 0x3F, the 32 bytes of data will fill 0x40-0x5F and the checksum and data length will flow into 0x60 and 0x61.

    Standard procedure example (Write [0x1, 0x2, 0x3, 0x4] to MAC address 0x4700 ):
    [ Start (W) ] 0x3E 0x00 0x47 0x01 0x02 0x03 0x04 [ Stop ]
    [ Start (W) ] 0x60 0xAE 0x08 [ Stop ]
    [ Start (W) ] 0x3E 0x00 0x47 [ Start (R) ] ( read number of bytes ) [ Stop ]

    Special Condition Example ( Write 32 bytes to 0x4700 ):
    [ Start (W) ] 0x3E 0x00 0x47 0xB4 0x2A 0x00 0x00 0x00 0x00 0x02 0x13 0x6D 0x02 0xBF 0x47 0xC1 0x47 0x65 0x01 0x02 0x92 0x6D 0x02 0xC2 0x47 0xC4 0x47 0x66 0x01 0x04 0x12 0x6D 0x02 0xC5 0x47 0xD3 0x24 [ Stop ]
    [ Start (W) ] 0x3E 0x00 0x47 [ Start (R) ] ( read 32 bytes ) [ Stop ]
    0x3E= 0x00
    0x3F = 0x47
    0x40-0x5F = 0xB4-0x47
    0x60 ( Checksum ) = 0xD3
    0x61 ( Total Length) = 0x24
    Checksum Calculation:

    Checksum = 0xFF - LSB( ADDR1 + ADDR2 + D1 + D2... + Dn )
*/
static Bq28z620Status bq28z620_sub_cmd_write(Bq28z620* instance, Bq28z620SubCmd sub_cmd, uint8_t* data, size_t data_length) {
    furi_check(instance);
    furi_check(data);
    furi_check(data_length <= 32);
    uint8_t* data_tx = malloc(data_length + 1 + 2); // 1 std_cmd, 2 bytes for sub_cmd
    data_tx[0] = Bq28z620StdCmdMACSubcmd;
    data_tx[1] = sub_cmd & 0xFF;
    data_tx[2] = (sub_cmd >> 8) & 0xFF;
    memcpy(&data_tx[3], data, data_length);

    uint8_t crc = bq28z620_calculate_crc(&data_tx[1], data_length + 2);
    uint8_t length = data_length + 2 + 2; // 2 bytes for sub_cmd + 1 bytes crc + 1data_length

    furi_hal_i2c_acquire(instance->i2c_handle);

    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, data_tx, data_length + 3, FURI_HAL_I2C_TIMEOUT_US);

    data_tx[0] = Bq28z620StdCmdMACDataSum;
    data_tx[1] = crc;
    data_tx[2] = length;

    ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, data_tx, 3, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret < PICO_OK) {
        FURI_LOG_E(TAG, "Failed to send sub cmd 0x%02X", sub_cmd);
    } else {
#ifdef BQ28Z620_DEBUG_ENABLE
        BQ28Z620_DEBUG(TAG, "Sent sub cmd 0x%02X with data:", sub_cmd);
        for(size_t i = 0; i < data_length + 3; i++) {
            BQ28Z620_DEBUG(TAG, "  %02X: %02X", i, data_tx[i]);
        }
#endif
    }
    free(data_tx);
    return bq28z620_check_status(ret);
}

Bq28z620Status bq28z620_get_device_type(Bq28z620* instance, Bq28z620MacSubcmdDeviceTypeRegBits* device_type) {
    furi_check(instance);
    furi_check(device_type);

    Bq28z620SubCmdResponse resp = {0};

    Bq28z620Status status = bq28z620_sub_cmd_read(instance, Bq28z620MacSubcmdDeviceType, &resp, 0);
    if(status == Bq28z620StatusOk) {
        *device_type = *(Bq28z620MacSubcmdDeviceTypeRegBits*)resp.response;
        BQ28Z620_DEBUG(TAG, "Raw DeviceType reg: %u, Device type: %u", device_type->device_type, device_type->device_type);
    } else {
        FURI_LOG_E(TAG, "Failed to get device type");
    }

    return status;
}

Bq28z620Status bq28z620_get_device_name(Bq28z620* instance, Bq28z620MacSubcmdDeviceNameRegBits* device_name) {
    furi_check(instance);
    furi_check(device_name);

    Bq28z620SubCmdResponse resp = {0};

    Bq28z620Status status = bq28z620_sub_cmd_read(instance, Bq28z620MacSubcmdManufacturerName, &resp, 0);
    if(status == Bq28z620StatusOk) {
        *device_name = *(Bq28z620MacSubcmdDeviceNameRegBits*)resp.response;
        BQ28Z620_DEBUG(TAG, "Raw DeviceName reg: %u, Device name: %s", Bq28z620MacSubcmdDeviceName, device_name->device_name);
    } else {
        FURI_LOG_E(TAG, "Failed to get device name");
    }

    return status;
}

Bq28z620Status bq28z620_get_operation_status(Bq28z620* instance, Bq28z620MacSubcmdOperationStatusRegBits* operation_status) {
    furi_check(instance);
    furi_check(operation_status);

    Bq28z620SubCmdResponse resp = {0};

    Bq28z620Status status = bq28z620_sub_cmd_read(instance, Bq28z620MacSubcmdOperationStatus, &resp, 0);
    if(status == Bq28z620StatusOk) {
        //*operation_status = *(Bq28z620MacSubcmdOperationStatusRegBits*)resp.response;
        memcpy(operation_status, resp.response, sizeof(*operation_status));
        BQ28Z620_DEBUG(
            TAG,
            "Raw OperationStatus reg: 0x%04X, Operation status: 0x%08lX,"
            " emshut: %d, cb: %d, slpcc: %d, slpad: %d, smbcal: %d,"
            " init: %d, sleepm: %d, xl: %d, cal_offset: %d, cal: %d,"
            " authcalm: %d, auth: %d, sdm: %d, sleep: %d, sec13_14: %u, pf: %d, ss: %d, sdv: %d, sec8_9: %u,"
            " dsg: %d, chg: %d",
            Bq28z620MacSubcmdOperationStatus,
            *(uint32_t*)resp.response,
            operation_status->emshut,
            operation_status->cb,
            operation_status->slpcc,
            operation_status->slpad,
            operation_status->smbcal,
            operation_status->init,
            operation_status->sleepm,
            operation_status->xl,
            operation_status->cal_offset,
            operation_status->cal,
            operation_status->authcalm,
            operation_status->auth,
            operation_status->sdm,
            operation_status->sleep,
            operation_status->sec13_14,
            operation_status->pf,
            operation_status->ss,
            operation_status->sdv,
            operation_status->sec8_9,
            operation_status->dsg,
            operation_status->chg);
    } else {
        FURI_LOG_E(TAG, "Failed to get operation status");
    }

    return status;
}

Bq28z620Status bq28z620_get_chemical_id(Bq28z620* instance, Bq28z620MacSubcmdChemicalIDRegBits* chemical_id) {
    furi_check(instance);
    furi_check(chemical_id);

    Bq28z620SubCmdResponse resp = {0};

    Bq28z620Status status = bq28z620_sub_cmd_read(instance, Bq28z620MacSubcmdChemID, &resp, 0);
    if(status == Bq28z620StatusOk) {
        *chemical_id = *(Bq28z620MacSubcmdChemicalIDRegBits*)resp.response;
        BQ28Z620_DEBUG(TAG, "Raw ChemicalID reg: 0x%02X, Chemical ID: 0x%02X", Bq28z620MacSubcmdChemID, chemical_id->chemical_id);
    } else {
        FURI_LOG_E(TAG, "Failed to get chemical ID");
    }

    return status;
}

Bq28z620Status bq28z620_reset(Bq28z620* instance) {
    furi_check(instance);

    Bq28z620Status status = bq28z620_sub_cmd_set(instance, Bq28z620MacSubcmdReset);

    return status;
}

Bq28z620Status bq28z620_get_i2c_configuration(Bq28z620* instance, uint8_t* i2c_configuration) {
    furi_check(instance);
    furi_check(i2c_configuration);

    Bq28z620SubCmdResponse resp = {0};

    Bq28z620Status status = bq28z620_sub_cmd_read(instance, 0x4602, &resp, 0);
    if(status == Bq28z620StatusOk) {
        *i2c_configuration = resp.response[0];
        BQ28Z620_DEBUG(TAG, "Raw I2C configuration reg: 0x%02X, I2C configuration: 0x%02X", 0x4602, *i2c_configuration);
    } else {
        FURI_LOG_E(TAG, "Failed to get I2C configuration");
    }

    return status;
}

Bq28z620Status bq28z620_set_i2c_configuration(Bq28z620* instance, uint8_t* i2c_configuration) {
    furi_check(instance);
    furi_check(i2c_configuration);

    Bq28z620Status status = bq28z620_sub_cmd_write(instance, 0x4602, i2c_configuration, 1);
    if(status == Bq28z620StatusOk) {
        BQ28Z620_DEBUG(TAG, "Set I2C configuration to: 0x%02X", i2c_configuration[0]);
    } else {
        FURI_LOG_E(TAG, "Failed to set I2C configuration");
    }

    return status;
}
