#include <xinhao.h>
#define ADC_LEN 1000  

uint32_t freq = 0;
uint16_t input1_data[ADC_LEN] = {0}; // 原始采集数据数组
uint16_t Music_data[3000] = {0}; // 音频信号数据数组
uint8_t max_point = 0;
uint8_t min_point = 0;
uint8_t Factor = 0;     //放大系数
uint8_t Bule_Data[100];

float Medianfilter_data [ADC_LEN] = {0}; // 中值滤波数据数组
float Meanfilter_data [ADC_LEN] = {0}; // 均值滤波数据数组
float Meanfilter_Type_data [ADC_LEN] = {0}; // 滤波波形判断数据数组
float input_data_float[1000] = {0};
float max = 0;
float min = 0;
float VPP = 0;    //峰峰值

uint8_t ty = 0;

/**
 * @brief 3点中值滤波
 * @param input  输入数组，uint16_t类型（原始ADC值）
 * @param output 输出数组，float类型（滤波后电压值或原始比例值）
 * @param length 数组长度
 */
void MedianFilter_3Point(uint16_t *input, float *output, uint16_t length)
{
    if (length < 1) return;
    
    for (uint16_t i = 0; i < length; i++)
    {
        uint16_t a, b, c;   // 窗口内的三个值
        
        // 左边界
        if (i == 0)
        {
            a = input[0];
            b = input[0];
            c = input[1];
        }
        // 右边界
        else if (i == length - 1)
        {
            a = input[length - 2];
            b = input[length - 1];
            c = input[length - 1];
        }
        // 正常内部
        else
        {
            a = input[i - 1];
            b = input[i];
            c = input[i + 1];
        }
        
        // 排序取中值
        uint16_t temp;
        if (a > b) { temp = a; a = b; b = temp; }
        if (a > c) { temp = a; a = c; c = temp; }
        if (b > c) { temp = b; b = c; c = temp; }
        
        // 中值为b，输出为float（可自行决定是否缩放，这里直接转float）
        output[i] = (float)b;
    }
}

/**
 * @brief 3点均值滤波（滑动平均）
 * @param input  输入数组，float类型
 * @param output 输出数组，float类型
 * @param length 数组长度
 */
void MeanFilter_3Point(float *input, float *output, uint16_t length)
{
    if (length < 1) return;
    
    for (uint16_t i = 0; i < length; i++)
    {
        float sum = 0.0f;
        
        // 左边界
        if (i == 0)
        {
            sum = input[0] + input[0] + input[1];
            output[i] = sum / 3.0f;
        }
        // 右边界
        else if (i == length - 1)
        {
            sum = input[length - 2] + input[length - 1] + input[length - 1];
            output[i] = sum / 3.0f;
        }
        // 正常内部
        else
        {
            sum = input[i - 1] + input[i] + input[i + 1];
            output[i] = sum / 3.0f;
        }
    }
}

void MODE_Switch()    //模拟开关选择
{	
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_5,GPIO_PIN_SET);        //关闭模拟开关
	if(VPP < 150)   //放大10倍
	{
		HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_SET);     // 010选择S2
		Factor = 10;
	}
    if(150 <= VPP && VPP < 300)    //放大5倍
	{
		HAL_GPIO_WritePin(GPIOB,GPIO_PIN_7,GPIO_PIN_SET);     // 001选择S1
		Factor = 5;
	}
    if(300 <= VPP)          //放大2倍
	{
        Factor = 2;		// 默认000选择S0
	}
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_5,GPIO_PIN_RESET);        //开启模拟开关
}

/**
 * @brief 识别波形类型（基于幅度分布统计）
 * @param input  浮点型波形数据数组（已滤波，包含直流偏置）
 * @param length 数组长度
 * @retval 1-方波，2-正弦波，3-三角波
 */
uint8_t WAVE_Type(float *input, uint16_t length)
{
    if (length == 0) return 2; // 默认正弦波
    
    float mean = 0.0f;
    float max_val = input[0];
    float min_val = input[0];
    
    // 第一遍：计算均值、最大值、最小值
    for (uint16_t i = 0; i < length; i++)
    {
        mean += input[i];
        if (input[i] > max_val) max_val = input[i];
        if (input[i] < min_val) min_val = input[i];
    }
    mean /= (float)length;
    
    // 阈值 A = (max + mean) / 2
    float A = (max_val + mean) * 0.5f;
    
    // 统计大于 A 的点数
    uint16_t cnt = 0;
    for (uint16_t i = 0; i < length; i++)
    {
        if (input[i] > A) cnt++;
    }
    
    float ratio = (float)cnt / (float)length;
    
    // 根据比例判断波形（理论值：三角波0.25，正弦波0.333，方波0.5）
    // 考虑实测误差和噪声，使用适当阈值区间
    if (ratio < 0.28f)
        return 3;   // 三角波
    else if (ratio < 0.38f)
        return 2;   // 正弦波
    else
        return 1;   // 方波
}

