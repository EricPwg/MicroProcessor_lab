//#include "../inc/helper_functions.c"
#include "../inc/stm32l476xx.h"
#include "../inc/7seg.h"

#define SEGgpio GPIOA
#define SEGdin 5
#define SEGcs 6
#define SEGclk 7

#define BUTTON_gpio GPIOC
#define BUTTON_pin 13

void set_gpio(GPIO_TypeDef* gpio, int pin){
	gpio->BSRR |= (1 << pin);
}
void reset_gpio(GPIO_TypeDef* gpio, int pin){
	gpio->BRR |= (1 << pin);
}

int read_gpio(GPIO_TypeDef* gpio, int pin){
	return (gpio->IDR >> pin) & 1;
}

void delay_without_interrupt_2(int msec_2){
	int loop_cnt = 5*msec_2;
	while(loop_cnt){
		loop_cnt--;
	}
	return;
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

int init_button(GPIO_TypeDef* gpio, int button_pin){
	if(gpio==GPIOC){
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
	}
	else{
		return -1;
	}
	gpio->MODER &= ~(0b11 << (2*button_pin));
	gpio->MODER |= (0b00 << (2*button_pin));

	return 0;
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

void display7seg(int n){
	if (n == 0){
		send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, 1, 0);
		for (int i=2;i<=8;i++) send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, i, 15);
		return ;
	}
	else if (n < 0){
		send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, 1, 1);
		send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, 2, 10);
		for (int i=3;i<=8;i++) send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, i, 15);
		return ;
	}
	for (int i=1;i<=8;i++){
		if (n == 0)send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, i, 15); // send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, i, 15);
		else{
			send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, i, n % 10);
			n/=10;
		}

	}
}

int main(){

	int debounce_cycle = 100;
	int debounce_threshold = 70;
	int last_button_state = 0;

	if (init_button(BUTTON_gpio, BUTTON_pin) != 0) return -1;
	init_7seg(SEGgpio, SEGdin, SEGcs, SEGclk);
	send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, 0x9, 0xFF);
	send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, 12, 0x01);
	send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, 10, 5);
	/*
	for (int i=2;i<=8;i++){
		send_7seg(SEGgpio, SEGdin, SEGcs, SEGclk, i, 15);
	}
	*/
	int t = 15;
	int cur_num = 0;
	int last_num = 0;
	//display7seg(35);
	delay_without_interrupt(1000);
	int cur = 1;
	int pre = 0;
/*
	while(1){
		display7seg(cur);
		int t = cur+pre;
		pre = cur;
		cur = t;
		delay_without_interrupt(1000);
	}
*/
	while(1){
		int pt = 0;
		while(read_gpio(BUTTON_gpio, BUTTON_pin) == 0){
			int pos_cnt = 0;
			for (int b = 0;b<debounce_cycle;b++){
				if (read_gpio(BUTTON_gpio, BUTTON_pin) == 0) pos_cnt++;
				//delay_without_interrupt(1);
			}

			if (pos_cnt > debounce_threshold) pt++;
			//display7seg(pt);
		}
		//display7seg(pt);
		//if (pt >0 )
		//delay_without_interrupt(1000);
		//continue;
		if (pt > 800) cur_num = 0;
		else if (pt > 0){
			if (cur_num == 0){
				cur_num = 1;
				last_num = 0;
			}
			else{
				int t = cur_num;
				cur_num = cur_num + last_num;
				last_num = t;
			}
		}

		display7seg(cur_num);
	}

	return 0;
}
