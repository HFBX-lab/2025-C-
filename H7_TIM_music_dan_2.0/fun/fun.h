#include "main.h"
//#include "memorymap.h"
#include "usart.h"
#include "gpio.h"

#include "math.h"
#include "stdio.h"
#include <string.h>
#include "arm_math.h"
#include "arm_const_structs.h"

extern float f;        
extern float fs;     
extern uint16_t input_data[];
extern float FFT_buffer[];
extern float Fitting_data[];

extern float FFT_mag[];
extern float FFT_mag_data[];
extern uint32_t FFT_mag_data_index[];
extern float FFT_phase[];
extern float FFT_Freq;
extern float FFT_Ampl;

void Generate_data();
void apply_hanning();
void FFT();
void energy_centroid_correction(uint32_t peak_idx, float* freq, float* amp, float* phase);
void Process_FFT_mag();
void FFT_Fitting_Component();
void Fitting_curve();
void Show_data(float* buffer, uint16_t n);
//void BLE_Send_String(char *str);
//void BLE_Send_Params(float thd, float v1, float v2, float v3, float v4, float v5);
//void BLE_Send_Waveform_100 (uint8_t *wave_data);