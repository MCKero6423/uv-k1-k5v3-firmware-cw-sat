/* MAIN RX / SUB TX role mode and inverse frequency tracking.
 *
 * Linear satellite transponder support: MAIN receives the downlink while SUB
 * owns the uplink, and INV applies every MAIN tuning delta to SUB in the
 * opposite direction for manual Doppler tracking on inverting transponders.
 *
 * Ported from the NR7Y UV-K5 V1 CW firmware
 * (atsunatsu/uv-k5-firmware-custom-cw, commits c60a39c..513180c).
 */

#ifndef APP_SPLITRX_H
#define APP_SPLITRX_H

#include <stdbool.h>
#include <stdint.h>

#include "radio.h"

bool SPLITRX_IsEnabled(void);
bool SPLITRX_IsInvEnabled(void);
bool SPLITRX_IsTxActive(void);

VFO_Info_t *SPLITRX_GetMainVfo(void);
VFO_Info_t *SPLITRX_GetSubVfo(void);
// VFO whose modulation/frequency configuration owns the next transmission.
// This is SUB in the fifth RxMode even while idle pointers still expose MAIN.
VFO_Info_t *SPLITRX_GetTransmitRoleVfo(void);

// Returns true and installs all role pointers when the fifth RxMode owns them.
bool SPLITRX_SelectRoleVfos(void);
void SPLITRX_BeginTx(void);
void SPLITRX_EndTx(void);

void SPLITRX_SetMode(bool enabled);
void SPLITRX_ToggleInv(void);
void SPLITRX_ApplyPendingInv(void);

// Atomically changes MAIN and, when INV is on, applies the opposite delta to
// SUB. Returns false without changing either VFO when the pair is illegal.
bool SPLITRX_TuneMainFrequency(uint32_t frequency);

#endif