/**
 * @brief 将全局数组前len个数据插值并变换为100个uint8_t值
 * @param len 原始数据个数（≤1000），如果len=100则直接处理；否则线性插值到100点
 */
void Get_Bule_Data(uint16_t len)
{
    if (len == 0) return;
    
    float temp[100];        // 存放插值后的100个浮点数
    uint16_t i;
    
    // 步骤1：将前len个数据插值为100个点
    if (len == 100) 
	{
        // 直接复制
        for (i = 0; i < 100; i++) 
		{
            temp[i] = Meanfilter_Type_data[i];
        }
    } 
	else 
	{
        // 线性插值：将索引范围 [0, len-1] 映射到 [0, 99]
        // 目标点 j 对应原始位置 pos = j * (len-1) / 99.0
        float step = (float)(len - 1) / 99.0f;
        for (i = 0; i < 100; i++) 
		{
            float pos = i * step;          // 原始数组中的浮点位置
            uint16_t idx = (uint16_t)pos;  // 整数索引
            float frac = pos - idx;        // 小数部分
            if (idx >= len - 1) 
			{
                // 边界处理：最后一个点直接取最后一个值
                temp[i] = Meanfilter_Type_data[len - 1];
            } 
			else 
			{
                float v0 = Meanfilter_Type_data[idx];
                float v1 = Meanfilter_Type_data[idx + 1];
                temp[i] = v0 + frac * (v1 - v0);
            }
        }
    }
    
    // 步骤2：减去平均值
    float sum = 0.0f;
    for (i = 0; i < 100; i++) 
	{
        sum += temp[i];
    }
    float mean = sum / 100.0f;
    for (i = 0; i < 100; i++) 
	{
        temp[i] -= mean;
    }
    
    // 步骤3：除以最大值（注意：最大值可能为0，需避免除零）
    float max_val = temp[0];
    for (i = 1; i < 100; i++) 
	{
        if (temp[i] > max_val) max_val = temp[i];
    }
    if (max_val != 0.0f) 
	{
        for (i = 0; i < 100; i++) 
		{
            temp[i] /= max_val;
        }
    }
    // 如果max_val==0，则所有数据均为0，此时除以任何数都是0，保持原样即可
    
    // 步骤4：乘以120
    for (i = 0; i < 100; i++) 
	{
        temp[i] *= 120.0f;
    }
    
    // 步骤5：加上128，然后饱和到uint8_t范围并存储
    for (i = 0; i < 100; i++) 
	{
        float val = temp[i] + 128.0f;
        if (val < 0.0f) val = 0.0f;
        if (val > 255.0f) val = 255.0f;
        Bule_Data[i] = (uint8_t)val;
    }
}

/**
 * @brief 计算方波峰峰值（跳过边沿附近点）
 * @param input   输入方波数据数组（float型）
 * @param length  数组长度（uint16_t）
 * @note 每个边沿（上升沿和下降沿）前后各跳过5个点（共10个点），
 *       再对剩余有效点按均值区分高低电平，分别平均后差值存入VPP。
 *       支持非整数周期数据。
 */
