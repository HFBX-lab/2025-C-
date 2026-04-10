#include <fun.h>
#define FFT_LEN 4096                     //FFT点数

float f = 10000.0f;                     // 信号频率
float fs = 1024000.0f;                   // 采样率
uint16_t input_data[FFT_LEN] = {0};       // 数据数组
float input_hanning_data [FFT_LEN] = {0};   // hanning窗处理数据数组
float FFT_buffer[FFT_LEN*2] = {0};    // FFT函数输入输出数组
float Fitting_data[256]={0};          // 拟合波形数据  
float Fitting_Bule_data [100]={0};
uint8_t Fitting_Bule_date_Uint8_t [100]={0};

float FFT_mag[FFT_LEN] = {0};         // 幅度谱
float FFT_mag_data[5] = {0};          // 幅度谱各谐波幅值数组
uint32_t FFT_mag_data_index[5]= {0};  // 各谐波幅值的索引
float FFT_phase[5] = {0};             // 各谐波的相位数组      
float FFT_Freq = 0;                   // FFT计算得到的基波频率
float FFT_Ampl = 0;                   // FFT计算得到基波幅值

void apply_hanning()
{
    // 1. 计算直流分量（均值）
    float mean = 0.0f;
    for (int i = 0; i < FFT_LEN; i++) 
	{
        mean += (float)input_data[i];
    }
    mean /= FFT_LEN;

    // 2. 去除直流并加窗
    for (int i = 0; i < FFT_LEN; i++)
    {
        float win = 0.5f * (1.0f - cosf(2.0f * PI * (float)i / (float)(FFT_LEN - 1)));
        float ac_val = (float)input_data[i] - mean;   // 交流分量
        input_hanning_data[i] = ac_val * win;
    }
}

void FFT()      // 复数FFT变换
{
    //复数据转换
    for(int i=0; i<FFT_LEN; i++)
    {
        FFT_buffer[2*i] = input_hanning_data [i];  //实部
        FFT_buffer[2*i+1] = 0;           //虚部
    }

    //显示数据
//    Show_data(FFT_buffer, FFT_LEN*2);
//    HAL_Delay(2000);

    //FFT计算
    arm_cfft_f32(&arm_cfft_sR_f32_len4096 , FFT_buffer, 0, 1);

	//清除直流量
	FFT_buffer[0]=0;
	FFT_buffer[1]=0;
	FFT_buffer[2]=0;
	FFT_buffer[3]=0;
	
    //计算复数幅值谱
    arm_cmplx_mag_f32(FFT_buffer, FFT_mag, FFT_LEN);
	
    //显示数据
//    Show_data(FFT_mag, FFT_LEN);
//    HAL_Delay(2000);
}

