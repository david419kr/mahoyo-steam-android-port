#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t woh_create(const char* game_root);
uint64_t woh_create_configured(const char* game_root, const char* edition, const char* language);
const char* woh_command(uint64_t handle, const char* type, const char* payload);
//Monotonic uptime sample for local wave phase only: no VM/snapshot/render work.
void woh_wave_clock(uint64_t handle, uint64_t uptime_ms);
const char* woh_snapshot(uint64_t handle);
void woh_destroy(uint64_t handle);
#ifdef __cplusplus
}
#endif
