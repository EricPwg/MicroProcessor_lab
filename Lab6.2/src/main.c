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

//Define pins for keypad
#define COL_gpio GPIOA
#define COL_pin 6	//6 7 8 9
#define ROW_gpio GPIOB
#define ROW_pin 3	//3 4 5 6

//Define Counter timer
#define COUNTER_timer TIM2

int now_col = 3;
//Used for debounce
int keyCnt = 0, keyValue = -1;

void set_gpio(GPIO_TypeDef* gpio, int pin)
{
		gpio->BSRR |=( 1<< pin);
}
void reset_gpio(GPIO_TypeDef* gpio, int pin)
{
		gpio->BRR |=( 1<< pin);
}

void SysTick_Handler(){
	if(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk){
		reset_push(COL_gpio, now_col+COL_pin);
		now_col = (now_col+1)%4;
		set_push(COL_gpio, now_col+COL_pin);
		//display_number(SEG_gpio, DIN_pin, CS_pin, CLK_pin, now_col, 5);
	}
}

void reset_push( GPIO_TypeDef* col_gpio, int col_pin){ //Set GPIO pins to push pull mode (0)
	col_gpio -> OTYPER &= ~(1 << (col_pin));
}

void set_push( GPIO_TypeDef* col_gpio, int col_pin){ //Set GPIO pins to open drain mode (1)
	col_gpio -> OTYPER |= (1 << (col_pin));

}

int init_7seg(GPIO_TypeDef* gpio, int DIN, int CS, int CLK)
{
	//Enable AHB2 Clock
	if(gpio == GPIOA)
	{
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	}
	else if(gpio == GPIOB)
	{
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	}
	else if(gpio == GPIOC)
	{
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
	}
	else
	{
		//Error! Add other cases to suit other GPIO pins
		return -1;
	}
	//Set GPIO pins to output mode (01)
	//First Clear bits(&) then set bits (|)
	gpio->MODER &= ~(0b11 << (2*DIN));
	gpio->MODER |= (0b01 << (2*DIN));
	gpio->MODER &= ~(0b11 << (2*CS));
	gpio->MODER |= (0b01 << (2*CS));
	gpio->MODER &= ~(0b11 << (2*CLK));
	gpio->MODER |= (0b01 << (2*CLK));

	//Close display test
	send_7seg(gpio, DIN, CS, CLK, SEG_ADDRESS_DISPLAY_TEST, 0x00);
	return 0;
}

void EXTI_Setup(){
	//Enable SYSCFG CLK
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
	//Select output bits
	SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI3_Msk;
	SYSCFG->EXTICR[0] |= (1 << SYSCFG_EXTICR1_EXTI3_Pos);
	SYSCFG->EXTICR[1] &= ~SYSCFG_EXTICR2_EXTI4_Msk;
	SYSCFG->EXTICR[1] |= (1 << SYSCFG_EXTICR2_EXTI4_Pos);
	SYSCFG->EXTICR[1] &= ~SYSCFG_EXTICR2_EXTI5_Msk;
	SYSCFG->EXTICR[1] |= (1 << SYSCFG_EXTICR2_EXTI5_Pos);
	SYSCFG->EXTICR[1] &= ~SYSCFG_EXTICR2_EXTI6_Msk;
	SYSCFG->EXTICR[1] |= (1 << SYSCFG_EXTICR2_EXTI6_Pos);

	//Enable interrupt
	EXTI->IMR1 |= EXTI_IMR1_IM3;
	EXTI->IMR1 |= EXTI_IMR1_IM4;
	EXTI->IMR1 |= EXTI_IMR1_IM5;
	EXTI->IMR1 |= EXTI_IMR1_IM6;

	//Enable Falling Edge
	EXTI->FTSR1 |= EXTI_FTSR1_FT3;
	EXTI->FTSR1 |= EXTI_FTSR1_FT4;
	EXTI->FTSR1 |= EXTI_FTSR1_FT5;
	EXTI->FTSR1 |= EXTI_FTSR1_FT6;

	//Enable NVIC**
	NVIC_EnableIRQ(EXTI3_IRQn);
	NVIC_EnableIRQ(EXTI4_IRQn);
	NVIC_EnableIRQ(EXTI9_5_IRQn);

}

const int keypad[4][4] = {
		{1,2,3,10},
		{4,5,6,11},
		{7,8,9,12},
		{15,0,14,13},
};

void EXTIKeypadHandler(int r){
	int nowKey = keypad[r][(now_col) %4];
	// A simple debounce
	if(nowKey == keyValue ){
		keyCnt++;
	}
	else{
		keyCnt = 0;
	}
	keyValue = nowKey;
	if(keyCnt >= 5){
		keyCnt = 5;
		display_number(SEG_gpio, DIN_pin, CS_pin, CLK_pin, keyValue, 2);
	}
}

void EXTI3_IRQHandler(){
	if(EXTI->PR1&EXTI_PR1_PIF3_Msk){
		EXTIKeypadHandler(0);
		EXTI->PR1 = EXTI_PR1_PIF3_Msk;
	}
}

void EXTI4_IRQHandler(){
	if(EXTI->PR1&EXTI_PR1_PIF4_Msk){
		EXTIKeypadHandler(1);
		EXTI->PR1 = EXTI_PR1_PIF4_Msk;
	}
}

