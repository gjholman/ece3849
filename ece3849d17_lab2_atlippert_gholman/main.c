/*
 *  ======== main.c ========
 */

// Imported from previous project
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "inc/lm3s8962.h"
#include "driverlib/sysctl.h"
#include "drivers/rit128x96x4.h"
#include "frame_graphics.h"
#include "utils/ustdlib.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/gpio.h"
#include "buttons.h"
#include "driverlib/adc.h"
#include <xdc/cfg/global.h>
#include "math.h"
#include "kiss_fft.h"
#include "_kiss_fft_guts.h"

#define PI 3.14159265358979f
#define NFFT 1024 // FFT length
#define KISS_FFT_CFG_SIZE (sizeof(struct kiss_fft_state)+sizeof(kiss_fft_cpx)*(NFFT-1))

//This was here before

#include <xdc/std.h>

#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>

#include <ti/sysbios/knl/Task.h>

// Previous Defines

#define BUTTON_CLOCK 200 // button scanning interrupt rate in Hz
#define ADC_BUFFER_SIZE 2048 // must be a power of 2
#define ADC_BUFFER_WRAP(i) ((i) & (ADC_BUFFER_SIZE - 1)) // index wrapping macro
#define ADCOFFSET 514 // ADC ranges from 418 to 609, 514 is the average of the two extremes
#define VIN_RANGE 6
#define PIXELS_PER_DIV 12
#define ADC_BITS 16
#define ADCBuffer_Center 1024

// Previous Globals

volatile int g_iADCBufferIndex = ADC_BUFFER_SIZE - 1; // latest sample index
volatile short g_psADCBuffer[ADC_BUFFER_SIZE]; // circular buffer
volatile unsigned long g_ulADCErrors = 0; // number of missed ADC deadlines
unsigned long g_ulSystemClock; // system clock frequency in Hz
volatile unsigned long g_ulTime = 8345; // time in hundredths of a second
unsigned int ff, ss, mm;
unsigned int vpd;
char pcStr[50]; // string buffer

int spec = 0;
int risingFalling = 0;
short specBuff[1024];

float fVoltsPerDiv[4] = {.100, .200, .500, 1};  //initially 500mV, changed by button.
int fLevel = 2;
short trigBuff[128];
float specOut[128];
short tempBuff[ADC_BUFFER_SIZE];//
int x_scale = 24; 				//24 default, every number must be multiple of 24

#define FIFO_SIZE 11		// FIFO capacity is 1 item fewer
typedef char DataType;		// FIFO data type
volatile DataType fifo[FIFO_SIZE];	// FIFO storage array
volatile int fifo_head = 0;	// index of the first item in the FIFO
volatile int fifo_tail = 0;	// index one step past the last item

void updateDisplay(); 			//Function to update the display
int fifo_put(DataType data);
int fifo_get(DataType *data);

//ADC ISR

void ADCISR(void)
{
	ADC_ISC_R = ADC_ISC_IN0; // clear ADC sequence0 interrupt flag in the ADCISC register
	if (ADC0_OSTAT_R & ADC_OSTAT_OV0) { // check for ADC FIFO overflow
		g_ulADCErrors++; // count errors - step 1 of the signoff
		ADC0_OSTAT_R = ADC_OSTAT_OV0; // clear overflow condition
	}
	int buffer_index = ADC_BUFFER_WRAP(g_iADCBufferIndex + 1);
	g_psADCBuffer[buffer_index] = ADC_SSFIFO0_R; // read sample from the ADC sequence0 FIFO
												// range from 418 to 609, ground/offestest is 514
	g_iADCBufferIndex = buffer_index;

	//Semaphore_post(sem_Waveform);

}


/*
 *  ======== taskFxn ========
 */
//Void taskFxn(UArg a0, UArg a1)
//{
//    System_printf("enter taskFxn()\n");
//
//    Task_sleep(10);
//
//    System_printf("exit taskFxn()\n");
//}

int initial = 1;

// Tasks Up the WAZOO
void ButtonTask(UArg arg0, UArg arg1){



	while(1){
		Semaphore_pend(sem_Button, BIOS_WAIT_FOREVER);

		unsigned long presses = g_ulButtons;
		char button = 0;

		ButtonDebounce(((~GPIO_PORTF_DATA_R & (GPIO_PIN_1)) << 3)|((~GPIO_PORTE_DATA_R & (GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3))));
		presses = ~presses & g_ulButtons; // button press detector

		switch(presses){
			case 1:
				button = 'U';
				break;
			case 2:
				button = 'D';
				break;
			case 4:
				button = 'L';
				break;
			case 8:
				button = 'R';
				break;
			case 16:
				button = 'S';
				break;
		}
		if(button != 0){
			Mailbox_post(mailbox0, &button, BIOS_NO_WAIT);
			Semaphore_post(sem_UI);
		}
	}
}

void ClockTick(UArg arg0, UArg arg1){
	Semaphore_post(sem_Button);
}

