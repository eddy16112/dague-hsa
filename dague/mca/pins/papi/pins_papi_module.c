/*
 * Copyright (c) 2012-2015 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 */

#include "dague_config.h"
#include "pins_papi.h"
#include "dague/mca/pins/pins.h"
#include "dague/mca/pins/pins_papi_utils.h"
#include "dague/utils/output.h"
#include "dague/utils/mca_param.h"
#include <stdio.h>
#include <papi.h>
#include "dague/execution_unit.h"

static void pins_papi_read(dague_execution_unit_t* exec_unit,
                             dague_execution_context_t* exec_context,
                             parsec_pins_next_callback_t* cb_data);

static char* mca_param_string;
static parsec_pins_papi_events_t* pins_papi_events = NULL;

static void pins_init_papi(dague_context_t * master_context)
{
    pins_papi_init(master_context);

    dague_mca_param_reg_string_name("pins", "papi_event",
                                    "PAPI events to be gathered at both socket and core level.\n",
                                    false, false,
                                    "", &mca_param_string);
    if( NULL != mca_param_string ) {
        pins_papi_events = parsec_pins_papi_events_new(mca_param_string);
    }
}

static void pins_fini_papi(dague_context_t * master_context)
{
    if( NULL != pins_papi_events ) {
        parsec_pins_papi_events_free(&pins_papi_events);
        pins_papi_events = NULL;
    }
}

static void pins_thread_init_papi(dague_execution_unit_t * exec_unit)
{
    parsec_pins_papi_callback_t* event_cb = NULL;
    parsec_pins_papi_event_t* event;
    parsec_pins_papi_values_t info;
    int i, my_socket, my_core, err;
    char *conv_string = NULL, *datatype;

    if( NULL == pins_papi_events )
        return;

    my_socket = exec_unit->socket_id;
    my_core = exec_unit->core_id;

    pins_papi_thread_init(exec_unit);

    for( i = 0; i < pins_papi_events->num_counters; i++ ) {
        event = pins_papi_events->events[i];
        conv_string = NULL;

      redo_event:
        do {
            if( (event->socket != -1) && (event->socket != my_socket) )
                continue;
            if( (event->core != -1) && (event->core != my_core) )
                continue;

            if( NULL == event_cb ) {  /* create the event and the PAPI eventset */

                event_cb = (parsec_pins_papi_callback_t*)malloc(sizeof(parsec_pins_papi_callback_t));

                event_cb->papi_eventset = PAPI_NULL;
                event_cb->num_counters  = 0;
                event_cb->event         = event;
                event_cb->frequency     = event->frequency;
                event_cb->begin_end     = 0;

                /* Create an empty eventset */
                if( PAPI_OK != (err = PAPI_create_eventset(&event_cb->papi_eventset)) ) {
                    dague_output(0, "%s: thread %d couldn't create the PAPI event set; ERROR: %s\n",
                                 __func__, exec_unit->th_id, PAPI_strerror(err));
                    continue;
                }
            } else {
                /* We are trying to hook on an already allocated event. If we don't have the same frequency
                 * we must fail, then force the registration of the event_cb and then finally add this
                 * event again.
                 */
                if( event_cb->frequency != event->frequency )
                    goto register_callback;
            }

            /* Add events to the eventset */
            if( PAPI_OK != (err = PAPI_add_event(event_cb->papi_eventset,
                                                 event->pins_papi_native_event)) ) {
                dague_output(0, "%s: failed to add event %s; ERROR: %s\n",
                             __func__, event->pins_papi_event_name, PAPI_strerror(err));
                continue;
            }
            event_cb->num_counters++;
            switch( event->papi_data_type ) {
            case PAPI_DATATYPE_INT64: datatype = "int64_t"; break;
            case PAPI_DATATYPE_UINT64: datatype = "uint64_t"; break;
            case PAPI_DATATYPE_FP64: datatype = "double"; break;
            case PAPI_DATATYPE_BIT64: datatype = "int64_t"; break;
            default: datatype = "int64_t"; break;
            }

            if( NULL == conv_string )
                asprintf(&conv_string, "%s{%s}"PARSEC_PINS_SEPARATOR, event->pins_papi_event_name, datatype);
            else {
                char* tmp = conv_string;
                asprintf(&conv_string, "%s%s{%s}"PARSEC_PINS_SEPARATOR, tmp, event->pins_papi_event_name, datatype);
                free(tmp);
            }
            event = event->next;  /* next event */
        } while( NULL != event );

      register_callback:
        if( NULL != event_cb ) {
            char* key_string;

            asprintf(&key_string, "PINS_PAPI_S%d_C%d_F%d", exec_unit->socket_id, exec_unit->core_id, event_cb->frequency);

            dague_profiling_add_dictionary_keyword(key_string, "fill:#00AAFF",
                                                   sizeof(long long) * event_cb->num_counters,
                                                   conv_string,
                                                   &event_cb->pins_prof_event[0],
                                                   &event_cb->pins_prof_event[1]);
            free(key_string);

            if( PAPI_OK != (err = PAPI_start(event_cb->papi_eventset)) ) {
                dague_output(0, "couldn't start PAPI eventset for thread %d; ERROR: %s\n",
                             exec_unit->th_id, PAPI_strerror(err));
                event_cb->num_counters = 0;
                continue;
            }

            if( PAPI_OK != (err = PAPI_read(event_cb->papi_eventset, info.values)) ) {
                dague_output(0, "couldn't read PAPI eventset for thread %d; ERROR: %s\n",
                             exec_unit->th_id, PAPI_strerror(err));
                continue;
                /*goto cleanup_and_return;*/
            }
            dague_output(0, "PAPI event %s core %d socket %d frequency %d enabled\n",
                         conv_string, event_cb->event->core, event_cb->event->socket, event_cb->event->frequency);

            (void)dague_profiling_trace(exec_unit->eu_profile, event_cb->pins_prof_event[event_cb->begin_end],
                                        45, 0, (void *)&info);

            event_cb->begin_end = (event_cb->begin_end + 1) & 0x1;  /* aka. % 2 */

            free(conv_string);

            if(event_cb->frequency == 1) {
                PINS_REGISTER(exec_unit, EXEC_BEGIN, pins_papi_read,
                              (parsec_pins_next_callback_t*)event_cb);
            }
            PINS_REGISTER(exec_unit, EXEC_END, pins_papi_read,
                          (parsec_pins_next_callback_t*)event_cb);
            /* Mark the event_cb as NULL so that we allocate a new one during the next round */
            event_cb = NULL;
            goto redo_event;
        }
    }

    if( NULL != event_cb ) {
        parsec_pins_papi_event_cleanup(event_cb, &info);
        free(event_cb); event_cb = NULL;
    }
    if( NULL != conv_string )
        free(conv_string);
}

