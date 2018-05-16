#include "../inc/stm32l476xx.h"

//#include "led_button.h"

#define LED_gpio GPIOA
#define LED1_pin 5
#define LED2_pin 6
#define LED3_pin 7
#define LED4_pin 8

#define BUTTON_gpio GPIOC
#define BUTTON_pin 13

int init_led(GPIO_TypeDef* gpio, int LED_pin){
	if (gpio == GPIOA) RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	else if (gpio == GPIOB) RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	else return -1;
	gpio->MODER &= ~(0b11 << (2*LED_pin));
	gpio->MODER |= (0b01 << (2*LED_pin));
	return 0;
}

int init_button(GPIO_TypeDef* gpio, int button_pin){
	// Enable AHB2 Clock
	if(gpio==GPIOC){
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
	}
	else{
		// Error! Add other cases to suit other GPIO pins
		return -1;
	}

	// Set GPIO pins to input mode (00)
	// First Clear bits(&) then set bits(|)
	gpio->MODER &= ~(0b11 << (2*button_pin));
	gpio->MODER |= (0b00 << (2*button_pin));

	return 0;
}

void set_gpio(GPIO_TypeDef* gpio, int pin){
	gpio->BSRR |= (1 << pin);
}
void reset_gpio(GPIO_TypeDef* gpio, int pin){
	gpio->BRR |= (1 << pin);
}

int read_gpio(GPIO_TypeDef* gpio, int pin){
	return (gpio->IDR >> pin) & 1;
}

void delay_without_interrupt(int msec){
	int loop_cnt = 500*msec;
	while(loop_cnt){
		loop_cnt--;
	}
	return;
}

int main(){
	if (init_led(LED_gpio, LED1_pin) != 0) return -1;
	if (init_led(LED_gpio, LED2_pin) != 0) return -1;
	if (init_led(LED_gpio, LED3_pin) != 0) return -1;
	if (init_led(LED_gpio, LED4_pin) != 0) return -1;
	if (init_button(BUTTON_gpio, BUTTON_pin) != 0) return -1;
	int shift_direction = 0;
	int led_num = 1;
	int led_data = 0b000010;
	int leds[4] = {LED1_pin, LED2_pin, LED3_pin, LED4_pin};
	int button_press_cycle_per_second = 10;
	int debounce_cycle = 100;
	int debounce_threshold = debounce_cycle*0.7;
	int last_button_state = 0;

	while(1){
		for (int c =0;c<button_press_cycle_per_second;c++){

			int pos_cnt = 0;
			for (int b = 0;b<debounce_cycle;b++){
				if (read_gpio(BUTTON_gpio, BUTTON_pin) == 0) pos_cnt++;
				delay_without_interrupt(1000/(button_press_cycle_per_second*debounce_cycle));
			}

			if (pos_cnt > debounce_threshold){
				last_button_state = 1;
			}
			else{
				if (last_button_state == 1){
					led_num = (led_num) % 3+1;
					led_data = (1 << led_num)-1;
					shift_direction = 0;
				}
				last_button_state = 0;
			}

			for (int a=0;a<4;a++){
				if ( (led_data >> (a+1)) & 0x1){
					reset_gpio(LED_gpio, leds[a]);
				}
				else{
					set_gpio(LED_gpio, leds[a]);
				}
			}
			if (shift_direction == 0) led_data <<= 1;
			else led_data >>= 1;

			if ((led_data & 0x1) == 1 || (led_data & 0b100000) == 0b100000) shift_direction = 1-shift_direction;
			delay_without_interrupt(100);
		}



	}
	return 0;
}
