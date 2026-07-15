#ifndef DUTY_CONTROL_H
#define DUTY_CONTROL_H

/*
 * L3 three-phase duty-cycle output abstraction.
 *
 * This public header intentionally exposes no MCU or L4 driver types.
 */

typedef enum {
    DUTY_CONTROL_PHASE_A = 0,
    DUTY_CONTROL_PHASE_B,
    DUTY_CONTROL_PHASE_C,
    DUTY_CONTROL_PHASE_COUNT
} duty_control_phase_t;

/* Initialize the three-phase PWM hardware while keeping all outputs disabled. */
void DutyControl_Init(void);

/* Start or stop all three synchronized phases. */
void DutyControl_Start(void);
void DutyControl_Stop(void);

/* Set one positive-output duty cycle. The L4 driver clamps it to 0..100%. */
void DutyControl_SetPhaseDuty(duty_control_phase_t phase, float duty_percent);

/* Set all three positive-output duty cycles. */
void DutyControl_SetThreePhaseDuty(float duty_a, float duty_b, float duty_c);

/* Return the actual quantized positive-output duty cycle. */
float DutyControl_GetPhaseDuty(duty_control_phase_t phase);

#endif /* DUTY_CONTROL_H */
