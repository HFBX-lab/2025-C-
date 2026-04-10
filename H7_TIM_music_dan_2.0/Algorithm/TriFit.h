#ifndef __TRIFIT_H
#define __TRIFIT_H

#include <stm32h7xx.h>

typedef struct{
	float k;
	float b;
}Line_Para;

Line_Para Line_fit(uint16_t* pIndex,uint8_t Point_num,float *pPoint);
float CalInterPoint(Line_Para line1,Line_Para line2);
float CalTriVpp(float* pData,uint16_t len);

#endif