void EXTI9_5_IRQHandler(){
	if(EXTI->PR1&EXTI_PR1_PIF5_Msk){
		EXTIKeypadHandler(2);
		EXTI->PR1 = EXTI_PR1_PIF5_Msk;
	}
	if(EXTI->PR1&EXTI_PR1_PIF6_Msk){
		EXTIKeypadHandler(3);
		EXTI->PR1 = EXTI_PR1_PIF6_Msk;
	}
}

void send_7seg(GPIO_TypeDef* gpio, int DIN, int CS, int CLK, int address, int data)
{
	//The payload to send
	int payload = ((address&0xFF)<<8)|(data&0xFF);

	//Start the sending cycles
	//16 data-bits + 1 CS signal
	int total_cycles = 16+1;

	for(int a=1;a<=total_cycles;a++)
	{
		//Reset CLK when enter
		reset_gpio(gpio,CLK);

		//Set DIN according to data except for last cycles(CS)
		if(((payload>>(total_cycles-1-a))&0x1) && a!= total_cycles)
		{
			set_gpio(gpio, DIN);
		}
		else
		{
			reset_gpio(gpio, DIN);
		}
		 //Set CS at last cycle
		if(a == total_cycles)
		{
			set_gpio(gpio, CS);
		}
		else
		{
			reset_gpio(gpio, CS);
		}

		//Set CLK when leaving (seg set data at rising edge)
		set_gpio(gpio, CLK);
	}

	return;
}

int display_number(GPIO_TypeDef* gpio, int DIN, int CS, int CLK, int num, int num_digs)
{
	for(int i = 1;i<=num_digs;i++)
	{
		send_7seg(gpio, DIN, CS, CLK, i, num % 10);
		num /= 10;
	}
	for(int i = num_digs+1;i<=8;i++)
	{
		num/=10;
		send_7seg(gpio, DIN, CS, CLK, i, 15);//blank
	}
	if(num!=0)
		return -1;
	return 0;
}

void SystemClock_Config_interrupt(int speed, int load){
	SystemClock_Config(speed);
	SysTick->LOAD = load;
	SysTick->CTRL |= (1 << SysTick_CTRL_CLKSOURCE_Pos);
	SysTick->CTRL |= (1 << SysTick_CTRL_TICKINT_Pos);
	SysTick->CTRL |= (1 << SysTick_CTRL_ENABLE_Pos);
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

int init_keypad(GPIO_TypeDef* row_gpio, GPIO_TypeDef* col_gpio, int row_pin, int col_pin){
	//Enable AHB2 Clock
	if(row_gpio == GPIOA || col_gpio == GPIOA){
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	}
	if(row_gpio == GPIOB || col_gpio == GPIOB){
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	}
	//First Clear bits(&) then set bits (|)
	for(int a=0; a<4; a++)
	{
		//Set GPIO pins to output mode (01)
		col_gpio -> MODER &= ~(0b11 << (2*(col_pin+a)));
		col_gpio -> MODER |= (0b01 << (2*(col_pin+a)));
		//Set GPIO pins to very high speed mode (11)
		col_gpio -> OSPEEDR &= ~(0b11 << (2*(col_pin+a)));
		col_gpio -> OSPEEDR |= (0b11 << (2*(col_pin+a)));
		//Set GPIO pins to open drain mode (1)
		col_gpio -> OTYPER &= ~(0b1 << (col_pin+a));
		col_gpio -> OTYPER |= (0b1 << (col_pin+a));
		//Set Output to high
		set_gpio(col_gpio, col_pin+a);
	}
	//First Clear bits(&) then set bits(|)
	for(int a=0; a<4; a++)
	{
		//Set GPIO pins to input mode (00)
		row_gpio -> MODER &= ~(0b11 << (2*(row_pin+a)));
		row_gpio -> MODER |= (0b00 << (2*(row_pin+a)));
		//Set GPIO pins to Pull-Down mode (10)
		row_gpio -> PUPDR &= ~(0b11 << (2*(row_pin+a)));
		row_gpio -> PUPDR |= (0b10 << (2*(row_pin+a)));
	}
	return 0;
}

int num_digits(int x)
{
	if(x==0)
	{
		return 1;
	}
	int res = 0;
	while(x)
	{
		res++;
		x/=10;
	}
	return res;
}

int main(){

	if(init_7seg(SEG_gpio, DIN_pin, CS_pin, CLK_pin) != 0)
	{
		//Fail to init 7seg
		return -1;
	}

	if(init_keypad(ROW_gpio,COL_gpio,ROW_pin,COL_pin)!=0){
		//Fail to init keypad
		return -1;
	}

	//Set Decode Mode to non-decode mode
	send_7seg(SEG_gpio, DIN_pin, CS_pin, CLK_pin, SEG_ADDRESS_DECODE_MODE, 0xFF);
	//Set Scan Limit to digit 0 only
	send_7seg(SEG_gpio, DIN_pin, CS_pin, CLK_pin, SEG_ADDRESS_SCAN_LIMIT, 0x07);
	//Wakeup 7seg
	send_7seg(SEG_gpio, DIN_pin, CS_pin, CLK_pin, SEG_ADDRESS_SHUTDOWN, 0x01);
	//Set brightness
	send_7seg(SEG_gpio, DIN_pin, CS_pin, CLK_pin, SEG_ADDRESS_ITENSITY, 0x05);


	//Set 10MHz 0.001s interrupt
	SystemClock_Config_interrupt(10,10000);
	//Init Interrupts
	EXTI_Setup();

	while(1){
		SysTick_Handler();
		EXTI3_IRQHandler();
		EXTI4_IRQHandler();
		EXTI9_5_IRQHandler();


	}

	return 0;
}
