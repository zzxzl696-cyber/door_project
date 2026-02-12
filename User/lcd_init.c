/********************************** (C) COPYRIGHT *******************************
* File Name          : lcd_init.c
* Author             : Ported from STM32 to CH32V30x
* Version            : V2.0.0
* Date               : 2025-12-30
* Description        : ST7735S 1.8" LCD初始化实现 (硬件SPI版本)
*                      使用SPI2硬件外设，速度提升10倍以上
*********************************************************************************/

#include "lcd_init.h"
#include "debug.h"

/**
 * @brief  SPI2硬件+GPIO初始化
 */
void LCD_SPI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef  SPI_InitStructure = {0};

    /* 使能时钟 */
    RCC_APB2PeriphClockCmd(LCD_SPI_GPIO_CLK, ENABLE);  // GPIOB时钟
    RCC_APB1PeriphClockCmd(LCD_SPI_CLK, ENABLE);       // SPI2时钟

    /* 配置SPI引脚: SCK和MOSI为复用推挽输出 */
    GPIO_InitStructure.GPIO_Pin = LCD_SPI_SCK_PIN | LCD_SPI_MOSI_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LCD_SPI_SCK_PORT, &GPIO_InitStructure);

    /* 配置控制引脚: CS, DC, RES, BLK为普通推挽输出 */
    GPIO_InitStructure.GPIO_Pin = LCD_CS_PIN | LCD_DC_PIN | LCD_RES_PIN | LCD_BLK_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* SPI2配置 */
    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;       // 单线发送模式
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;                   // 主机模式
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;               // 8位数据
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;                     // 时钟空闲高电平
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;                    // 第二个时钟沿采样
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;                       // 软件片选
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2; // 分频系数2 (48MHz / 2 = 24MHz)
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;              // 高位先传
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(LCD_SPI, &SPI_InitStructure);

    /* 使能SPI2 */
    SPI_Cmd(LCD_SPI, ENABLE);

    /* 初始化控制引脚状态 */
    LCD_CS_Set();
    LCD_DC_Set();
    LCD_RES_Set();
    LCD_BLK_Set();  // 点亮背光
}

/**
 * @brief  硬件SPI写入一个字节
 * @param  dat: 要写入的字节
 */
void LCD_Writ_Bus(uint8_t dat)
{
    LCD_CS_Clr();  // 片选拉低

    /* 等待发送缓冲区空 */
    while (SPI_I2S_GetFlagStatus(LCD_SPI, SPI_I2S_FLAG_TXE) == RESET);

    /* 发送数据 */
    SPI_I2S_SendData(LCD_SPI, dat);

    /* 等待发送完成 */
    while (SPI_I2S_GetFlagStatus(LCD_SPI, SPI_I2S_FLAG_BSY) == SET);

    LCD_CS_Set();  // 片选拉高
}

/**
 * @brief  写入8位数据
 */
void LCD_WR_DATA8(uint8_t dat)
{
    LCD_Writ_Bus(dat);
}

/**
 * @brief  写入16位数据
 */
void LCD_WR_DATA(uint16_t dat)
{
    LCD_Writ_Bus(dat >> 8);  // 高字节先发送
    LCD_Writ_Bus(dat);       // 低字节后发送
}

/**
 * @brief  写入寄存器/命令
 */
void LCD_WR_REG(uint8_t dat)
{
    LCD_DC_Clr();      // DC=0 表示写命令
    LCD_Writ_Bus(dat);
    LCD_DC_Set();      // DC=1 表示写数据
}

/**
 * @brief  设置显示窗口地址
 * @param  x1,y1: 起始坐标
 * @param  x2,y2: 结束坐标
 */
void LCD_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    if (USE_HORIZONTAL == 0)
    {
        LCD_WR_REG(0x2a);  // 列地址设置
        LCD_WR_DATA(x1 + 2);
        LCD_WR_DATA(x2 + 2);

        LCD_WR_REG(0x2b);  // 行地址设置
        LCD_WR_DATA(y1 + 1);
        LCD_WR_DATA(y2 + 1);

        LCD_WR_REG(0x2c);  // 写入显存
    }
    else if (USE_HORIZONTAL == 1)
    {
        LCD_WR_REG(0x2a);
        LCD_WR_DATA(x1 + 2);
        LCD_WR_DATA(x2 + 2);

        LCD_WR_REG(0x2b);
        LCD_WR_DATA(y1 + 1);
        LCD_WR_DATA(y2 + 1);

        LCD_WR_REG(0x2c);
    }
    else if (USE_HORIZONTAL == 2)
    {
        LCD_WR_REG(0x2a);
        LCD_WR_DATA(x1 + 1);
        LCD_WR_DATA(x2 + 1);

        LCD_WR_REG(0x2b);
        LCD_WR_DATA(y1 + 2);
        LCD_WR_DATA(y2 + 2);

        LCD_WR_REG(0x2c);
    }
    else
    {
        LCD_WR_REG(0x2a);
        LCD_WR_DATA(x1 + 1);
        LCD_WR_DATA(x2 + 1);

        LCD_WR_REG(0x2b);
        LCD_WR_DATA(y1 + 2);
        LCD_WR_DATA(y2 + 2);

        LCD_WR_REG(0x2c);
    }
}

