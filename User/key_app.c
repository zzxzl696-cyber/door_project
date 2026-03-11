/********************************** (C) COPYRIGHT *******************************
 * File Name          : key_app.c
 * Author             : Key driver for CH32V30x (adapted from STM32 HAL)
 * Version            : V1.1.0
 * Date               : 2026-02-05
 * Description        : 按键驱动实现,保留原有状态机和去抖逻辑
 *                      集成密码输入和用户管理功能
 *********************************************************************************/

#include "bsp_system.h"

// 按键数组(只有1个按键)
button btns[1];

// 矩阵键盘按键数组(16个按键)
button matrix_btns[16];

// 矩阵键盘行列引脚定义
static GPIO_TypeDef *const row_ports[4] = {GPIOC, GPIOE, GPIOE, GPIOE};
static const uint16_t row_pins[4] = {GPIO_Pin_5, GPIO_Pin_7, GPIO_Pin_9, GPIO_Pin_11};
static GPIO_TypeDef *const col_ports[4] = {GPIOE, GPIOE, GPIOD, GPIOD};
static const uint16_t col_pins[4] = {GPIO_Pin_13, GPIO_Pin_15, GPIO_Pin_9, GPIO_Pin_11};

// changnishi

/**
 * @brief 初始化按键GPIO和按键结构体
 * @note 配置PC4为上拉输入,初始化按键状态机
 */
void key_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};

	// 使能GPIOC时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

	// 配置PC4为上拉输入
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 上拉输入
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	// 初始化按键结构体
	btns[0].gpiox = GPIOC;
	btns[0].pin = GPIO_Pin_4;
	btns[0].level = 1; // 初始电平为高(上拉)
	btns[0].id = 0;
	btns[0].state = 0;
	btns[0].ticks = 0;
	btns[0].debouce_cnt = 0;
	btns[0].repeat = 0;
	return;
}

/**
 * @brief 按键状态机任务(保留原有逻辑)
 * @param btn 按键结构体指针
 * @note 实现3次采样消抖、短按/长按/双击检测
 */
void key_task(button *btn)
{
	// 读取按键当前电平 (CH32V30x标准库)
	uint8_t gpio_level = GPIO_ReadInputDataBit(btn->gpiox, btn->pin);

	// 如果按键状态不为0,增加计时器
	if (btn->state > 0)
		btn->ticks++;

	// 如果当前电平与按键记录的电平不同,进行消抖处理
	if (btn->level != gpio_level)
	{
		// 连续达到3次,确认电平变化
		if (++(btn->debouce_cnt) >= 3)
		{
			btn->level = gpio_level; // 更新电平
			btn->debouce_cnt = 0;	 // 清除消抖计数器
		}
	}
	else
	{
		btn->debouce_cnt = 0; // 电平没有变化,清除消抖计数器
	}

	// 按键状态机
	switch (btn->state)
	{
	case 0:					 // 初始状态
		if (btn->level == 0) // 等待按键按下
		{
			btn->ticks = 0;	 // 清除计时器
			btn->repeat = 1; // 开始计数重复按下
			btn->state = 1;	 // 进入按键按下状态
		}
		break;
	case 1:					 // 按键按下状态
		if (btn->level != 0) // 等待按键松开
		{
			if (btn->ticks >= 30) // 长按检测
			{
				// 长按事件 - 可在此处添加回调
				printf("长按\n");
				btn->state = 0; // 返回初始状态
			}
			else
			{
				btn->ticks = 0; // 清除计时器
				btn->state = 2; // 进入按键释放状态
			}
		}
		else if (btn->ticks >= 30) // 长按检测
		{
			btn->repeat = 0; // 防止释放的时候再次触发短按事件
		}
		break;
	case 2:					  // 按键释放状态
		if (btn->ticks >= 15) // 超时达到阈值
		{
			btn->state = 0; // 返回初始状态
			if (btn->repeat == 1)
			{
				door_control_unlock(AUTH_NONE, 8000);
				printf("单击\n");
				// 单击事件 - 可在此处添加回调
			}
			else if (btn->repeat == 2)
			{
				printf("双击\n");
				// 双击事件 - 可在此处添加回调
			}
		}
		else
		{
			if (btn->level == 0) // 再次按下
			{
				btn->repeat++;	// 增加重复计数
				btn->ticks = 0; // 清除计时器
				btn->state = 1; // 返回按键按下状态
			}
		}
		break;
	}
}

/**
 * @brief 按键状态扫描函数
 * @note 需要在定时任务中周期调用(建议10-20ms)
 */
void key_state(void)
{
	for (uint8_t i = 0; i < 1; i++) // 遍历所有按键(只有1个)
	{
		key_task(&btns[i]); // 处理每个按键的状态
	}
}

/**
 * @brief 初始化4x4矩阵键盘
 * @note 行: PC5, PE7, PE9, PE11 (输出推挽)
 *       列: PE13, PE15, PD9, PD11 (输入上拉)
 */
