/**
 * Copyright (C) Bosch Sensortec GmbH. All Rights Reserved. Confidential.
 *
 * Disclaimer
 *
 * Common:
 * Bosch Sensortec products are developed for the consumer goods industry. They may only be used
 * within the parameters of the respective valid product data sheet. Bosch Sensortec products are
 * provided with the express understanding that there is no warranty of fitness for a particular purpose.
 * They are not fit for use in life-sustaining, safety or security sensitive systems or any system or device
 * that may lead to bodily harm or property damage if the system or device malfunctions. In addition,
 * Bosch Sensortec products are not fit for use in products which interact with motor vehicle systems.
 * The resale and/or use of products are at the purchaser's own risk and his own responsibility. The
 * examination of fitness for the intended use is the sole responsibility of the Purchaser.
 *
 * The purchaser shall indemnify Bosch Sensortec from all third party claims, including any claims for
 * incidental, or consequential damages, arising from any product use not covered by the parameters of
 * the respective valid product data sheet or not approved by Bosch Sensortec and reimburse Bosch
 * Sensortec for all costs in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products, particularly with regard to
 * product safety and inform Bosch Sensortec without delay of all security relevant incidents.
 *
 * Engineering Samples are marked with an asterisk (*) or (e). Samples may vary from the valid
 * technical specifications of the product series. They are therefore not intended or fit for resale to third
 * parties or for use in end products. Their sole purpose is internal client testing. The testing of an
 * engineering sample may in no way replace the testing of a product series. Bosch Sensortec
 * assumes no liability for the use of engineering samples. By accepting the engineering samples, the
 * Purchaser agrees to indemnify Bosch Sensortec from all claims arising from the use of engineering
 * samples.
 *
 * Special:
 * This software module (hereinafter called "Software") and any information on application-sheets
 * (hereinafter called "Information") is provided free of charge for the sole purpose to support your
 * application work. The Software and Information is subject to the following terms and conditions:
 *
 * The Software is specifically designed for the exclusive use for Bosch Sensortec products by
 * personnel who have special experience and training. Do not use this Software if you do not have the
 * proper experience or training.
 *
 * This Software package is provided `` as is `` and without any expressed or implied warranties,
 * including without limitation, the implied warranties of merchantability and fitness for a particular
 * purpose.
 *
 * Bosch Sensortec and their representatives and agents deny any liability for the functional impairment
 * of this Software in terms of fitness, performance and safety. Bosch Sensortec and their
 * representatives and agents shall not be liable for any direct or indirect damages or injury, except as
 * otherwise stipulated in mandatory applicable law.
 *
 * The Information provided is believed to be accurate and reliable. Bosch Sensortec assumes no
 * responsibility for the consequences of use of such Information nor for any infringement of patents or
 * other rights of third parties which may result from its use. No license is granted by implication or
 * otherwise under any patent or patent rights of Bosch. Specifications mentioned in the Information are
 * subject to change without notice.
 *
 * It is not allowed to deliver the source code of the Software to any third party without permission of
 * Bosch Sensortec.
 *
 * @file bmv080_defs.h
 *
 * @brief BMV080 sensor driver definitions
 *
 *
 */

#ifndef BMV080_DEFS_H_
#define BMV080_DEFS_H_

#include <stdint.h>
#include <stdbool.h>

/*********************************************************************************************************************
* Type definitions
*********************************************************************************************************************/

