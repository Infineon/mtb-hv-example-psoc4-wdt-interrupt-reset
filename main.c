/**********************************************************************************************************************
 * \file main.c
 * \copyright Copyright (C) Infineon Technologies AG 2024
 *
 * Use of this file is subject to the terms of use agreed between (i) you or the company in which ordinary course of
 * business you are acting and (ii) Infineon Technologies AG or its licensees. If and as long as no such terms of use
 * are agreed, use of this file is subject to following:
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and
 * accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute,
 * and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the
 * Software is furnished to do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including the above license grant, this restriction
 * and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all
 * derivative works of the Software, unless such copies or derivative works are solely in the form of
 * machine-executable object code generated by a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.p
 *********************************************************************************************************************/
/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "cy_pdl.h"
#include "cybsp.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
/* WDT demo options */
#define WDT_RESET_MODE             (0U)
#define WDT_INTERRUPT_MODE         (1U)

/* Select WDT_DEMO as either WDT_RESET_MODE or WDT_INTERRUPT_MODE */
#define WDT_DEMO                   (WDT_INTERRUPT_MODE)

/* Interval between LED blinks in milliseconds */
#define DELAY_IN_MS                (200U)

/* WDT interrupt priority */
#define WDT_INTERRUPT_PRIORITY     (0U)

/* Lower, upper and warn limit of watchdog count */
#define LOWER_LIMIT                (0U)
#define UPPER_LIMIT                (40000U)
#define WARN_LIMIT                 (40000U)

/* LED states */
#define LED_STATE_ON               (1U)
#define LED_STATE_OFF              (0U)

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
/* WDT interrupt service routine configuration */
const cy_stc_sysint_t wdt_isr_cfg =
{
    .intrSrc = srss_wdt_irq_IRQn,                      /* Interrupt source is WDT interrupt */
    .intrPriority = WDT_INTERRUPT_PRIORITY             /* Interrupt priority is 0 */
};

/* Variable to check whether WDT interrupt is triggered */
volatile bool g_warn_limit_flag = false;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
/* LED blinking function */
void blink_led(GPIO_PRT_Type* base, uint32_t pinNum, uint32_t milliseconds);

/* WDT initialization function */
void init_wdt(void);

/* Serve watchdog counter */
void serve_wdt(void);

/* WDT interrupt service routine */
void wdt_isr(void);

/**********************************************************************************************************************
 * Function Name: main
 * Summary:
 *  1. Initializes BSP.
 *  2. Checks whether the reset is caused due to WDT or other reset causes and blinks LED accordingly.
 *  3. Initializes the WDT.
 *  4. Checks continuously if interrupt is triggered due to WDT and toggles LED and also clears the interrupt.
 * Parameters:
 *  none
 * Return:
 *  int
 **********************************************************************************************************************
 */
int main(void)
{
    cy_rslt_t result;
    cy_en_sysint_status_t status = CY_SYSINT_BAD_PARAM;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Check the device reset reason */
    if(CY_SYSLIB_RESET_HWWDT == Cy_SysLib_GetResetReason())
    {
        /* If the reset caused by WDT reset, blink LED thrice */
        for(uint8_t i = 0; i < 3; i++)
        {
            blink_led(CYBSP_USER_LED6_PORT, CYBSP_USER_LED6_NUM, DELAY_IN_MS);
        }
    }

    /* If the reset caused by other reset types, blink LED once */
    else
    {
        blink_led(CYBSP_USER_LED6_PORT, CYBSP_USER_LED6_NUM, DELAY_IN_MS);
    }

    /* Clear the reset cause register */
    Cy_SysLib_ClearResetReason();

        if(WDT_DEMO == WDT_INTERRUPT_MODE)
        {
            /* Initialize interrupt and its handler */
            status = Cy_SysInt_Init(&wdt_isr_cfg, wdt_isr);

            if(status != CY_SYSINT_SUCCESS)
            {
                CY_ASSERT(0);
            }

            /* Enable interrupt by enabling interrupt request and unmasking WDT interrupt */
            NVIC_EnableIRQ(wdt_isr_cfg.intrSrc);
            Cy_WDT_UnmaskInterrupt();
        }

    /* Initialize WDT */
    init_wdt();

    for (;;)
    {
        #if(WDT_DEMO == WDT_INTERRUPT_MODE)
            /* Check if the WDT interrupt has been triggered */
            if (g_warn_limit_flag == true)
            {
                /* Clear the flag */
                g_warn_limit_flag = false;

                /* Invert the state of LED */
                Cy_GPIO_Inv(CYBSP_USER_LED6_PORT, CYBSP_USER_LED6_PIN);

                /* Clear the WDT interrupt */
                Cy_WDT_ClearInterrupt();

                /* Unmask the WDT interrupt */
                Cy_WDT_UnmaskInterrupt();

                /* Service WDT */
                serve_wdt();
            }
        #elif(WDT_DEMO == WDT_RESET_MODE)
            /* Just execute an infinite loop until WDT reset occurs */
            while(1);
        #endif
    }
}

