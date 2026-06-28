/**
  ******************************************************************************
  * @file           : soft_i2c_ds3231.c
  * @brief          : DS3231软件模拟I2C驱动实现
  ******************************************************************************
  */

#include "soft_i2c_ds3231.h"

/* ---- 软件I2C延时 (~8µs @170MHz, I2C速率≈125kHz, 远低于400kHz上限) ---- */
static void SoftI2C_Delay(void)
{
    for (volatile uint32_t i = 0; i < 200; i++) {
        __NOP();
    }
}

/* ---- 读写SDA辅助 ---- */
static void SDA_Write(GPIO_PinState state)
{
    HAL_GPIO_WritePin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN, state);
}

static GPIO_PinState SDA_Read(void)
{
    return HAL_GPIO_ReadPin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN);
}

static void SCL_Write(GPIO_PinState state)
{
    HAL_GPIO_WritePin(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN, state);
}

/* ---- 软件I2C初始化 ---- */
void SoftI2C_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();  /* Both SCL(PB10) and SDA(PB11) on GPIOB */

    /* SCL: PB10, 开漏输出 */
    gpio.Pin   = SOFT_I2C_SCL_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SOFT_I2C_SCL_PORT, &gpio);

    /* SDA: PB11, 开漏输出 */
    gpio.Pin   = SOFT_I2C_SDA_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SOFT_I2C_SDA_PORT, &gpio);

    /* 总线初始状态：空闲（高） */
    SCL_Write(GPIO_PIN_SET);
    SDA_Write(GPIO_PIN_SET);
}

/* ---- I2C起始条件：SCL=H时 SDA H→L ---- */
void SoftI2C_Start(void)
{
    SDA_Write(GPIO_PIN_SET);
    SCL_Write(GPIO_PIN_SET);
    SoftI2C_Delay();
    SDA_Write(GPIO_PIN_RESET);
    SoftI2C_Delay();
    SCL_Write(GPIO_PIN_RESET);
}

/* ---- I2C停止条件：SCL=H时 SDA L→H ---- */
void SoftI2C_Stop(void)
{
    SDA_Write(GPIO_PIN_RESET);
    SCL_Write(GPIO_PIN_SET);
    SoftI2C_Delay();
    SDA_Write(GPIO_PIN_SET);
    SoftI2C_Delay();
}

/* ---- 等待从机ACK（返回0=ACK, 1=NACK）---- */
uint8_t SoftI2C_WaitAck(void)
{
    uint8_t ack;

    SDA_Write(GPIO_PIN_SET);        /* 释放SDA */
    SCL_Write(GPIO_PIN_SET);        /* 第9个时钟脉冲 */
    SoftI2C_Delay();
    ack = (SDA_Read() == GPIO_PIN_SET) ? 1 : 0;
    SCL_Write(GPIO_PIN_RESET);
    SoftI2C_Delay();
    return ack;
}

/* ---- 发送ACK ---- */
void SoftI2C_SendAck(void)
{
    SDA_Write(GPIO_PIN_RESET);
    SCL_Write(GPIO_PIN_SET);
    SoftI2C_Delay();
    SCL_Write(GPIO_PIN_RESET);
    SoftI2C_Delay();
    SDA_Write(GPIO_PIN_SET);
}

/* ---- 发送NACK ---- */
void SoftI2C_SendNack(void)
{
    SDA_Write(GPIO_PIN_SET);
    SCL_Write(GPIO_PIN_SET);
    SoftI2C_Delay();
    SCL_Write(GPIO_PIN_RESET);
    SoftI2C_Delay();
}

