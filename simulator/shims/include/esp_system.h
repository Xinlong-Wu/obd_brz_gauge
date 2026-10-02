#pragma once
/* Simulator shim: esp_restart() is a logged no-op on PC — quit the simulator
 * and rerun to simulate a reboot. */

void esp_restart(void);
