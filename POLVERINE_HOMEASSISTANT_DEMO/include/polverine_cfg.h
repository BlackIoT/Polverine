#ifndef __POLVERINE_PRJ_CFG_H
#define __POLVERINE_PRJ_CFG_H

#define PVLN_CFG_BSEC_OUTPUT_UPDATE_GATED_BY_BMV080 false
#define PLVN_CFG_BSEC_LOOP_DELAY_TIME_MS    50

#define PLVN_CFG_BMV080_DUTY_CYCLE_PERIOD_S 60

#define PLVN_CFG_BMV080_CONNECTIVITY_WIFI   true

/* Selects the board self-heating compensation profile in
 * deps/bosch-sensortec/bsec_library_wrappers/bsec_temp_offset_tab_target_board.h.
 * The (client_id, duty_cycle) pair picks one tocp_cfg table: client_id names a
 * compensation profile (a set of temperature-offset polynomials, keyed together
 * with the BMV080 duty-cycle period), NOT the board revision:
 *   0 -> tocp_cfg1/2, 1 -> tocp_cfg3/4, 2 -> tocp_cfg6/7.
 * Without this define bsec_integration.c does not compile. An unmatched pair is
 * not fatal: the lookup returns 0 and no compensation is applied. */
#define PLVN_CFG_TEMP_PROFILE_CLIENT_ID     2

#endif
