#pragma once
/* Simulator shim: portmacro.h — critical sections are no-ops (the simulator
 * is single-threaded; producers and the UI share one thread). */

typedef uint32_t portMUX_TYPE;

#define portMUX_INITIALIZER_UNLOCKED 0

#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux)  ((void)(mux))

#define portTICK_PERIOD_MS 1
