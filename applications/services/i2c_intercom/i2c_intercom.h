#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_I2C_INTERCOM "i2c_intercom"

typedef struct I2cIntercom I2cIntercom;

void i2c_intercom_setup_end(I2cIntercom* instance);

#ifdef __cplusplus
}
#endif
