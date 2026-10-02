/* Simulator shim: cross-task event queue. The simulator is single-threaded
 * and nothing produces events; recv() always reports "empty". */
#include "app_obd_dsp/app_event.h"

#include <stddef.h>

bool app_event_recv(app_event_t *out)
{
    (void)out;
    return false;
}

void app_event_init(void) {}

bool app_event_send(app_event_type_t type, uint32_t data)
{
    (void)type; (void)data;
    return false;
}

QueueHandle_t app_event_queue(void) { return NULL; }
