#include "stm32l476xx.h"
#include "7seg.h"
#include "keypad.h"
#include "helper_functions.h"

#define SEG_gpio GPIOC //PC port
#define DIN_pin 5//��ܪ����_PC5
#define CS_pin 6//���Ū���}�� PC6
#define CLK_pin 8//���� PC8

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

//Ū���@�����
void send_7seg(GPIO_TypeDef* gpio,int DIN,int CS,int CLK,int address,int data){
	int payload = ((address & 0xFF) << 8) | (data & 0xFF);

	//�C����Ʀ@16��D0~D15 �̫�j��+1�NCS���]
	int total_cycles = 16+1;

	for(int a=1 ; a <= total_cycles ; a++){
		reset_gpio(gpio,CLK);//�p��CLK����Ĳ�o rising

		//���b�̫�j��ɡAŪ��DIN���
		if( ( (payload >> (total_cycles-1-a)) &0x01) && a!=total_cycles){
			set_gpio(gpio,DIN);
		}
		else{
			reset_gpio(gpio,DIN);
		}

		//�̫�j�魫�]CS
		if(a==total_cycles)
			set_gpio(gpio,CS);
		else
			reset_gpio(gpio,CS);

		set_gpio(gpio,CLK);//�C���^��CLK�k��
	}

	return;
}

//��l�ơA�]�w�}��
int init_7seg(GPIO_TypeDef* gpio,int DIN,int CS,int CLK){
	if(gpio == GPIOA)
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	else if(gpio == GPIOB)
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
	else if(gpio ==GPIOC)
		RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN;
	else
		return -1;

	//�N�}��]��OUTPUT
	gpio -> MODER &= ~(0b11 << (2*DIN));
	gpio -> MODER |= (0b01 << (2*DIN));
	gpio -> MODER &= ~(0b11 << (2*CS));
	gpio -> MODER |= (0b01 << (2*CS));
	gpio -> MODER &= ~(0b11 << (2*CLK));
	gpio -> MODER |= (0b01 << (2*CLK));

	//����DISPLAY TEST
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

	//�}��DECODE MODE D0~07
	send_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin,SEG_ADDRESS_DECODE_MODE,0xFF);
	//SCAN LIMIT �]�� DIGIT_0~7
	send_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin,SEG_ADDRESS_SCAN_LIMIT,0x07);
	//shutdown����->normal operation p.s. shutdown���}(0x00)�AŪ���T���}�쪽�����a�A�Y���AŪ������T��
	send_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin,SEG_ADDRESS_SHUTDOWN,0x01);
	//�վ���j��
	send_7seg(SEG_gpio,DIN_pin,CS_pin,CLK_pin,SEG_ADDRESS_ITENSITY,0x05);

	display_number(SEG_gpio,DIN_pin,CS_pin,CLK_pin,0,0);

	if(init_keypad(ROW_gpio,COL_gpio,ROW_pin,COL_pin)!=0)
		return -1;

	int B=0;//�Ȧs��B�A�s���B���
	double equ[1000];//�s������B�⦡�A�åB��J�����ᰵ�|�h�B��C�]���B��L�{�|�X�{�p�ơA�ŧi��double
	//�B�⤸�M�B��Ȥ]�i�H���}�Ӧs�C
	int k=0;//�B�⦡equ������m
	int while_switch=1;//Ū���B�⦡������A���_while
	int check_state=0;//���~����
	int check=0;//���~����
	restart:
	while(while_switch){//while���u�@�w���GŪ���B�⦡�BCLEAR��\��(�����B��L�{��b����if�O�@�˪�)
		for(int i=0;i<4;i++){
			for(int j=0;j<4;j++){
				if(check_keypad_input_one(ROW_gpio,COL_gpio,ROW_pin,COL_pin,i,j)){
					int A = keypad[i][j];//�Ȧs��A�A�s�������J�A�]�t�B�⤸�B�B��ȡBclear
					delay_without_interrupt(100);
					//Ū���B�⦡�A�ݥ��ݧ����l�~��M�w�|�h�B�⪺����
					if(A<10){//��J�B���
						B = B*10+A;
						if(B>999)//�B��Ȥ��i�W�L999
							B/=10;
						display_number(SEG_gpio,DIN_pin,CS_pin,CLK_pin,B,num_digits(B));
						check_state=1;//����J�B��ȮɡAstate�ܦ�1�Acheck�~�|+1
					}
					else if(A<14){
						if(check_state==1){//���קK�B�⦡��J���~�A�����T��J��check�|����0
							check+=1;
							check_state=0;//��J�B�⤸��Astate�ܦ�0�A�s���J�B�⤸�A�άO�Ĥ@�ӿ�J�����N�O�B�⤸�ɡAcheck�N�|�֥[�ܦ��t
						}

						equ[k]=B;//�s���B���
						display_number(SEG_gpio,DIN_pin,CS_pin,CLK_pin,0,0);
						B=0;//�B��ȼȦs�k�s
						equ[k+1]=A;//�s���B�⤸
						k += 2;
						check-=1;
					}
					else if(A==15){//�B�⦡Ū������
						equ[k]=B;
						B=0;

						display_number(SEG_gpio,DIN_pin,CS_pin,CLK_pin,0,0);
						delay_without_interrupt(200);

						if(check==0)//check���T�A���|�h�B��
							while_switch=0;//���Xwhile���|�h�B��A�^��(�]�i�H��|�h�B�⪺���e��b�o��)
						else{//check���~�A���-1
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

	//�|�h�B��
	//�u���� ���k>>���k>>�[&��

	//���k
	for(int j=1;j<=k-1;j+=2){
		if(equ[j]==13){
			equ[j-1] = equ[j-1]/equ[j+1];
			for(int m=j+2;m<=k;m++)//��襼�p�⪺�������e�h�ʾ�z�A�qj+2��k���������n�Q�h��
				equ[m-2]=equ[m];
			k -= 2;//��������B��A�]���h�ʡA���n�ֱ��̫���A�Y�s���B�⦡�|�Y�p2�Ӧ�m
			j -= 2;//�קK�s��ۦP�B�⤸�Q�|���A�����n��^�s���_�l��m
		}
	}

	//���k
	for(int j=1;j<=k-1;j+=2){
		if(equ[j]==12){
			equ[j-1] = equ[j-1]*equ[j+1];
			for(int m=j+2;m<=k;m++)
				equ[m-2]=equ[m];
			k -= 2;
			j -= 2;
		}
	}

	//�[�k
	for(int j=1;j<=k-1;j+=2){
		if(equ[j]==10){
			equ[j-1] = equ[j-1]+equ[j+1];
			for(int m=j+2;m<=k;m++)
				equ[m-2]=equ[m];
			k -= 2;
			j -= 2;
		}
	}

	//��k
	for(int j=1;j<=k-1;j+=2){
		if(equ[j]==11){
			equ[j-1] = equ[j-1]-equ[j+1];
			for(int m=j+2;m<=k;m++)
				equ[m-2]=equ[m];
			k -= 2;
			j -= 2;
		}
	}

	//�B�z�B�⵲�G�A�t���B�p�ơA�p���I�G0b10000000
	//�ӰϬq�i�H�m�Jdisplay_number
	//���p�ƲĤT��
	int ANS= equ[0]*1000;
	int DP=4;
	for(int i=1;i<=3;i++){
		if(ANS%10==0){
			ANS/=10;
			DP--;
		}
	}
	//�M��P�p���I�@�_���Ʀr
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
	goto restart;//��^while
}
