 /*
 * MAIN Generated Driver File
 * 
 * @file main.c
 * 
 * @defgroup main MAIN
 * 
 * @brief This is the generated driver implementation file for the MAIN driver.
 *
 * @version MAIN Driver Version 1.0.0
*/

/*
© [2022] Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms, you may use Microchip 
    software and any derivatives exclusively with Microchip products. 
    You are responsible for complying with 3rd party license terms  
    applicable to your use of 3rd party software (including open source  
    software) that may accompany Microchip software. SOFTWARE IS ?AS IS.? 
    NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS 
    SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT,  
    MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT 
    WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY 
    KIND WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF 
    MICROCHIP HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE 
    FORESEEABLE. TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP?S 
    TOTAL LIABILITY ON ALL CLAIMS RELATED TO THE SOFTWARE WILL NOT 
    EXCEED AMOUNT OF FEES, IF ANY, YOU PAID DIRECTLY TO MICROCHIP FOR 
    THIS SOFTWARE.
 */
#include "mcc_generated_files/system/system.h"
#include <math.h>

#define PROJECT_NAME                     ("\r \n ***-----Water TDS Meter using PIC16F17146-----*** \r \n")

#define TMR1_1_COUNT_PERIOD_DIVIDER      (8U) // 1/(0.125)
#define CCP_CAPTURING_nTH_EDGE           (4U) 
#define TIME_DIVISION_FACTOR             (TMR1_1_COUNT_PERIOD_DIVIDER * CCP_CAPTURING_nTH_EDGE) 
#define TMR1_MAX_COUNT                   (65535U) // 16-bt timer
#define MAX_TIME_CAPTURED                (690001U)
#define TDS_MAX_RANGE                    (2000U)
#define CALIBRATION_SALT_FACTOR          (0.5F) // user editable, 0.5 for NaCl, 0.71 for 422 and o.55 for KCl
#define RELAXATION_CKT_CONVERSION_FACTOR (0.14F) // conductivity = 1.4*C*Fq; = "0.14" * Fq as C is 0.1 uF
#define TMR_OVERFLOW_COUNT_LIMIT         (34U) // for TDS value 1 or less then 1

#define TEMPARATURE_MULTIPLIER           (0.01F) // need to update from TDS Probe datasheet
#define MAX_ADC_COUNT                    (4096U)
#define VDD                              (3.3F)
#define TEMPARATURE_COEFFICIENT          (0.0217F) // 2.17%
#define SERIES_RESISTANCE_NTC            (10U) // 10kOhm
#define NOMINAL_TEMPERATURE_k            (298.15F) // room temperature
#define NOMINAL_TEMPERATURE_c            (25) // room temperature
#define NOMINAL_RESISTANCE_NTC           (10U) 
#define B_NTC                            (3950U)

volatile uint8_t TMR1_overflow = 0; // used in TMR1 ISR
volatile bool TMR0_overflowFlag = false;
volatile uint16_t CCP_capturedValue = 0; // used in CCP ISR
volatile uint8_t TMR1_overflowValue = 0; // used for atomic value store in CCP ISR
volatile bool ccpCapturedFqFlag = false;

uint32_t timePeriod = 0; // timePeriod to calculate TDS value
uint16_t tdsValue = 0;
uint16_t tdsProbeTemperature = 0;
uint16_t capturedFq;

static void CCP1_UserCallBack(uint16_t capturedValue); // user call back function; called over CCP Interrupt
static void TMR1_UserOverflowCallback(void); // user ISR ; called over TMR1 Interrupt
static void TMR0_UserOverflowCallback(void); // user ISR ; called over TMR0 Interrupt
static uint8_t TMR1_GetOverflowCount(void);
static uint16_t CCP1_GetCapturedCount(void);
static uint32_t GetCapturedTimePeriod(void);
static uint16_t GetTdsProbeTemperature(void);
static uint16_t GetCapturedFrequency(uint32_t capturedTimeIn_us);
static uint16_t GetWaterTDS(uint16_t capturedFq, uint16_t temperature);
static float GetTempCompensatedConductivityValue(float conductivity, uint16_t temperature);
static void CCP1_ResetCapturedCount(void);
static void TMR1_ResetOverflowCount(void);