// 能量重心矫正函数（优先使用四点法，条件不足时回退到三点法）
// 输入：粗略峰值索引 peak_idx（需在1~FFT_LEN/2-2范围内）
// 输出：矫正后的频率、幅值、相位
void energy_centroid_correction(uint32_t peak_idx, float* freq, float* amp, float* phase)
{
    // 确保峰值索引在有效范围内
    if (peak_idx < 1 || peak_idx >= FFT_LEN/2) {
        *freq = peak_idx * fs / FFT_LEN;
        *amp = 0;
        *phase = 0;
        return;
    }

    // 尝试四点法（需要索引 ≥2 且 ≤ N/2-2）
    if (peak_idx >= 2 && peak_idx <= FFT_LEN/2 - 2) {
        // 提取四条谱线的实部和虚部
        float real_m2 = FFT_buffer[2*(peak_idx-2)];
        float imag_m2 = FFT_buffer[2*(peak_idx-2)+1];
        float real_m1 = FFT_buffer[2*(peak_idx-1)];
        float imag_m1 = FFT_buffer[2*(peak_idx-1)+1];
        float real_m  = FFT_buffer[2*peak_idx];
        float imag_m  = FFT_buffer[2*peak_idx+1];
        float real_p1 = FFT_buffer[2*(peak_idx+1)];
        float imag_p1 = FFT_buffer[2*(peak_idx+1)+1];

        // 计算幅值（模值）
        float mag_m2 = sqrtf(real_m2*real_m2 + imag_m2*imag_m2);
        float mag_m1 = sqrtf(real_m1*real_m1 + imag_m1*imag_m1);
        float mag_m  = sqrtf(real_m*real_m + imag_m*imag_m);
        float mag_p1 = sqrtf(real_p1*real_p1 + imag_p1*imag_p1);

        float sum_mag = mag_m2 + mag_m1 + mag_m + mag_p1;
        if (sum_mag > 0) {
            // 频率矫正：加权平均索引
            float weight_idx = (mag_m2*(peak_idx-2) + mag_m1*(peak_idx-1) + mag_m*peak_idx + mag_p1*(peak_idx+1)) / sum_mag;
            *freq = weight_idx * fs / FFT_LEN;

            // 幅值矫正（四点法公式，系数基于Hanning窗能量恢复，已包含单边谱修正）
            *amp = (mag_m2 + 2.0f*mag_m1 + 2.0f*mag_m + mag_p1) * 2.0f / FFT_LEN;

            // 相位矫正（取中心谱线相位）
            *phase = atan2f(imag_m, real_m);
            return;
        }
    }

    // 四点法条件不满足，尝试三点法（需要索引 ≥1 且 ≤ N/2-1）
    if (peak_idx >= 1 && peak_idx <= FFT_LEN/2 - 1) {
        // 提取三条谱线的实部和虚部
        float real_m1 = FFT_buffer[2*(peak_idx-1)];
        float imag_m1 = FFT_buffer[2*(peak_idx-1)+1];
        float real_m  = FFT_buffer[2*peak_idx];
        float imag_m  = FFT_buffer[2*peak_idx+1];
        float real_p1 = FFT_buffer[2*(peak_idx+1)];
        float imag_p1 = FFT_buffer[2*(peak_idx+1)+1];

        float mag_m1 = sqrtf(real_m1*real_m1 + imag_m1*imag_m1);
        float mag_m  = sqrtf(real_m*real_m + imag_m*imag_m);
        float mag_p1 = sqrtf(real_p1*real_p1 + imag_p1*imag_p1);

        float sum_mag = mag_m1 + mag_m + mag_p1;
        if (sum_mag > 0) {
            float weight_idx = (mag_m1*(peak_idx-1) + mag_m*peak_idx + mag_p1*(peak_idx+1)) / sum_mag;
            *freq = weight_idx * fs / FFT_LEN;
            *amp = (mag_m1 + 2.0f*mag_m + mag_p1) * 2.0f / FFT_LEN;
            *phase = atan2f(imag_m, real_m);
            return;
        }
    }

    // 均失败，回退到原始值
    *freq = peak_idx * fs / FFT_LEN;
    *amp = FFT_mag[peak_idx] * 2.0f / FFT_LEN;
    *phase = atan2f(FFT_buffer[2*peak_idx+1], FFT_buffer[2*peak_idx]);
}

// 处理基波（能量重心矫正）
void Process_FFT_mag()
{
    // 在幅度谱前一半数据中找最大值（跳过索引0，避免直流残留）
    // 注意：arm_max_f32 会返回最大值和相对索引，需加1补偿
    arm_max_f32(&FFT_mag[1], FFT_LEN/2 - 1, &FFT_mag_data[0], &FFT_mag_data_index[0]);
    FFT_mag_data_index[0] += 1;   // 转换为真实索引

    uint32_t peak_idx = FFT_mag_data_index[0];
    float freq_corr, amp_corr, phase_corr;
    energy_centroid_correction(peak_idx, &freq_corr, &amp_corr, &phase_corr);

    // 保存基波矫正结果
    FFT_Freq = freq_corr;
    FFT_Ampl = amp_corr;
    FFT_phase[0] = phase_corr;      // 基波相位存入数组
    FFT_mag_data[0] = amp_corr;     // 基波幅值存入数组（覆盖原未矫正值）
}

