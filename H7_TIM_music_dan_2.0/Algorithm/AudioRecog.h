#ifndef __AUDIORECOG_H
#define __AUDIORECOG_H

#include <stm32h7xx.h>

void Start_Audio_Sample(void);
void Data_Preprocess(uint16_t *pData,uint32_t len,float *Audio_buf);
void Extract_Example_Feature(uint8_t Audio_Label,uint16_t* pData,uint32_t len);//提取样本音频特征，返回样本音频平均过零率和方差
void Audio_Recognize(uint16_t* pData,uint32_t len);
#endif
