/* MAIN RX / SUB TX role mode and inverse frequency tracking. */

#include <stdint.h>

#include "app/splitrx.h"
#include "audio.h"
#include "frequencies.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"

static bool tx_active;
static bool inv_enabled;
static bool inv_pending;
static bool inv_pending_value;

bool SPLITRX_IsEnabled(void)
{
    return gEeprom.MAIN_RX_SUB_TX;
}

bool SPLITRX_IsInvEnabled(void)
{
    return gEeprom.MAIN_RX_SUB_TX && inv_enabled;
}

bool SPLITRX_IsTxActive(void)
{
    return gEeprom.MAIN_RX_SUB_TX && tx_active;
}

VFO_Info_t *SPLITRX_GetMainVfo(void)
{
    return &gEeprom.VfoInfo[gEeprom.TX_VFO];
}

VFO_Info_t *SPLITRX_GetSubVfo(void)
{
    return &gEeprom.VfoInfo[gEeprom.TX_VFO ^ 1u];
}

VFO_Info_t *SPLITRX_GetTransmitRoleVfo(void)
{
    return gEeprom.MAIN_RX_SUB_TX ? SPLITRX_GetSubVfo() : gTxVfo;
}

bool SPLITRX_SelectRoleVfos(void)
{
    if (!gEeprom.MAIN_RX_SUB_TX)
        return false;

    gEeprom.RX_VFO = gEeprom.TX_VFO;
    gRxVfo         = SPLITRX_GetMainVfo();
    gTxVfo         = tx_active ? SPLITRX_GetSubVfo() : SPLITRX_GetMainVfo();
    gCurrentVfo    = tx_active ? gTxVfo : gRxVfo;
    return true;
}

void SPLITRX_BeginTx(void)
{
    if (!gEeprom.MAIN_RX_SUB_TX)
        return;

    tx_active = true;
    SPLITRX_SelectRoleVfos();
}

void SPLITRX_EndTx(void)
{
    if (!tx_active)
        return;

    tx_active = false;
    SPLITRX_SelectRoleVfos();
    // Drained here rather than at each call site: a queued toggle belongs to the
    // transmission that just ended, and a missed drain would silently apply it
    // to a later, unrelated transmission (a wrong-direction Doppler flip).
    SPLITRX_ApplyPendingInv();
}

void SPLITRX_ResetRoleState(void)
{
    // Called when MAIN_RX_SUB_TX is installed outside SPLITRX_SetMode (the
    // EEPROM loader). Drops any stale transmit role so the module cannot be
    // left believing a transmission is still in progress.
    tx_active   = false;
    inv_pending = false;
    if (!gEeprom.MAIN_RX_SUB_TX)
        inv_enabled = false;
}

void SPLITRX_SetMode(const bool enabled)
{
    if (gEeprom.MAIN_RX_SUB_TX == enabled)
        return;

    if (!enabled)
        SPLITRX_EndTx();

    gEeprom.MAIN_RX_SUB_TX = enabled;
    inv_enabled            = false;
    inv_pending            = false;
    // Entering the mode always starts from receive. Without this, any missed
    // EndTx elsewhere would persist across a mode toggle and install the role
    // pointers as if a transmission were still in progress.
    tx_active              = false;

    if (enabled) {
        // The role pointers below are incompatible with dual watch and
        // crossband, so this mode owns them exclusively.
        gEeprom.DUAL_WATCH       = DUAL_WATCH_OFF;
        gEeprom.CROSS_BAND_RX_TX = CROSS_BAND_OFF;
#ifdef ENABLE_CW_MODULATOR
        // A linear transponder needs true keyed carrier, never CW-as-SSB-tone.
        gCW_CrossMode = false;
#endif
        // A repeater shift on SUB would put the uplink somewhere other than the
        // frequency shown, and inverse tracking assumes displayed == radiated.
        VFO_Info_t *const sub = SPLITRX_GetSubVfo();
        if (sub->TX_OFFSET_FREQUENCY_DIRECTION != TX_OFFSET_FREQUENCY_DIRECTION_OFF) {
            sub->TX_OFFSET_FREQUENCY_DIRECTION = TX_OFFSET_FREQUENCY_DIRECTION_OFF;
            RADIO_ApplyOffset(sub);
        }
        SPLITRX_SelectRoleVfos();
    }
    gUpdateStatus = true;
}

