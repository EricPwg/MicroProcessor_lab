#include "../inc/stm32l476xx.h"
#include "../inc/7seg.h"
#include "../inc/keypad.h"
#include "../inc/helper_functions.h"
#include "../inc/led_button.h"
#include "../inc/timer.h"

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

//Define pins for button (default use  on-board button PC13)
#define BUTTON_gpio GPIOC
#define BUTTON_pin 13

int k=0;
int flag=1;
#define C 523
#define D 587
#define E 659
#define F 698
#define G 784
const int keypad[4][4] = {
		{1,2,3,10},
		{4,5,6,11},
		{7,8,9,12},
		{15,0,14,13},
};
//button_press_cycle_per_second (How many button press segments in a second)
int button_press_cycle_per_second = 20;
//Use to state how many cycles to check per button_press_cycle
int debounce_cycles = 50;
//Use to state the threshold when we consider a button press
int debounce_threshold = 35;

void FPU_init(){
	//Setup FPU
	SCB -> CPACR |= (0xF << 20);
	__DSB();
	__ISB();
}
int read_gpio(GPIO_TypeDef* gpio, int pin)
{
	if((gpio->IDR)>>pin & 1)
		return 1; //no touch 傳1
	else
		return 0; //touch 傳0
}
void set_gpio(GPIO_TypeDef* gpio, int pin)
{
		gpio->BSRR |=( 1<< pin);
}
void reset_gpio(GPIO_TypeDef* gpio, int pin)
{
		gpio->BRR |=( 1<< pin);
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

void ring(){
	while(flag==0){
		int family[31] = {E,E,F,G,G,F,E,D,C,C,D,E,E,D,D,D,\
						  E,E,F,G,G,F,E,D,C,C,D,E,D,C,C};
		timer_enable(TIM2);
		for(int i = 0;i<=31;i++){
			timer_init(TIM2, 200000/family[i]-1, 19);
			delay_without_interrupt(400);
		}
	}
}

void toggle_output(int Value){
	if((Value-1)>0){
		Value=Value-1;
		display_number(SEG_gpio, DIN_pin, CS_pin, CLK_pin, Value, num_digits(Value));
		k=Value;
	}
	else{
		display_number(SEG_gpio, DIN_pin, CS_pin, CLK_pin, 0, num_digits(Value));
		k=0;
		ring();
		SysTick->CTRL &= (0 << SysTick_CTRL_ENABLE_Pos);
		SysTick->CTRL |= (0 << SysTick_CTRL_ENABLE_Pos);
	}
}

void SysTick_Handler(){
	if(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk){
		toggle_output(k);
	}
}

void EXTI15_10_IRQHandler(){
	if(EXTI->PR1&EXTI_PR1_PIF13_Msk){
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
					if(k>0){
						flag=0;
						SystemClock_Config_interrupt(10,10000000);
					}
					else{
						timer_disable(TIM2);
						flag=1;
					}
				}
				else{
					EXTI->PR1 = EXTI_PR1_PIF13_Msk;
				}
			}
		EXTI->PR1 = EXTI_PR1_PIF13_Msk;
	}
}

