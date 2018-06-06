#include "stm32l476xx.h"
#include "7seg.h"
#include "keypad.h"
#include "helper_functions.h"
#include "led_button.h"
#include "timer.h"

//Define pins for 7seg
#define SEG_gpio GPIOC //PC port
#define DIN_pin 5//顯示的資料_PC5
#define CS_pin 6//資料讀取開關 PC6
#define CLK_pin 8//取樣 PC8

/*
//Define pins for keypad
#define COL_gpio GPIOA
#define COL_pin 6	//6 7 8 9
#define ROW_gpio GPIOB
#define ROW_pin 3	//3 4 5 6
*/

//Define pins for led(default use on-board led PA5)
#define LED_gpio GPIOA
#define LED_pin 5

//Define pins for button (default use  on-board button PC13)
#define BUTTON_gpio GPIOC
#define BUTTON_pin 13

//Define Counter timer
#define COUNTER_timer TIM2

void set_gpio(GPIO_TypeDef* gpio, int pin)
{
		gpio->BSRR |=( 1<< pin);
}
void reset_gpio(GPIO_TypeDef* gpio, int pin)
{
		gpio->BRR |=( 1<< pin);
}

int read_gpio(GPIO_TypeDef* gpio, int pin)
{

	if((gpio->IDR)>>pin & 1)
		return 0; //no touch 傳1
	else
		return 1; //touch 傳0o

}

int init_led(GPIO_TypeDef* gpio, int led_pin){
	//Enable AHB2 Clock
	if(gpio==GPIOA)
	{
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	}
	else if(gpio==GPIOB)
	{
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	}
	else
	{
		//Error! Add other cases to suit other GPIO pins
		return -1;
	}

	//Set GPIO pins to output mode (01)
	//First Clear bits(&) then set bits (|)
	gpio->MODER &= ~(0b11<<(2*led_pin));
	gpio->MODER |= (0b01<<(2*led_pin));

	return 0;
}

int init_button(GPIO_TypeDef* gpio, int button_pin)
{
	//Enable AHB2 Clock
	if(gpio==GPIOC)
	{
		RCC->AHB2ENR |=RCC_AHB2ENR_GPIOCEN;
	}
	else
	{
		//Error! Add other cases to suit other GPIO pins
		return -1;
	}
	//Set GPIO pins to input mode (00)
	//First Clear bits(&) then set bits (|)
	gpio->MODER &= ~(0b11<<(2*button_pin));
	gpio->MODER |= (0b00<<(2*button_pin));

	return 0;
}

void delay_without_interrupt(float msec){
	int loop_cnt= 500*msec;
	while(loop_cnt)
	{
		loop_cnt--;
	}
	return ;
}

void FPU_init(){
	//Setup FPU
	SCB -> CPACR |= (0xF << 20);
	__DSB();
	__ISB();
}

void SystemClock_Config(int speed){
	//system clock -> MSI
	RCC->CFGR &= ~RCC_CFGR_SW_Msk;
	RCC->CFGR |= RCC_CFGR_SW_MSI;

	while(!(((RCC->CFGR & RCC_CFGR_SWS_Msk)>> RCC_CFGR_SWS_Pos)==0));

	RCC->CR &= ~RCC_CR_PLLON;//Disabe PLL
	while((RCC->CR & RCC_CR_PLLRDY) != 0); //Make sure PLL is ready (unlocked)

	//Set PLL to MSI
	RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLSRC_Msk;
	RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_MSI;

	int set_R=0, set_N=0, set_M=0;
	//Change R N M (4*N)/(2*(R+1)*(M+1))
	if(speed == 40){
		set_R = 1;
		set_N = 40;
		set_M = 0;
	}
	else if(speed == 16){
		set_R = 0;
		set_N = 8;
		set_M = 0;
	}
	else if(speed == 10){
		set_R = 0;
		set_N = 5;
		set_M = 0;
	}
	else if(speed == 6){
		set_R = 0;
		set_N = 3;
		set_M = 0;
	}
	else if(speed == 1){
		set_R = 3;
		set_N = 8;
		set_M = 3;
	}
	else{
		//Default 4 MHz
		set_R = 3;
		set_N = 8;
		set_M = 0;
	}

	//Set PLLR
	RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLR_Msk;
	RCC->PLLCFGR |= (set_R << RCC_PLLCFGR_PLLR_Pos);
	//Set PLLN
	RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLN_Msk;
	RCC->PLLCFGR |= (set_N << RCC_PLLCFGR_PLLN_Pos);
	//Set PLLM
	RCC->PLLCFGR &= ~RCC_PLLCFGR_PLLM_Msk;
	RCC->PLLCFGR |= (set_M << RCC_PLLCFGR_PLLM_Pos);

	//Enable PLLR
	RCC->PLLCFGR |= RCC_PLLCFGR_PLLREN;
	//Enable PLL
	RCC->CR |= RCC_CR_PLLON;

	//system clock -> PLL
	RCC->CFGR &= ~RCC_CFGR_SW_Msk;
	RCC->CFGR |= RCC_CFGR_SW_PLL;

	while(!(((RCC->CFGR & RCC_CFGR_SWS_Msk)>>RCC_CFGR_SWS_Pos)==3));
}

int main()
{
	FPU_init();

	if(init_led(LED_gpio, LED_pin) != 0)
	{
		//Fail to init led
		return -1;
	}

	if(init_button(BUTTON_gpio, BUTTON_pin)!=0){
		//Failed to init button
		return -1;
	}

	int speed = 0, trans[5]={1, 6, 10, 16, 40};
	SystemClock_Config(trans[speed]);

	//Used to indicate led state: un-lit(0) or lit(1)
	int state = 0;
	//button_press_cycle_per_second (How many button press segments in a second)
	int button_press_cycle_per_second = 10;
	//Use to state how many cycles to check per button_press_cycle
	int debounce_cycles = 100;
	//Use to state the treshold when we consider a button press
	int debounce_threshold = debounce_cycles*0.7;
	//Used to implement negative edge trigger 0 = not-presses, 1 = pressed
	int last_button_state = 0;

	while(1){
		for(int a=0; a<button_press_cycle_per_second; a++){
			//Simple Debounce without interrupt
			int pos_cnt = 0;
			for(int a=0; a<debounce_cycles; a++){
				//If button press add count
				if(read_gpio(BUTTON_gpio, BUTTON_pin)==0){
					pos_cnt++;
				}
				delay_without_interrupt(1000/(button_press_cycle_per_second*debounce_cycles));
			}
			//Check if need to change state
			if(pos_cnt>debounce_threshold){
				if(last_button_state==0){
					//Pressed button - Pos edge
					//Do nothing
				}
				else{
					//Press button - Continued pressing
					//Do nothing
				}
				last_button_state = 1;
			}
			else{
				if(last_button_state == 0){
					//Released button - Not pressing
					//Do nothing
				}
				else{
					//Released button - Neg edge
					//Change speed and change system clock
					speed = (speed+1)%5;
					SystemClock_Config(trans[speed]);
				}
				last_button_state = 0;
			}
		}
		if(state == 1){
			reset_gpio(LED_gpio, LED_pin);
		}
		else{
			set_gpio(LED_gpio, LED_pin);
		}
		state = 1-state;
	}
	while(1){}

	return 0;
}
