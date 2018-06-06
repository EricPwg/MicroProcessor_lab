#include "stm32l476xx.h"
#include "7seg.h"
#include "keypad.h"
#include "helper_functions.h"

#define SEG_gpio GPIOC //PC port
#define DIN_pin 5//顯示的資料_PC5
#define CS_pin 6//資料讀取開關 PC6
#define CLK_pin 8//取樣 PC8

#define COL_gpio GPIOA
#define COL_pin 6	//5 6 7 8

#define ROW_gpio GPIOB
#define ROW_pin 3	//3 4 5 6

void delay_without_interrupt(int msec){
	int loop_cnt=500*msec;

	while(loop_cnt)
		loop_cnt --;
	return;
}

void set_gpio(GPIO_TypeDef* gpio, int pin){
		gpio->BSRR |=( 1<< pin);
}

void reset_gpio(GPIO_TypeDef* gpio, int pin){
		gpio->BRR |=( 1<< pin);
}

int init_keypad(GPIO_TypeDef* R_gpio,GPIO_TypeDef* C_gpio,int ROW,int COL){
	if(R_gpio==GPIOA || C_gpio==GPIOA)
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	if(R_gpio==GPIOB || C_gpio==GPIOB)
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

	for(int a=0;a<4;a++){
		C_gpio->MODER &= ~(0b11 << (2*(COL+a)));
		C_gpio->MODER |= (0b01 << (2*(COL+a)));

		C_gpio->OSPEEDR &= ~(0b11 << (2*(COL+a)));
		C_gpio->OSPEEDR |= (0b11 << (2*(COL+a)));

		C_gpio->OTYPER &= ~(0b1 << (COL+a));
		C_gpio->OTYPER |= (0b1 << (COL+a));

		set_gpio(C_gpio,COL+a);
	}

	for(int a=0;a<4;a++){
		R_gpio->MODER &= ~(0b11 << (2*(ROW+a)));
		R_gpio->MODER |= (0b00 << (2*(ROW+a)));

		R_gpio->PUPDR &= ~(0b11 << (2*(ROW+a)));
		R_gpio->PUPDR |= (0b10 << (2*(ROW+a)));
	}

	return 0;
}

const int keypad[4][4] = {
		{1,2,3,10},
		{4,5,6,11},
		{7,8,9,12},
		{15,0,14,13},
};

int num_digits(int x){
	if(x==0)
		return 1;

	int res = 0;
	while(x){
		res ++;
		x /= 10;
	}
	return res;
}

int read_gpio(GPIO_TypeDef* gpio, int pin){
	if((gpio->IDR)>>pin &1)
			return 1;
		else
			return 0;
}

//讀取一筆資料
void send_7seg(GPIO_TypeDef* gpio,int DIN,int CS,int CLK,int address,int data){
	int payload = ((address & 0xFF) << 8) | (data & 0xFF);

	//每筆資料共16位D0~D15 最後迴圈+1將CS重設
	int total_cycles = 16+1;

	for(int a=1 ; a <= total_cycles ; a++){
		reset_gpio(gpio,CLK);//計時CLK重複觸發 rising

		//不在最後迴圈時，讀取DIN資料
		if( ( (payload >> (total_cycles-1-a)) &0x01) && a!=total_cycles){
			set_gpio(gpio,DIN);
		}
		else{
			reset_gpio(gpio,DIN);
		}

		//最後迴圈重設CS
		if(a==total_cycles)
			set_gpio(gpio,CS);
		else
			reset_gpio(gpio,CS);

		set_gpio(gpio,CLK);//每次回圈CLK歸位
	}

	return;
}