/**********************************************************************************************************************
 * Function Name: blink_led
 * Summary:
 *  This function blinks LED once.
 * Parameters:
 *  GPIO_PRT_Type* base, uint32_t pinNum, uint32_t milliseconds
 * Return:
 *  none
 **********************************************************************************************************************
*/
void blink_led(GPIO_PRT_Type* base, uint32_t pinNum, uint32_t milliseconds)
{
    Cy_GPIO_Write(base, pinNum, LED_STATE_ON);

    Cy_SysLib_Delay(milliseconds);

    Cy_GPIO_Write(base, pinNum, LED_STATE_OFF);

    Cy_SysLib_Delay(milliseconds);
}

/**********************************************************************************************************************
 * Function Name: init_wdt
 * Summary:
 *  This function initializes the WDT block and starts the WDT block.
 * Parameters:
 *  none
 * Return:
 *  none
 **********************************************************************************************************************
 */
void init_wdt(void)
{
    /* Unlock WDT registers */
    Cy_WDT_Unlock();

    /* Disable WDT before the initialization */
    Cy_WDT_Disable();

    /* Set each limit */
    Cy_WDT_SetLowerLimit(LOWER_LIMIT);
    Cy_WDT_SetUpperLimit(UPPER_LIMIT);
    Cy_WDT_SetWarnLimit(WARN_LIMIT);

    /* Set lower action none */
    Cy_WDT_SetLowerAction(CY_WDT_LOW_UPPER_LIMIT_ACTION_NONE);

    #if (WDT_DEMO == WDT_INTERRUPT_MODE)
         /* Set warn action to trigger interrupt when the count reaches warn limit */
         Cy_WDT_SetUpperAction(CY_WDT_LOW_UPPER_LIMIT_ACTION_NONE);
         Cy_WDT_SetWarnAction(CY_WDT_WARN_ACTION_INT);
    #elif(WDT_DEMO == WDT_RESET_MODE)
         /* Set upper action to reset when the count reaches upper limit */
         Cy_WDT_SetUpperAction(CY_WDT_LOW_UPPER_LIMIT_ACTION_RESET);
         Cy_WDT_SetWarnAction(CY_WDT_WARN_ACTION_NONE);
    #endif

    /* Other settings that is set under WDT unlocked and disabled */
    Cy_WDT_SetAutoService(CY_WDT_DISABLE);
    Cy_WDT_SetDeepSleepPause(CY_WDT_DISABLE);
    Cy_WDT_SetDebugRun(CY_WDT_ENABLE);

    /* Enabling WDT must be done under WDT is unlocked*/
    Cy_WDT_Enable();

    /* It takes time to enable up to three clk_lf cycles. */
    while(!Cy_WDT_IsEnabled());

    /* Service WDT to reset count before going back to main routine */
    Cy_WDT_SetService();

    /* Lock WDT registers */
    Cy_WDT_Lock();
}

/**********************************************************************************************************************
 * Function Name: serve_wdt
 * Summary:
 *  This function serve the WDT block to reset the watchdog count.
 * Parameters:
 *  none
 * Return:
 *  none
 **********************************************************************************************************************
 */
void serve_wdt(void)
{
    /* Need to unlock WDT registers before the service */
    Cy_WDT_Unlock();

    /* Serve WDT */
    Cy_WDT_SetService();

    /* Lock WDT registers */
    Cy_WDT_Lock();
}

/**********************************************************************************************************************
 * Function Name: wdt_isr
 * Summary:
 *  This function is the handler for the WDT interrupt.
 *  It just sets flag that is used to check if WDT interrupt occurs in main routine.
 * Parameters:
 *  handlerArg (unused)
 *  event (unused)
 * Return:
 *  none
 **********************************************************************************************************************
*/
void wdt_isr(void)
{
    #if (WDT_DEMO == WDT_INTERRUPT_MODE)
        /* Mask the WDT interrupt to prevent from further triggers */
        Cy_WDT_MaskInterrupt();

        /* Set the flag */
        g_warn_limit_flag = true;
    #elif(WDT_DEMO == WDT_RESET_MODE)
        /* The interrupt never occurs if it's the reset demo */
        CY_ASSERT(0);
    #endif
}

/* [] END OF FILE */
