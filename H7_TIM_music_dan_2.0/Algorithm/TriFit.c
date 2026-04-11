#include "stm32h7xx.h"                  // Device header
#include "arm_math.h"                   // ARM::CMSIS:DSP
#include "TriFit.h"

#define LEAR_FIT_POINT_NUM 		30

Line_Para Line_fit(uint16_t* pIndex,uint8_t Point_num,float *pPoint)
{
	Line_Para line = {0.0,0.0};
	float sum_xy,x_bar,y_bar,sum_x2;
	arm_dot_prod_f32(pPoint,(float *)pIndex,Point_num,&sum_xy);
	arm_mean_f32(pPoint,Point_num,&y_bar);
	arm_mean_f32((float *)pIndex,Point_num,&x_bar);
	
	arm_dot_prod_f32((float *)pIndex,(float *)pIndex,Point_num,&sum_x2);
	line.k = (sum_xy - Point_num*x_bar*y_bar)/(sum_x2 - Point_num*x_bar*x_bar);
	line.b = y_bar - line.k*x_bar;
	return line;
}

float CalInterPoint(Line_Para line1,Line_Para line2)
{
	if(fabs(line1.k - line2.k) < 0.00001f) return 0;
		
	float y = 0;
	y = (line2.b * line1.k - line2.k - line1.b)/(line1.k - line2.k);
	return y;
}

float CalTriVpp(float* pData,uint16_t len)
{
	static uint16_t index[2000] = {0};
	float max_v = 0,min_v = 0;
	uint8_t max_point = 0,min_point = 0;
	for(int i = 0;i < len;i++)
	{
		index[i] = i;
	}
	for (int i = 50;i < len-50;i++)
    {
		Line_Para line1,line2;
        if (pData[i-1] <= pData[i] && pData[i+1] <= pData[i]) //极大值点
        {
            line1 = Line_fit(&index[i - LEAR_FIT_POINT_NUM - 5],LEAR_FIT_POINT_NUM,&pData[i-LEAR_FIT_POINT_NUM - 5]);
			line2 = Line_fit(&index[i + 5],LEAR_FIT_POINT_NUM,&pData[i + 5]);
			max_v += CalInterPoint(line1,line2);
			max_point++;
        }
        else if(pData[i-1] >= pData[i] && pData[i+1] >= pData[i]) //极小值点
        {
            line1 = Line_fit(&index[i - LEAR_FIT_POINT_NUM - 5],LEAR_FIT_POINT_NUM,&pData[i-LEAR_FIT_POINT_NUM - 5]);
			line2 = Line_fit(&index[i + 5],LEAR_FIT_POINT_NUM,&pData[i + 5]);
			min_v += CalInterPoint(line1,line2);
			min_point++;
        }
    }
	max_v /= max_point;
	min_v /= min_point;
	return max_v - min_v;
}
