#pragma once
/* Interactive data-adjustment panel rendered on the simulator's second LVGL
 * display (240x360 logical, scrollable). Call with the panel display set as
 * the LVGL default (main.c handles the switch); builds its own screen and a
 * 100ms tick that drives the obd_data_cache. */
void control_panel_build(void);