//初始化，設定腳位
int init_7seg(GPIO_TypeDef* gpio,int DIN,int CS,int CLK){
	if(gpio == GPIOA)
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	else if(gpio == GPIOB)
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	else if(gpio ==GPIOC)
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
	else
		return -1;

	//將腳位設為OUTPUT
	gpio -> MODER &= ~(0b11 << (2*DIN));
	gpio -> MODER |= (0b01 << (2*DIN));
	gpio -> MODER &= ~(0b11 << (2*CS));
	gpio -> MODER |= (0b01 << (2*CS));
	gpio -> MODER &= ~(0b11 << (2*CLK));
	gpio -> MODER |= (0b01 << (2*CLK));

	//關閉DISPLAY TEST
	send_7seg(gpio,DIN,CS,CLK,SEG_ADDRESS_DISPLAY_TEST,0x00);

	return 0;
}

int display_number(GPIO_TypeDef* gpio,int DIN,int CS,int CLK,int num,int num_digs){

	for(int i=1;i<=num_digs;i++){
		send_7seg(gpio,DIN,CS,CLK,i,num%10);
		num /=10;
	}

	for(int i=num_digs+1;i<=8;i++){
		num /= 10;
		send_7seg(gpio,DIN,CS,CLK,i,15);
	}

	if(num!=0)
		return -1;

	return 0;
}

int check_keypad_input_one(GPIO_TypeDef* R_gpio, GPIO_TypeDef* C_gpio, int ROW, int COL, int x,int y){
	int cycles=400;
	C_gpio->OTYPER &=~(1 << (COL+y));
	int cnt=0;
	for(int a=0;a<cycles;a++){
		cnt += read_gpio(R_gpio,ROW+x);
	}

	C_gpio->OTYPER |= (1<<(COL+y));

	return (cnt>cycles*0.7);
}