void WaveformTask(UArg arg0, UArg arg1){
	IntMasterEnable();

	while(1){

		Semaphore_pend(sem_Waveform, BIOS_WAIT_FOREVER);

		// make the waveform
		if (spec==0){
			int i;
			int tempIndex = g_iADCBufferIndex;
			if (risingFalling == 0){
				for(i=ADC_BUFFER_WRAP(tempIndex = FRAME_SIZE_X/2); i != (tempIndex - ADCBuffer_Center - FRAME_SIZE_X/2); i--){
					if ((g_psADCBuffer[ADC_BUFFER_WRAP(i)] <= ADCOFFSET ) && (g_psADCBuffer[ADC_BUFFER_WRAP(i+1)]> ADCOFFSET)){
						int k;
						for (k=0; k<FRAME_SIZE_X; k++){
							trigBuff[k] = g_psADCBuffer[ADC_BUFFER_WRAP(i - 64 + k*x_scale/24)];
						}

						//if (trigBuff[63] <= ADCOFFSET && trigBuff[64] > ADCOFFSET) // Error Check
						//updateDisplay();
					}
				}
			}
			else{
				for(i=ADC_BUFFER_WRAP(tempIndex = FRAME_SIZE_X/2); i != (tempIndex - ADCBuffer_Center - FRAME_SIZE_X/2); i--){
					if ((g_psADCBuffer[ADC_BUFFER_WRAP(i)] > ADCOFFSET ) && (g_psADCBuffer[ADC_BUFFER_WRAP(i+1)] <= ADCOFFSET)){
						int k;
						for (k=0; k<FRAME_SIZE_X; k++){
							trigBuff[k] = g_psADCBuffer[ADC_BUFFER_WRAP(i - 64 + k*x_scale/24)];
						}

						//if (trigBuff[63] > ADCOFFSET && trigBuff[64] <= ADCOFFSET) // Error Check
						//updateDisplay();
					}
				}
			}
			Semaphore_post(sem_Display);
		}
		else{
			// do the fft thingy
			int i;

			for(i=0; i<1024; i++){
				specBuff[i] = g_psADCBuffer[i+1024];
			}
			Semaphore_post(sem_FFT);
		}

	}
}

void UITask(UArg arg0, UArg arg1){ //WE ARE HERE, WORKING ON THIS FUNCTION, GONNA DO GR8 Things

	char c;

	while(1){

		// Semaphore_pend(sem_UI, BIOS_WAIT_FOREVER);

		Mailbox_pend(mailbox0, &c, BIOS_WAIT_FOREVER);
		switch (c){
		case 'S':
			if (risingFalling == 0){
				risingFalling = 1;
				break;
			}
			else{
				risingFalling = 0;
				break;
			}
		case 'U':
			if (fLevel < 3)
				fLevel++;
			break;
		case 'D':
			if (fLevel > 0)
				fLevel--;
			break;
		case 'L': // Left signals spectrum toggle
			if (spec == 0){
				spec = 1;
				break;
			}
			else{
				spec = 0;
				break;
			}
		case 'R':
			break;
		}
		Semaphore_post(sem_Display);
	}
}