/*!
* @brief Status codes returned by the BMV080 sensor driver.
*/
typedef enum 
{
    /* Ok ***********************************************************************************************************/

    /*! 0: Default result if there is no warning or error */
    E_BMV080_OK = 0,

    /* Warnings *****************************************************************************************************/

    /*! 1: The requested register is volatile */
    E_BMV080_WARNING_VOLATILE_REG_ACCESS = 1,
    /*! 2: The requested register cannot be set because it is read only */
    E_BMV080_WARNING_WRITE_READ_ONLY_REG = 2,

    /*! 3: The FIFO is only partially read, i.e. a frame consists of less than 4 words */
    E_BMV080_WARNING_FIFO_FRAME_PARTIALLY_READ = 3,

    /*! 4: More particle features occurred in a measurement cycle than the postprocessor can process */
    E_BMV080_WARNING_POSTPROCESSOR_PARTICLE_FEATURE_OVERFLOW = 4,

    /*! 208: The amount of FIFO entries exceed the buffer to store them */
    E_BMV080_WARNING_FIFO_FRAME_BUFFER_TOO_SMALL = 208,
    /*! 209: Not all FIFO entries were retrieved */
    E_BMV080_WARNING_FIFO_MISSING_FRAME = 209,

    /* Errors *******************************************************************************************************/

    /*! 100: A reference points to an invalid null address */
    E_BMV080_ERROR_NULLPTR = 100,

    /*! 101: The requested register address exceeds the available register map */
    E_BMV080_ERROR_REG_ADDR_OUT_OF_BOUNDS = 101,

    /*! 179: The parameter is locked because it is currently in use */
    E_BMV080_ERROR_PARAM_LOCKED = 179,
    /*! 115: The parameter (key) is invalid */
    E_BMV080_ERROR_PARAM_INVALID = 115,
    /*! 102: The parameter "channel id" is invalid */
    E_BMV080_ERROR_PARAM_INVALID_CHANNEL_ID = 102,
    /*! 123: The parameter's value is invalid */
    E_BMV080_ERROR_PARAM_INVALID_VALUE = 123,
    /*! 104: The parameter "configuration version" is invalid */
    E_BMV080_ERROR_PARAM_INVALID_CONFIG_VERSION = 104,

    /*! 180: An API call is invalid because a precondition is still unsatisfied */
    E_BMV080_ERROR_PRECONDITION_UNSATISFIED = 180,

    /*! 105: Reading via the provided hardware communication interface failed */
    E_BMV080_ERROR_HW_READ = 105,
    /*! 106: Writing via the provided hardware communication interface failed */
    E_BMV080_ERROR_HW_WRITE = 106,

    /*! 107: The immutable "chip id" changed */
    E_BMV080_ERROR_MISMATCH_CHIP_ID = 107,
    /*! 160: A register value does not match the expectation */
    E_BMV080_ERROR_MISMATCH_REG_VALUE = 160,

    /*! 116: The requested mode is invalid */
    E_BMV080_ERROR_MODE_INVALID = 116,
    /*! 113: Changing to the requested mode failed */
    E_BMV080_ERROR_MODE_CHANGE = 113,
    /*! 114: The modes of different channels diverge */
    E_BMV080_ERROR_MODE_CHANNELS_OUT_OF_SYNC = 114,
    /*! 157: The ASIC(s) are were not configured before switching to active mode */
    E_BMV080_ERROR_ASIC_NOT_CONFIGURED = 157,

    /*! 133: Reading the NVM image failed */
    E_BMV080_ERROR_NVM_READ_IMAGE = 133,
    /*! 135: The requested address is outside the available NVM registers */
    E_BMV080_ERROR_NVM_ADDRESS_BOUNDARY = 135,
    /*! 136: The command fail flag of the NVM status register triggered */
    E_BMV080_ERROR_NVM_CMD_FAIL = 136,
    /*! 137: Waiting for the NVM to get ready exceeded the timeout duration */
    E_BMV080_ERROR_NVM_CMD_TIMEOUT_ELAPSED = 137,
    /*! 138: The NVM image is invalid */
    E_BMV080_ERROR_NVM_INVALID_IMAGE = 138,
    /*! 139: No reliable image of the NVM is available */
    E_BMV080_ERROR_NVM_OBSOLETE_IMAGE = 139,
    /*! 140: The unit is not in standby (or configuration) mode */
    E_BMV080_ERROR_NVM_STANDBY_MODE = 140,
    /*! 153: The CRC-checksum of the NVM differs from the CRC-checked NVM registers */
    E_BMV080_ERROR_NVM_CRC_MISMATCH = 153,
    /*! 154: The CRC-checksum of the NVM differs from the CRC-checked NVM registers of the wafer level test */
    E_BMV080_ERROR_NVM_CRC_MISMATCH_WLT = 154,
    /*! 156: The unit did not pass the wafer level test */
    E_BMV080_ERROR_NVM_WLT_FAIL = 156,
    /*! 159: The CRC-checksum of the NVM fields for the final test does not match */
    E_BMV080_ERROR_NVM_CRC_MISMATCH_FINAL_TEST = 159,
    /*! 161: The laser control constants fields of the NVM do not match expectations */
    E_BMV080_ERROR_NVM_CONTROL_CONSTANTS_INVALID = 161,

    /*! 210: A FIFO entry does not follow the expected formats */
    E_BMV080_ERROR_FIFO_UNEXPECTED_FRAME = 210,
    /*! 213: During FIFO status read-out, it is recognized that the PFFA counter is saturated */
    E_BMV080_ERROR_FIFO_PFFA_FRAME_CNT_SATURATED = 213,
    /*! 174: Requesting the FIFO only returns zeros indicating a not available data path */
    E_BMV080_ERROR_FIFO_UNAVAILABLE = 174,
    /*! 214: During the tracking of the PFFA counter a difference got negative, which should never happen */
    E_BMV080_ERROR_FIFO_PFFA_FRAME_CNT_DIFF_NEGATIVE = 214,

    /*! 161: An error occurred regarding the communication synchronization */
    E_BMV080_ERROR_STATUS_SYNC_COMM = 161,
    /*! 162: An error occurred regarding the control synchronization */
    E_BMV080_ERROR_STATUS_SYNC_CTRL = 162,
    /*! 163: An error occurred regarding the measurement synchronization */
    E_BMV080_ERROR_STATUS_SYNC_MEAS = 163,
    /*! 164: An error occurred regarding a locked synchronization */
    E_BMV080_ERROR_STATUS_SYNC_LOCKED = 164,
    /*! 165: An error occurred regarding the DC cancellation range */
    E_BMV080_ERROR_STATUS_DC_CANCEL_RANGE = 165,
    /*! 166: An error occurred regarding the DC estimation range */
    E_BMV080_ERROR_STATUS_DC_ESTIM_RANGE = 166,
    /*! 167: An error occurred regarding the LPWR temperature of the master ASIC */
    E_BMV080_ERROR_STATUS_LPWR_T_ASICMASTER = 167,
    /*! 168: An error occurred regarding LPWR range */
    E_BMV080_ERROR_STATUS_LPWR_RANGE = 168,
    /*! 169: An error occurred regarding the power of the first photo diode */
    E_BMV080_ERROR_STATUS_POWER_PD1 = 169,
    /*! 170: An error occurred regarding the headroom detection */
    E_BMV080_ERROR_STATUS_HEADROOM_DET = 170,
    /*! 171: An error occurred regarding the headroom LDV output */
    E_BMV080_ERROR_STATUS_HEADROOM_LDV_OUTPUT = 171,
    /*! 172: An error occurred regarding the headroom LDV reference */
    E_BMV080_ERROR_STATUS_HEADROOM_LDV_REF = 172,
    /*! 173: The eye safety cannot be guaranteed since an error occurred regarding the headroom detection */
    E_BMV080_ERROR_STATUS_HEADROOM_DET_USER = 173,
    /*! 120: The eye safety cannot be guaranteed since eye safety mechanism were violated */
    E_BMV080_ERROR_STATUS_EYE_SAFETY = 120,
    /*! 211: The timestamp rose too much compared to the previous timestamp which is an indication for mis-ordered frames */
    E_BMV080_ERROR_TIMESTAMP_DIFFERENCE = 211,
    /*! 212: The timestamp is smaller than the previous timestamp */
    E_BMV080_ERROR_TIMESTAMP_OVERFLOW = 212,

    /*! 300: The postprocessor version is incompatible with the version of the BMV080 sensor driver */
    E_BMV080_ERROR_POST_PROCESSOR_VERSION_INCOMPATIBLE = 300,
    /*! 301: The given postprocessor parameter format version is invalid */
    E_BMV080_ERROR_POST_PROCESSOR_PARAMETER_VERSION_INVALID = 301,
    /*! 302: The given postprocessor parameter index is invalid */
    E_BMV080_ERROR_POST_PROCESSOR_PARAMETER_INDEX_INVALID = 302,

    /*! 403: Dynamic allocation of memory required for a sub-operation failed */
    E_BMV080_ERROR_MEMORY_ALLOCATION = 403,

    /*! 303: A linked callback function for delays produced an error */
    E_BMV080_ERROR_CALLBACK_DELAY = 303

}bmv080_status_code_t;

