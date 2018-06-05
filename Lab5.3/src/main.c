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

//Define pins for led(default use on-board led PA5)
#define LED_gpio GPIOA
#define LED_pin 5

//Define pins for button (default use  on-board button PC13)
#define BUTTON_gpio GPIOC
#define BUTTON_pin 13

//Define Counter timer
#define COUNTER_timer TIM2

void set_gpio(GPIO_TypeDef* gpio, int pin){
		gpio->BSRR |=( 1<< pin);
}

void reset_gpio(GPIO_TypeDef* gpio, int pin){
		gpio->BRR |=( 1<< pin);
}

int read_gpio(GPIO_TypeDef* gpio, int pin)
{
	if((gpio->IDR)>>pin & 1)
		return 1; //no touch 傳1
	else
		return 0; //touch 傳0
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
/*
void delay_without_interrupt(int msec){
	int loop_cnt=500*msec;

	while(loop_cnt)
		loop_cnt --;
	return;
}
*/
void FPU_init(){
	//Setup FPU
	SCB -> CPACR |= (0xF << 20);
	__DSB();
	__ISB();
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

void GPIO_init_AF(){
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	//Set to Alternate function mode
	GPIOA->MODER &= ~GPIO_MODER_MODE0_Msk;
	GPIOA->MODER |= (2 << GPIO_MODER_MODE0_Pos);
	//Set AFRL
	GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL0_Msk;
	GPIOA->AFR[0] |= (1 << GPIO_AFRL_AFSEL0_Pos);
}


void timer_enable(TIM_TypeDef *timer){
	if(timer == TIM2){
		RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
	}
	else if(timer == TIM3){
		RCC->APB1ENR1 |= RCC_APB1ENR1_TIM3EN;
	}
}

void timer_disable(TIM_TypeDef *timer){
	if(timer == TIM2){
		RCC->APB1ENR1 &= ~RCC_APB1ENR1_TIM2EN;
	}
	else if(timer == TIM3){
		RCC->APB1ENR1 &= ~RCC_APB1ENR1_TIM3EN;
	}
}


void timer_init(TIM_TypeDef *timer, int psc, int arr){
	//fout =
	timer->PSC = (uint32_t)(psc-1);  //PreScalser
	timer->ARR = (uint32_t)(arr-1);  //Reload value
	timer->EGR |= TIM_EGR_UG;        //Reinitialze the counter

}

void timer_start(TIM_TypeDef *timer){
	timer->CR1 |= TIM_CR1_CEN;
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

#define zero_C 61
#define zero_Db 65
#define zero_D 69
#define zero_Eb 73
#define zero_E 78
#define zero_F 82
#define zero_Gb 87
#define zero_G 93
#define zero_Ab 98
#define zero_A 104
#define zero_Bb 110
#define zero_B 116
#define one_C 123

#define treble 1
#define bass -1
#define reset 3

const int zero[4][4]={
		{treble, zero_Db, zero_Eb, bass},
		{zero_C,zero_D,zero_E, zero_F},
		{zero_Gb,zero_Ab,zero_Bb,reset},
		{zero_G,zero_A,zero_B,one_C}
};

#define one_Db 131
#define one_D 139
#define one_Eb 147
#define one_E 156
#define one_F 165
#define one_Gb 175
#define one_G 185
#define one_Ab 196
#define one_A 208
#define one_Bb 220
#define one_B 233
#define two_C 247

const int one[4][4]={
		{treble, one_Db, one_Eb, bass},
		{one_C, one_D, one_E, one_F},
		{one_Gb, one_Ab, one_Bb,reset},
		{one_G, one_A, one_B, two_C}
};

#define two_Db 262
#define two_D 277
#define two_Eb 294
#define two_E 311
#define two_F 330
#define two_Gb 349
#define two_G 370
#define two_Ab 392
#define two_A 415
#define two_Bb 440
#define two_B 466
#define three_C 494

const int two[4][4]={
		{treble, two_Db, two_Eb, bass},
		{two_C, two_D, two_E, two_F},
		{two_Gb, two_Ab, two_Bb,reset},
		{two_G, two_A, two_B, three_C}
};

#define four_C 988

const int three[4][4]={
		{treble, 523, 587, bass},
		{494,554,622, 659},
		{698,784,880,reset},
		{740,831,932,988}
};

#define four_Db 1046
#define four_D 1109
#define four_Eb 1175
#define four_E 1245
#define four_F 1318
#define four_Gb 1397
#define four_G 1480
#define four_Ab 1568
#define four_A 1661
#define four_Bb 1760
#define four_B 1865
#define five_C 1976

const int four[4][4]={
		{treble, four_Db, four_Eb, bass},
		{four_C, four_D, four_E, four_F},
		{four_Gb, four_Ab, four_Bb,reset},
		{four_G, four_A, four_B, five_C}
};

#define five_Db 2093
#define five_D 2217
#define five_Eb 2349
#define five_E 2489
#define five_F 2637
#define five_Gb 2794
#define five_G 2960
#define five_Ab 3136
#define five_A 3322
#define five_Bb 3520
#define five_B 3729
#define six_C 3951

const int five[4][4]={
		{treble, five_Db, five_Eb, bass},
		{five_C, five_D, five_E, five_F},
		{five_Gb, five_Ab, five_Bb,reset},
		{five_G, five_A, five_B, six_C}
};

#define six_Db 4186
#define six_D 4435
#define six_Eb 4699
#define six_E 4978
#define six_F 5274
#define six_Gb 5587
#define six_G 5919
#define six_Ab 6271
#define six_A 6645
#define six_Bb 7040
#define six_B 7459
#define seven_C 7902

const int six[4][4]={
		{treble, six_Db, six_Eb, bass},
		{six_C, six_D, six_E, six_F},
		{six_Gb, six_Ab, six_Bb,reset},
		{six_G, six_A, six_B, seven_C}
};

void timer_stop(TIM_TypeDef *timer){
	timer->CR1 &= ~TIM_CR1_CEN;
}

int main(){
	FPU_init();
	if(init_7seg(SEG_gpio, DIN_pin, CS_pin, CLK_pin) != 0)
	{
		//Fail to init 7seg
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

	/*
	if(init_button(BUTTON_gpio, BUTTON_pin)!=0){
		//Failed to init button
		return -1;
	}
	*/

	if(init_keypad(ROW_gpio,COL_gpio,ROW_pin,COL_pin)!=0){
		return -1;
	}

	/*
	//Used to indicate led state: un-lit(0) or lit(1)
	int state = 0;
	//button_press_cycle_per_second (How many button press segments in a second)
	int button_press_cycle_per_second = 20;
	//Use to state how many cycles to check per button_press_cycle
	int debounce_cycles = 50;
	//Use to state the treshold when we consider a button press
	int debounce_threshold = debounce_cycles*0.7;
	//Used to implement negative edge trigger 0 = not-presses, 1 = pressed
	int last_button_state = 0;
	*/

	//Change PA0 to AF mode - use for PWM
	GPIO_init_AF();
	//Enable timer
	timer_enable(TIM2);
	//Init the timer
	timer_init(TIM2,1, 20); //4M/2/21
	PWM_channel_init();
	timer_start(TIM2);

	int keypad[4][4]={
			{treble, 523, 587, bass},
			{494,554,622, 659},
			{698,784,880,reset},
			{740,831,932,988}
	};
	int octave = 3;
	int f;

	while(1){
		display_number(SEG_gpio, DIN_pin, CS_pin, CLK_pin, octave, 2);
		int input = 0;
		for(int i=0;i<4;i++)
		{
			for(int j=0;j<4;j++)
			{
				if(check_keypad_input_one(ROW_gpio, COL_gpio, ROW_pin, COL_pin, i, j))
				{
					if((keypad[i][j]==treble)&&(octave < 6)){ //treble
						octave = octave+1;

					}
					else if((keypad[i][j]==bass)&&(octave > 0)){ //bass
						octave = octave-1;

					}
					else if(keypad[i][j]==reset){
						octave = 3;
					}
					else{
						if(octave == 0){
							f = zero[i][j];
						}
						else if(octave == 1){
							f = one[i][j];
						}
						else if(octave == 2){
							f = two[i][j];
						}
						else if(octave == 3){
							f = three[i][j];
						}
						else if(octave == 4){
							f = four[i][j];
						}
						else if(octave == 5){
							f = five[i][j];
						}
						else if(octave == 6){
							f = six[i][j];
						}
						timer_enable(TIM2);
						input = 1;
						timer_init(TIM2, 200000/f-1, 19); //4M/20/(200000/f)=f
					}

				}
			}
		}
		if(input == 0)
		{
			timer_disable(TIM2);
			//timer_init(TIM2, 200000/523-1, 19);
		}
	}
	//timer_init(TIM2, 200000/831-1, 19); //40M/20=2000000 2000000/(2000000/988)=988
}
