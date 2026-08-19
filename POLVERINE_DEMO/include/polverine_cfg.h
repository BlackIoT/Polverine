#ifndef __POLVERINE_PRJ_CFG_H
#define __POLVERINE_PRJ_CFG_H

#define PVLN_CFG_BSEC_OUTPUT_UPDATE_GATED_BY_BMV080 true
#define PLVN_CFG_BSEC_LOOP_DELAY_TIME_MS    50

#define PLVN_CFG_BMV080_DUTY_CYCLE_PERIOD_S 30

#define PLVN_CFG_BMV080_CONNECTIVITY_USB    true
/* Selects the board self-heating temperature-compensation profile in
 * bsec_temp_offset_tab_target_board.h; the profile is looked up by the pair
 * (PLVN_CFG_TEMP_PROFILE_CLIENT_ID, PLVN_CFG_BMV080_DUTY_CYCLE_PERIOD_S).
 * It is not a board revision - do not change it without recalibrating. */
#define PLVN_CFG_TEMP_PROFILE_CLIENT_ID     1

#endif
