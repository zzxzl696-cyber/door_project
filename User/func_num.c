#include "bsp_system.h"

void show_string()
{
	uint32_t changshi = TIM_Get_MillisCounter();
	printf("这是一个测试:%d\n",changshi);
}