/**
 * @brief  LCD完整初始化
 */
void LCD_Init(void)
{
    /* 初始化SPI和GPIO */
    LCD_SPI_Init();

    /* 硬件复位 */
    LCD_RES_Clr();
    Delay_Ms(100);
    LCD_RES_Set();
    Delay_Ms(100);

    /* ST7735S初始化序列 */
    LCD_BLK_Set();  // 打开背光

    LCD_WR_REG(0x11);  // Sleep out
    Delay_Ms(120);

    // ST7735R Frame Rate
    LCD_WR_REG(0xB1);
    LCD_WR_DATA8(0x01);
    LCD_WR_DATA8(0x2C);
    LCD_WR_DATA8(0x2D);

    LCD_WR_REG(0xB2);
    LCD_WR_DATA8(0x01);
    LCD_WR_DATA8(0x2C);
    LCD_WR_DATA8(0x2D);

    LCD_WR_REG(0xB3);
    LCD_WR_DATA8(0x01);
    LCD_WR_DATA8(0x2C);
    LCD_WR_DATA8(0x2D);
    LCD_WR_DATA8(0x01);
    LCD_WR_DATA8(0x2C);
    LCD_WR_DATA8(0x2D);

    LCD_WR_REG(0xB4);  // Column inversion
    LCD_WR_DATA8(0x07);

    // ST7735R Power Sequence
    LCD_WR_REG(0xC0);
    LCD_WR_DATA8(0xA2);
    LCD_WR_DATA8(0x02);
    LCD_WR_DATA8(0x84);

    LCD_WR_REG(0xC1);
    LCD_WR_DATA8(0xC5);

    LCD_WR_REG(0xC2);
    LCD_WR_DATA8(0x0A);
    LCD_WR_DATA8(0x00);

    LCD_WR_REG(0xC3);
    LCD_WR_DATA8(0x8A);
    LCD_WR_DATA8(0x2A);

    LCD_WR_REG(0xC4);
    LCD_WR_DATA8(0x8A);
    LCD_WR_DATA8(0xEE);

    LCD_WR_REG(0xC5);  // VCOM
    LCD_WR_DATA8(0x0E);

    LCD_WR_REG(0x36);  // MX, MY, RGB mode
    if (USE_HORIZONTAL == 0)
        LCD_WR_DATA8(0x08);
    else if (USE_HORIZONTAL == 1)
        LCD_WR_DATA8(0xC8);
    else if (USE_HORIZONTAL == 2)
        LCD_WR_DATA8(0x78);
    else
        LCD_WR_DATA8(0xA8);

    // ST7735R Gamma Sequence
    LCD_WR_REG(0xe0);
    LCD_WR_DATA8(0x0f);
    LCD_WR_DATA8(0x1a);
    LCD_WR_DATA8(0x0f);
    LCD_WR_DATA8(0x18);
    LCD_WR_DATA8(0x2f);
    LCD_WR_DATA8(0x28);
    LCD_WR_DATA8(0x20);
    LCD_WR_DATA8(0x22);
    LCD_WR_DATA8(0x1f);
    LCD_WR_DATA8(0x1b);
    LCD_WR_DATA8(0x23);
    LCD_WR_DATA8(0x37);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x07);
    LCD_WR_DATA8(0x02);
    LCD_WR_DATA8(0x10);

    LCD_WR_REG(0xe1);
    LCD_WR_DATA8(0x0f);
    LCD_WR_DATA8(0x1b);
    LCD_WR_DATA8(0x0f);
    LCD_WR_DATA8(0x17);
    LCD_WR_DATA8(0x33);
    LCD_WR_DATA8(0x2c);
    LCD_WR_DATA8(0x29);
    LCD_WR_DATA8(0x2e);
    LCD_WR_DATA8(0x30);
    LCD_WR_DATA8(0x30);
    LCD_WR_DATA8(0x39);
    LCD_WR_DATA8(0x3f);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x07);
    LCD_WR_DATA8(0x03);
    LCD_WR_DATA8(0x10);

    LCD_WR_REG(0x2a);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x7f);

    LCD_WR_REG(0x2b);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x9f);

    LCD_WR_REG(0xF0);  // Enable test command
    LCD_WR_DATA8(0x01);

    LCD_WR_REG(0xF6);  // Disable ram power save mode
    LCD_WR_DATA8(0x00);

    LCD_WR_REG(0x3A);  // 65k mode
    LCD_WR_DATA8(0x05);

    LCD_WR_REG(0x29);  // Display on
}
