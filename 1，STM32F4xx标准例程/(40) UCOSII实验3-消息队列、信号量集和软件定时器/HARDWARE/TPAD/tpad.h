#ifndef __TPAD_H
#define __TPAD_H
#include "sys.h"
	
//////////////////////////////////////////////////////////////////////////////////	 
 
//TPAD驱动代码	   
//STM32F4工程-库函数版本
//淘宝店铺：http://mcudev.taobao.com							  
////////////////////////////////////////////////////////////////////////////////// 	
	   
//空载的时候(没有手按下),计数器需要的时间
//这个值应该在每次开机的时候被初始化一次
extern vu16 tpad_default_val;
							   	    
void TPAD_Reset(void);
u16  TPAD_Get_Val(void);
u16 TPAD_Get_MaxVal(u8 n);
u8   TPAD_Init(u8 systick);
u8   TPAD_Scan(u8 mode);
void TIM2_CH1_Cap_Init(u32 arr,u16 psc);     
#endif























