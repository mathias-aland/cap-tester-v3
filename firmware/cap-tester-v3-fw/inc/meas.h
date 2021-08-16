/*
 * meas.h
 *
 *  Created on: 14 feb. 2021
 *      Author: Mathias
 */

#ifndef INC_MEAS_H_
#define INC_MEAS_H_



typedef enum {  MEAS_STATE_NONE,
				MEAS_STATE_IDLE,
                MEAS_STATE_C_INIT,
                MEAS_STATE_C_START,
                MEAS_STATE_C_DIS_CHECK_0,
                MEAS_STATE_C_DIS_CHECK_1,
                MEAS_STATE_C_DIS_CHECK_2,
                MEAS_STATE_C_DIS_CHECK_3,
                MEAS_C_CHG_1,
                MEAS_C_CHG_2,
                MEAS_C_CHG_3,
                MEAS_C_CHG_4,
				MEAS_ESR_INIT,
				MEAS_ESR_MEAS,
				MEAS_ESR_READY,
                MEAS_STATE_MEAS,
                MEAS_STATE_DCR_INIT,
                MEAS_STATE_DCR_MEAS,
                MEAS_STATE_DCR_COMPLETE,


} measurement_state_t;


typedef enum  {MEAS_LAST_RES_NONE, MEAS_LAST_RES_CHG_TIMEOUT=1, MEAS_LAST_RES_OK=2} meas_res_status_t;

typedef struct
{
    meas_res_status_t status;
    uint32_t c_time;
    float capacitance;
    uint8_t range;
} meas_c_result_t;

typedef struct
{
    meas_res_status_t status;
    float dcr;
    float zero;
    uint8_t range;
} meas_dcr_result_t;




extern meas_dcr_result_t xdata meas_dcr_result;
extern meas_c_result_t xdata meas_c_result;

extern volatile measurement_state_t measurement_curState;
extern volatile measurement_state_t measurement_reqState;

extern uint8_t meas_sel_curr;
extern uint8_t meas_sel_curr_c;

void measurement_esrStart();
void measurement_capStart();
void measurement_stateMachine();

#endif /* INC_MEAS_H_ */
