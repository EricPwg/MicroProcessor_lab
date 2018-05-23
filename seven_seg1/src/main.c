//#include "../inc/helper_functions.c"
#include "../inc/stm32l476xx.h"
#include "../inc/7seg.h"

#define SEGgpio GPIOA
#define SEGdin 5
#define SEGcs 6
#define SEGclk 7

void set_gpio(GPIO_TypeDef* gpio, int pin){
	gpio->BSRR |= (1 << pin);
}
void reset_gpio(GPIO_TypeDef* gpio, int pin){
	gpio->BRR |= (1 << pin);
}

void delay_without_interrupt(int msec){
	int loop_cnt = 500*msec;
	while(loop_cnt){
		loop_cnt--;
	}
	return;
}

void send_7seg_onebit(GPIO_TypeDef* gpio, int DIN, int CLK, int data){
	reset_gpio(gpio, CLK);
	if (data == 1) set_gpio(gpio, DIN);
	else reset_gpio(gpio, DIN);
	set_gpio(gpio, CLK);

}

void send_7seg(GPIO_TypeDef* gpio, int DIN, int CS, int CLK, int addr, int data){
	set_gpio(gpio, CLK);
	reset_gpio(gpio, CS);
	for (int i=0;i<4;i++) send_7seg_onebit(gpio, DIN, CLK, 0);
	for (int i=0;i<4;i++){
		int t = (addr >> (3-i))&0x1;
		if (t == 1) send_7seg_onebit(gpio, DIN, CLK, 1);
		else send_7seg_onebit(gpio, DIN, CLK, 0);
	}
	for (int i=0;i<8;i++){
		int t = (data >> (7-i))&0x1;
		if (t == 1) send_7seg_onebit(gpio, DIN, CLK, 1);
		else send_7seg_onebit(gpio, DIN, CLK, 0);
	}
	reset_gpio(gpio, CLK);
	set_gpio(gpio, CS);
	set_gpio(gpio, CLK);
}

int init_7seg(GPIO_TypeDef* gpio, int DIN, int CS, int CLK){
	if(gpio==GPIOA){
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	}
	else if(gpio==GPIOB){
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	}
	else return -1;

	gpio->MODER &= ~(0b11 << (2*DIN));
	gpio->MODER |= (0b01 << (2*DIN));
	gpio->MODER &= ~(0b11 << (2*CS));
	gpio->MODER |= (0b01 << (2*CS));
	gpio->MODER &= ~(0b11 << (2*CLK));
	gpio->MODER |= (0b01 << (2*CLK));

	send_7seg(gpio, DIN, CS, CLK, SEG_ADDRESS_DISPLAY_TEST, 0x00);
	return 0;
}

int init_led(GPIO_TypeDef* gpio, int LED_pin){
	// Enable AHB2 Clock
	if(gpio==GPIOA){
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	}
	else if(gpio==GPIOB){
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	}
	else{
		// Error! Add other cases to suit other GPIO pins
		return -1;
	}

	// Set GPIO pins to output mode (01)
	// First Clear bits(&) then set bits(|)
	gpio->MODER &= ~(0b11 << (2*LED_pin));
	gpio->MODER |= (0b01 << (2*LED_pin));

	return 0;
}



int main(){
	//init_led(GPIOA, 5);
	init_7seg(SEGgpio, SEGdin, SEGcs, SEGclk);
	int t = 0;
	for (int i=1;i<=8;i++){
		send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, i, 15);
	}
	send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, 10, 20);
	while(1){
		send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, 0x9, 0xFF);
		send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, 12, 0x01);
		send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, 2, t);
		t = (t+1)%10;
		delay_without_interrupt(1000);
		/*
		for (int i=0;i<5;i++){
			send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, 10, i*3);
			delay_without_interrupt(1000);
		}
		*/
	}
	return 0;
}