// 各谐波分量的幅值相位计算（能量重心矫正）
void FFT_Fitting_Component()
{
    // 基波已经矫正，直接使用 FFT_Ampl 和 FFT_phase[0]，但 FFT_mag_data[0] 已在 Process_FFT_mag 中赋值

    // 处理2~5次谐波 (i=1,2,3,4 分别对应二次至五次谐波)
    for (int i = 1; i < 5; i++)
    {
        // 理论谐波频率 = (i+1) * 基频
        float harm_freq_theory = (i+1) * FFT_Freq;
        // 理论索引（浮点）
        float theory_idx_float = harm_freq_theory * FFT_LEN / fs;
        uint32_t theory_idx = (uint32_t)(theory_idx_float + 0.5f); // 四舍五入取整

        // 确保索引在有效范围内
        if (theory_idx < 1) theory_idx = 1;
        if (theory_idx >= FFT_LEN/2) theory_idx = FFT_LEN/2 - 1;

        // 在理论索引附近 ±2 范围内搜索实际峰值
        uint32_t search_start = (theory_idx > 2) ? theory_idx - 2 : 1;
        uint32_t search_end = (theory_idx + 2 < FFT_LEN/2) ? theory_idx + 2 : FFT_LEN/2 - 1;
        uint32_t peak_idx = theory_idx;
        float max_mag = FFT_mag[peak_idx];
        for (uint32_t j = search_start; j <= search_end; j++) 
		{
            if (FFT_mag[j] > max_mag) 
            {
                max_mag = FFT_mag[j];
                peak_idx = j;
            }
        }

        // 对找到的峰值进行能量重心矫正
        float freq_corr, amp_corr, phase_corr;
        energy_centroid_correction(peak_idx, &freq_corr, &amp_corr, &phase_corr);

        // 保存矫正后的幅值和相位
        FFT_mag_data[i] = amp_corr;
        FFT_phase[i] = phase_corr;
    }

    // 统一相位修正：余弦波相位转正弦波（根据你的需要）
    for (int j = 0; j < 5; j++) 
	{
        FFT_phase[j] += PI/2;
    }
}

