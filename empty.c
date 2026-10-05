/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISIN<G IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"
#include "core/inc/motor_commutation.h"
//#include <timera.h>
//C:\ti\mspm0_sdk_2_10_00_04\source\ti\driverlib\dl_timera.h
//C:\Users\slin9\workspace_ccstheia\esc\empty.c

/*18.2.2.4 Up Counting Mode
In up counting mode, TIMx counts from zero up to the value defined in TIMx.LOAD. When the TIMx.CTR value 
equals TIMx.LOAD and TIMx.CTRCTL.REPEAT is not equal to 0, the zero is loaded into TIMx.CTR and the 
timer repeats the up counting pattern as shown in Figure 18-5.
A Load event is generated when TIMx counts to TIMx.LOAD. A Zero event is generated when TIMx counts from 
TIMx.LOAD to the zero value. Figure 18-6 shows the event generating cycle*/

volatile uint16_t period;
volatile float duty_cycle;
volatile uint16_t last_period_start = 0;
extern volatile uint8_t commutation_step;
extern volatile uint8_t curr_commutation_step;
/*isr assumes following settings: timer-up counting, no zero event on any capture, and load value of 64799, i.e. 2.7ms timer-period*/
void CAPTURE_0_INST_IRQHandler() {
    //TODO: bei einer supply voltage von 3.3 Volt für den uController saturiert die input voltage für den
    //ADC bei 3.2951 Volt bei einer 10bit auflösung, siehe seite 673 im datenblatt für die formel.
    //bisjetzt ist es allerdings so, da bei einer 16V batteriespannung bei deinem voltage divider maximal BEMF von 3.72
    //Volt möglich sind, d.h. den voltage divider must du noch so ändern, dass er 16V auf fast 3.3 volt runterbricht
    //(oder du erhöhst die supply voltage des uc)
    //irgendwas togglen
    switch (DL_TimerA_getPendingInterrupt(CAPTURE_0_INST)) {
        case DL_TIMERA_IIDX_CC0_UP: //a low-to-high transition was detected
            uint16_t period_start = (uint16_t)DL_TimerA_getCaptureCompareValue(CAPTURE_0_INST, DL_TIMER_CC_0_INDEX); 
            period = period_start - (uint16_t)last_period_start;
            last_period_start = period_start;
            break;
        case DL_TIMERA_IIDX_CC1_UP: // a high-to-low transition was detected
            uint16_t duty_cycle_end = (uint16_t)DL_TimerA_getCaptureCompareValue(CAPTURE_0_INST, DL_TIMER_CC_1_INDEX);
            duty_cycle = (float)(duty_cycle_end - (uint16_t)last_period_start)/(float)period;
            break;
        case DL_TIMERA_IIDX_ZERO: //timeroverflow 
            last_period_start = last_period_start - 64799; //64799 ist der load-value, durch diese aktion wird 
            //das capture des nächsten duty-cycles und periode nicht verfälscht, nachdem der timer "überlauft", weil
            //er den load value erreicht hat.
        default: break;
    }
    //DL_GPIO_togglePins(GPIO_GRP_0_PORT, 1<<6);
}

uint16_t retrieve_phase_a() {
    return DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_3); //TODO: welche werte kann das BEMF annehmen?
    //wenn es negativ wird haben wir ein prob, weil dafür die funktion hier nicht ausgelegt ist:
    /*die formeln sind auf seite 673 dargestellt, bei einer negativen voltage wird auch das ergebnis sehr schnell negativ
    tatsächlich kann der ADC anscheinend auch negative werte bilden (seite 680), allerdings ist das hier 
    (DL_ADC12_getMemResult) nicht die richtige funktion dafür
    BEMF wird laut diesem blogeintrag nicht negativ:
    https://www.eevblog.com/forum/beginners/bemf-zero-crossing-in-bldc-motors/
     */
}

uint16_t retrieve_phase_b() {
    return DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_1); //TODO: 11.2.4 Hardware Averaging könnte sich als nützlich zum
    //debouncen um den nullpunkt herum erweisen
}

uint16_t retrieve_phase_c() {
    return DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_3);
}

uint16_t retrieve_phase_reference() {
    return DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
}

uint16_t * neutral_phase_val_ptr, phase_a_val, phase_b_val, phase_c_val; //TODO: du musst den wert hier noch richtig initialisieren, damit wenn 
//das erste mal phase_commutation_controll aufgerufen wird, nicht 

