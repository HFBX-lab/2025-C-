#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "math.h"
#include "stdio.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

extern uint32_t freq;
extern uint16_t input1_data[];
extern uint16_t Music_data[];
extern uint8_t max_point;
extern uint8_t min_point;
extern uint8_t Factor;     //放大系数
extern uint8_t Bule_Data[];

extern float Medianfilter_data []; // 中值滤波数据数组
extern float Meanfilter_data []; // 均值滤波数据数组
extern float Meanfilter_Type_data [];
extern float input_data_float[];
extern float max;
extern float min;
extern float VPP;

void MedianFilter_3Point(uint16_t *input, float *output, uint16_t length);
void MeanFilter_3Point(float *input, float *output, uint16_t length);
void MODE_Switch();
uint8_t WAVE_Type(float *input, uint16_t length);
void Get_Bule_Data(uint16_t len);
void Calculate_Square_VPP (float *input, uint16_t length);
void Square_VPP_data(float *intput, uint16_t length);
void Calculate_VPP(float *input, uint16_t length);
void Calculate_VPP_Remove3(float *input, uint16_t length);
void Calculate_VPP_Remove4(float *input, uint16_t length);

void BLE_Send_String(char *str);
void BLE_Send_Waveform_100 (uint8_t *wave_data);