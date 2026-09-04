#include "furi_bsp_saradc.h"

#include <furi.h>
#include <furi_bsp.h>
#include <furi_hal_resources.h>
#include <furi_hal_pwm.h>

#define TAG "FuriBspSaradc"

#define SARADC_PWM_RESOLUTION 12 // 12-bit PWM for SARADC
#define SARADC_PWM_FREQ_HZ    2000 // 2kHz PWM for SARADC
#define SARADC_DEFAULT_ID     FuriBspSaradcId1

typedef struct {
    FuriHalPwm* pwm_saradc;
    FuriBspSaradcId current_id;
} FuriBspSaradc;

const uint16_t saradc_id_pwm[FuriBspSaradcIdMax] = {
    [FuriBspSaradcId1] = 0,
    [FuriBspSaradcId2] = 416,
    [FuriBspSaradcId3] = 816,
    [FuriBspSaradcId4] = 1231,
    [FuriBspSaradcId5] = 1658,
    [FuriBspSaradcId6] = 2048,
    [FuriBspSaradcId7] = 2437,
    [FuriBspSaradcId8] = 2862,
    [FuriBspSaradcId9] = 3279,
    [FuriBspSaradcId10] = 3680,
    [FuriBspSaradcId11] = 4095,
};

static FuriBspSaradc* furi_bsp_saradc_instance = NULL;

void furi_bsp_saradc_alloc(void) {
    furi_check(furi_bsp_saradc_instance == NULL);
    furi_bsp_saradc_instance = malloc(sizeof(FuriBspSaradc));
    furi_bsp_saradc_instance->pwm_saradc = furi_hal_pwm_init(&gpio_cpu_adc_in1_boot, SARADC_PWM_RESOLUTION, SARADC_PWM_FREQ_HZ, false);
    furi_hal_pwm_set_duty_cycle(furi_bsp_saradc_instance->pwm_saradc, 0);
    furi_bsp_saradc_instance->current_id = SARADC_DEFAULT_ID;
    FURI_LOG_I(
        TAG,
        "SARADC PWM initialized on pin %d with frequency %d Hz and resolution %d bits",
        gpio_cpu_adc_in1_boot.pin,
        SARADC_PWM_FREQ_HZ,
        SARADC_PWM_RESOLUTION);
}

void furi_bsp_saradc_free(void) {
    furi_check(furi_bsp_saradc_instance != NULL);
    furi_hal_pwm_deinit(furi_bsp_saradc_instance->pwm_saradc);
    free(furi_bsp_saradc_instance);
    furi_bsp_saradc_instance = NULL;
    FURI_LOG_I(TAG, "SARADC PWM deinitialized");
}

void furi_bsp_saradc_set_id(FuriBspSaradcId id) {
    furi_check(furi_bsp_saradc_instance != NULL);
    furi_hal_pwm_set_duty_cycle(furi_bsp_saradc_instance->pwm_saradc, saradc_id_pwm[id]);
    furi_bsp_saradc_instance->current_id = id;

    FURI_LOG_I(TAG, "SARADC ID set to %d with PWM duty cycle %d", id, saradc_id_pwm[id]);
}

FuriBspSaradcId furi_bsp_saradc_get_id(void) {
    furi_check(furi_bsp_saradc_instance != NULL);
    return furi_bsp_saradc_instance->current_id;
}