volatile uint16_t zero_crossings = 0;
uint8_t pre_high, sample_neutral;
uint16_t neutral_phase_val, reference_val;

typedef struct adc_channel_config {
    uint32_t adc_channel_nr;
    uint32_t memory_idx;
} adc_channel_config;

const adc_channel_config channel_0 = {.adc_channel_nr = DL_ADC12_INPUT_CHAN_0, .memory_idx = DL_ADC12_SEQ_START_ADDR_00};//.memory_idx = ADC12_0_ADCMEM_0};
const adc_channel_config channel_9 = {.adc_channel_nr = DL_ADC12_INPUT_CHAN_9, .memory_idx = DL_ADC12_SEQ_START_ADDR_01}; //ADC12_0_ADCMEM_1};
const adc_channel_config channel_2 = {.adc_channel_nr = DL_ADC12_INPUT_CHAN_2, .memory_idx = DL_ADC12_SEQ_START_ADDR_03};//ADC12_0_ADCMEM_3};
const adc_channel_config channel_3 = {.adc_channel_nr = DL_ADC12_INPUT_CHAN_3, .memory_idx = DL_ADC12_SEQ_START_ADDR_02};//ADC12_0_ADCMEM_2};

adc_channel_config commutation_step_to_adc_channel_map[] = {channel_0, channel_9, channel_2, channel_0, channel_9, channel_2};

volatile uint8_t phase_commutation_controll() {
    if (zero_crossings >= 8) {
        bldc_move_star_winding(neutral_phase_val_ptr, &phase_a_val, &phase_b_val, &phase_c_val); 
        ++commutation_step;
        commutation_step %= 6;
        return 0;
    }
    return 1;
    //TODO: das ganze lässt sich vll. eleganter mit dem window comparator des adc lösen 
}

float max_duty_cylce_val = 240; //TODO: hier noch den richtigen wert bestimmten

/*TODO: das hier gilt es noch durch eine PI-loop irgendwie zu regeln. je länger die drone fliegt desto weniger batteriespannung
existiert, d.h. die drone wird mit dieser art der stromsteuerung immer langsamer, weil der maximalwert mit
der spannung sinkt, weil hier praktisch ein maximaler duty cylce festgelegt wird. weitere gründe: gegenwind, turbulenzen etc.
eine idee der PI regelung. man könnte messen wie schnell sich der motor dreht in dem man mittels eines timers misst
wie schnell die phasenwechsel sind, aus der vorhandenen geschwindigkeit und gewünschten geschwindigkeit einen PI-Regler
machen. schau dir in diesem zusammenhang auch mal FOC an*/
void motor_amperage_controll_v0(float incoming_duty_cycle) {
    float duty_cycle = max_duty_cylce_val*incoming_duty_cycle;
    DL_Timer_setCaptureCompareValue(PWM_0_INST, duty_cycle, 0);
    DL_Timer_setCaptureCompareValue(PWM_0_INST, duty_cycle, 1);
    DL_Timer_setCaptureCompareValue(PWM_0_INST, duty_cycle, 3);
}

//void start_motor

void main_task() {
    phase_commutation_controll(); //determines when next phase of electrical cycle shall be initiated
    float incoming_duty_cycle = (float)duty_cycle/(float)period;
    motor_amperage_controll(incoming_duty_cycle); //controll the amperage through the motor windings
}

#define PHASE_A 0
#define PHASE_B 1
#define PHASE_C 2
#define ZERO_REFERENCE 3

void PWM_0_INST_IRQHandler() {
        DL_ADC12_startConversion(ADC12_0_INST);
        while (DL_ADC12_isConversionStarted(ADC12_0_INST));
        pre_high = 1;
        switch(curr_commutation_step) {
            case 0: pre_high = 0; neutral_phase_val = retrieve_phase_c(); break;
            case 3: pre_high = 1; neutral_phase_val = retrieve_phase_c(); break;
            case 2: pre_high = 0; neutral_phase_val = retrieve_phase_a(); break;
            case 5: pre_high = 1; neutral_phase_val = retrieve_phase_a(); break;
            case 4: pre_high = 0; neutral_phase_val = retrieve_phase_b(); break;
            case 1: pre_high = 1; neutral_phase_val = retrieve_phase_b(); break;
        }
        DL_ADC12_startConversion(ADC12_0_INST);
        while (DL_ADC12_isConversionStarted(ADC12_0_INST));
        DL_ADC12_setStartAddress(ADC12_0_INST, channel_3.memory_idx); 
        sample_neutral = 0;
        reference_val = retrieve_phase_reference();
        adc_channel_config adc_channel = commutation_step_to_adc_channel_map[curr_commutation_step];
        DL_ADC12_setStartAddress(ADC12_0_INST, adc_channel.memory_idx);
        sample_neutral = 1;
        if (pre_high) {
            if (neutral_phase_val >= reference_val) {
                ++zero_crossings;
            }
        } else {
            if (neutral_phase_val <= reference_val) {
                ++zero_crossings;
            }
        }
    DL_ADC12_enableConversions(ADC12_0_INST); // enable next conversion
}

