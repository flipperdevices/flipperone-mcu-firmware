#pragma once

#include <stdint.h>

typedef enum {
    FuriBspSaradcId1,
    FuriBspSaradcId2,
    FuriBspSaradcId3,
    FuriBspSaradcId4,
    FuriBspSaradcId5,
    FuriBspSaradcId6,
    FuriBspSaradcId7,
    FuriBspSaradcId8,
    FuriBspSaradcId9,
    FuriBspSaradcId10,
    FuriBspSaradcId11,
    FuriBspSaradcIdMax,
} FuriBspSaradcId;

#ifdef __cplusplus
extern "C" {
#endif

void furi_bsp_saradc_alloc(void);
void furi_bsp_saradc_free(void);
void furi_bsp_saradc_set_id(FuriBspSaradcId id);
FuriBspSaradcId furi_bsp_saradc_get_id(void);

#ifdef __cplusplus
}
#endif
