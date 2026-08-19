/*
 * SPDX-License-Identifier: MIT
 *
 * bsec_temp_offset_tab_target_board.h -- Polverine board self-heating
 * compensation tables for the BSEC heat-source input.
 *
 * Contributed by Bosch Sensortec (zgg <zhengguang.guo@us.bosch.com>, 2025)
 * for the BlackIoT Polverine board and released under this repository's MIT
 * licence.  It is NOT part of the licence-gated Bosch BSEC/BMV080 SDK material.
 *
 * Each tocp_cfgN[] table maps the raw BME690 temperature (deg C) to the
 * board's self-heating offset via piecewise polynomials
 *   offset = w[0] + w[1]*t + w[2]*t^2 + w[3]*t^3 + offset_ref
 * over contiguous [range_min, range_max] intervals covering -45..85 deg C.
 * temp_profile_tab[] selects the table from the pair
 * (PLVN_CFG_TEMP_PROFILE_CLIENT_ID, PLVN_CFG_BMV080_DUTY_CYCLE_PERIOD_S)
 * defined in each demo's polverine_cfg.h; the lookup is done by
 * _get_board_specific_temp_offset() in the generated bsec_integration.c.
 */

typedef struct {
    float range_min;
    float range_max;
    float w[4];
    float offset_ref;
} temp_offset_cf_param_t;


#define OFFSET_REF_CFG1 2.0f
const 
temp_offset_cf_param_t tocp_cfg1[] = {
    {-45, 15,   {4.68f, 0, 0, 0}, OFFSET_REF_CFG1},
    {15, 30,    {4.57f, 7.56e-3, 0, 0}, OFFSET_REF_CFG1},
    {30, 55,    {4.39f, 0.0151f, 0, 0}, OFFSET_REF_CFG1},
    {55, 65,    {95.3f, -3.26f, 0.0295f, 0}, OFFSET_REF_CFG1},
    {65, 85,    {8.24f, 0, 0, 0}, OFFSET_REF_CFG1},
};

#define OFFSET_REF_CFG2 2.0f
const 
temp_offset_cf_param_t tocp_cfg2[] = {
    {-45, 15,   {1.62f, 0, 0, 0}, OFFSET_REF_CFG2},
    {15, 28,    {-21.3f, 2.84f, -0.108f, 1.37e-3}, OFFSET_REF_CFG2},
    {28, 50,    {2.93f, 0.0243f, 0, 0}, OFFSET_REF_CFG2},
    {50, 62,    {125.8f, -4.86f, 0.0485f, 0}, OFFSET_REF_CFG2},
    {62, 85,    {10.91f,0, 0, 0}, OFFSET_REF_CFG2},
};

#define OFFSET_REF_CFG3 2.0f
const 
temp_offset_cf_param_t tocp_cfg3[] = {
    {-45, 15,   {7.08f, 0, 0, 0}, OFFSET_REF_CFG3},
    {15, 28,    {6.61f, 0.0315f, 0, 0}, OFFSET_REF_CFG3},
    {28, 50,    {6.07f, 0.0451f, 0, 0}, OFFSET_REF_CFG3},
    {50, 62,    {-66.3f, 2.58f, -0.0218f, 0}, OFFSET_REF_CFG3},
    {62, 85,    {9.86f, 0, 0, 0}, OFFSET_REF_CFG3},
};


#define OFFSET_REF_CFG4 2.0f
const 
temp_offset_cf_param_t tocp_cfg4[] = {
    {-45, 15,   {6.94f, 0, 0, 0}, OFFSET_REF_CFG4},
    {15, 22,    {6.51f, 0.0287f, 0, 0}, OFFSET_REF_CFG4},
    {22, 57.5f, {6.21f, 0.0396f, 0, 0}, OFFSET_REF_CFG4},
    {57.5f, 65, {-120.5f, 4.03f, -0.0311f, 0}, OFFSET_REF_CFG4},
    {65, 85,    {10.05f, 0, 0, 0}, OFFSET_REF_CFG4},
};



#define OFFSET_REF_CFG6 1.3f
const 
temp_offset_cf_param_t tocp_cfg6[] = {
    {-45, -1.3f,{9.12f, 0.0f, 0, 0}, OFFSET_REF_CFG6},
    {-1.3f, 10.37f,{9.2f, 0.0541f, 0, 0}, OFFSET_REF_CFG6},
    {10.37f, 37.2f,{9.2f, 0.0541f, 0, 0}, OFFSET_REF_CFG6},
    {37.2f, 56,    {8.72f, 0.0658f, 0, 0}, OFFSET_REF_CFG6},
    {56, 85,    {12.29f, 0, 0, 0}, OFFSET_REF_CFG6},
};

#define OFFSET_REF_CFG7 1.3f
const 
temp_offset_cf_param_t tocp_cfg7[] = {
    {-45, 0.37f,{11.62f, 0.0f, 0, 0}, OFFSET_REF_CFG7},
    {0.37f, 13.61f,{11.6f, 0.0502f, 0, 0}, OFFSET_REF_CFG7},
    {13.61f, 39.75f,{11.6f, 0.0502f, 0, 0}, OFFSET_REF_CFG7},
    {39.75f, 55.5f,    {11.4f, 0.0562f, 0, 0}, OFFSET_REF_CFG7},
    {55.5f, 85,    {14.52f, 0, 0, 0}, OFFSET_REF_CFG7},
};


typedef struct {
    uint8_t client_id;
    struct {
        uint16_t bmv080_duty_cycle_period_s;
        const temp_offset_cf_param_t *tocp_cfg;
        uint8_t tocp_len;
    } map;
} temp_profile_tab_entry_t;

const temp_profile_tab_entry_t temp_profile_tab[] = {
    {
        0, 
        {30, tocp_cfg1, sizeof(tocp_cfg1)/sizeof(tocp_cfg1[0])}
    },
    {
        0, 
        {60, tocp_cfg2, sizeof(tocp_cfg2)/sizeof(tocp_cfg2[0])}
    },
    {
        1, 
        {30, tocp_cfg3, sizeof(tocp_cfg3)/sizeof(tocp_cfg3[0])}
    },
    {
        1, 
        {60, tocp_cfg4, sizeof(tocp_cfg4)/sizeof(tocp_cfg4[0])}
    },
    {
        2, 
        {60, tocp_cfg6, sizeof(tocp_cfg6)/sizeof(tocp_cfg6[0])}
    },
    {
        2, 
        {30, tocp_cfg7, sizeof(tocp_cfg7)/sizeof(tocp_cfg7[0])}
    },
};

