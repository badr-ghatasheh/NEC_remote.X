#include <stdint.h>        /* For uint8_t definition */
#include <stdbool.h>       /* For true/false definition */
#include <delays.h>
#include <xc.h>

#define P16F84A

#if defined(P16F84A)
    #pragma config FOSC = HS        // Oscillator Selection bits (HS oscillator)
    #pragma config WDTE = OFF       // Watchdog Timer (WDT disabled)
    #pragma config PWRTE = OFF      // Power-up Timer Enable bit (Power-up Timer is disabled)
    #pragma config CP = OFF         // Code Protection bit (Code protection disabled)
#elif defined(P16F877A)
    #pragma config FOSC = HS        // Oscillator Selection bits (HS oscillator)
    #pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
    #pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
    #pragma config BOREN = ON       // Brown-out Reset Enable bit (BOR enabled)
    #pragma config LVP = OFF        // Low-Voltage (Single-Supply) In-Circuit Serial Programming Enable bit (RB3 is digital I/O, HV on MCLR must be used for programming)
    #pragma config CPD = OFF        // Data EEPROM Memory Code Protection bit (Data EEPROM code protection off)
    #pragma config WRT = OFF        // Flash Program Memory Write Enable bits (Write protection off; all program memory may be written to by EECON control)
    #pragma config CP = OFF         // Flash Program Memory Code Protection bit (Code protection off)
#endif

#define _XTAL_FREQ 4000000L  // required by delays.h, crystal frequency

// button commands, I've decoded those values earlier using a similar program
#define txt_command     0x44
#define sub_command     0x45
#define zoom_command    0x42
#define epg_command     0x17

/*
 * Initialize ports tri state buffers, timer 0 and ADC if exists
 */
void init() {
    TRISA = 0xFF; // Set PORTA as input port
    TRISB = 0x00; // Set PORTB as output port
    PORTB = 0b01110000 & EEPROM_READ(0); // read the last state from eeprom address 0
    OPTION_REG = 0b11010101; // Setup Timer 0 Module, internal clock with 64 divider
    // Timer 0 Frequency = FCYC / 64 = 1MHz / 64 = 15.625KHz
    #if defined(P16F877A)
     ADCON1 = 0b00000110; // this is in case of using PIC16F877A, turn off ADC module
    #endif
}

/*
 * This function writes the new states [PORTB value] to the eeprom
 * and lights an indication LED on pin RB7 for 500ms
 * those 500ms is also used to provide time separation between bounces
 */
void update_status() {
    EEPROM_WRITE(0,PORTB);
    __delay_ms(500L);
}
void main(void)
{
    init();

    unsigned int time = 0;
    short start_bit = 0;
    unsigned int command;
    unsigned int index = 0;

    while(1) // infinite loop
    {
        TMR0 = 0x00; // Reset Timer
        while (PORTAbits.RA2 == 1); // wait until sensor changes from high to low
        time = TMR0; // get timer 0 register value
        if (time >= 67 && time <= 71) { // Start bit detected {around 4.5ms}
            start_bit = 1; // set flag
            command = 0; // reset old command value
        }
        
        if (start_bit) {
            // Logic 1 is around 1690us, around 26 timer clicks
            // Logic 0 otherwise {560us} around 8 timer clicks
            if (index > 16 && time >= 24 && time <= 26) { // discard address bits [for the sake of simplicity
                // setting each bit of the command variable according to the corresponding decoded value in the stream
                // our command starts at index 17 [1 start bit + 16 address bits = 17]
                command = command | (1 << (index - 17));
            }
            index++;
        }

        while (PORTAbits.RA2 == 0); // wait until sensor flips
        
        if(index == 24) // a full 8 bit command received
        {
            // Toggle outputs depending on the value of the command
            switch(command) {
                case txt_command:
                    PORTBbits.RB4 = ~PORTBbits.RB4;
                    update_status();
                    break;
                case sub_command:
                    PORTBbits.RB5 = ~PORTBbits.RB5;
                    update_status();
                    break;
                case zoom_command:
                    PORTBbits.RB6 = ~PORTBbits.RB6;
                    update_status();
                    break;
            }

            // reset values for another round
            index = 0;
            start_bit = 0;
        }
    }
}

