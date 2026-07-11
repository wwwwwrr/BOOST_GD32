#include "main.h"

/* Function prototypes */
static void LED_Blink_1Hz(void);
static void AS5600_Test(void);
static void ADC_Test(void);

int main(void)
{
    systick_config();
    LED_Init();
    
    USART1_Init();
    
    PWM_Init(24,2);
    PWM_Start();
    
    Timer1_Algorithm_Init();
    
    /* Initialize I2C and AS5600 */
    I2C0_Init();
    AS5600_Init();
    
    /* Initialize ADC for current sampling */
    ADC_Init();
    ADC_Start();
    
    /* Calibrate ADC zero offset */
    ADC_CalibrateZeroOffset();
    
    /* Set LED blink callback for 1Hz task */
    Timer1_SetAlgorithmCallback(TIMER1_CALLBACK_1HZ, LED_Blink_1Hz);

    Set_LED(3);

    USART1_SendString("\r\n=== GD32F303CC Framework Started ===\r\n");
    USART1_SendString("Basic peripherals initialized\r\n");
    USART1_SendString("Ready for application development\r\n\r\n");

    while (1)
    {
        AS5600_Test();
        ADC_Test();
        /* Framework ready - add your application logic here */
    }
}

static void LED_Blink_1Hz(void)
{
    static uint8_t led3_state = 0;
    
    if (led3_state == 0) {
        Set_LED(3);
        led3_state = 1;
    } else {
        Reset_LED(3);
        led3_state = 0;
    }
}

static void AS5600_Test(void)
{
    static uint32_t last_print_time = 0;
    static uint16_t angle_count = 0;
    as5600_data_t sensor_data;
    i2c_status_t status;
    
    /* Read AS5600 sensor data */
    status = AS5600_ReadAll(&sensor_data);
    
    if (status == I2C_OK)
    {
        /* Check magnet status */
        as5600_magnet_status_t magnet_status = AS5600_CheckMagnet();
        
        /* Print sensor data every 500ms */
        if (systick_get_tick() - last_print_time > 500)
        {
            last_print_time = systick_get_tick();
            
            switch (magnet_status)
            {
                case AS5600_MAGNET_OK:
                    USART1_SendString("Magnet: OK | ");
                    break;
                case AS5600_MAGNET_TOO_STRONG:
                    USART1_SendString("Magnet: TOO STRONG | ");
                    break;
                case AS5600_MAGNET_TOO_WEAK:
                    USART1_SendString("Magnet: TOO WEAK | ");
                    break;
                case AS5600_MAGNET_NOT_DETECTED:
                    USART1_SendString("Magnet: NOT DETECTED | ");
                    break;
            }
            
            /* Create a simple formatted output */
            char buffer[128];
            sprintf(buffer, "Angle: %4d (%6.1f°) | Raw: %4d | Mag: %4d | AGC: %3d\r\n",
                    sensor_data.angle, sensor_data.angle_deg,
                    sensor_data.raw_angle, sensor_data.magnitude, sensor_data.agc);
            USART1_SendString(buffer);
            
            /* Blink LED2 for visual feedback */
            if ((angle_count % 5) == 0)
            {
                static uint8_t led2_state = 0;
                if (led2_state == 0)
                {
                    Set_LED(2);
                    led2_state = 1;
                }
                else
                {
                    Reset_LED(2);
                    led2_state = 0;
                }
            }
            
            angle_count++;
        }
    }
    else if (status == I2C_TIMEOUT)
    {
        /* I2C timeout - device not responding */
        if (systick_get_tick() - last_print_time > 1000)
        {
            last_print_time = systick_get_tick();
            USART1_SendString("AS5600: I2C Timeout - Check connection\r\n");
            Reset_LED(2);
        }
    }
    else
    {
        /* Other I2C error */
        if (systick_get_tick() - last_print_time > 1000)
        {
            last_print_time = systick_get_tick();
            USART1_SendString("AS5600: I2C Error\r\n");
            Reset_LED(2);
        }
    }
}

/*!
    \brief      ADC test function - reads and prints current measurements
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void ADC_Test(void)
{
    static uint32_t last_print_time = 0;
    static uint16_t sample_count = 0;
    adc_sample_t sample;
    adc_status_t status;
    
    /* Read ADC sample */
    status = ADC_GetSample(&sample);
    
    if (status == ADC_STATUS_OK)
    {
        /* Print sample data every 200ms */
        if (systick_get_tick() - last_print_time > 200)
        {
            last_print_time = systick_get_tick();
            
            /* Create formatted output */
            char buffer[192];
            sprintf(buffer, "ADC: Phase A: %4u (%.3fV, %+.2fA) | Phase B: %4u (%.3fV, %+.2fA)\r\n",
                    sample.phase_a_raw, sample.phase_a_voltage, sample.phase_a_current,
                    sample.phase_b_raw, sample.phase_b_voltage, sample.phase_b_current);
            USART1_SendString(buffer);
            
            /* Blink LED1 for visual feedback */
            if ((sample_count % 3) == 0)
            {
                static uint8_t led1_state = 0;
                if (led1_state == 0)
                {
                    Set_LED(1);
                    led1_state = 1;
                }
                else
                {
                    Reset_LED(1);
                    led1_state = 0;
                }
            }
            
            sample_count++;
            
            /* Check for calibration request (if raw value indicates need) */
            if (sample_count % 50 == 0)
            {
                /* Periodically re-calibrate if needed */
                if (fabs(sample.phase_a_current) > 0.5f || fabs(sample.phase_b_current) > 0.5f)
                {
                    USART1_SendString("ADC: Recalibrating zero offset...\r\n");
                    ADC_CalibrateZeroOffset();
                }
            }
        }
    }
    else if (status == ADC_STATUS_NOT_INITIALIZED)
    {
        /* ADC not initialized */
        if (systick_get_tick() - last_print_time > 2000)
        {
            last_print_time = systick_get_tick();
            USART1_SendString("ADC: Not initialized\r\n");
        }
    }
    else
    {
        /* ADC error */
        if (systick_get_tick() - last_print_time > 1000)
        {
            last_print_time = systick_get_tick();
            USART1_SendString("ADC: Error reading sample\r\n");
        }
    }
}


