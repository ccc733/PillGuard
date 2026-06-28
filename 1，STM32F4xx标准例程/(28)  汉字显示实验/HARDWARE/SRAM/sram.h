#ifndef __SRAM_H
#define __SRAM_H															    
#include "sys.h" 
//////////////////////////////////////////////////////////////////////////////////	 
 
//外部SRAM 驱动代码	   
//STM32F4工程-库函数版本
//淘宝店铺：http://mcudev.taobao.com										  
//////////////////////////////////////////////////////////////////////////////////

											  
void FSMC_SRAM_Init(void);
void FSMC_SRAM_WriteBuffer(u8* pBuffer,u32 WriteAddr,u32 NumHalfwordToWrite);
void FSMC_SRAM_ReadBuffer(u8* pBuffer,u32 ReadAddr,u32 NumHalfwordToRead);


void fsmc_sram_test_write(u32 addr,u8 data);
u8 fsmc_sram_test_read(u32 addr);


#endif