int main(void)
{
    SYSCFG_DL_init();
    DL_Timer_startCounter(PWM_0_INST);
    NVIC_EnableIRQ(CAPTURE_0_INST_INT_IRQN);
    DL_TimerA_startCounter(CAPTURE_0_INST);
    //NVIC_EnableIRQ(PWM_0_INST_INT_IRQN);
    DL_ADC12_enableConversions(ADC12_0_INST);
    neutral_phase_val = reference_val = 0;
    uint16_t duty_cylce_1 = 1060; //Merke: ab 1155/1154 frühestens erst wird die erfassung mittels adc möglich
    //merke für eine umdrehung bei 500 in open_loop(100, 500) ist ein wert von 900 notwendig, vll. auch höher (d.h. näher
    //an 1200)
    DL_ADC12_enableDMA(ADC12_0_INST);
    //outdatet bei 31000khz: 530 scheint bei 500 als wert in open_loop(100, 500) der nächstmögliche wert zu sein
    //der eine solche drehzahl ermöglicht 495 bei 450
    DL_Timer_setCaptureCompareValue(PWM_0_INST, duty_cylce_1, 0);
    DL_Timer_setCaptureCompareValue(PWM_0_INST, duty_cylce_1, 1);
    DL_Timer_setCaptureCompareValue(PWM_0_INST, duty_cylce_1, 2);
    //(PWM_1_INST, duty_cylce, 0);
    //#defineDL_Timer_setCaptureCompareValue GPIO_GRP_0_PORT GPIOA
    
    uint32_t pinnumber = 18;
    DL_GPIO_initDigitalOutput(pinnumber);
    DL_GPIO_enableOutput(GPIO_GRP_0_PORT, 1<<pinnumber);
    pinnumber = 4;
    DL_GPIO_initDigitalOutput(pinnumber);
    DL_GPIO_enableOutput(GPIO_GRP_0_PORT, 1<<pinnumber);
    pinnumber = 11;
    DL_GPIO_initDigitalOutput(pinnumber);
    DL_GPIO_enableOutput(GPIO_GRP_0_PORT, 1<<pinnumber);
    pinnumber = 22;
    DL_GPIO_initDigitalOutput(pinnumber);
    DL_GPIO_enableOutput(GPIO_GRP_0_PORT, 1<<pinnumber);
    pinnumber = 0;
    DL_GPIO_initDigitalOutput(pinnumber);
    DL_GPIO_enableOutput(GPIO_GRP_0_PORT, 1<<pinnumber);
    while (1) {
        DL_GPIO_setPins(GPIO_GRP_0_PORT, 1<<0);
        //for (volatile uint16_t i = 0; i<(uint16_t)666; ++i);
        for (volatile uint16_t i = 0; i<(uint16_t)(1000.0f*duty_cycle); ++i);
        DL_GPIO_clearPins(GPIO_GRP_0_PORT, 1<<0);
        //for (volatile uint16_t i = 0; i<(uint16_t)333; ++i);
        for (volatile uint16_t i = 0; i<(uint16_t)(1000.0f*(1.0f-duty_cycle)); ++i);
    }
    set_all_hb_low();  
    for (volatile uint32_t i = 0; i<1000; ++i); // wait dead-time
    //set_hb_b_high();
    open_loop(40, 3500); // 450 und 800 bei zerstoerung
    //zero_crossings = 0;
    while(1) {
        volatile uint8_t res = phase_commutation_controll(); 
        if (res==0) {
            for (volatile uint8_t i = 0; i<20; ++i);
            NVIC_DisableIRQ(PWM_0_INST_INT_IRQN);
            zero_crossings = 0;
            NVIC_EnableIRQ(PWM_0_INST_INT_IRQN);
        }
    }
    #define C_CHANNEL 2
    adc_channel_config arr[] = {channel_0, channel_9, channel_3, channel_2};
    while (1) {
    }
}
//11, 4, 18