/*
    Main application
*/
int main(void)
{
    SYSTEM_Initialize();
    
    printf("\r \n Project Name: %s ",PROJECT_NAME);
    
    CCP1_SetCallBack(CCP1_UserCallBack); 
    Timer1.TimeoutCallbackRegister(TMR1_UserOverflowCallback); 
    Timer0.TimeoutCallbackRegister(TMR0_UserOverflowCallback);
    
    // If using interrupts in PIC18 High/Low Priority Mode you need to enable the Global High and Low Interrupts 
    // If using interrupts in PIC Mid-Range Compatibility Mode you need to enable the Global and Peripheral Interrupts 
    // Use the following macros to: 

    // Enable the Global Interrupts 
    INTERRUPT_GlobalInterruptEnable(); 

    // Disable the Global Interrupts 
    //INTERRUPT_GlobalInterruptDisable(); 

    // Enable the Peripheral Interrupts 
    INTERRUPT_PeripheralInterruptEnable(); 

    // Disable the Peripheral Interrupts 
    //INTERRUPT_PeripheralInterruptDisable(); 
    
    while (1)
    {
        if (ccpCapturedFqFlag)
        {
            ccpCapturedFqFlag = false;
            timePeriod = GetCapturedTimePeriod();
            capturedFq = GetCapturedFrequency(timePeriod);
        }
        if (TMR0_overflowFlag)
        {
            TMR0_overflowFlag = false;
            
            tdsProbeTemperature = GetTdsProbeTemperature();
            tdsValue = GetWaterTDS(capturedFq, tdsProbeTemperature);
            capturedFq = 0;
                 
            if (tdsValue == 0)
            {
                printf("\r \n Probe Disconnected");
            }
            else if (tdsValue > TDS_MAX_RANGE)
            {
                printf("\r \n TDS value out of Range!!");
            }
            else
            {
                printf("\r \n Water Temperature: %u", tdsProbeTemperature);
                printf("\r \n Water TDS: %u", tdsValue);
            }
        }
    }
}

static uint32_t GetCapturedTimePeriod(void)
{
    uint32_t tmr1TotalCount = 0;
    uint32_t capturedTimePeriod = 0; // used to store time period
    uint8_t TMR1_overflowCount = 0; // used in application to store timer overflow
    uint16_t CCP_captureCount = 0; // used in application to store CCP value

    CCP_captureCount = CCP1_GetCapturedCount();
    TMR1_overflowCount = TMR1_GetOverflowCount();
    TMR1_ResetOverflowCount();
    CCP1_ResetCapturedCount();

    if (TMR1_overflowCount <= TMR_OVERFLOW_COUNT_LIMIT)
    {
        tmr1TotalCount = TMR1_overflowCount * TMR1_MAX_COUNT;
        tmr1TotalCount = tmr1TotalCount + CCP_captureCount;
        capturedTimePeriod = tmr1TotalCount / TIME_DIVISION_FACTOR;
    }
    else
    {
        capturedTimePeriod = MAX_TIME_CAPTURED;
    }
    
    return capturedTimePeriod;
}

static uint16_t CCP1_GetCapturedCount(void)
{
    return CCP_capturedValue;
}

static uint8_t TMR1_GetOverflowCount(void)
{
    return TMR1_overflowValue;
}

static void TMR1_ResetOverflowCount(void)
{
    TMR1_overflowValue = 0;
}

static void CCP1_ResetCapturedCount(void)
{
    CCP_capturedValue = 0;
}

static uint16_t GetCapturedFrequency(uint32_t capturedTimeIn_us)
{
    uint16_t fq = 0;

    fq = (uint16_t) (1000000 / capturedTimeIn_us);
    fq = fq + (fq >> 4); // Multiplier by 1.25 for compensation

    return fq;
}

static uint16_t GetTdsProbeTemperature(void)
{
    uint16_t adcCount = 0;
    float probeTemperature = 0;

    adcCount = ADCC_GetSingleConversion(channel_Temperature);

    probeTemperature = adcCount * (VDD / MAX_ADC_COUNT); // convert ADC count to equivalent voltage
    probeTemperature = (probeTemperature * SERIES_RESISTANCE_NTC) / (VDD - probeTemperature); // convert voltage into resistance

    // use R_NTC value in steinhart and hart equation Calculate temperature value in kelvin
    probeTemperature = 1 / ((1 / NOMINAL_TEMPERATURE_k) + ((logf(probeTemperature / NOMINAL_RESISTANCE_NTC)) / B_NTC));

    probeTemperature = probeTemperature - 273.15F; // converting kelvin to celsius

    return (uint16_t) probeTemperature;
}

static uint16_t GetWaterTDS(uint16_t capturedFq, uint16_t temperature)
{
    float conductivity = 0.0;
    uint16_t waterTDS = 0;

    conductivity = (RELAXATION_CKT_CONVERSION_FACTOR * capturedFq);
    conductivity = GetTempCompensatedConductivityValue(conductivity, temperature);
    
    waterTDS = (uint16_t) (CALIBRATION_SALT_FACTOR * conductivity);

    return waterTDS;
}

static float GetTempCompensatedConductivityValue(float conductivity, uint16_t temperature)
{
    /*
     * Equation for temperature compensation
     * ?(T) = ?(To) ( 1 + ?(T - To))
     * where ?(T) = conductivity at T temperature
     * ?(To) = conductivity at To temperature (25 degree C)
     * ? = Temperature Coefficient (For NaCl = 2.17%)
     */
    return (conductivity * (1.0F + TEMPARATURE_COEFFICIENT * ((int16_t)temperature - NOMINAL_TEMPERATURE_c)));
}

static void CCP1_UserCallBack(uint16_t capturedValue)
{
    CCP_capturedValue = capturedValue;
    TMR1_overflowValue = TMR1_overflow;
    TMR1_overflow = 0;
    Timer1_Write(0);
    ccpCapturedFqFlag = true;
}

static void TMR1_UserOverflowCallback(void)
{
    TMR1_overflow++;
}

static void TMR0_UserOverflowCallback(void)
{
    TMR0_overflowFlag = true;
}