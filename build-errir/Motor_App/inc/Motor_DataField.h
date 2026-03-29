
#ifndef MOTOR_DATAFIELD_H
#define MOTOR_DATAFIELD_H

#include "Motor_Model.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    char label[16];
    char value[24];
} motor_data_field_text_t;

void Motor_DataField_Format(motor_data_field_id_t field_id,
                            const motor_state_t *state,
                            motor_data_field_text_t *out_text);
const char *Motor_DataField_GetFieldName(motor_data_field_id_t field_id);
uint8_t Motor_DataField_GetCatalogCount(void);
motor_data_field_id_t Motor_DataField_GetByCatalogIndex(uint8_t index);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_DATAFIELD_H */