void Calculate_Square_VPP(float *input, uint16_t length)
{
    if (length < 20) return;   // 长度不足，无法有效处理
    
    // 1. 计算均值作为阈值
    float sum = 0.0f;
    for (uint16_t i = 0; i < length; i++) {
        sum += input[i];
    }
    float mean = sum / length;
    
    // 2. 检测所有边沿（上升沿和下降沿）
    #define MAX_EDGES 200   // 足够存储最多200个边沿（100周期）
    uint16_t edge_idx[MAX_EDGES];
    uint16_t edge_cnt = 0;
    
    for (uint16_t i = 1; i < length; i++) {
        bool rising  = (input[i-1] <= mean && input[i] > mean);
        bool falling = (input[i-1] >= mean && input[i] < mean);
        if (rising || falling) {
            if (edge_cnt < MAX_EDGES) {
                edge_idx[edge_cnt++] = i;   // 边沿位置（i为跳变后的第一个点）
            }
        }
    }
    
    // 如果没有检测到边沿（如直流或噪声过大），退化为全局max-min
    if (edge_cnt == 0) {
        float max_val = input[0], min_val = input[0];
        for (uint16_t i = 1; i < length; i++) {
            if (input[i] > max_val) max_val = input[i];
            if (input[i] < min_val) min_val = input[i];
        }
        VPP = max_val - min_val;
        return;
    }
    
    // 3. 标记无效点：每个边沿前后各跳过5个点（共10个点）
    #define SKIP_BEFORE 5
    #define SKIP_AFTER  5
    bool valid[length];
    for (uint16_t i = 0; i < length; i++) valid[i] = true;
    
    for (uint16_t e = 0; e < edge_cnt; e++) {
        int start = (int)edge_idx[e] - SKIP_BEFORE;
        int end   = (int)edge_idx[e] + SKIP_AFTER;
        if (start < 0) start = 0;
        if (end >= (int)length) end = length - 1;
        for (int i = start; i <= end; i++) {
            valid[i] = false;
        }
    }
    
    // 4. 对有效点，按高低电平分别累加
    float high_sum = 0.0f, low_sum = 0.0f;
    uint16_t high_cnt = 0, low_cnt = 0;
    for (uint16_t i = 0; i < length; i++) {
        if (!valid[i]) continue;
        if (input[i] > mean) {
            high_sum += input[i];
            high_cnt++;
        } else {
            low_sum += input[i];
            low_cnt++;
        }
    }
    
    // 5. 计算平均值和峰峰值（避免除零）
    float high_avg = (high_cnt > 0) ? (high_sum / high_cnt) : mean;
    float low_avg  = (low_cnt  > 0) ? (low_sum / low_cnt)  : mean;
    VPP = high_avg - low_avg;
}

void Square_VPP_data(float *intput, uint16_t length)    //计算方波信号峰峰值
{
	    max = intput[0];
		min = intput[0];		
		for(int i=1; i<length; i++)
		{
			if(max < intput[i])
			{
				max = intput[i];
			}
			if(min > intput[i])
			{
				min = intput[i];
			}
		}
		float average = (max + min) / 2.0f;
		
		float max_p = 0;
		float min_p = 0;
		uint16_t a = 0,b = 0;		
		for(int i=0; i<length ;i++)
		{
			if(average < intput[i])
			{
				max_p += intput[i];
				a += 1;
			}
			else if(average > intput[i])
			{
				min_p += intput[i];
				b += 1;
			}			
		}
		max_p /= (float)a;
		min_p /= (float)b;
		VPP = max_p - min_p;
}

#include <math.h>      // 使用 fabsf
#include <stdint.h>
/**
 * @brief 计算稳健峰峰值（去除极端离群值）
 * @param input   输入浮点数组（如滤波后的ADC电压值）
 * @param length  数组长度（实际约1000）
 */