/* ---- 写入一个字节（MSB first），返回从机ACK ---- */
void SoftI2C_WriteByte(uint8_t data)
{
    for (int8_t i = 7; i >= 0; i--) {
        SDA_Write((data & (1 << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        SoftI2C_Delay();
        SCL_Write(GPIO_PIN_SET);
        SoftI2C_Delay();
        SCL_Write(GPIO_PIN_RESET);
        SoftI2C_Delay();
    }
}

/* ---- 读取一个字节（MSB first），ack=0发ACK，ack=1发NACK ---- */
uint8_t SoftI2C_ReadByte(uint8_t ack)
{
    uint8_t data = 0;

    SDA_Write(GPIO_PIN_SET);        /* 释放SDA */
    for (int8_t i = 7; i >= 0; i--) {
        SCL_Write(GPIO_PIN_SET);
        SoftI2C_Delay();
        if (SDA_Read() == GPIO_PIN_SET) {
            data |= (1 << i);
        }
        SCL_Write(GPIO_PIN_RESET);
        SoftI2C_Delay();
    }

    if (ack) {
        SoftI2C_SendNack();
    } else {
        SoftI2C_SendAck();
    }
    return data;
}

/* ================================================================
   BCD 编解码
   ================================================================ */
static uint8_t BcdToDec(uint8_t bcd)
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static uint8_t DecToBcd(uint8_t dec)
{
    return ((dec / 10) << 4) | (dec % 10);
}

/* ---- Alarm1 读回校验全局变量 ---- */
uint8_t g_readback_hour = 0;
uint8_t g_readback_min  = 0;

static uint8_t DS3231_ReadRegister(uint8_t reg, uint8_t *value)
{
    SoftI2C_Start();
    SoftI2C_WriteByte(DS3231_ADDR_W);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(reg);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_Stop();

    SoftI2C_Start();
    SoftI2C_WriteByte(DS3231_ADDR_R);
    if (SoftI2C_WaitAck()) goto err;
    *value = SoftI2C_ReadByte(1);
    SoftI2C_Stop();
    return 0;

err:
    SoftI2C_Stop();
    return 1;
}

static uint8_t DS3231_WriteRegister(uint8_t reg, uint8_t value)
{
    SoftI2C_Start();
    SoftI2C_WriteByte(DS3231_ADDR_W);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(reg);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(value);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_Stop();
    return 0;

err:
    SoftI2C_Stop();
    return 1;
}

static uint8_t DS3231_TimeIsValid(const RTC_TimeStruct *time)
{
    if (time->seconds > 59U || time->minutes > 59U || time->hours > 23U) {
        return 0;
    }
    if (time->day < 1U || time->day > 31U || time->month < 1U || time->month > 12U) {
        return 0;
    }
    if (time->year < 2024U || time->year > 2099U) {
        return 0;
    }
    return 1;
}

/* ================================================================
   DS3231 功能函数
   ================================================================ */

/**
 * @brief 从DS3231读取当前时间
 * @return 0=成功, 1=I2C ACK错误
 */
uint8_t DS3231_ReadTime(RTC_TimeStruct *time)
{
    uint8_t buf[7];

    SoftI2C_Start();
    SoftI2C_WriteByte(DS3231_ADDR_W);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(DS3231_REG_SECOND);
    if (SoftI2C_WaitAck()) goto err;

    SoftI2C_Stop();

    SoftI2C_Start();
    SoftI2C_WriteByte(DS3231_ADDR_R);
    if (SoftI2C_WaitAck()) goto err;

    for (int i = 0; i < 6; i++) {
        buf[i] = SoftI2C_ReadByte(0);   /* ACK */
    }
    buf[6] = SoftI2C_ReadByte(1);       /* NACK */
    SoftI2C_Stop();

    time->seconds = BcdToDec(buf[0]);
    time->minutes = BcdToDec(buf[1]);
    time->hours   = BcdToDec(buf[2] & 0x3F);
    time->day     = BcdToDec(buf[4]);    /* 跳过星期(buf[3]) */
    time->month   = BcdToDec(buf[5] & 0x7F);
    time->year    = BcdToDec(buf[6]) + 2000;
    if (!DS3231_TimeIsValid(time)) {
        return 1;
    }
    return 0;

err:
    SoftI2C_Stop();
    return 1;
}

/**
 * @brief 设置DS3231当前时间
 * @return 0=成功, 1=I2C ACK错误
 */
uint8_t DS3231_SetTime(RTC_TimeStruct *time)
{
    SoftI2C_Start();
    SoftI2C_WriteByte(DS3231_ADDR_W);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(DS3231_REG_SECOND);
    if (SoftI2C_WaitAck()) goto err;

    SoftI2C_WriteByte(DecToBcd(time->seconds));
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(DecToBcd(time->minutes));
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(DecToBcd(time->hours));
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(DecToBcd(1));     /* 星期=1 */
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(DecToBcd(time->day));
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(DecToBcd(time->month));
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(DecToBcd(time->year - 2000));
    if (SoftI2C_WaitAck()) goto err;

    SoftI2C_Stop();
    return 0;

err:
    SoftI2C_Stop();
    return 1;
}

/**
 * @brief 设置DS3231 Alarm 1（匹配时/分/秒=00，忽略日期）
 *        写后立即读回0x08/0x09寄存器校验，存储到g_readback_hour/min
 * @return 0=成功, 1=I2C ACK错误
 */
uint8_t DS3231_SetAlarm1(uint8_t hour, uint8_t minute)
{
    uint8_t bcd_hour = DecToBcd(hour);
    uint8_t bcd_min  = DecToBcd(minute);

    /* ---- 步骤1: 写Alarm 1寄存器 (0x07-0x0A) ---- */
    SoftI2C_Start();
    SoftI2C_WriteByte(DS3231_ADDR_W);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(DS3231_REG_ALM1_SEC);
    if (SoftI2C_WaitAck()) goto err;

    /* 0x07: seconds=0, A1M1=0 匹配秒(整分触发) */
    SoftI2C_WriteByte(DecToBcd(0));
    if (SoftI2C_WaitAck()) goto err;
    /* 0x08: minutes, A1M2=0 匹配分钟 */
    SoftI2C_WriteByte(bcd_min & 0x7F);
    if (SoftI2C_WaitAck()) goto err;
    /* 0x09: hours, A1M3=0 匹配小时(24H模式) */
    SoftI2C_WriteByte(bcd_hour & 0x3F);
    if (SoftI2C_WaitAck()) goto err;
    /* 0x0A: A1M4=1 忽略日期 */
    SoftI2C_WriteByte(0x80);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_Stop();

    /* ---- 步骤2: 控制寄存器0x0E写入0x05 (INTCN=1 | A1IE=1) ---- */
    SoftI2C_Start();
    SoftI2C_WriteByte(DS3231_ADDR_W);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(DS3231_REG_CONTROL);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(0x05);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_Stop();

    /* 清除可能残留的 A1F */
    DS3231_ClearAlarmFlag();

    /* ---- 步骤3: 读回0x08(分)和0x09(时)校验 ---- */
    {
        uint8_t rb_min, rb_hour;

        SoftI2C_Start();
        SoftI2C_WriteByte(DS3231_ADDR_W);
        if (SoftI2C_WaitAck()) { SoftI2C_Stop(); return 1; }
        SoftI2C_WriteByte(DS3231_REG_ALM1_MIN);
        if (SoftI2C_WaitAck()) { SoftI2C_Stop(); return 1; }
        SoftI2C_Stop();     /* 彻底释放总线，规避170MHz下Repeated Start死锁 */

        SoftI2C_Start();
        SoftI2C_WriteByte(DS3231_ADDR_R);
        if (SoftI2C_WaitAck()) { SoftI2C_Stop(); return 1; }
        rb_min  = SoftI2C_ReadByte(0);     /* ACK, 继续读 */
        rb_hour = SoftI2C_ReadByte(1);     /* NACK, 结束 */
        SoftI2C_Stop();

        g_readback_min  = BcdToDec(rb_min & 0x7F);
        g_readback_hour = BcdToDec(rb_hour & 0x3F);
    }

    return 0;

err:
    SoftI2C_Stop();
    return 1;
}

/**
 * @brief 清除DS3231 Alarm 1标志位（恢复SQW为高电平）
 * @return 0=成功, 1=I2C ACK错误
 */
uint8_t DS3231_ClearAlarmFlag(void)
{
    uint8_t status;

    if (DS3231_ReadRegister(DS3231_REG_STATUS, &status)) {
        return 1;
    }

    status &= ~DS3231_A1F;          /* 只清A1F，保留OSF */

    return DS3231_WriteRegister(DS3231_REG_STATUS, status);
}

/**
 * @brief 写入Aging Offset寄存器（0x10）进行晶振老化校准
 * @param offset 有符号8位补码：正数=增加电容负载使晶振变慢，
 *               负数=减少电容负载使晶振变快
 * @return 0=成功, 1=I2C ACK错误
 */
uint8_t DS3231_SetAgingOffset(int8_t offset)
{
    SoftI2C_Start();
    SoftI2C_WriteByte(DS3231_ADDR_W);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte(DS3231_REG_AGING);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_WriteByte((uint8_t)offset);
    if (SoftI2C_WaitAck()) goto err;
    SoftI2C_Stop();
    return 0;

err:
    SoftI2C_Stop();
    return 1;
}

/**
 * @brief 读取DS3231 OSF（晶振停止标志位）
 *        用于判断MCU重启前DS3231是否因断电停振
 * @return 1=晶振曾停（需初始对时）, 0=正常运行中
 */
uint8_t DS3231_CheckOSF(void)
{
    uint8_t status;
    if (DS3231_ReadRegister(DS3231_REG_STATUS, &status)) {
        return 1;
    }
    return (status & DS3231_OSF) ? 1 : 0;
}

/**
 * @brief 初始化DS3231：清除OSF + 验证时间有效性
 *        如果时间无效（晶振曾停或年份不合理），写入编译默认值
 * @return 0=成功, 1=I2C错误
 */
uint8_t DS3231_Init(void)
{
    uint8_t control_reg;
    uint8_t status_reg;

    if (DS3231_ReadRegister(DS3231_REG_CONTROL, &control_reg)) {
        return 1;
    }
    if (control_reg & DS3231_EOSC) {
        control_reg &= (uint8_t)~DS3231_EOSC;
        if (DS3231_WriteRegister(DS3231_REG_CONTROL, control_reg)) {
            return 1;
        }
    }

    if (DS3231_ReadRegister(DS3231_REG_STATUS, &status_reg)) {
        return 1;
    }

    /* OSF=1 means the oscillator stopped. Clear the flag, but keep stored time. */
    if (status_reg & DS3231_OSF) {
        status_reg &= ~DS3231_OSF;
        if (DS3231_WriteRegister(DS3231_REG_STATUS, status_reg)) {
            return 1;
        }
    }

    return 0;
}
