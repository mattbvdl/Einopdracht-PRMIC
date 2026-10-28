
/* Copyright (C) 2026, HvA
** All rights reserved.
**
** Unauthorized copying of this file, via any medium is strictly prohibited
** Proprietary and confidential
**
** Written by M.B. van der Linden, October 2026
*/

#define F_CPU 32000000UL
#define INT_DIV 16                              // Aantal timer-overflows per meetinterval

#define MINRES 3480                             // ADC-waarde bij 25 graden
#define MAXRES 2470                             // ADC-waarde bij 40 graden
#define MINTEMP 25.0f                           // Temperatuur bij MINRES
#define MAXTEMP 40.0f                           // Temperatuur bij MAXRES

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "clock.h"
#include "serialF0.h"
#include "ucglib_xmega.h"

// Variabelen voor de timerinterrupt
volatile uint8_t iflag = 0;                             // Vlag voor het uitvoeren van een ADC-meting
volatile uint8_t int_div = INT_DIV;                     // Teller voor het aantal timer-overflows


/*
 * Bron: Wim Dolman, De taal C en de Xmega,
 * Hoofdstuk 20, code 20.1, pagina 338.
 */
void init_adc(void)
{
    PORTB.DIRCLR = PIN1_bm;                             // Stel PB1 in als ingang voor ADCB
    ADCB.CH0.MUXCTRL = ADC_CH_MUXPOS_PIN1_gc;           // Verbind PB1 met ADC-kanaal 0
    ADCB.CH0.CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;    // Stel kanaal 0 in op single-ended
    ADCB.REFCTRL = ADC_REFSEL_INTVCC_gc;                // Gebruik VCC/1,6 als referentiespanning
    ADCB.CTRLB = ADC_RESOLUTION_12BIT_gc;               // Stel de ADC in op 12-bit unsigned
    ADCB.PRESCALER = ADC_PRESCALER_DIV256_gc;           // Stel de ADC-klok in op 32 MHz / 256 = 125 kHz
    ADCB.CTRLA = ADC_ENABLE_bm;                         // Schakel ADCB in
}


/*
 * Bron: Wim Dolman, De taal C en de Xmega,
 * Hoofdstuk 20, code 20.1, pagina 338.
 */
uint16_t read_adc(void)
{
    uint16_t res;                                       // Variabele voor het 12-bit ADC-resultaat

    ADCB.CH0.CTRL |= ADC_CH_START_bm;                   // Start de ADC-conversie op kanaal 0
    while (!(ADCB.CH0.INTFLAGS & ADC_CH_CHIF_bm));      // Wacht totdat de conversie voltooid is

    res = ADCB.CH0.RES;                                 // Lees het ADC-resultaat uit
    ADCB.CH0.INTFLAGS |= ADC_CH_CHIF_bm;                // Wis de interruptvlag van de ADC

    return res;                                         // Geef de gemeten ADC-waarde terug
}


// Initialiseer het grafische LCD
void ucg_init(ucg_t *ucg)
{
    ucg_com_fnptr ucg_xmega_func = &ucg_com_xmega_cb;   // Selecteer de communicatiefunctie voor de Xmega
    ucg_Init(ucg, ucg_dev_st7735_18x128x160,
             ucg_ext_st7735_18, ucg_xmega_func);        // Initialiseer het ST7735-display
}


// Initialiseer timer TCE0
void timer_init(void)
{
    TCE0.CTRLB = TC_WGMODE_NORMAL_gc;                   // Stel de timer in op normale telmodus
    TCE0.CTRLA = TC_CLKSEL_DIV1_gc;                     // Gebruik de systeemklok zonder prescaler
    TCE0.INTCTRLA = TC_OVFINTLVL_LO_gc;                 // Schakel overflow-interrupt op laag niveau in
    TCE0.PER = 31999;                                   // Genereer iedere 1 ms een timer-overflow
}


int main(void)
{
    // Initialiseer de microcontroller
    init_clock();                                       // Stel de systeemklok in op 32 MHz
    timer_init();                                       // Initialiseer timer TCE0
    init_stream(F_CPU);                                 // Initialiseer de seriële communicatie

    // Initialiseer de externe LED
    PORTD.DIRSET = PIN0_bm;                             // Stel PD0 in als uitgang
    PORTD.OUTCLR = PIN0_bm;                             // Schakel de externe LED uit

    sei();                                              // Schakel globale interrupts in

    // Initialiseer het grafische LCD
    ucg_t ucg;
    ucg_init(&ucg);                                     // Initialiseer het display
    ucg_SetFontMode(&ucg, UCG_FONT_MODE_TRANSPARENT);   // Stel een transparante tekstachtergrond in
    ucg_ClearScreen(&ucg);                              // Wis het scherm
    ucg_SetFont(&ucg, ucg_font_8x13_mr);                // Selecteer het lettertype
    ucg_SetColor(&ucg, 0, 255, 255, 0);                 // Stel kleurindex 0 in op geel
    ucg_SetRotate180(&ucg);                             // Draai de schermoriëntatie 180 graden
    // ucg_SetColor(&ucg, 0, 255, 180, 180);               // Stel kleurindex 0 in op lichtroze
    // ucg_DrawString(&ucg, 0, 50, 0, "OUT");              // Schrijf de tekst OUT op het scherm
    // ucg_DrawLine(&ucg, 20, 50, 140, 50);                // Teken een horizontale lijn

    // Initialiseer de ADC
    uint16_t res;                                       // Variabele voor de gemeten ADC-waarde
    init_adc();                                         // Initialiseer ADCB

    while (1)
    {
        if (iflag)                                      // Controleer of een nieuwe meting nodig is
        {
            iflag = 0;                                  // Wis de meetvlag

            res = read_adc();

            float OUT = MINTEMP + (((MAXTEMP - MINTEMP) / (MAXRES - MINRES)) * ((float)res - MINRES));

            char buffer[20];

            ucg_SetFontMode(&ucg, UCG_FONT_MODE_SOLID);
            ucg_SetColor(&ucg, 0, 255, 255, 255);       // Tekstkleur wit
            ucg_SetColor(&ucg, 1, 0, 0, 0);             // Achtergrondkleur zwart

            sprintf(buffer, "ADC = %u", res);
            ucg_DrawString(&ucg, 0, 60, 0, buffer);     // Toon ADC-waarde

            sprintf(buffer, "Temp = %d C", (int)OUT);
            ucg_DrawString(&ucg, 0, 80, 0, buffer);     // Toon temperatuur
        }
    }
}


// Interrupt Service Routine voor timer TCE0
ISR(TCE0_OVF_vect)
{
    int_div--;                                          // Verlaag de interruptteller

    if (int_div == 0)                                   // Controleer of het meetinterval verstreken is
    {
        iflag = 1;                                      // Geef aan dat een ADC-meting nodig is
        int_div = INT_DIV;                              // Herstart de interruptteller
    }
}
