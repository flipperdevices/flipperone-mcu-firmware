#pragma once
#include <furi.h>
#include <furi_hal_gpio.h>
#include <pico.h>
#include <pico/time.h>

typedef struct FuriHalI2cBus FuriHalI2cBus;
typedef struct FuriHalI2cBusHandle FuriHalI2cBusHandle;

/** FuriHal i2c bus states */
typedef enum {
    FuriHalI2cBusEventInit, /**< Bus initialization event, called on system start */
    FuriHalI2cBusEventDeinit, /**< Bus deinitialization event, called on system stop */
    FuriHalI2cBusEventLock, /**< Bus lock event, called before activation */
    FuriHalI2cBusEventUnlock, /**< Bus unlock event, called after deactivation */
    FuriHalI2cBusEventActivate, /**< Bus activation event, called before handle activation */
    FuriHalI2cBusEventDeactivate, /**< Bus deactivation event, called after handle deactivation  */
} FuriHalI2cBusEvent;

typedef enum {
    FuriHalI2cBusSlaveEventStart, /**< Slave start event, called when master sends a start signal. */
    FuriHalI2cBusSlaveEventReceive, /**< Slave write event, called when master wants to write data from slave. */
    FuriHalI2cBusSlaveEventRequest, /**< Slave read event, called when master wants to read data from slave. */
    FuriHalI2cBusSlaveEventRepeatedStart, /**< Slave repeated start event, called when master sends a repeated start signal. */
    FuriHalI2cBusSlaveEventStop, /**< Slave stop event, called when master finishes transaction with slave. */
} FuriHalI2cBusSlaveEvent;

/** FuriHal i2c bus event callback */
typedef void (*FuriHalI2cBusEventCallback)(FuriHalI2cBus* bus, FuriHalI2cBusEvent event);

/** FuriHal i2c bus write callback */
typedef int (*FuriHalI2cBusWriteCallback)(void* instance, uint8_t addr, const uint8_t* src, size_t len, bool nostop, absolute_time_t until);

/** FuriHal i2c bus read callback */
typedef int (*FuriHalI2cBusReadCallback)(void* instance, uint8_t addr, uint8_t* rxbuf, uint len, bool nostop, absolute_time_t until);

/** FuriHal i2c handle states */
typedef enum {
    FuriHalI2cBusHandleEventActivate, /**< Handle activate: connect gpio and apply bus config */
    FuriHalI2cBusHandleEventDeactivate, /**< Handle deactivate: disconnect gpio and reset bus config */
} FuriHalI2cBusHandleEvent;

/** FuriHal i2c handle event callback */
typedef void (*FuriHalI2cBusHandleEventCallback)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusHandleEvent event);

/** FuriHal i2c bus slave callback */
typedef void (*FuriHalI2cBusSlaveCallback)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusSlaveEvent event, void* context);

/** FuriHal i2c bus slave write callback */
typedef uint8_t (*FuriHalI2cBusSlaveWriteCallback)(const FuriHalI2cBusHandle* handle, uint8_t* data, size_t size);

/** FuriHal i2c bus slave read callback */
typedef uint8_t (*FuriHalI2cBusSlaveReadCallback)(const FuriHalI2cBusHandle* handle, uint8_t* data, size_t size);

//** FuriHal reset i2c bus slave callback */
typedef void (*FuriHalI2cBusSlaveResetCallback)(const FuriHalI2cBusHandle* handle);

/** FuriHal i2c handle */
struct FuriHalI2cBusHandle {
    FuriHalI2cBus* bus;
    FuriHalI2cBusHandleEventCallback callback;
};

/** FuriHal i2c bus API */
typedef struct {
    FuriHalI2cBusEventCallback event;
    union {
        struct {
            FuriHalI2cBusReadCallback read_blocking;
            FuriHalI2cBusWriteCallback write_blocking;
        } master;
        struct {
            FuriHalI2cBusSlaveWriteCallback write_blocking;
            FuriHalI2cBusSlaveReadCallback read_blocking;
            FuriHalI2cBusSlaveResetCallback bus_reset;
            FuriHalI2cBusSlaveCallback callback;
            void* context;
        } slave;
    };

} FuriHalI2cBusAPI;

typedef enum {
    FuriHalI2cModeMaster,
    FuriHalI2cModeSlave,
} FuriHalI2cMode;

/** FuriHal i2c bus */
struct FuriHalI2cBus {
    void* data;
    const char* name;
    const FuriHalI2cBusHandle* current_handle;
    const GpioPin* sda;
    const GpioPin* scl;
    const FuriHalI2cMode mode;
    FuriMutex* mutex;
    FuriHalI2cBusAPI api;
};
