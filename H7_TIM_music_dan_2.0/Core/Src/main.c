/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <xinhao.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// 自定义函数：动态改变采样频率
// 参数 psc: 新的预分频器值 (TIM_Prescaler)
// 参数 arr: 新的自动重载值 (TIM_Period)
void Change_Sample_Frequency (uint16_t psc, uint16_t arr)
{
    // 1. 停止定时器（如果有正在进行的ADC采集，最好也暂停）
    HAL_TIM_Base_Stop(&htim3); // 假设你的定时器句柄是 htim

    // 2. 修改定时器的核心参数
    //    ⚠️ 重要：修改参数前必须关闭定时器，否则可能导致配置错误
    __HAL_TIM_SET_PRESCALER(&htim3, psc);
    __HAL_TIM_SET_AUTORELOAD(&htim3, arr);

    // 3. 强制产生一个更新事件，让新的ARR值立即生效
    __HAL_TIM_SET_COUNTER(&htim3, 0); // 计数器清零
	HAL_TIM_GenerateEvent(&htim3, TIM_EVENTSOURCE_UPDATE);

    // 4. 重新启动定时器
    HAL_TIM_Base_Start(&htim3);
}

uint8_t dmaComp = 0;        //ADC1采集完成标志位
uint8_t dmaComp_ADC2 = 0;   //ADC2采集完成标志位

uint8_t key_presses = 1;     //对按键次数计次，初始值为1。一次按下按键后，只有按键消抖完成才会变成2，
uint8_t Signal_switch = 1;   //设置一个标志位，用于开启或者关闭信号测量。1为开启，0为关闭。初始化为1
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)      //GPIO外部中断回调函数
{ 
	if(GPIO_Pin==GPIO_PIN_0)//判断是否是PI0引脚
    {	  
		if(key_presses == 1)
		{
			Signal_switch = 0;      //关闭信号测量
		}
		else if(key_presses == 2)
		{
			Signal_switch = 3;
		}
    }
}         

uint8_t switch_KG = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim == &htim4 && Signal_switch == 1)
  {
	  freq = __HAL_TIM_GET_COUNTER(&htim2);   //获取信号频率
	  HAL_TIM_Base_Stop_IT (&htim4);          //关闭定时器中断
	  __HAL_TIM_SET_COUNTER (&htim4,0);       //将中断定时器的计数器值归零
	  printf("t15.txt=\"%u\"\xff\xff\xff",freq);
	  
	  uint16_t arr = 0;
	  if(freq == 0)                          
	  {
	      Change_Sample_Frequency (0, 59);	  //1000HZ采样率，采集时间固定1s
	  }  
	  else if(freq == 1)                          
	  {
	      Change_Sample_Frequency (119, 1999);	  //1000HZ采样率，采集时间固定1s
	  }
	  else if(freq > 1 && freq <= 20)                          
	  {
	      Change_Sample_Frequency (59, 1999);	  //2000HZ采样率，采集时间固定0.5s
	  }
	  else if(freq > 20 && freq <= 1000)         //根据频率修改采样率
	  {
		  arr = 10000000.0f / 100.0f / (float)freq;           //预分频后 10MHz 的频率，最后每个周期采集100个点
	      Change_Sample_Frequency (23, arr);	  
	  }
	  else if(freq > 1000 && freq <= 40000)
	  {
		  arr = 240000000.0 / 100.0 / (float)freq;          //预分频后 240MHz 的频率，最后每个周期采集100个点
	      Change_Sample_Frequency (0, arr);	  
	  }
	  else if(freq > 40000)
	  {
		  arr = 2400000.0 * 101.0 / (float)freq;          //预分频后 240MHz 的频率，最后每个周期采集100个点
	      Change_Sample_Frequency (0, arr);	  
	  }
	  
	  if(Signal_switch == 1)     //控制信号测量是否执行
	  {	  
		  switch_KG = 1;     //得到频率测量结果后进入ADC中断通道1进行放大	  
		  HAL_ADC_Start_DMA (&hadc1, (uint32_t*)input1_data , 1000);   //启动ADC采集信号
	  }
  }
}


