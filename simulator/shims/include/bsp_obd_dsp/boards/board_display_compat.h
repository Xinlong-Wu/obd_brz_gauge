#pragma once
/* Simulator shim: board_display_compat.h — the real header dispatches to the
 * selected board (sdkconfig); the simulator always acts like WS185 and simply
 * forwards to the ST77916 shim. */
#include "bsp_obd_dsp/lcd_driver/ST77916.h"
