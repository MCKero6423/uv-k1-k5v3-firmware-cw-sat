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
// Resynchronises the module after MAIN_RX_SUB_TX is written directly (EEPROM
// load), which can happen mid-transmission via a serial config write.
void SPLITRX_ResetRoleState(void);
void SPLITRX_ToggleInv(void);
void SPLITRX_ApplyPendingInv(void);

// True when the mode is enabled but SUB is not set to CW. A linear transponder
// uplink must be true keyed carrier, so transmission is refused in that case.
bool SPLITRX_TxBlockedNotCw(void);

// Atomically changes MAIN and, when INV is on, applies the opposite delta to
// SUB. Returns false without changing either VFO when the pair is illegal.
bool SPLITRX_TuneMainFrequency(uint32_t frequency);

#endif