int main(){
	if(init_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin)!=0)
			return -1;

	//開啟DECODE MODE D0~07
	send_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin,SEG_ADDRESS_DECODE_MODE,0xFF);
	//SCAN LIMIT 設為 DIGIT_0~7
	send_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin,SEG_ADDRESS_SCAN_LIMIT,0x07);
	//shutdown關閉->normal operation p.s. shutdown打開(0x00)，讀取訊號腳位直接接地，即不再讀取任何訊號
	send_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin,SEG_ADDRESS_SHUTDOWN,0x01);
	//調整光強度
	send_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin,SEG_ADDRESS_ITENSITY,0x05);

	display_number(SEG_gpio,DIN_pin,CS_pin,CLK_pin,0,0);

	if(init_keypad(ROW_gpio,COL_gpio,ROW_pin,COL_pin)!=0)
		return -1;

	int B=0;//暫存器B，存取運算值
	double equ[1000];//存取整條運算式，並且輸入等號後做四則運算。因為運算過程會出現小數，宣告用double
	//運算元和運算值也可以分開來存。
	int k=0;//運算式equ內的位置
	int while_switch=1;//讀取運算式結束後，中斷while
	int check_state=0;//錯誤偵測
	int check=0;//錯誤偵測
	restart:
	while(while_switch){//while的工作定為：讀取運算式、CLEAR鍵功能(其實跟把運算過程放在等號if是一樣的)
		for(int i=0;i<4;i++){
			for(int j=0;j<4;j++){
				if(check_keypad_input_one(ROW_gpio,COL_gpio,ROW_pin,COL_pin,i,j)){
					int A = keypad[i][j];//暫存器A，存取按鍵輸入，包含運算元、運算值、clear
					delay_without_interrupt(100);
					//讀取運算式，需先看完式子才能決定四則運算的順序
					if(A<10){//輸入運算值
						B = B*10+A;
						if(B>999)//運算值不可超過999
							B/=10;
						display_number(SEG_gpio,DIN_pin,CS_pin,CLK_pin,B,num_digits(B));
						check_state=1;//有輸入運算值時，state變成1，check才會+1
					}
					else if(A<14){
						if(check_state==1){//為避免運算式輸入錯誤，當有正確輸入時check會等於0
							check+=1;
							check_state=0;//輸入運算元後，state變成0，連續輸入運算元，或是第一個輸入元素就是運算元時，check就會少加變成負
						}

						equ[k]=B;//存取運算值
						display_number(SEG_gpio,DIN_pin,CS_pin,CLK_pin,0,0);
						B=0;//運算值暫存歸零
						equ[k+1]=A;//存取運算元
						k += 2;
						check-=1;
					}
					else if(A==15){//運算式讀取完畢
						equ[k]=B;
						B=0;

						display_number(SEG_gpio,DIN_pin,CS_pin,CLK_pin,0,0);
						delay_without_interrupt(200);

						if(check==0)//check正確，做四則運算
							while_switch=0;//跳出while做四則運算再回來(也可以把四則運算的內容放在這裡)
						else{//check錯誤，顯示-1
							send_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin,3,SEG_DATA_DECODE_DASH);
							send_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin,2,1);
						}
					}
					else{//clear
						B=0;
						k=0;
						check=0;
						display_number(SEG_gpio,DIN_pin,CS_pin,CLK_pin,0,0);
					}
				}
			}
		}
	}

	//四則運算
	//優先序 除法>>乘法>>加&減

	//除法
	for(int j=1;j<=k-1;j+=2){
		if(equ[j]==13){
			equ[j-1] = equ[j-1]/equ[j+1];
			for(int m=j+2;m<=k;m++)//後方未計算的元素往前搬動整理，從j+2到k的元素都要被搬動
				equ[m-2]=equ[m];
			k -= 2;//有做任何運算，因為搬動，都要少掉最後兩位，即新的運算式會縮小2個位置
			j -= 2;//避免連續相同運算元被漏掉，偵測要返回新的起始位置
		}
	}

	//乘法
	for(int j=1;j<=k-1;j+=2){
		if(equ[j]==12){
			equ[j-1] = equ[j-1]*equ[j+1];
			for(int m=j+2;m<=k;m++)
				equ[m-2]=equ[m];
			k -= 2;
			j -= 2;
		}
	}

	//加法
	for(int j=1;j<=k-1;j+=2){
		if(equ[j]==10){
			equ[j-1] = equ[j-1]+equ[j+1];
			for(int m=j+2;m<=k;m++)
				equ[m-2]=equ[m];
			k -= 2;
			j -= 2;
		}
	}

	//減法
	for(int j=1;j<=k-1;j+=2){
		if(equ[j]==11){
			equ[j-1] = equ[j-1]-equ[j+1];
			for(int m=j+2;m<=k;m++)
				equ[m-2]=equ[m];
			k -= 2;
			j -= 2;
		}
	}

	//處理運算結果，負號、小數，小數點：0b10000000
	//該區段可以置入display_number
	//取小數第三位
	int ANS= equ[0]*1000;
	int DP=4;
	for(int i=1;i<=3;i++){
		if(ANS%10==0){
			ANS/=10;
			DP--;
		}
	}
	//尋找與小數點一起的數字
	int num_with_DP=ANS;
	for(int i=1;i<DP;i++){
		num_with_DP/=10;
	}
	num_with_DP%=10;

	//display_number(SEG_gpio,DIN_pin,CS_pin,CLK_pin,num_with_DP,num_digits(num_with_DP));
	//delay_without_interrupt(1000);
	if(ANS<0){
		ANS *= -1;
		num_with_DP *= -1;
		display_number(SEG_gpio,DIN_pin,CS_pin,CLK_pin,ANS,num_digits(ANS));
		send_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin,DP,num_with_DP|0b10000000);
		send_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin,num_digits(ANS)+2,SEG_DATA_DECODE_DASH);
	}
	else{
		display_number(SEG_gpio,DIN_pin,CS_pin,CLK_pin,ANS,num_digits(ANS));
		send_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin,DP,num_with_DP|0b10000000);
	}

	while_switch=1;
	goto restart;//返回while
}