void Calculate_VPP(float *input, uint16_t length)
{
    // 1. 找出最大的12个值和最小的12个值
    // 使用部分排序：先复制前12个作为初始候选，然后遍历更新
    float max_vals[12];
    float min_vals[12];
    
    // 初始化候选数组
    for (uint16_t i = 0; i < 12; i++) {
        max_vals[i] = input[i];
        min_vals[i] = input[i];
    }
    
    // 排序辅助函数（用于维护有序的候选列表）
    // 对max_vals降序排序，对min_vals升序排序，以便快速替换
    // 由于数组很小(12)，直接使用插入排序简单高效
    
    // 先排序初始数组
    // 最大值数组降序
    for (int i = 0; i < 11; i++) {
        for (int j = i+1; j < 12; j++) {
            if (max_vals[i] < max_vals[j]) {
                float t = max_vals[i]; max_vals[i] = max_vals[j]; max_vals[j] = t;
            }
        }
    }
    // 最小值数组升序
    for (int i = 0; i < 11; i++) {
        for (int j = i+1; j < 12; j++) {
            if (min_vals[i] > min_vals[j]) {
                float t = min_vals[i]; min_vals[i] = min_vals[j]; min_vals[j] = t;
            }
        }
    }
    
    // 遍历剩余元素，更新候选列表
    for (uint16_t i = 12; i < length; i++) {
        float val = input[i];
        // 更新最大值候选（如果大于当前第12大的值）
        if (val > max_vals[11]) {
            max_vals[11] = val;
            // 重新排序（降序）
            for (int j = 11; j > 0 && max_vals[j] > max_vals[j-1]; j--) {
                float t = max_vals[j]; max_vals[j] = max_vals[j-1]; max_vals[j-1] = t;
            }
        }
        // 更新最小值候选（如果小于当前第12小的值）
        if (val < min_vals[11]) {
            min_vals[11] = val;
            // 重新排序（升序）
            for (int j = 11; j > 0 && min_vals[j] < min_vals[j-1]; j--) {
                float t = min_vals[j]; min_vals[j] = min_vals[j-1]; min_vals[j-1] = t;
            }
        }
    }
    
    // 现在 max_vals[0..11] 是降序的12个最大值，min_vals[0..11] 是升序的12个最小值
    
    // 2. 对12个最大值，计算均值，找出偏差最大的两个点并剔除
    // 计算均值
    float sum_max = 0;
    for (int i = 0; i < 12; i++) sum_max += max_vals[i];
    float mean_max = sum_max / 12.0f;
    
    // 计算每个值与均值的绝对偏差，并记录最大的两个偏差对应的索引
    float dev[12];
    for (int i = 0; i < 12; i++) dev[i] = fabsf(max_vals[i] - mean_max);
    
    // 找到最大的两个偏差的索引（可重复，但实际会选两个不同的）
    int idx1 = 0, idx2 = 1;
    if (dev[1] > dev[0]) { idx1 = 1; idx2 = 0; }
    for (int i = 2; i < 12; i++) {
        if (dev[i] > dev[idx1]) {
            idx2 = idx1;
            idx1 = i;
        } else if (dev[i] > dev[idx2]) {
            idx2 = i;
        }
    }
    
    // 剔除这两个索引，求剩余10个的平均
    float sum_max_trim = 0;
    int count_max = 0;
    for (int i = 0; i < 12; i++) {
        if (i != idx1 && i != idx2) {
            sum_max_trim += max_vals[i];
            count_max++;
        }
    }
    float avg_max = sum_max_trim / count_max;
    
    // 3. 对12个最小值同理
    float sum_min = 0;
    for (int i = 0; i < 12; i++) sum_min += min_vals[i];
    float mean_min = sum_min / 12.0f;
    
    float dev_min[12];
    for (int i = 0; i < 12; i++) dev_min[i] = fabsf(min_vals[i] - mean_min);
    
    int idx1_min = 0, idx2_min = 1;
    if (dev_min[1] > dev_min[0]) { idx1_min = 1; idx2_min = 0; }
    for (int i = 2; i < 12; i++) {
        if (dev_min[i] > dev_min[idx1_min]) {
            idx2_min = idx1_min;
            idx1_min = i;
        } else if (dev_min[i] > dev_min[idx2_min]) {
            idx2_min = i;
        }
    }
    
    float sum_min_trim = 0;
    int count_min = 0;
    for (int i = 0; i < 12; i++) {
        if (i != idx1_min && i != idx2_min) {
            sum_min_trim += min_vals[i];
            count_min++;
        }
    }
    float avg_min = sum_min_trim / count_min;
    
    // 4. 峰峰值 = 平均最大 - 平均最小
    VPP = avg_max - avg_min;
}

/**
 * @brief 计算稳健峰峰值（去除3个极端离群值）
 * @param input   输入浮点数组（电压值或ADC码值）
 * @param length  数组长度（约1000）
 */