void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  /* Prevent unused argument(s) compilation warning */
  if(hadc == &hadc1)    //ADC1中断回调
  {
	  HAL_ADC_Stop_DMA(&hadc1);  //关闭ADC1

	  dmaComp=1;  //采样结束标志位置1
  }
  else if(hadc == &hadc2)  //ADC2中断回调
  {
	  HAL_ADC_Stop_DMA(&hadc2);  //关闭ADC2
	  
	  dmaComp_ADC2 = 1;
  }
  /* NOTE : This function should not be modified. When the callback is needed,
            function HAL_ADC_ConvCpltCallback must be implemented in the user file.
   */
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_TIM3_Init();
  MX_TIM2_Init();
  MX_ADC1_Init();
  MX_TIM4_Init();
  MX_ADC2_Init();
  MX_TIM8_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start(&htim2);
  HAL_TIM_Base_Start_IT (&htim4);
  HAL_TIM_Base_Start(&htim3);
//  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)input_data, 4096);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  if(Signal_switch == 3)         //对第二次按下按键消抖并开启信号测量
	  {
		  HAL_Delay(300);             //按键消抖(同时消去按下和弹起时产生的抖动)
		  if(HAL_GPIO_ReadPin(GPIOI,GPIO_PIN_0)== GPIO_PIN_SET)  //确认是否为高电平（此时按键已经弹起）
		  {
			  printf("page 0\xff\xff\xff");
			  key_presses = 1;			
			  Signal_switch = 1;     //开启信号测量			
			  dmaComp = 1;            //确保主循环里面可以进入信号测量部分代码
		  }
	  }
	  if(dmaComp_ADC2 == 1 && Signal_switch == 2)    //音频信号处理及识别
	  {
		  dmaComp_ADC2 = 0;
		                           //音频信号处理及识别（补）
		  HAL_ADC_Stop_DMA(&hadc1);  //关闭ADC1
		  HAL_ADC_Start_DMA (&hadc2, (uint32_t*)Music_data , 3000);   //启动ADC2采集音频信号
	  }
	  if(Signal_switch == 0)         //对第一次按键按下消抖以及开启音频测量
	  {
		  HAL_Delay(300);             //按键消抖(同时消去按下和弹起时产生的抖动)
		  if(HAL_GPIO_ReadPin(GPIOI,GPIO_PIN_0)== GPIO_PIN_SET)  //确认是否为高电平（此时按键已经弹起）
		  {
			  printf("page 1\xff\xff\xff");
			  key_presses = 2;         
			  Signal_switch = 2;      //阻止ADC采集完毕前再次进入循环			  
			  HAL_ADC_Start_DMA (&hadc2, (uint32_t*)Music_data , 3000);   //启动ADC2采集音频信号
		  }
	  }
	  if(dmaComp == 1 && Signal_switch == 1)
	  {		  	  	  
		  dmaComp = 0;
		  if(switch_KG == 1)   //修改PGA电路放大系数
	      {	  	  
			  MedianFilter_3Point(input1_data, Medianfilter_data , 1000);         //中值滤波		  
			  MeanFilter_3Point(Medianfilter_data , Meanfilter_data , 1000);     //均值滤波			  			  
             
			  for(int i=0; i<1000; i++)
			  {
				  Meanfilter_Type_data[i] = Meanfilter_data [i];
			  } 
			  
			  uint8_t Type = 0;
			  if(freq > 20 || freq == 1)
			  {
				  Type = WAVE_Type(Meanfilter_data , 1000); 			  //波形识别	  
			  }
			  else if(freq <= 20 && freq !=1)
			  {
				  uint16_t len = 0;
				  float length = 0;
				  
				  len = freq / 2;
				  length = 1000.0f / (float)freq * 2.0f; 
				  len = (float)len * length; 
				  printf("t13.txt=\"%d\"\xff\xff\xff" , len);
				  Type = WAVE_Type(Meanfilter_Type_data, len);
			  }
			  
			  if(Type == 1)	  		  
			  {		  			  
				  printf("t4.txt=\"方波\"\xff\xff\xff");	  		  
			  }	  		  
			  else if(Type == 2)	  		  
			  {		  			 
				  printf("t4.txt=\"正弦波\"\xff\xff\xff");	  		  
			  }	  		 
			  else if(Type == 3)	  		  
			  {		  			 
				  printf("t4.txt=\"三角波\"\xff\xff\xff");	  		  
			  }	
			  
			  if(Type == 1)     //方波(已矫正)
			  {
				  Calculate_Square_VPP (Meanfilter_data, 1000);
				  //Square_VPP_data(Meanfilter_data, 1000);
				  VPP = VPP * 3320.0f / 4095.0f; 
			  }
			  else if(Type == 2)  //正弦波(已矫正)
			  {
				  Calculate_VPP_Remove3(Meanfilter_data, 1000);               //12取9
				  VPP = VPP * 3330.0f / 4095.0f; 
			  }
			  else          //三角波
			  {
				  Calculate_VPP_Remove4(Meanfilter_data, 1000);       //12取8
				  VPP = VPP * 3345.0f / 4095.0f; 
			  }	
			  
			  printf("t5.txt=\"%.1f\"\xff\xff\xff" , VPP);	
			  
			  MODE_Switch();      //模拟开关选择
			  printf("t20.txt=\"%d\"\xff\xff\xff" , Factor);
			  
              HAL_Delay(20);    //放大系数调整缓冲时间
              switch_KG = 2;   //使放大后再进行下一次ADC采样
              HAL_ADC_Start_DMA (&hadc1, (uint32_t*)input1_data , 1000);   //启动ADC采集信号
	      }	  
	      else if(switch_KG == 2)   //放大后数据处理
	      {		  
			  MedianFilter_3Point(input1_data, Medianfilter_data , 1000);         //中值滤波		  
			  MeanFilter_3Point(Medianfilter_data , Meanfilter_data , 1000);     //均值滤波			  			  
             
			  for(int i=0; i<1000; i++)
			  {
				  Meanfilter_Type_data[i] = Meanfilter_data [i];
			  } 
			  
			  uint8_t Type = 0;
			  if(freq > 20 || freq == 1)
			  {
				  Type = WAVE_Type(Meanfilter_data , 1000); 			  //波形识别	  
			  }
			  else if(freq <= 20 && freq !=1)
			  {
				  uint16_t len = 0;
				  float length = 0;
				  
				  len = freq / 2;
				  length = 1000.0f / (float)freq * 2.0f; 
				  len = (float)len * length; 
				  printf("t13.txt=\"%d\"\xff\xff\xff" , len);
				  Type = WAVE_Type(Meanfilter_Type_data, len);
			  }
			  
			  if(freq >= 20)                 //线性插值得到100个波形数据点，刚好一个完整周期
			  {
				  Get_Bule_Data(100);
			  }
			  if(freq < 20)
			  {
				  Get_Bule_Data((uint16_t)(1000.0f / (float)freq * 2.0f));
			  }
			  
			  BLE_Send_Waveform_100 (Bule_Data);  //发送数据到蓝牙
			 
//			  for(int s=0; s<1000; s++)
//			  {
//				   printf("%.1f\n", Meanfilter_data[s]);
//			  }
			  
			  if(Type == 1)	  		  
			  {		  			  
				  printf("t12.txt=\"方波\"\xff\xff\xff");	  		  
			  }	  		  
			  else if(Type == 2)	  		  
			  {		  			 
				  printf("t12.txt=\"正弦波\"\xff\xff\xff");	  		  
			  }	  		 
			  else if(Type == 3)	  		  
			  {		  			 
				  printf("t12.txt=\"三角波\"\xff\xff\xff");	  		  
			  }		 
			  
			  //峰峰值精确计算		  
			  for(int i=0; i<1000; i++)
			  {
				  input_data_float[i] = (float)input1_data[i];
			  }	  
			 
			  if(Type == 1)     //方波(已矫正)
			  {
				  Calculate_Square_VPP (Meanfilter_data, 1000);
				  //Square_VPP_data(Meanfilter_data, 1000);
				  VPP = VPP * 3320.0f / 4095.0f; 
			  }
			  else if(Type == 2)  //正弦波(已矫正)
			  {
				  Calculate_VPP_Remove3(Meanfilter_data, 1000);               //12取9
				  VPP = VPP * 3330.0f / 4095.0f; 
			  }
			  else          //三角波
			  {
				  Calculate_VPP_Remove4(Meanfilter_data, 1000);       //12取8
				  VPP = VPP * 3345.0f / 4095.0f; 
			  }				                   
			  printf("t17.txt=\"%.1f\"\xff\xff\xff" , VPP);			  
              
			  if(Signal_switch == 1)     //控制信号测量是否执行
			  {
				  HAL_TIM_Base_Start_IT (&htim4);			  			  
				  __HAL_TIM_SET_COUNTER (&htim2,0);
			  }				  
		  }
	  }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 10;
  RCC_OscInitStruct.PLL.PLLN = 384;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 10;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInitStruct.PLL2.PLL2M = 4;
  PeriphClkInitStruct.PLL2.PLL2N = 24;
  PeriphClkInitStruct.PLL2.PLL2P = 2;
  PeriphClkInitStruct.PLL2.PLL2Q = 2;
  PeriphClkInitStruct.PLL2.PLL2R = 2;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_2;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
  PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x08000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_2MB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_PRIV_RO;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
