/*
 * Copyright (c) 2015-2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== gpiointerrupt.c ========
 */
#include <stdint.h>
#include <stddef.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>

/* Driver configuration */
#include "ti_drivers_config.h"
#include <ti/drivers/Timer.h>

/* housekeeping variables */
volatile unsigned char TimerFlag = 0;
volatile unsigned char change_message = 0;
volatile unsigned char message_ended = 0;
static unsigned int sos_length = 34;
static unsigned int ok_length = 30;

/*
 * --- set up timer ---
 */
void timerCallback(Timer_Handle myHandle, int_fast16_t status)
{
    TimerFlag = 1;
}

void initTimer(void)
{
    Timer_Handle timer0;
    Timer_Params params;

    Timer_init();
    Timer_Params_init(&params);
    params.period = 1000000;
    params.periodUnits = Timer_PERIOD_US;
    params.timerMode = Timer_CONTINUOUS_CALLBACK;
    params.timerCallback = timerCallback;

    timer0 = Timer_open(CONFIG_TIMER_0, &params);

    if (timer0 == NULL) {
        /* Failed to initialize timer */
        while (1) {}
    }

    if (Timer_start(timer0) == Timer_STATUS_ERROR) {
        /* Failed to start timer */
        while (1) {}
    }
}

/*
 *  ======== gpioButtonFxn0 ========
 *  Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_0.
 *
 *  Note: GPIO interrupts are cleared prior to invoking callbacks.
 */
void gpioButtonFxn0(uint_least8_t index)
{
    /* set change_message = 1 if button pressed
     * at least during the message's cycle */
    change_message = 1;
}

/*
 *  ======== gpioButtonFxn1 ========
 *  Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_1.
 *  This may not be used for all boards.
 *
 *  Note: GPIO interrupts are cleared prior to invoking callbacks.
 */
void gpioButtonFxn1(uint_least8_t index)
{
    /* set change_message = 1 if button pressed
     * at least during the message's cycle */
    change_message = 1;
}

/* --- functions to switch on one or other, or both, or neither of the LEDs --- */
// red light on, green light off
void red_light_only() {
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
    GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);
}

// green light on, red light off
void green_light_only() {
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_ON);
}

// both lights off
void neither_light() {
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);
}

// both lights on
void both_lights() {
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
    GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_ON);
}

/* --- functions to switch on one or other, or both, or neither of the LEDs ---
 * signal 'SOS'
 * @param phase -> the phase of the current message*/
void flash_sos(int phase) {

    switch (phase) {
        // red light for dots, green light off
        case 1:
        case 3:
        case 5:
        case 23:
        case 25:
        case 27: // fall through
            red_light_only();
            break;

        // green light for dashes, red light off
        case 9:
        case 10:
        case 11:
        case 13:
        case 14:
        case 15:
        case 17:
        case 18:
        case 19: // fall through
            green_light_only();
            break;

        // both lights off between dots/dashes, letters and words
        default:
            neither_light();
            break;
    }

}

/* signal 'SOS'
 * @param phase -> the phase of the current message*/
void flash_ok(int phase) {

    switch (phase) {
        // green light for dashes, red light off
        case 1:  // first dash in 'O'
        case 2:
        case 3:
        case 5:  // second dash in 'O'
        case 6:
        case 7:
        case 9:  // third dash in 'O'
        case 10:
        case 11:
        case 15: // first dash in 'K'
        case 16:
        case 17:
        case 21: // second dash in 'K'
        case 22:
        case 23: // fall through
            green_light_only();
            break;

        // red light for dots, green light off
        case 19: // dot in 'K'
            red_light_only();
            break;

        // both lights off between dots/dashes, letters and words
        default:
            neither_light();
            break;
    }
}

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    int phase;                      // counter for the phase of the current message
    unsigned int message_length;    // message_length, effectively max phase value for current message
    enum states {                   // possible states
        sos_loop,
        ok_loop,
    } state;

    // initialize variables
    state = sos_loop;
    phase = 0;
    message_length = sos_length;

    /* Call driver init functions */
    GPIO_init();

    /* Configure the LED and button pins */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_LED_1, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_0, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    /* Turn all LEDs off to begin with */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
    GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF);


    /* Install Button callback */
    GPIO_setCallback(CONFIG_GPIO_BUTTON_0, gpioButtonFxn0);

    /* Enable interrupts */
    GPIO_enableInt(CONFIG_GPIO_BUTTON_0);

    /*
     *  If more than one input pin is available for your device, interrupts
     *  will be enabled on CONFIG_GPIO_BUTTON1.
     */
    if (CONFIG_GPIO_BUTTON_0 != CONFIG_GPIO_BUTTON_1) {
        /* Configure BUTTON1 pin */
        GPIO_setConfig(CONFIG_GPIO_BUTTON_1, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

        /* Install Button callback */
        GPIO_setCallback(CONFIG_GPIO_BUTTON_1, gpioButtonFxn1);

        /* Enable interrupts */
        GPIO_enableInt(CONFIG_GPIO_BUTTON_1);
    }

    // initialize timer and counter variables
    initTimer();
    unsigned long checkTime = 0;
    const unsigned long checkPeriod = 500;

    // flash both lights on and off (500ms) thrice to test that they are working correctly
    unsigned int i = 0;
    while (i < 3) {

        both_lights();

        while (!TimerFlag) {}
        TimerFlag = 0;
        checkTime += 100;

        neither_light();

        while (!TimerFlag) {}
        TimerFlag = 0;
        checkTime += 100;

        ++i;
    }

    // main loop to toggle between 'SOS' and 'OK' messages
    while(1) {

       /* increment phase first as it initializes/resets to zero but
        * phase=1 is the first phase defined for each message
        */
       ++phase;

        if (checkTime >= checkPeriod) {

            switch (state) { // actions
                case sos_loop:
                    flash_sos(phase);
                    break;
                case ok_loop:
                    flash_ok(phase);
                    break;
            }
        }

        /* set message_ended = 1 and phase = 0 if current
         * message has reached its final phase; otherwise, set
         * message_ended = 0
         *
         */
        if (phase >= message_length)
        {
            message_ended = 1;
            phase = 0;
        }

        else {
            message_ended = 0;
        }


        /*
         * transition to next state iff button has been prssed at least once
         * and the current message has reached its end
         */
        if (change_message == 1 && message_ended == 1) {

            switch (state) { // transitions
                case sos_loop:
                    state = ok_loop;
                    change_message = 0;
                    message_length = ok_length;
                    break;
                case ok_loop:
                    state = sos_loop;
                    change_message = 0;
                    message_length = sos_length;
                    break;
            }

        }

        while (!TimerFlag) {}
        TimerFlag = 0;
        checkTime += 100;
    }

    return (NULL);
}