void EXTI_Setup(){
	//Enable SYSCFG CLK
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
	//Select output bits
	SYSCFG->EXTICR[3] &= ~SYSCFG_EXTICR4_EXTI13_Msk;
	SYSCFG->EXTICR[3] |= (2 << SYSCFG_EXTICR4_EXTI13_Pos);

	//Enable interrupt
	EXTI->IMR1 |= EXTI_IMR1_IM13;

	//Enable Falling Edge
	EXTI->FTSR1 |= EXTI_FTSR1_FT13;

	//Enable NVIC**
	NVIC_EnableIRQ(EXTI15_10_IRQn);

	//Set Priority of External interrupt & Internal interrupt
	NVIC_SetPriority(EXTI15_10_IRQn,0);
	NVIC_SetPriority(SysTick_IRQn,1);
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

void timer_enable(TIM_TypeDef *timer){
	if(timer == TIM2){
		RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
	}
	else if(timer == TIM3){
		RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
	}
}

void timer_init(TIM_TypeDef *timer, int psc, int arr){
	timer->PSC = (uint32_t)(psc-1);  //PreScalser
	timer->ARR = (uint32_t)(arr-1);  //Reload value
	timer->EGR |= TIM_EGR_UG;        //Reinitialze the counter

}

void timer_start(TIM_TypeDef *timer){
	timer->CR1 |= TIM_CR1_CEN;
}

void timer_stop(TIM_TypeDef *timer){
	timer->CR1 &= ~TIM_CR1_CEN;
}
void timer_disable(TIM_TypeDef *timer){
	if(timer == TIM2){
		RCC->APB1ENR1 &= ~RCC_APB1ENR1_TIM2EN;
	}
	else if(timer == TIM3){
		RCC->APB1ENR1 &= ~RCC_APB1ENR1_TIM3EN;
	}
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

int check_keypad_input_one(GPIO_TypeDef* row_gpio, GPIO_TypeDef* col_gpio, int row_pin, int col_pin, int x, int y)
{
	int cycles = 400;
	//Set Column to push-pull mode
	col_gpio -> OTYPER &= ~(1 << (col_pin+y));
	//Count the total number of time it is pressed in a certain period
	int cnt = 0;
	for(int a=0; a<cycles; a++){
		cnt += read_gpio(row_gpio, row_pin+x);
	}
	//Set Column back to open drain mode
	col_gpio -> OTYPER |= (1 << (col_pin+y));
	//return if the key is pressed(1) or not(0)
	return (cnt > (cycles*0.7));
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

void GPIO_init_AF(){
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	//Set to Alternate function mode
	GPIOA->MODER &= ~GPIO_MODER_MODE0_Msk;
	GPIOA->MODER |= (2 << GPIO_MODER_MODE0_Pos);
	//Set AFRL
	GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL0_Msk;
	GPIOA->AFR[0] |= (1 << GPIO_AFRL_AFSEL0_Pos);
}

void PWM_channel_init(){
	//p.883 915 920 924
	//PA0 for PWM
	//PWM mode 1
	TIM2->CCMR1 &= ~TIM_CCMR1_OC1M_Msk;
	TIM2->CCMR1 |= (6 << TIM_CCMR1_OC1M_Pos);
	//OCPreload_Enable
	TIM2->CCMR1 &= ~TIM_CCMR1_OC1PE_Msk;
	TIM2->CCMR1 |= (1 << TIM_CCMR1_OC1PE_Pos);
	//Active high for channel 1 polarity
	TIM2->CCER &= ~TIM_CCER_CC1P_Msk;
	TIM2->CCER |= (0 << TIM_CCER_CC1P_Pos);
	//Enable for channel 1 output
	TIM2->CCER &= ~TIM_CCER_CC1E_Msk;
	TIM2->CCER |= (1 << TIM_CCER_CC1E_Pos);
	//Set Compare Register
	TIM2->CCR1 = 10;
	//Set PreScaler
	TIM2->PSC = 0;
}

void delay_without_interrupt(float msec){
	int loop_cnt= 1000*msec;
	while(loop_cnt)
	{
		loop_cnt--;
	}
	return ;
}

int main(){

	FPU_init();
	init_button(BUTTON_gpio, BUTTON_pin);
	init_7seg(SEG_gpio, DIN_pin, CS_pin, CLK_pin);
	init_keypad(ROW_gpio,COL_gpio,ROW_pin,COL_pin);
	EXTI_Setup();
	//SystemClock_Config(1);

	//Change PA0 to AF mode - use for PWM
	GPIO_init_AF();
	//Enable timer
	timer_enable(TIM2);
	//Init the timer
	timer_init(TIM2,1, 20); //4M/2/21
	PWM_channel_init();
	timer_start(TIM2);
	send_7seg(SEG_gpio, DIN_pin, CS_pin, CLK_pin, SEG_ADDRESS_DECODE_MODE, 0xFF);
	//Set Scan Limit to digit 0 only
	send_7seg(SEG_gpio, DIN_pin, CS_pin, CLK_pin, SEG_ADDRESS_SCAN_LIMIT, 0x07);
	//Wakeup 7seg
	send_7seg(SEG_gpio, DIN_pin, CS_pin, CLK_pin, SEG_ADDRESS_SHUTDOWN, 0x01);
	//Set brightness
	send_7seg(SEG_gpio, DIN_pin, CS_pin, CLK_pin, SEG_ADDRESS_ITENSITY, 0x05);

	display_number(SEG_gpio, DIN_pin, CS_pin, CLK_pin, 0,0);

	k=0;
	while(1){
		if(flag){
			for(int i=0;i<4;i++)
			{
				for(int j=0;j<4;j++)
				{
					if(check_keypad_input_one(ROW_gpio, COL_gpio, ROW_pin, COL_pin, i, j))
					{
						k=keypad[i][j];
						if(k>0)
							display_number(SEG_gpio, DIN_pin, CS_pin, CLK_pin, keypad[i][j], num_digits(keypad[i][j]));
					}
				}
			}
			EXTI15_10_IRQHandler();
		}
		}
	return 0;
}
