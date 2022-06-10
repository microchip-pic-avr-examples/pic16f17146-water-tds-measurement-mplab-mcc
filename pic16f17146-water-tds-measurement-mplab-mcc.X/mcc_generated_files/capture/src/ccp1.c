/**
  CCP1 Generated Driver File

  @Company
    Microchip Technology Inc.

  @File Name
    ccp1.c

  @Summary
    This is the generated driver implementation file for the CCP1 driver using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  @Description
    This source file provides implementations for driver APIs for CCP1.
    Generation Information :
        Driver Version    :  2.01
    The generated drivers are tested against the following:
        Compiler          :  XC8 v2.20 and above
         MPLAB 	          :  MPLABX v5.40
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

 /**
   Section: Included Files
 */

#include <xc.h>
#include "../ccp1.h"

static void (*CCP1_CallBack)(uint16_t);

/**
  Section: Capture Module APIs
*/

static void CCP1_DefaultCallBack(uint16_t capturedValue) {
    // Add your code here
}

void CCP1_Initialize(void) 
{
    // Set the CCP1 to the options selected in the User Interface
    
    // CCPM 4th rising edge; EN enabled; FMT right_aligned; 
    CCP1CON = 0x86;
    
    // CTS CCP1PPS; 
    CCP1CAP = 0x0;
    
    // CCPRH 0; 
    CCPR1H = 0x0;
    
    // CCPRL 0; 
    CCPR1L = 0x0;

    // Set the default call back function for CCP1
    CCP1_SetCallBack(CCP1_DefaultCallBack);

    // Clear the CCP1 interrupt flag    
    PIR2bits.CCP1IF = 0;    
    
    // Enable the CCP1 interrupt
    PIE2bits.CCP1IE = 1;
}

void CCP1_CaptureISR(void)
{
    CCPR1_PERIOD_REG_T module;

    // Clear the CCP1 interrupt flag
    PIR2bits.CCP1IF = 0;
    
    // Copy captured value.
    module.ccpr1l = CCPR1L;
    module.ccpr1h = CCPR1H;
    
    // Return 16bit captured value
    CCP1_CallBack(module.ccpr1_16Bit);
}

void CCP1_SetCallBack(void (*customCallBack)(uint16_t)){
    CCP1_CallBack = customCallBack;
}
/**
 End of File
*/