void Calculate_VPP_Remove3(float *input, uint16_t length)
{
    // 1. 找出最大的12个值和最小的12个值
    float max_vals[12];
    float min_vals[12];
    
    // 初始化候选数组
    for (uint16_t i = 0; i < 12; i++) {
        max_vals[i] = input[i];
        min_vals[i] = input[i];
    }
    
    // 排序辅助：最大值降序
    for (int i = 0; i < 11; i++) {
        for (int j = i+1; j < 12; j++) {
            if (max_vals[i] < max_vals[j]) {
                float t = max_vals[i]; max_vals[i] = max_vals[j]; max_vals[j] = t;
            }
        }
    }
    // 最小值升序
    for (int i = 0; i < 11; i++) {
        for (int j = i+1; j < 12; j++) {
            if (min_vals[i] > min_vals[j]) {
                float t = min_vals[i]; min_vals[i] = min_vals[j]; min_vals[j] = t;
            }
        }
    }
    
    // 遍历剩余元素，更新候选列表
    for (uint16_t i = 12; i < length; i++) {
        float val = input[i];
        if (val > max_vals[11]) {
            max_vals[11] = val;
            for (int j = 11; j > 0 && max_vals[j] > max_vals[j-1]; j--) {
                float t = max_vals[j]; max_vals[j] = max_vals[j-1]; max_vals[j-1] = t;
            }
        }
        if (val < min_vals[11]) {
            min_vals[11] = val;
            for (int j = 11; j > 0 && min_vals[j] < min_vals[j-1]; j--) {
                float t = min_vals[j]; min_vals[j] = min_vals[j-1]; min_vals[j-1] = t;
            }
        }
    }
    
    // 2. 处理最大值：去掉3个偏离均值最大的点，剩下9个平均
    // 计算均值
    float sum_max = 0;
    for (int i = 0; i < 12; i++) sum_max += max_vals[i];
    float mean_max = sum_max / 12.0f;
    
    // 计算每个值的绝对偏差，并记录索引
    typedef struct {
        float dev;
        int idx;
    } DevPair;
    
    DevPair devs[12];
    for (int i = 0; i < 12; i++) {
        devs[i].dev = fabsf(max_vals[i] - mean_max);
        devs[i].idx = i;
    }
    // 按偏差降序排序（冒泡，因为只有12个）
    for (int i = 0; i < 11; i++) {
        for (int j = i+1; j < 12; j++) {
            if (devs[i].dev < devs[j].dev) {
                DevPair t = devs[i]; devs[i] = devs[j]; devs[j] = t;
            }
        }
    }
    // 前3个偏差最大，剔除其索引
    float sum_max_trim = 0;
    int count_max = 0;
    for (int i = 0; i < 12; i++) {
        int remove = 0;
        for (int k = 0; k < 3; k++) {
            if (i == devs[k].idx) { remove = 1; break; }
        }
        if (!remove) {
            sum_max_trim += max_vals[i];
            count_max++;
        }
    }
    float avg_max = sum_max_trim / count_max;  // count_max应为9
    
    // 3. 处理最小值：同理去掉3个偏离最大的
    float sum_min = 0;
    for (int i = 0; i < 12; i++) sum_min += min_vals[i];
    float mean_min = sum_min / 12.0f;
    
    DevPair devs_min[12];
    for (int i = 0; i < 12; i++) {
        devs_min[i].dev = fabsf(min_vals[i] - mean_min);
        devs_min[i].idx = i;
    }
    for (int i = 0; i < 11; i++) {
        for (int j = i+1; j < 12; j++) {
            if (devs_min[i].dev < devs_min[j].dev) {
                DevPair t = devs_min[i]; devs_min[i] = devs_min[j]; devs_min[j] = t;
            }
        }
    }
    float sum_min_trim = 0;
    int count_min = 0;
    for (int i = 0; i < 12; i++) {
        int remove = 0;
        for (int k = 0; k < 3; k++) {
            if (i == devs_min[k].idx) { remove = 1; break; }
        }
        if (!remove) {
            sum_min_trim += min_vals[i];
            count_min++;
        }
    }
    float avg_min = sum_min_trim / count_min;
    
    // 4. 峰峰值
    VPP = avg_max - avg_min;
}

#include <math.h>
#include <stdint.h>

extern float VPP;   // 全局变量

/**
 * @brief 计算稳健峰峰值（去除4个极端离群值）
 * @param input   输入浮点数组（电压值或ADC码值）
 * @param length  数组长度（约1000）
 */
