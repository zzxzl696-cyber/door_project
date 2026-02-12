/********************************** (C) COPYRIGHT *******************************
 * File Name          : as608.h
 * Author             : Ported from STM32 to CH32V30x
 * Version            : V1.0.0
 * Date               : 2026-01-21
 * Description        : AS608指纹识别模块驱动头文件
 *                      移植自STM32F103，适配CH32V30x
 *********************************************************************************/

#ifndef __AS608_H
#define __AS608_H

#include <stdio.h>
#include "ch32v30x.h"

// 指纹模块状态引脚 (根据实际硬件连接修改)
#define PS_STA_GPIO_PORT GPIOB
#define PS_STA_GPIO_PIN GPIO_Pin_0
#define PS_Sta GPIO_ReadInputDataBit(PS_STA_GPIO_PORT, PS_STA_GPIO_PIN)

// 特征缓冲区定义
#define CharBuffer1 0x01
#define CharBuffer2 0x02

// 全局变量
extern uint32_t AS608Addr; // 模块地址

// 搜索结果结构体
typedef struct
{
	uint16_t pageID;	// 指纹ID
	uint16_t mathscore; // 匹配得分
} SearchResult;

// 系统参数结构体
typedef struct
{
	uint16_t PS_max;  // 指纹最大容量
	uint8_t PS_level; // 安全等级
	uint32_t PS_addr; // 模块地址
	uint8_t PS_size;  // 通讯数据包大小
	uint8_t PS_N;	  // 波特率倍数N
} SysPara;

// 函数声明

/**
 * @brief 初始化状态引脚GPIO
 */
void PS_StaGPIO_Init(void);

/**
 * @brief 录入图像
 * @return 确认码
 */
uint8_t PS_GetImage(void);

/**
 * @brief 生成特征
 * @param BufferID 缓冲区ID (CharBuffer1/CharBuffer2)
 * @return 确认码
 */
uint8_t PS_GenChar(uint8_t BufferID);

/**
 * @brief 精确比对两枚指纹特征
 * @return 确认码
 */
uint8_t PS_Match(void);

/**
 * @brief 搜索指纹
 * @param BufferID 缓冲区ID
 * @param StartPage 起始页
 * @param PageNum 页数
 * @param p 搜索结果指针
 * @return 确认码
 */
uint8_t PS_Search(uint8_t BufferID, uint16_t StartPage, uint16_t PageNum, SearchResult *p);

/**
 * @brief 合并特征生成模板
 * @return 确认码
 */
uint8_t PS_RegModel(void);

/**
 * @brief 储存模板
 * @param BufferID 缓冲区ID
 * @param PageID 指纹库位置号
 * @return 确认码
 */
uint8_t PS_StoreChar(uint8_t BufferID, uint16_t PageID);

/**
 * @brief 删除模板
 * @param PageID 指纹库模板号
 * @param N 删除的模板个数
 * @return 确认码
 */
uint8_t PS_DeletChar(uint16_t PageID, uint16_t N);

/**
 * @brief 清空指纹库
 * @return 确认码
 */
uint8_t PS_Empty(void);

/**
 * @brief 写系统寄存器
 * @param RegNum 寄存器序号
 * @param DATA 数据
 * @return 确认码
 */
uint8_t PS_WriteReg(uint8_t RegNum, uint8_t DATA);

/**
 * @brief 读系统基本参数
 * @param p 参数结构体指针
 * @return 确认码
 */
uint8_t PS_ReadSysPara(SysPara *p);

/**
 * @brief 设置模块地址
 * @param addr 新地址
 * @return 确认码
 */
uint8_t PS_SetAddr(uint32_t addr);

/**
 * @brief 写记事本
 * @param NotePageNum 记事本页码(0~15)
 * @param content 内容(32字节)
 * @return 确认码
 */
uint8_t PS_WriteNotepad(uint8_t NotePageNum, uint8_t *content);

/**
 * @brief 读记事本
 * @param NotePageNum 记事本页码(0~15)
 * @param note 读取的内容(32字节)
 * @return 确认码
 */
uint8_t PS_ReadNotepad(uint8_t NotePageNum, uint8_t *note);

/**
 * @brief 高速搜索
 * @param BufferID 缓冲区ID
 * @param StartPage 起始页
 * @param PageNum 页数
 * @param p 搜索结果指针
 * @return 确认码
 */
uint8_t PS_HighSpeedSearch(uint8_t BufferID, uint16_t StartPage, uint16_t PageNum, SearchResult *p);

/**
 * @brief 读有效模板个数
 * @param ValidN 有效模板个数指针
 * @return 确认码
 */
uint8_t PS_ValidTempleteNum(uint16_t *ValidN);

/**
 * @brief 与AS608模块握手
 * @param PS_Addr 地址指针
 * @return 0=成功, 1=失败
 */
uint8_t PS_HandShake(uint32_t *PS_Addr);

/**
 * @brief 确认码转错误信息
 * @param ensure 确认码
 * @return 错误信息字符串
 */
const char *EnsureMessage(uint8_t ensure);

/**
 * @brief 录入指纹流程
 */
uint8_t Add_FR(uint16_t PageID);

/**
 * @brief 验证指纹流程
 */
uint8_t Press_FR(SearchResult *result);

/**
 * @brief 删除指纹流程
 */
uint8_t Del_FR(uint16_t PageID);

#endif // __AS608_H
