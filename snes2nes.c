#include "defines.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include "bits.h"
#include "gamepad.h"

#define LED_ON unset_bit(PORTB, 2)
#define LED_OFF set_bit(PORTB, 2)

volatile uint8_t nes_button_data;
volatile uint8_t shift;
volatile uint8_t turbo_counter;

// Clock
ISR(INT0_vect)
{
	shift <<= 1;
	if (nes_button_data & shift)
		PORTD |= 1;
	else 
		PORTD &= ~1;
}

// Strobe
ISR(INT1_vect)
{
	shift = 1;
	if (nes_button_data & shift)
		PORTD |= 1;
	else 
		PORTD &= ~1;
}

uint16_t get_template_buttons()
{
	uint16_t snes_buttons;
	if ((snes_buttons&0x0FFF) != 0x0FFF)
	do
	{
		PORTB ^= (1<<2);
		_delay_ms(50);
		snes_buttons = get_snes_gamepad();
	} while ((snes_buttons&0x0FFF) != 0x0FFF);
	LED_OFF;
	_delay_ms(200);
	
	int hold_time = 0;
	do
	{
		PORTB ^= (1<<2);
		_delay_ms(50);
		snes_buttons = get_snes_gamepad();
		if ((snes_buttons&0x0FFF) != 0x0FFF) 
			hold_time++;
		else
			hold_time = 0;
	} while (hold_time < 30);
	LED_OFF;
	_delay_ms(500);
	return snes_buttons;
}

int main (void)
{
	set_bit(DDRD, 0); // Данные - на выход
	unset_bit2(DDRD, 2, 3); // clock, strobe - на ввод
	set_bit(DDRB, 2); // Светодиод на вывод	

	set_bit2(MCUCR, ISC11, ISC10); // Прерывание при растущем strobe
	set_bit2(MCUCR, ISC01, ISC00); // Прерывание при растущем  clock
	set_bit(GICR, INT0); set_bit(GICR, INT1); // Активируем их

	sei(); // Глобальная активация прерываний
	
	// Right, Left, Down, Up, Start, Select, B, A
	init_snes_gamepad();
	
	int lr_hold_time = 0;
	uint16_t l_template = 0x0FFF;
	uint16_t r_template = 0x0FFF;
	eeprom_read_block(&r_template, (void*)0, 2);
	eeprom_read_block(&l_template, (void*)2, 2);
	
	while(1)		
	{
		// R, L, X, A, Right, Left, Down, Up, Start, Select, Y, B
		uint16_t snes_buttons = get_snes_gamepad();
		if ((snes_buttons&0x0FFF) != 0x0FFF)
		{
			LED_ON;
		} else {
			LED_OFF;
		}
		
		if (((snes_buttons&0x0FFF) == 0x07FB) || ((snes_buttons&0x0FFF) == 0x0BFB)) // R + select or L + select
		{
			if (lr_hold_time++ == 200)
			{
				nes_button_data = 0xFF; // Unpress nes buttons
				uint16_t template_buttons = get_template_buttons();
				template_buttons |= (0b11<<10); // Exclude L&R
				if ((snes_buttons&0x0FFF) == 0x07FB)
				{
					r_template = template_buttons;
					eeprom_write_block(&r_template, (void*)0, 2);
				} else {
					l_template = template_buttons;
					eeprom_write_block(&l_template, (void*)2, 2);
				}
				snes_buttons = 0x0FFF;
			}
		} else lr_hold_time = 0;
		
		if (!(snes_buttons&(1<<11))) snes_buttons &= r_template;
		if (!(snes_buttons&(1<<10))) snes_buttons &= l_template;
		
		uint8_t tmp = snes_buttons & 0b11111100;
		tmp |= (snes_buttons>>8)&1; // A
		tmp |= (snes_buttons&1)<<1; // B
		if (!(snes_buttons&(1<<9)) && ((turbo_counter%8) <= 3)) tmp &= ~1; // X -> Turbo A
		if (!(snes_buttons&(1<<1)) && ((turbo_counter%8) <= 3)) tmp &= ~2; // Y -> Turbo B
		
		if (!(snes_buttons&(1<<7)) && !(snes_buttons&(1<<6))) tmp |= 1<<7; // No left+right!
		if (!(snes_buttons&(1<<5)) && !(snes_buttons&(1<<4))) tmp |= 1<<5; // No up+down!
		
		nes_button_data = tmp;
		_delay_ms(10);
		turbo_counter++;
	}
}