void SPLITRX_ToggleInv(void)
{
    if (!gEeprom.MAIN_RX_SUB_TX) {
        gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
        return;
    }

    // Never retune SUB while it is radiating: flipping the tracking direction
    // mid-transmission would move the live carrier (and, with the CW keyer,
    // would do it between elements). Defer until the transmission releases.
    if (tx_active) {
        inv_pending_value = !(inv_pending ? inv_pending_value : inv_enabled);
        inv_pending       = true;
        return;
    }

    inv_enabled   = !inv_enabled;
    gUpdateStatus = true;
}

void SPLITRX_ApplyPendingInv(void)
{
    if (!inv_pending)
        return;

    inv_enabled   = gEeprom.MAIN_RX_SUB_TX && inv_pending_value;
    inv_pending   = false;
    gUpdateStatus = true;
}

bool SPLITRX_TxBlockedNotCw(void)
{
#ifdef ENABLE_CW_MODULATOR
    // A linear transponder uplink has to be true keyed carrier. SSB/FM on SUB
    // would radiate a wideband signal through the transponder passband, so
    // refuse instead of transmitting the wrong thing.
    return gEeprom.MAIN_RX_SUB_TX &&
           SPLITRX_GetSubVfo()->Modulation != MODULATION_CW;
#else
    return false;
#endif
}

bool SPLITRX_KeyerShouldBeArmed(void)
{
#ifdef ENABLE_CW_MODULATOR
    // Transmit-role VFO, never gTxVfo: in this mode the idle gTxVfo is MAIN (the
    // downlink, typically USB) while SUB carries the CW uplink. Several call
    // sites used to recompute this from gTxVfo and disarmed the keyer.
    return SPLITRX_GetTransmitRoleVfo()->Modulation == MODULATION_CW;
#else
    return false;
#endif
}

bool SPLITRX_MonitorShouldBeOpen(void)
{
    // A satellite downlink sits at or below the noise floor, so the squelch must
    // never gate it -- whatever MAIN's modulation happens to be.
    if (gEeprom.MAIN_RX_SUB_TX)
        return true;

#ifdef ENABLE_CW_MODULATOR
    // Stock rule otherwise: CW and USB default to an open squelch.
    return gRxVfo->Modulation == MODULATION_CW ||
           gRxVfo->Modulation == MODULATION_USB;
#else
    return false;
#endif
}

bool SPLITRX_TuneMainFrequency(const uint32_t frequency)
{
    VFO_Info_t *const main = SPLITRX_GetMainVfo();

    if (!SPLITRX_IsInvEnabled()) {
        // Plain assignment, exactly as before this feature existed: callers have
        // already clamped, and adding a check here would reject values the stock
        // firmware accepted (e.g. a 30 kHz step rounding up past the 1300 MHz
        // limit on wide-RX builds).
        main->freq_config_RX.Frequency = frequency;
        return true;
    }

    if (RX_freq_check(frequency) != 0 || tx_active)
        return false;

    VFO_Info_t *const sub    = SPLITRX_GetSubVfo();
    const int64_t     delta  = (int64_t)frequency - main->freq_config_RX.Frequency;
    const int64_t     paired = (int64_t)sub->freq_config_RX.Frequency - delta;

    if (paired < 0 || paired > UINT32_MAX || RX_freq_check((uint32_t)paired) != 0)
        return false;

    const uint32_t old_sub_rx = sub->freq_config_RX.Frequency;
    const uint32_t old_sub_tx = sub->freq_config_TX.Frequency;
    sub->freq_config_RX.Frequency = (uint32_t)paired;
    RADIO_ApplyOffset(sub);

    if (TX_freq_check(sub->pTX->Frequency) != 0) {
        sub->freq_config_RX.Frequency = old_sub_rx;
        sub->freq_config_TX.Frequency = old_sub_tx;
        return false;
    }

    main->freq_config_RX.Frequency = frequency;
    const FREQUENCY_Band_t band = FREQUENCY_GetBand((uint32_t)paired);
    if (sub->Band != band) {
        sub->Band = band;
        RADIO_ConfigureSquelchAndOutputPower(sub);
    }
    return true;
}
