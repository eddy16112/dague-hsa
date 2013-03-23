#ifndef PINS_PAPI_SELECT_H
#define PINS_PAPI_SELECT_H

#include "dague.h"
#include "dague_config.h"
#include "dague/mca/mca.h"
#include "dague/mca/pins/pins.h"

#define NUM_SELECT_EVENTS 2
#define SYSTEM_QUEUE_VP -2

typedef struct select_info_s {
	int kernel_type;
	int vp_id;
	int th_id;
	int victim_vp_id;
	int victim_th_id;
	unsigned long long int exec_context;
	int values_len;
	long long values[NUM_SELECT_EVENTS];
} select_info_t;

void pins_init_papi_select(dague_context_t * master_context);
void pins_fini_papi_select(dague_context_t * master_context);
void pins_thread_init_papi_select(dague_execution_unit_t * exec_unit);

BEGIN_C_DECLS

/**
 * Globally exported variable
 */
DAGUE_DECLSPEC extern const dague_pins_base_component_t dague_pins_papi_select_component;
DAGUE_DECLSPEC extern const dague_pins_module_t dague_pins_papi_select_module;
/* static accessor */
mca_base_component_t * pins_papi_select_static_component(void);

END_C_DECLS

#endif
