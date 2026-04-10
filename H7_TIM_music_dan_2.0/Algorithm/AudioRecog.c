#include <stm32h7xx.h>
#include "arm_math.h"
#include <stdio.h>

#define SAMPLE_RATE 2000 //采样率
#define SEG_NUM 30 //切分音频数据
#define AUDIO_BUF SAMPLE_RATE*3 //音频缓冲数组
#define TRI_THRETHOLD   0.01 //触发门限

extern ADC_HandleTypeDef hadc2;
extern TIM_HandleTypeDef htim8;

uint16_t Audio_Data[AUDIO_BUF] = {0}; //音频数据数组
float Sam_mean = 0;
float Sam_var = 0;
float Exam_mean[8] = {}; //根据提取结果填入
float Exam_var[8] = {}; //根据提取结果填入
float Audio_buf[AUDIO_BUF] = {0};
float ADC_Conv_rate = 3.3f/4095;


void Start_Audio_Sample(void) 
{
    uint32_t APB2_Clock = 240000000;
	
	uint16_t new_arr = APB2_Clock/SAMPLE_RATE - 1;
	
	__HAL_TIM_SET_AUTORELOAD(&htim3,new_arr); //设置采样率
    HAL_ADC_Start_DMA(&ADC2,(uint32_t *)Audio_Data,AUDIO_BUF);
    HAL_TIM_Base_Start(&htim8);
}

void Data_Preprocess(uint16_t *pData,uint32_t len,float *Audio_buf)
{
    arm_mult_f32((float *)pData,&ADC_Conv_rate,Audio_buf,len);
}

void Extract_Example_Feature(uint8_t Audio_Label,uint16_t* pData,uint32_t len) //提取样本音频特征，返回样本音频平均过零率和方差
{
    float mean = 0
    uint32_t cnt = 0;
    uint32_t Window_Size = AUDIO_BUF/SEG_NUM;
    float rate_mean = 0;
    float rate_var = 0;
    float rate[SEG_NUM] = {0};
    Data_Preprocess(pData,len,Audio_buf);
    arm_mean_f32(Audio_buf,len,&mean);
    float Tri_th = TRI_THRETHOLD - mean;
    for(int i = 0;i < SEG_NUM;i++)
    {
        for(int j = i*Window_Size;j < i*Window_Size + Window_Size - 1;j++)
        if(((Audio_buf[j] - mean)< Tri_th && (Audio_buf[j+1] - mean) > Tri_th) ||((Audio_buf[j] - mean)>Tri_th && (Audio_buf[j+1] - mean) < Tri_th))
        {
            cnt++;
        }
        rate[i] = (float)cnt/(Window_Size-1);
    }
    arm_mean_f32(rate,SEG_NUM,&rate_mean);
    arm_var_f32(rate,SEG_NUM,&rate_var);
    printf("Audio Label:%d",Audio_Label); //输出音频标签
    printf("Exam_mean:%f",rate_mean); //输出样本平均过零率
    printf("Exam_var:%f",rate_var); //输出样本过零率方差
}

void Audio_Recognize(uint16_t* pData,uint32_t len)
{
    float mean = 0
    uint32_t cnt = 0;
    uint32_t Window_Size = AUDIO_BUF/SEG_NUM;
    float rate_mean = 0;
    float rate_var = 0;
    float rate[SEG_NUM] = {0};
    Data_Preprocess(pData,len,Audio_buf);
    arm_mean_f32(Audio_buf,len,&mean);
    float Tri_th = TRI_THRETHOLD - mean;
    for(int i = 0;i < SEG_NUM;i++)
    {
        for(int j = i*Window_Size;j < i*Window_Size + Window_Size - 1;j++)
        if(((Audio_buf[j] - mean)< Tri_th && (Audio_buf[j+1] - mean) > Tri_th) ||((Audio_buf[j] - mean)>Tri_th && (Audio_buf[j+1] - mean) < Tri_th))
        {
            cnt++;
        }
        rate[i] = (float)cnt/(Window_Size-1);
    }
    arm_mean_f32(rate,SEG_NUM,&rate_mean);
    arm_var_f32(rate,SEG_NUM,&rate_var);

    float diff_mean = 0;
    float diff_var = 0;
    float err = 0;
    float min_err = 100000.0f;
    uint8_t index = 0;
    for(int i = 0;i < 8;i++)
    {
        diff_mean = rate_mean - Exam_mean[i];
        diff_var = rate_var - Exam_var[i];
        err = diff_mean*diff_mean + diff_var*diff_var;
        if (err < min_err)
        {
            min_err = err;
            index = i;
        }
    }
    printf("Audio Label:%d",index+1);
}