void DisplayTask(UArg arg0, UArg arg1){

	while(1){
		Semaphore_pend(sem_Display, BIOS_WAIT_FOREVER);
		//put the old display code here, but pend on the ...?
		if (spec == 0){
			float fScale = (6 * 12)/((1 << 10) * fVoltsPerDiv[fLevel]);
			int j;
			FillFrame(0);
			for (j=1; j<=5; j++){
				DrawLine(FRAME_SIZE_X/2+j*12, 0, FRAME_SIZE_X/2+j*12, FRAME_SIZE_Y, 3);
				DrawLine(FRAME_SIZE_X/2-j*12, 0, FRAME_SIZE_X/2-j*12, FRAME_SIZE_Y, 3);
			}
			for (j=1; j<=3; j++){
					DrawLine(0, FRAME_SIZE_Y/2+j*12, FRAME_SIZE_X, FRAME_SIZE_Y/2+j*12, 3);
					DrawLine(0, FRAME_SIZE_Y/2-j*12, FRAME_SIZE_X, FRAME_SIZE_Y/2-j*12, 3);
				}


			DrawLine(0, FRAME_SIZE_Y/2, FRAME_SIZE_X, FRAME_SIZE_Y/2, 8);
			DrawLine(FRAME_SIZE_X/2, 0, FRAME_SIZE_X/2, FRAME_SIZE_Y, 8);
			int y2 = FRAME_SIZE_Y/2;
			for (j=0; j<128; j++){
				int y = FRAME_SIZE_Y/2 - (int)roundf((trigBuff[j] - ADCOFFSET) * fScale); //Offset is calculated from oscillator circuit
				DrawLine(j-1,y2,j,y,15);
				y2 = y;
			}
			char div[50];
			int fVolts_int = fVoltsPerDiv[fLevel]*10000;
			usprintf(div, "%d mV/div", fVolts_int/10, fVolts_int % 10);
			DrawString(0, 0, "24us", 15, false);

			DrawString(FRAME_SIZE_X/2, 0, div, 15, false);

			RIT128x96x4ImageDraw(g_pucFrame, 0, 0, FRAME_SIZE_X, FRAME_SIZE_Y);
			Semaphore_post(sem_Waveform);
		}
		else{
			//float fScale = (6 * 12)/((1 << 10) * fVoltsPerDiv[fLevel]);
			int j;

			FillFrame(0);
			for (j=1; j<=2; j++){
				DrawLine(FRAME_SIZE_X/2+j*24, 0, FRAME_SIZE_X/2+j*24, FRAME_SIZE_Y, 3);
				DrawLine(FRAME_SIZE_X/2-j*24, 0, FRAME_SIZE_X/2-j*24, FRAME_SIZE_Y, 3);
			}
			for (j=1; j<=2; j++){
				DrawLine(0, FRAME_SIZE_Y/2+j*24, FRAME_SIZE_X, FRAME_SIZE_Y/2+j*24, 3);
				DrawLine(0, FRAME_SIZE_Y/2-j*24, FRAME_SIZE_X, FRAME_SIZE_Y/2-j*24, 3);
			}
			DrawLine(0, FRAME_SIZE_Y/2, FRAME_SIZE_X, FRAME_SIZE_Y/2, 3);
			DrawLine(FRAME_SIZE_X/2, 0, FRAME_SIZE_X/2, FRAME_SIZE_Y, 3);
			int y2 = FRAME_SIZE_Y/2;
			for (j=0; j<128; j++){
				int y = FRAME_SIZE_Y/2 - (int)roundf(specOut[j])/2; // need to figure out scale and offset
				DrawLine(j-1,y2,j,y,15);
				y2 = y;
			}
			DrawString(0, 0, "7.3kHz", 15, false);
			DrawString(FRAME_SIZE_X/2, 0, "20dBV", 15, false);
			RIT128x96x4ImageDraw(g_pucFrame, 0, 0, FRAME_SIZE_X, FRAME_SIZE_Y);
			Semaphore_post(sem_Waveform);
		}
	}
}

void FFTTask(UArg arg0, UArg arg1){
	static char kiss_fft_cfg_buffer[KISS_FFT_CFG_SIZE]; // Kiss FFT config memory
	size_t buffer_size = KISS_FFT_CFG_SIZE;
	kiss_fft_cfg cfg; // Kiss FFT config
	static kiss_fft_cpx in[NFFT], out[NFFT]; // complex waveform and spectrum buffers

	cfg = kiss_fft_alloc(NFFT, 0, kiss_fft_cfg_buffer, &buffer_size); // init Kiss FFT

	while(1){
		Semaphore_pend(sem_FFT, BIOS_WAIT_FOREVER);

		int j;
		for (j = 0; j < NFFT; j++) { // generate an input waveform
			in[j].r = specBuff[j];
			in[j].i = 0;

//			in[j].r = sinf(20*PI*j/NFFT); // real part of waveform
//			in[j].i = 0; // imaginary part of waveform
		}

		kiss_fft(cfg, in, out); // compute FFT
		// convert first 128 bins of out[] to dB for display

		for (j = 0; j < FRAME_SIZE_X; j++){
			specOut[j] = 10*log10f((out[j].r*out[j].r+out[j].i*out[j].i)/NFFT*NFFT)-96;
		}

		//Semaphore_post(sem_Waveform);
		Semaphore_post(sem_Display);
	}
}

//

/*
 *  ======== main ========
 */
void main()
{ 
    /*
     * use ROV->SysMin to view the characters in the circular buffer
     */
	IntMasterDisable();

	RIT128x96x4Init(3500000); // initialize the OLED display

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
	GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_1);
	GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_0);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
	GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_1);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
	GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_2);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_2, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
	GPIOPinTypeGPIOInput(GPIO_PORTE_BASE, GPIO_PIN_3);
	GPIOPadConfigSet(GPIO_PORTE_BASE, GPIO_PIN_3, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);


	SysCtlPeripheralReset(SYSCTL_PERIPH_ADC0); // reset the ADC
	SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); // enable the ADC
	SysCtlADCSpeedSet(SYSCTL_ADCSPEED_500KSPS); // specify 500ksps
	ADCSequenceDisable(ADC_BASE, 0); // choose ADC sequence 0; disable before configuring
	ADCSequenceConfigure(ADC_BASE, 0, ADC_TRIGGER_ALWAYS, 0); // specify the "Always" trigger
	ADCSequenceStepConfigure(ADC_BASE, 0, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END); // in the 0th step, sample channel 0
	 // enable interrupt, and make it the end of sequence
	ADCIntEnable(ADC_BASE, 0); // enable ADC interrupt from sequence 0
	ADCSequenceEnable(ADC_BASE, 0); // enable the sequence. it is now sampling

    BIOS_start();        /* enable interrupts and start SYS/BIOS */
}
