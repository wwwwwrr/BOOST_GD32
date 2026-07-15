#include "duty_control.h"
#include "pwm.h"

static pwm_phase_t DutyControl_MapPhase(duty_control_phase_t phase);

void DutyControl_Init(void)
{
    PWM_Init();
}

void DutyControl_Start(void)
{
    PWM_Start();
}

void DutyControl_Stop(void)
{
    PWM_Stop();
}

void DutyControl_SetPhaseDuty(duty_control_phase_t phase, float duty_percent)
{
    if ((unsigned int)phase >= (unsigned int)DUTY_CONTROL_PHASE_COUNT) {
        return;
    }

    PWM_SetDutyCycle(DutyControl_MapPhase(phase), duty_percent);
}

void DutyControl_SetThreePhaseDuty(float duty_a, float duty_b, float duty_c)
{
    PWM_SetThreePhaseDuty(duty_a, duty_b, duty_c);
}

float DutyControl_GetPhaseDuty(duty_control_phase_t phase)
{
    if ((unsigned int)phase >= (unsigned int)DUTY_CONTROL_PHASE_COUNT) {
        return 0.0f;
    }

    return PWM_GetDutyCycle(DutyControl_MapPhase(phase));
}

static pwm_phase_t DutyControl_MapPhase(duty_control_phase_t phase)
{
    pwm_phase_t pwm_phase;

    switch (phase) {
    case DUTY_CONTROL_PHASE_A:
        pwm_phase = PWM_PHASE_A;
        break;
    case DUTY_CONTROL_PHASE_B:
        pwm_phase = PWM_PHASE_B;
        break;
    case DUTY_CONTROL_PHASE_C:
        pwm_phase = PWM_PHASE_C;
        break;
    default:
        pwm_phase = PWM_PHASE_A;
        break;
    }

    return pwm_phase;
}