/*!
* @brief Unique handle for a sensor unit.
*/
typedef void* bmv080_handle_t;

/*!
* @brief Unique handle for a serial communication, i.e. the hardware connection to the sensor unit.
*/
typedef void* bmv080_sercom_handle_t;

/*!
* @brief Modes of performing a duty cycling measurement.
*/
typedef enum
{
    /*! 0: Mode with fixed duty cycle, ON time = integration_time and OFF time = sleep time */
    E_BMV080_DUTY_CYCLING_MODE_0 = 0
} bmv080_duty_cycling_mode_t;

/*!
* @brief Power modes for use case based power consumption optimization
*/
typedef enum
{
    /*! 1: Power mode 1 enables particulate matter measurement in the range of 0.02 - 1.5 m/s (e.g., indoor use cases) */
    E_BMV080_POWER_MODE_1 = 1,
    /*! 2: Power mode 2 enables particulate matter measurement in the range of 0.02 - 3.0 m/s (e.g., outdoor use cases) */
    E_BMV080_POWER_MODE_2,
    /*! Placeholder: max value (for internal use only) */
    E_BMV080_POWER_MODE_N
} bmv080_power_mode_t;

/*!
* @brief Placeholder structure for extended sensor output information.
*/
struct bmv080_extended_info_s;