void matrix_key_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};

	// 使能时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOE | RCC_APB2Periph_GPIOD, ENABLE);

	// 配置行引脚为输出推挽,默认输出高电平
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	for (uint8_t i = 0; i < 4; i++)
	{
		GPIO_InitStructure.GPIO_Pin = row_pins[i];
		GPIO_Init(row_ports[i], &GPIO_InitStructure);
		GPIO_SetBits(row_ports[i], row_pins[i]);
	}

	// 配置列引脚为上拉输入
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	for (uint8_t i = 0; i < 4; i++)
	{
		GPIO_InitStructure.GPIO_Pin = col_pins[i];
		GPIO_Init(col_ports[i], &GPIO_InitStructure);
	}

	// 初始化16个按键结构体
	for (uint8_t i = 0; i < 16; i++)
	{
		matrix_btns[i].gpiox = NULL; // 矩阵键盘不直接关联GPIO
		matrix_btns[i].pin = 0;
		matrix_btns[i].level = 1;
		matrix_btns[i].id = i + 1;
		matrix_btns[i].state = 0;
		matrix_btns[i].ticks = 0;
		matrix_btns[i].debouce_cnt = 0;
		matrix_btns[i].repeat = 0;
	}
}

/**
 * @brief 扫描矩阵键盘并更新按键状态
 * @note 需要在定时任务中周期调用(建议10-20ms)
 */
void matrix_key_scan(void)
{
	static uint8_t current_row = 0;

	// 所有行先拉高
	for (uint8_t i = 0; i < 4; i++)
		GPIO_SetBits(row_ports[i], row_pins[i]);

	// 当前行拉低
	GPIO_ResetBits(row_ports[current_row], row_pins[current_row]);

	// 延时稳定
	for (volatile uint16_t i = 0; i < 10; i++)
		;

	// 读取列状态
	for (uint8_t col = 0; col < 4; col++)
	{
		uint8_t key_id = current_row * 4 + col;
		uint8_t gpio_level = GPIO_ReadInputDataBit(col_ports[col], col_pins[col]);

		button *btn = &matrix_btns[key_id];

		// 更新按键电平并执行状态机
		if (btn->state > 0)
			btn->ticks++;

		if (btn->level != gpio_level)
		{
			if (++(btn->debouce_cnt) >= 3)
			{
				btn->level = gpio_level;
				btn->debouce_cnt = 0;
			}
		}
		else
		{
			btn->debouce_cnt = 0;
		}

		// 按键状态机
		switch (btn->state)
		{
		case 0:
			if (btn->level == 0)
			{
				btn->ticks = 0;
				btn->repeat = 1;
				btn->state = 1;
			}
			break;
		case 1:
			if (btn->level != 0)
			{
				if (btn->ticks >= 30)
				{
					printf("矩阵键盘[%d]长按\n", btn->id);
					btn->state = 0;
				}
				else
				{
					btn->ticks = 0;
					btn->state = 2;
				}
			}
			else if (btn->ticks >= 30)
			{
				btn->repeat = 0;
			}
			break;
		case 2:
			if (btn->ticks >= 15)
			{
				btn->state = 0;
				if (btn->repeat == 1)
				{
					/* 互斥分发：管理模式优先（内部已含密码转发），其次独立密码输入 */
					if (user_admin_is_active())
					{
						user_admin_on_key(btn->id);
					}
					else if (pwd_input_is_active())
					{
						/* 密码输入模式：15=取消，0-9=数字输入 */
						if (btn->id == 15)
						{
							pwd_input_cancel();
						}
						else if (btn->id >= 1 && btn->id <= 10)
						{
							pwd_input_on_key(btn->id);
						}
					}
					else
					{
						/* 正常模式：按键功能 */
						switch (btn->id)
						{
						case 1:	 // 录入指纹（需要管理模式）
						case 2:	 // 录入指纹（需要管理模式）
						case 3:	 // 录入指纹（需要管理模式）
						case 4:	 // 录入指纹（需要管理模式）
						case 5:	 // 录入指纹（需要管理模式）
						case 6:	 // 录入指纹（需要管理模式）
						case 7:	 // 录入指纹（需要管理模式）
						case 8:	 // 录入指纹（需要管理模式）
						case 9:	 // 录入指纹（需要管理模式）
						case 10: // 数字0
							/* 数字按键，启动密码输入 */
							if (!pwd_input_is_active())
							{
								auth_start_password();
							}
							pwd_input_on_key(btn->id);
							break;

						case 13: // 指纹验证
						{
							SearchResult search_result;
							uint8_t result = Press_FR(&search_result);

							if (result == 0)
							{
								printf("Matched! ID=%d, Score=%d\r\n",
									   search_result.pageID,
									   search_result.mathscore);

								/* 使用认证管理器处理指纹认证 */
								auth_process_fingerprint(&search_result);
							}
							else
							{
								printf("No match: %s\r\n", EnsureMessage(result));
							}
							break;
						}

						case 14: // 进入管理模式
						{
							printf("[KEY] Start admin password input\r\n");
							/* 启动管理员密码输入 */
							auth_start_password();
							break;
						}

						case 15: // 取消/退出
						{
							/* 正常模式下15键无操作 */
							break;
						}

						case 16: // 确认
						{
							/* 正常模式下16键无操作 */
							break;
						}
						}
					}
					printf("矩阵键盘[%d]单击\n", btn->id);
				}
				else if (btn->repeat == 2)
				{
					/* 双击键16：强制退出管理模式 */
					if (btn->id == 16 && user_admin_is_active())
					{
						user_admin_exit();
					}
					printf("矩阵键盘[%d]双击\n", btn->id);
				}
			}
			else
			{
				if (btn->level == 0)
				{
					btn->repeat++;
					btn->ticks = 0;
					btn->state = 1;
				}
			}
			break;
		}
	}

	// 切换到下一行
	current_row = (current_row + 1) % 4;
}
