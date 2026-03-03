#pragma once

#include <hardware/i2c.h>
#include <drivers/i2c_master_pio/pio_i2c.h>

typedef struct FuriHalI2cBus FuriHalI2cBus;
typedef struct FuriHalI2cBusHandle FuriHalI2cBusHandle;

/**
 * I2C channels
 */
typedef enum {
    FuriHalI2cIdI2c0 = 0,
    FuriHalI2cIdI2c1,
    FuriHalI2cIdPio,

    FuriHalI2cIdMax,
} FuriHalI2cId;

typedef enum {
    FuriHalI2cPinSda,
    FuriHalI2cPinScl,

    FuriHalI2cPinMax,
} FuriHalI2cPin;

// typedef enum {
//     FuriHalI2cModeMaster,
//     FuriHalI2cModeSlave,
// } FuriHalI2cMode;

/** FuriHal i2c bus states */
typedef enum {
    FuriHalI2cBusEventInit, /**< Bus initialization event, called on system start */
    FuriHalI2cBusEventDeinit, /**< Bus deinitialization event, called on system stop */
    FuriHalI2cBusEventLock, /**< Bus lock event, called before activation */
    FuriHalI2cBusEventUnlock, /**< Bus unlock event, called after deactivation */
    FuriHalI2cBusEventActivate, /**< Bus activation event, called before handle activation */
    FuriHalI2cBusEventDeactivate, /**< Bus deactivation event, called after handle deactivation  */
} FuriHalI2cBusEvent;

/** FuriHal i2c bus event callback */
typedef void (*FuriHalI2cBusEventCallback)(FuriHalI2cBus* bus, FuriHalI2cBusEvent event);

/** FuriHal i2c handle states */
typedef enum {
    FuriHalI2cBusHandleEventActivate, /**< Handle activate: connect gpio and apply bus config */
    FuriHalI2cBusHandleEventDeactivate, /**< Handle deactivate: disconnect gpio and reset bus config */
} FuriHalI2cBusHandleEvent;

/** FuriHal i2c handle event callback */
typedef void (*FuriHalI2cBusHandleEventCallback)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusHandleEvent event);

/** FuriHal i2c handle */
struct FuriHalI2cBusHandle {
    FuriHalI2cBus* bus;
    FuriHalI2cBusHandleEventCallback callback;
};

/** FuriHal i2c bus */
struct FuriHalI2cBus {
    union {
        i2c_inst_t* i2c;
        I2cMasterPio* pio_i2c;
    } as;
    FuriHalI2cId id;
    const FuriHalI2cBusHandle* current_handle;
    FuriHalI2cBusEventCallback callback;
};