void Fitting_curve()   // 计算THD以及拟合波形数据
{
	float FFT_THD = 0;   //信号THD值
	float FFT_GuiYiHua[5] = {0};   //归一化幅值数组
	FFT_GuiYiHua[0] = 1;
	
	for(int a=1;a<5;a++)           //计算归一化幅值
	{
		FFT_GuiYiHua[a]= FFT_mag_data[a]/FFT_mag_data[0];
		FFT_THD += FFT_GuiYiHua[a]*FFT_GuiYiHua[a];   //计算THD中的平方和
		
		if(a == 4)
		{
			FFT_THD = sqrtf(FFT_THD) * 100;   //平方和开根
		}
	}
	printf("t15.txt=\"%.3f \"\xff\xff\xff" , FFT_Freq /(float)(1000));  //输出基频
	for(int b=0;b<5;b++)           //输出归一化幅值
    {
		printf("t%d.txt=\"%.3f \"\xff\xff\xff" ,  b+6, FFT_GuiYiHua[b]);
	}
	printf("t12.txt=\"%.1f \"\xff\xff\xff" , FFT_THD);  //输出THD值
	
	float FFT_weight[5]={0};   //各个谐波分量的代数式
    for(int i=0; i<256; i++)   //计算原信号一个周期内的256个数据点
    {
		for(int j=0; j<5; j++)
		{
			FFT_weight[j]=FFT_mag_data[j]*sinf(2.0f * PI *(float)(j+1)* (float)i /256.0f +FFT_phase[j]);
		}
		Fitting_data[i]=(FFT_weight[0]+FFT_weight[1]+FFT_weight[2]+FFT_weight[3]+FFT_weight[4]);  //实际采集数据
    }
	float Fitting_data_MX = Fitting_data[0];
	for(int i=0; i<255; i++)   //把计算得到的256个数据点进行缩放
    {
		if(Fitting_data_MX  < Fitting_data[i+1])
		{
			Fitting_data_MX  = Fitting_data[i+1];
		}
		if(i == 254)
		{
			for(int s=0; s<256; s++)
			{
				Fitting_data[s] = Fitting_data[s] / Fitting_data_MX  * 120.0f + 128.0f;
			}
		}
    }
    for(int i=0; i<100; i++)   //计算原信号一个周期内的100个数据点
    {
		for(int j=0; j<5; j++)
		{
			FFT_weight[j]=FFT_mag_data[j]*sinf(2.0f * PI *(float)(j+1)* (float)i /100.0f +FFT_phase[j]);
		}
		Fitting_Bule_data [i]=(FFT_weight[0]+FFT_weight[1]+FFT_weight[2]+FFT_weight[3]+FFT_weight[4]);  //实际采集数据
    }
	float Fitting_Bule_data_MX = Fitting_Bule_data[0];
	for(int i=0; i<99; i++)   //把计算得到的100个数据点进行缩放
    {
		if(Fitting_Bule_data_MX  < Fitting_Bule_data[i+1])
		{
			Fitting_Bule_data_MX  = Fitting_Bule_data[i+1];
		}
		if(i == 98)
		{
			for(int s=0; s<100; s++)
			{
				Fitting_Bule_data[s] = Fitting_Bule_data[s] / Fitting_Bule_data_MX  * 120.0f + 128.0f;
			}
			for(int a=0; a<100; a++)
			{
				Fitting_Bule_date_Uint8_t [a] = (uint8_t)Fitting_Bule_data[a];
			}
		}
    }
	
    //显示数据
	 for(int s=0; s<256; s++)
    {
        //printf("%d\n", (uint8_t)Fitting_data[s]);
        printf("add 1,0,%d\xff\xff\xff", (uint8_t)Fitting_data[s]);
    }
	
	// 1. 发送THD和归一化幅值
    //BLE_Send_Params(FFT_THD, 1.000f, FFT_GuiYiHua[1], FFT_GuiYiHua[2], FFT_GuiYiHua[3], FFT_GuiYiHua[4]);
    
    // 2. 发送波形数据（100个点）
    //BLE_Send_Waveform_100 (Fitting_Bule_date_Uint8_t );
	
    HAL_Delay(500);
}

void Show_data(float* buffer, uint16_t n)  //打印波形
{
    for(int i=0; i<n; i++)
    {
        printf("%.3f\n", buffer[i]);
    }
}

//// 通过蓝牙发送字符串
//void BLE_Send_String(char *str)
//{
//    HAL_UART_Transmit(&huart2, (uint8_t*)str, strlen(str), 100);
//}

//void BLE_Send_Params(float thd, float v1, float v2, float v3, float v4, float v5)
//{
//    char buffer[128];
//    
//    // 格式: THD:0.123,V1:1.000,V2:0.500,V3:0.200,V4:0.100,V5:0.050,
//    sprintf(buffer, "THD:%.1f,V1:%.3f,V2:%.3f,V3:%.3f,V4:%.3f,V5:%.3f,",
//            thd, v1, v2, v3, v4, v5);
//    
//    BLE_Send_String(buffer);
//}

//// 发送波形数据（100个点），使用 Fitting_Bule_data 数组
//void BLE_Send_Waveform_100 (uint8_t *wave_data)
//{
//    char buffer[2048];  // 100个点 * 最大4字符/点 + 键名等，足够
//    uint16_t idx = 0;
//    
//    // 添加键名 "y:"
//    idx += sprintf(buffer + idx, "y:");
//    
//    // 添加所有波形点，用空格分隔
//    for (uint16_t i = 0; i < 100; i++) 
//	{
//        if (i > 0) 
//		{
//            idx += sprintf(buffer + idx, " ");
//        }
//        idx += sprintf(buffer + idx, "%d", wave_data[i]);
//    }
//    
//    // 添加结束逗号
//    idx += sprintf(buffer + idx, ",");
//    
//    BLE_Send_String(buffer);
//}