void Calculate_VPP_Remove4(float *input, uint16_t length)
{
    if (length < 24) return;   // 至少需要24个点
    
    // 1. 找出最大的12个值和最小的12个值
    float max_vals[12];
    float min_vals[12];
    
    // 初始化候选数组
    for (uint16_t i = 0; i < 12; i++) {
        max_vals[i] = input[i];
        min_vals[i] = input[i];
    }
    
    // 排序辅助：最大值降序
    for (int i = 0; i < 11; i++) {
        for (int j = i+1; j < 12; j++) {
            if (max_vals[i] < max_vals[j]) {
                float t = max_vals[i]; max_vals[i] = max_vals[j]; max_vals[j] = t;
            }
        }
    }
    // 最小值升序
    for (int i = 0; i < 11; i++) {
        for (int j = i+1; j < 12; j++) {
            if (min_vals[i] > min_vals[j]) {
                float t = min_vals[i]; min_vals[i] = min_vals[j]; min_vals[j] = t;
            }
        }
    }
    
    // 遍历剩余元素，更新候选列表
    for (uint16_t i = 12; i < length; i++) {
        float val = input[i];
        if (val > max_vals[11]) {
            max_vals[11] = val;
            for (int j = 11; j > 0 && max_vals[j] > max_vals[j-1]; j--) {
                float t = max_vals[j]; max_vals[j] = max_vals[j-1]; max_vals[j-1] = t;
            }
        }
        if (val < min_vals[11]) {
            min_vals[11] = val;
            for (int j = 11; j > 0 && min_vals[j] < min_vals[j-1]; j--) {
                float t = min_vals[j]; min_vals[j] = min_vals[j-1]; min_vals[j-1] = t;
            }
        }
    }
    
    // 2. 处理最大值：去掉4个偏离均值最大的点，剩下8个平均
    // 计算均值
    float sum_max = 0;
    for (int i = 0; i < 12; i++) sum_max += max_vals[i];
    float mean_max = sum_max / 12.0f;
    
    // 计算每个值的绝对偏差，并记录索引
    typedef struct {
        float dev;
        int idx;
    } DevPair;
    
    DevPair devs[12];
    for (int i = 0; i < 12; i++) {
        devs[i].dev = fabsf(max_vals[i] - mean_max);
        devs[i].idx = i;
    }
    // 按偏差降序排序（冒泡，因为只有12个）
    for (int i = 0; i < 11; i++) {
        for (int j = i+1; j < 12; j++) {
            if (devs[i].dev < devs[j].dev) {
                DevPair t = devs[i]; devs[i] = devs[j]; devs[j] = t;
            }
        }
    }
    // 前4个偏差最大，剔除其索引
    float sum_max_trim = 0;
    int count_max = 0;
    for (int i = 0; i < 12; i++) {
        int remove = 0;
        for (int k = 0; k < 4; k++) {
            if (i == devs[k].idx) { remove = 1; break; }
        }
        if (!remove) {
            sum_max_trim += max_vals[i];
            count_max++;
        }
    }
    float avg_max = sum_max_trim / count_max;  // count_max应为8
    
    // 3. 处理最小值：同理去掉4个偏离最大的
    float sum_min = 0;
    for (int i = 0; i < 12; i++) sum_min += min_vals[i];
    float mean_min = sum_min / 12.0f;
    
    DevPair devs_min[12];
    for (int i = 0; i < 12; i++) {
        devs_min[i].dev = fabsf(min_vals[i] - mean_min);
        devs_min[i].idx = i;
    }
    for (int i = 0; i < 11; i++) {
        for (int j = i+1; j < 12; j++) {
            if (devs_min[i].dev < devs_min[j].dev) {
                DevPair t = devs_min[i]; devs_min[i] = devs_min[j]; devs_min[j] = t;
            }
        }
    }
    float sum_min_trim = 0;
    int count_min = 0;
    for (int i = 0; i < 12; i++) {
        int remove = 0;
        for (int k = 0; k < 4; k++) {
            if (i == devs_min[k].idx) { remove = 1; break; }
        }
        if (!remove) {
            sum_min_trim += min_vals[i];
            count_min++;
        }
    }
    float avg_min = sum_min_trim / count_min;
    
    // 4. 峰峰值
    VPP = avg_max - avg_min;
}

// 通过蓝牙发送字符串
void BLE_Send_String(char *str)
{
    HAL_UART_Transmit(&huart3, (uint8_t*)str, strlen(str), 100);
}

// 发送波形数据（100个点），使用 Fitting_Bule_data 数组
void BLE_Send_Waveform_100 (uint8_t *wave_data)
{
    char buffer[2048];  // 100个点 * 最大4字符/点 + 键名等，足够
    uint16_t idx = 0;
    
    // 添加键名 "y:"
    idx += sprintf(buffer + idx, "y:");
    
    // 添加所有波形点，用空格分隔
    for (uint16_t i = 0; i < 100; i++) 
	{
        if (i > 0) 
		{
            idx += sprintf(buffer + idx, " ");
        }
        idx += sprintf(buffer + idx, "%d", wave_data[i]);
    }
    
    // 添加结束逗号
    idx += sprintf(buffer + idx, ",");
    
    BLE_Send_String(buffer);
}

