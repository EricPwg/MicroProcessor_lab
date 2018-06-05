#include "../inc/keypad.h"
#include "../inc/imd_keypadpins.h"

// Mapping for key-press
const int keypad[4][4] = {
	{1, 2, 3, 10},
	{4, 5, 6, 11},
	{7, 8, 9, 12},
	{15, 0, 14, 13}
};

const GPIO_TypeDef* real_COL_GPIO[4] ={COL_GPIO_1, COL_GPIO_2, COL_GPIO_3, COL_GPIO_4};
const GPIO_TypeDef* real_ROW_GPIO[4] ={ROW_GPIO_1, ROW_GPIO_2, ROW_GPIO_3, ROW_GPIO_4};

const int real_COL_num[4] = {COL_pin_n1, COL_pin_n2, COL_pin_n3, COL_pin_n4};
const int real_ROW_num[4] = {ROW_pin_n1, ROW_pin_n2, ROW_pin_n3, ROW_pin_n4};

// Only allow GPIOA and GPIOB for now
// Can easily extended by adding "else if" cases
int init_keypad(){
	// Enable AHB2 Clock
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

	// First Clear bits(&) then set bits(|)
	for(int a=0;a<4;a++){
		GPIO_TypeDef* COL_gpio = real_COL_GPIO[a];
		int real_col_pin_n = real_COL_num[a];
		// Set GPIO pins to output mode (01)
		COL_gpio->MODER &= ~(0b11 << (2*real_col_pin_n));
		COL_gpio->MODER |= (0b01 << (2*real_col_pin_n));
		// Set GPIO pins to very high speed mode (11)
		COL_gpio->OSPEEDR &= ~(0b11 << (2*real_col_pin_n));
		COL_gpio->OSPEEDR |= (0b11 << (2*real_col_pin_n));
		// Set GPIO pins to open drain mode (1)
		COL_gpio->OTYPER &= ~(0b1 << real_col_pin_n);
		COL_gpio->OTYPER |= (0b1 << real_col_pin_n);
		// Set Output to high
		set_gpio(COL_gpio, real_col_pin_n);
	}

	// First Clear bits(&) then set bits(|)
	for(int a=0;a<4;a++){
		GPIO_TypeDef* ROW_gpio = real_ROW_GPIO[a];
		int real_row_pin_n = real_ROW_num[a];
		// Set GPIO pins to input mode (00)
		ROW_gpio->MODER &= ~(0b11 << (2*real_row_pin_n));
		ROW_gpio->MODER |= (0b00 << (2*real_row_pin_n));
		// Set GPIO pins to Pull-Down mode (10)
		ROW_gpio->PUPDR &= ~(0b11 << (2*real_row_pin_n));
		ROW_gpio->PUPDR |= (0b10 << (2*real_row_pin_n));
	}

	return 0;
}

int check_keypad_input_one(int x, int y){
	int cycles = 400;
	// Set Column to push-pull mode
	GPIO_TypeDef* this_colgpio = real_COL_GPIO[y];
	int this_col_num = real_COL_num[y];
	GPIO_TypeDef* this_rowgpio = real_ROW_GPIO[x];
	int this_row_num = real_ROW_num[x];
	this_colgpio->OTYPER &= ~(1 << (this_col_num));
	// Count the total number of time it is pressed in a certain period
	int cnt = 0;
	for(int a=0;a<cycles;a++){
		cnt += read_gpio(this_rowgpio, this_row_num);
	}
	// Set Column back to open drain mode
	this_colgpio->OTYPER |= (1 << (this_col_num));
	// return if the key is pressed(1) or not(0)
	int r = (cnt > (cycles*0.7));
	return r;
}

int check_keypad_input_multiple(GPIO_TypeDef* ROW_gpio, GPIO_TypeDef* COL_gpio, int ROW_pin, int COL_pin){
	// Count the total number of time each input is pressed in a certain period
	int cnt[4][4];
	for(int i=0;i<4;i++){
		for(int j=0;j<4;j++){
			cnt[i][j] = 0;
		}
	}
	int cycles = 400;
	for(int a=0;a<cycles;a++){
		for(int j=0;j<4;j++){
			// Set Column to push-pull mode
			GPIO_TypeDef* this_colgpio = real_COL_GPIO[j];
			int this_col_num = real_COL_num[j];
			this_colgpio->OTYPER &= ~(1 << (this_col_num));
			// Read the whole row
			for(int i=0;i<4;i++){
				GPIO_TypeDef* this_rowgpio = real_ROW_GPIO[i];
				int this_row_num = real_ROW_num[i];
				cnt[i][j] += read_gpio(this_rowgpio, this_row_num);
			}
			// Set Column back to open drain mode
			this_colgpio->OTYPER |= (1 << (this_col_num));
		}
	}
	// Use a int to represent a bitmap of 16 bits
	int res = 0;
	for(int i=3;i>=0;i--){
		for(int j=3;j>=0;j--){
			res = (res << 1) | (cnt[i][j] >= 300);
		}
	}
	return res;
}
