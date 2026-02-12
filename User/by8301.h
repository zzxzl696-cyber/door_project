/********************************** (C) COPYRIGHT *******************************
 * File Name          : by8301.h
 * Author             :
 * Version            : V1.2.0
 * Date               : 2026-01-30
 * Description        : BY8301-16P语音模块驱动头文件
 *                      使用UART4进行通信
 *********************************************************************************/

#ifndef __BY8301_H
#define __BY8301_H

#include "ch32v30x.h"

// ============== 硬件配置 ==============
#define BY8301_USART          UART4
#define BY8301_USART_CLK      RCC_APB1Periph_UART4
#define BY8301_GPIO_CLK       RCC_APB2Periph_GPIOC
#define BY8301_GPIO_PORT      GPIOC
#define BY8301_TX_PIN         GPIO_Pin_10    // PC10: UART4_TX
#define BY8301_RX_PIN         GPIO_Pin_11    // PC11: UART4_RX
#define BY8301_BAUDRATE       9600

// ============== 协议帧定义 ==============
#define BY8301_FRAME_HEAD         0x7E  // 帧头
#define BY8301_FRAME_END          0xEF  // 帧尾

// ============== 控制命令定义 (无参数) ==============
#define BY8301_CMD_PLAY           0x01  // 播放
#define BY8301_CMD_PAUSE          0x02  // 暂停
#define BY8301_CMD_NEXT           0x03  // 下一曲
#define BY8301_CMD_PREV           0x04  // 上一曲
#define BY8301_CMD_VOL_UP         0x05  // 音量加
#define BY8301_CMD_VOL_DOWN       0x06  // 音量减
#define BY8301_CMD_STANDBY        0x07  // 待机/正常工作
#define BY8301_CMD_RESET          0x09  // 复位
#define BY8301_CMD_STOP           0x0E  // 停止

// ============== 设置命令定义 (单参数) ==============
#define BY8301_CMD_SET_VOLUME     0x31  // 设置音量 (0-30)
#define BY8301_CMD_SET_EQ         0x32  // 设置EQ (0-5)
#define BY8301_CMD_SET_LOOP_MODE  0x33  // 设置循环模式 (0-4)

// ============== 播放命令定义 (双参数) ==============
#define BY8301_CMD_PLAY_INDEX     0x41  // 选择播放曲目 (1-255)
#define BY8301_CMD_INSERT_PLAY    0x43  // 插播功能

// ============== 查询命令定义 ==============
#define BY8301_CMD_QUERY_STATUS   0x10  // 查询播放状态
#define BY8301_CMD_QUERY_VOLUME   0x11  // 查询音量大小
#define BY8301_CMD_QUERY_EQ       0x12  // 查询当前EQ
#define BY8301_CMD_QUERY_LOOP     0x13  // 查询播放模式
#define BY8301_CMD_QUERY_VERSION  0x14  // 查询版本号
#define BY8301_CMD_QUERY_TOTAL    0x17  // 查询FLASH总文件数
#define BY8301_CMD_QUERY_CURRENT  0x1B  // 查询FLASH当前曲目

// ============== 循环模式 ==============
#define BY8301_LOOP_ALL           0x00  // 全盘循环
#define BY8301_LOOP_FOLDER        0x01  // 文件夹循环
#define BY8301_LOOP_SINGLE        0x02  // 单曲循环
#define BY8301_LOOP_RANDOM        0x03  // 随机播放
#define BY8301_LOOP_NONE          0x04  // 无循环(播放一次停止)

// ============== EQ模式 ==============
#define BY8301_EQ_NORMAL          0x00  // 正常
#define BY8301_EQ_POP             0x01  // 流行
#define BY8301_EQ_ROCK            0x02  // 摇滚
#define BY8301_EQ_JAZZ            0x03  // 爵士
#define BY8301_EQ_CLASSIC         0x04  // 古典
#define BY8301_EQ_BASS            0x05  // 低音

// ============== 播放状态 ==============
#define BY8301_STATUS_STOP        0x00  // 停止
#define BY8301_STATUS_PLAY        0x01  // 播放
#define BY8301_STATUS_PAUSE       0x02  // 暂停

// ============== 函数声明 ==============

/**
 * @brief 初始化BY8301语音模块
 */
void BY8301_Init(void);

/**
 * @brief 播放指定曲目
 * @param index 曲目编号 (1-255)
 */
void BY8301_PlayIndex(uint16_t index);

/**
 * @brief 设置音量
 * @param volume 音量值 (0-30)
 */
void BY8301_SetVolume(uint8_t volume);

/**
 * @brief 播放 (从暂停/停止状态恢复)
 */
void BY8301_Play(void);

/**
 * @brief 暂停
 */
void BY8301_Pause(void);

/**
 * @brief 停止
 */
void BY8301_Stop(void);

/**
 * @brief 上一曲
 */
void BY8301_Prev(void);

/**
 * @brief 下一曲
 */
void BY8301_Next(void);

/**
 * @brief 音量加 (每次增加1级)
 */
void BY8301_VolumeUp(void);

/**
 * @brief 音量减 (每次减少1级)
 */
void BY8301_VolumeDown(void);

/**
 * @brief 设置循环模式
 * @param mode 循环模式 (BY8301_LOOP_xxx)
 */
void BY8301_SetLoopMode(uint8_t mode);

/**
 * @brief 设置EQ音效
 * @param eq EQ模式 (BY8301_EQ_xxx)
 */
void BY8301_SetEQ(uint8_t eq);

/**
 * @brief 插播功能 (播放完后返回原曲目继续)
 * @param index 要插播的曲目编号
 */
void BY8301_InsertPlay(uint16_t index);

/**
 * @brief 复位模块 (恢复出厂设置)
 */
void BY8301_Reset(void);

/**
 * @brief 进入/退出待机模式
 */
void BY8301_Standby(void);

/**
 * @brief 组合播放多首曲目 (连续发送，间隔<6ms)
 * @param indexes 曲目编号数组
 * @param count 曲目数量 (最多10首)
 */
void BY8301_PlayCombine(uint16_t *indexes, uint8_t count);

/**
 * @brief 查询播放状态
 * @return 0=停止, 1=播放, 2=暂停
 */
uint8_t BY8301_QueryStatus(void);

/**
 * @brief 查询当前音量
 * @return 音量值 (0-30)
 */
uint8_t BY8301_QueryVolume(void);

#endif /* __BY8301_H */
