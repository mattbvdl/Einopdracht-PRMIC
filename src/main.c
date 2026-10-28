/* Copyright (C) 2026, HvA
** All rights reserved.
**
** Unauthorized copying of this file, via any medium is strictly prohibited
** Proprietary and confidential
**
** Written by M.B. van der Linden, October 2026
*/


#define F_CPU 32000000UL

#define ADC_SAMPLES 16
#define ADC_Hysterese 20

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "clock.h"
#include "serialF0.h"
#include "ucglib_xmega.h"

// Interrupt flag for letting the main program know that
// an interrupt has occurred.
volatile uint8_t iflag = 0;

// Variable used to slow down the interrupts. Only after
// INT_DIV interrupts, will the flag be set.
#define INT_DIV 16
volatile uint8_t int_div = INT_DIV;

void init_adc(void)
{
    PORTB.DIRCLR = PIN0_bm;                                                  // configure PA2 as input for ADCA
    ADCB.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN0_gc;                                // PA2 to channel 0
    ADCB.CH0.CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;                         // channel 0 single-ended
    ADCB.REFCTRL = ADC_REFSEL_INTVCC_gc;                                     // internal VCC/1.6 reference
    ADCB.CTRLB = ADC_RESOLUTION_12BIT_gc;                                    // 12 bits conversion, unsigned
    ADCB.PRESCALER = ADC_PRESCALER_DIV16_gc;                                 // 2MHz/16 = 125kHz
    ADCB.CTRLA = ADC_ENABLE_bm;                                              // enable adc
}

uint16_t read_adc()
{
    uint16_t res = 0;
    uint16_t previous = 0;
    uint8_t i;

    for (i = 0; i < ADC_SAMPLES; i = i + 1)
    {
        ADCB.CH0.CTRL |= ADC_CH_START_bm;                                   // start ADC conversion
        while ( !(ADCB.CH0.INTFLAGS & ADC_CH_CHIF_bm) ) ;                   // wait until it's ready'

        res = res + ADCB.CH0.RES;
        ADCB.CH0.INTFLAGS = ADC_CH_CHIF_bm;                                 // reset interrupt flag
    }

    res = res / ADC_SAMPLES;                                                // return average of samples

    if (res > previous + ADC_Hysterese || res + ADC_Hysterese < previous)   //Hysterese
    {
        previous = res;
    }

    return previous;
}


void ucg_init(ucg_t *ucg) {
  ucg_com_fnptr ucg_xmega_func = &ucg_com_xmega_cb;
  ucg_Init(ucg, ucg_dev_st7735_18x128x160, ucg_ext_st7735_18, ucg_xmega_func);

}


void timer_init(void)
{
    TCE0.CTRLB = TC_WGMODE_NORMAL_gc;
    TCE0.CTRLA = TC_CLKSEL_DIV1_gc;
    TCE0.INTCTRLA = TC_OVFINTLVL_LO_gc;
    TCE0.PER = 31999;
}


int main(void)
{
    init_clock();
    timer_init();
    init_stream(F_CPU);

    // Set pin D0 (external LED) to output
    PORTD.DIRSET = PIN0_bm;
    // Turn off external LED
    PORTD.OUTCLR = PIN0_bm;

    // Enable interrupts globally
    sei();

    // Initialize the data structure of the UCG library and
    // draw some stuff on the display
    ucg_t ucg;
    ucg_init(&ucg);
    ucg_SetFontMode(&ucg, UCG_FONT_MODE_TRANSPARENT);
    ucg_ClearScreen(&ucg);
    ucg_SetFont(&ucg, ucg_font_8x13_mr);
    ucg_SetColor(&ucg, 0, 255, 255, 0);
    ucg_SetRotate180(&ucg);
    ucg_SetColor(&ucg, 0, 255, 180, 180);
    ucg_DrawString(&ucg, 0, 50, 0, "OUT");
    ucg_DrawLine(&ucg, 20, 50, 140, 50);
    
    uint16_t res;
    init_adc();

    while (1) 
    {


        if (iflag) 
        {
            iflag = 0;
            res = read_adc();
            char buffer[5];
            sprintf(buffer, "%4u", res);
            ucg_SetFontMode(&ucg, UCG_FONT_MODE_SOLID);
            ucg_SetColor(&ucg, 1, 0, 0, 0);
            ucg_DrawString(&ucg, 0, 50, 0, buffer);
        }
    }
}

ISR(TCE0_OVF_vect)
{
    // Interrupt has occurred, decrease the interrupt divider
    int_div--;

    if (int_div == 0) {
        iflag = 1;
        // Reset the interrupt divider for the next interrupts
        int_div = INT_DIV;
    }
}