/*!
* @brief Output structure which is updated by bmv080_serve_interrupt when sensor output is available.
*/
typedef struct
{
    float reserved_0;
    float pm2_5;
    float reserved_1;
    float runtime_in_sec;
    bool is_static_obstructed[3];
    bool is_dynamic_obstructed;
    bool is_outside_detection_limits;
    struct bmv080_extended_info_s *extended_info;
}bmv080_output_t;


/*********************************************************************************************************************
* Callback definitions
*********************************************************************************************************************/

/*!
* @brief Function pointer for reading an array of _payload_length_ words of 16 bit _payload_.
*
* @details All data, _header_ and _payload_, is transferred as MSB first.
*
* @pre Both _header_ and _payload_ words are 16 bit and combined.
*      A _payload_ is only transferred on a complete transmission of 16 bits.
* @pre Burst transfers, i.e. reading a _header_ followed by several _payload_ elements, must be supported.
*
* @param[in] sercom_handle : Handle for a serial communication interface to a specific sensor unit.
* @param[in] header : Header information for the following _payload_.
* @param[out] payload : Payload to be read consisting of 16 bit words.
* @param[in] payload_length : Number of _payload_ elements to be read.
*
* @return Zero if successful, otherwise the return value is an externally defined error code.
*/
typedef int8_t(*bmv080_callback_read_t)(bmv080_sercom_handle_t sercom_handle, uint16_t header, uint16_t* payload,
    uint16_t payload_length);

/*!
* @brief Function pointer for writing an array of _payload_length_ words of 16 bit _payload_.
*
* @details All data, _header_ and _payload_, is transferred as MSB first.
* 
* @pre Both _header_ and _payload_ words are 16 bit.
*      A _payload_ is only transferred on a complete transmission of 16 bits.
* @pre Burst transfers, i.e. writing a _header_ followed by several _payload_ elements, must be supported.
*
* @param[in] sercom_handle : Handle for a serial communication interface to a specific sensor unit.
* @param[in] header : Header information for the following _payload_.
* @param[in] payload : Payload to be written consisting of 16 bit words.
* @param[in] payload_length : Number of _payload_ elements to be written.
*
* @return Zero if successful, otherwise the return value is an externally defined error code.
*/
typedef int8_t(*bmv080_callback_write_t)(bmv080_sercom_handle_t sercom_handle, uint16_t header, const uint16_t* payload,
    uint16_t payload_length);

/*!
* @brief Function pointer for executing a software delay operation.
*
* @param[in] duration_in_ms : Duration of the delay in milliseconds.
*
* @return Zero if successful, otherwise the return value is an externally defined error code.
*/
typedef int8_t(*bmv080_callback_delay_t)(uint32_t duration_in_ms);

/*!
* @brief Function pointer for getting a tick value in milliseconds (based on the host system clock). 
*
* @details This serves as a measure of the elapsed time since start up,
*          .e.g uint32_t HAL_GetTick(void) of the STM32 HAL Framework or uint32_t GetTickCount(void) of the Windows API.
*
* @return Tick value in milliseconds
*/
typedef uint32_t(*bmv080_callback_tick_t)(void);

/*!
* @brief Function pointer for handling the sensor's output information.
*
* @param[in] bmv080_output : Structure containing sensor output.
* @param[inout] callback_parameters : user defined parameters to be passed to the callback function.
*/
typedef void(*bmv080_callback_data_ready_t)(bmv080_output_t bmv080_output, void* callback_parameters);

#endif /* BMV080_DEFS_H_ */