static void pins_thread_fini_papi(dague_execution_unit_t* exec_unit)
{
    parsec_pins_papi_callback_t* event_cb;
    parsec_pins_papi_values_t info;
    int err, i;

    do {
        /* Should this be in a loop to unregister all the callbacks for this thread? */
        PINS_UNREGISTER(exec_unit, EXEC_END, pins_papi_read, (parsec_pins_next_callback_t**)&event_cb);

        if( NULL == event_cb )
            return;

        if( PAPI_NULL == event_cb->papi_eventset ) {
            parsec_pins_papi_event_cleanup(event_cb, &info);

            /* If the last profiling event was an 'end' event */
            if(event_cb->begin_end == 0) {
                (void)dague_profiling_trace(exec_unit->eu_profile, event_cb->pins_prof_event[0],
                                            45, 0, (void *)&info);
            }
            (void)dague_profiling_trace(exec_unit->eu_profile, event_cb->pins_prof_event[1],
                                        45, 0, (void *)&info);
        }
        free(event_cb);
    } while(1);

    pins_papi_thread_fini(exec_unit);
}

static void pins_papi_read(dague_execution_unit_t* exec_unit,
                           dague_execution_context_t* exec_context,
                           parsec_pins_next_callback_t* cb_data)
{
    parsec_pins_papi_callback_t* event_cb = (parsec_pins_papi_callback_t*)cb_data;
    parsec_pins_papi_values_t info;
    int i, err;

    if(1 == event_cb->frequency ) {  /* trigger the event */

        if( PAPI_OK != (err = PAPI_read(event_cb->papi_eventset, info.values)) ) {
            dague_output(0, "couldn't read PAPI eventset for thread %d; ERROR: %s\n",
                         exec_unit->th_id, PAPI_strerror(err));
            return;
        }
        (void)dague_profiling_trace(exec_unit->eu_profile, event_cb->pins_prof_event[event_cb->begin_end],
                                    45, 0, (void *)&info);
        event_cb->begin_end = (event_cb->begin_end + 1) & 0x1;  /* aka. % 2 */
        event_cb->frequency = event_cb->event->frequency;
    } else event_cb->frequency--;
}

const dague_pins_module_t dague_pins_papi_module = {
    &dague_pins_papi_component,
    {
        pins_init_papi,
        pins_fini_papi,
        NULL,
        NULL,
        pins_thread_init_papi,
        pins_thread_fini_papi,
    }
};
