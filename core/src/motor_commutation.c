

#include <stdint.h>
#include "ti_msp_dl_config.h"
#include "..\inc\motor_commutation.h"

//static GPIO_Regs * const GPIOA = ((GPIO_Regs *) GPIOA_BASE);
//#define GPIO_GRP_0_PORT GPIOA

uint32_t HB_LOW = DL_TIMER_CCP_DIS_OUT_LOW;
uint32_t HB_HIGH = DL_TIMER_CCP_DIS_OUT_SET_BY_OCTL;
uint8_t hb_a_high_stat, hb_b_high_stat, hb_c_high_stat;

#define A_LOW_SIDE 11
#define B_LOW_SIDE 18
#define C_LOW_SIDE 4

//#define A_LOW_SIDE 11 TODO: ursprüngliche zuweisung für das finale projekt
//#define B_LOW_SIDE 17
// #define C_LOW_SIDE 14

extern uint16_t * neutral_phase_val_ptr, phase_a_val, phase_b_val, phase_c_val;
extern volatile uint8_t zero_crossings;

#define ACTIVATE_C_LOW_SIDE DL_GPIO_setPins(GPIO_GRP_0_PORT, 1<<C_LOW_SIDE)
#define ACTIVATE_B_LOW_SIDE DL_GPIO_setPins(GPIO_GRP_0_PORT, 1<<B_LOW_SIDE)
#define ACTIVATE_A_LOW_SIDE DL_GPIO_setPins(GPIO_GRP_0_PORT, 1<<A_LOW_SIDE)

#define DEACTIVATE_C_LOW_SIDE DL_GPIO_clearPins(GPIO_GRP_0_PORT, 1<<C_LOW_SIDE)
#define DEACTIVATE_B_LOW_SIDE DL_GPIO_clearPins(GPIO_GRP_0_PORT, 1<<B_LOW_SIDE)
#define DEACTIVATE_A_LOW_SIDE DL_GPIO_clearPins(GPIO_GRP_0_PORT, 1<<A_LOW_SIDE)

uint8_t ccp_output = 0x00;

#define C_CHANNEL 2   //3 in final version
#define B_CHANNEL 1
#define A_CHANNEL 0

#define DEACTIVATE_C_HIGH_SIDE DL_Timer_setCCPOutputDisabledAdv(PWM_0_INST, (ccp_output |= (1<<C_CHANNEL)))
#define DEACTIVATE_B_HIGH_SIDE DL_Timer_setCCPOutputDisabledAdv(PWM_0_INST, (ccp_output |= (1<<B_CHANNEL)))
#define DEACTIVATE_A_HIGH_SIDE DL_Timer_setCCPOutputDisabledAdv(PWM_0_INST, (ccp_output |= (1<<A_CHANNEL)))
#define ACTIVATE_C_HIGH_SIDE DL_Timer_setCCPOutputDisabledAdv(PWM_0_INST, (ccp_output &= ~(1<<C_CHANNEL)))
#define ACTIVATE_B_HIGH_SIDE DL_Timer_setCCPOutputDisabledAdv(PWM_0_INST, (ccp_output &= ~(1<<B_CHANNEL)))
#define ACTIVATE_A_HIGH_SIDE DL_Timer_setCCPOutputDisabledAdv(PWM_0_INST, (ccp_output &= ~(1<<A_CHANNEL)))
#define ACTIVATE_HIGH_HALF_BRIDGE DL_GPIO_setPins(GPIO_GRP_0_PORT, 1<<6)
#define DEACTIVATE_HIGH_HALF_BRIDGE DL_GPIO_clearPins(GPIO_GRP_0_PORT, 1<<6)
#define ACTIVATE_LOW_HALF_BRIDGE DL_GPIO_setPins(GPIO_GRP_0_PORT, 1<<6)
#define DEACTIVATE_LOW_HALF_BRIDGE DL_GPIO_clearPins(GPIO_GRP_0_PORT, 1<<6)

#define SWITCH_PHASE_A DL_GPIO_togglePins(GPIO_GRP_0_PORT, 1<<C_CHANNEL)
#define SWITCH_PHASE_B DL_GPIO_togglePins(GPIO_GRP_0_PORT, 1<<11)
#define SWITCH_PHASE_C DL_GPIO_togglePins(GPIO_GRP_0_PORT, 1<<24)

typedef enum phase {
	no_phase,
	ah_bl_cn,
	ah_bn_cl,
	an_bh_cl,
	al_bh_cn,
	al_bn_ch,
	an_bl_ch
} phase;

volatile phase current_phase = no_phase;

volatile uint8_t commutation_step = 0, motor_speed;
volatile uint8_t curr_commutation_step;

unsigned int i;

void setup() {
}

void SET_PWM_DUTY(uint8_t duty){
}

uint8_t is_hb_a_low() {
    return !DL_GPIO_readPins(GPIO_GRP_0_PORT, 1<<A_LOW_SIDE);
	//return PORTD & (1<<PIND7);
}

void set_hb_a_high() {
	if (is_hb_a_low()) {DEACTIVATE_A_LOW_SIDE;for (volatile uint8_t i = 0; i<50;++i);}//prevent shoot-through
	ACTIVATE_A_HIGH_SIDE;
    hb_a_high_stat = HB_HIGH;
}

uint8_t is_hb_a_high() {
	return 1;//DL_GPIO_readPins(GPIO_GRP_0_PORT, 1<<9);
}

void set_hb_a_low() {
	if (is_hb_a_high()) {DEACTIVATE_A_HIGH_SIDE; for (volatile uint8_t i = 0; i<50;++i);}//prevent shoot-through
	ACTIVATE_A_LOW_SIDE;
    hb_a_high_stat = HB_LOW;
}

void set_hb_a_neutral() {
	DEACTIVATE_A_HIGH_SIDE;
	DEACTIVATE_A_LOW_SIDE;
    hb_a_high_stat = HB_LOW;
}

uint8_t is_hb_b_low() {
    return !DL_GPIO_readPins(GPIO_GRP_0_PORT, 1<<B_LOW_SIDE);
	//return PORTB & (1<<PINB6);
}

void set_hb_b_high() {
	if (is_hb_b_low()) {DEACTIVATE_B_LOW_SIDE;for (volatile uint8_t i = 0; i<50;++i);}//prevent shoot-through
	ACTIVATE_B_HIGH_SIDE;
    hb_b_high_stat = HB_HIGH;
}

uint8_t is_hb_b_high() {
    return 1; //!DL_GPIO_readPins(GPIO_GRP_0_PORT, 1<<10);
	//return PORTB & (1<<PINB7);
}


void set_hb_b_low() {
	if (is_hb_b_high()) {DEACTIVATE_B_HIGH_SIDE; for (volatile uint8_t i = 0; i<50;++i);}//prevent shoot-through
	ACTIVATE_B_LOW_SIDE;
    hb_b_high_stat = HB_LOW;
}

void set_hb_b_neutral() {
	DEACTIVATE_B_HIGH_SIDE;
	DEACTIVATE_B_LOW_SIDE;
    hb_b_high_stat = HB_LOW;
}

uint8_t is_hb_c_low() {
    return !DL_GPIO_readPins(GPIO_GRP_0_PORT, 1<<C_LOW_SIDE);
	//return PORTD & (1<<PIND0);
}

void set_hb_c_high() {
	if (is_hb_c_low()) {DEACTIVATE_C_LOW_SIDE; for (volatile uint8_t i = 0; i<50;++i);}//prevent shoot-through
	ACTIVATE_C_HIGH_SIDE;
    hb_c_high_stat = HB_HIGH;
}

uint8_t is_hb_c_high() {
    return 1;
	//return PORTD & (1<<PIND1);
}

void set_hb_c_low() {
	if (is_hb_c_high()) {DEACTIVATE_C_HIGH_SIDE; for (volatile uint8_t i = 0; i<50;++i);}//prevent shoot-through
	ACTIVATE_C_LOW_SIDE;
    hb_c_high_stat = HB_LOW;
}

void set_hb_c_neutral() {
	DEACTIVATE_C_HIGH_SIDE;
	DEACTIVATE_C_LOW_SIDE;
    hb_c_high_stat = HB_LOW;
}


void BEMF_A_RISING(){
}

void BEMF_A_FALLING(){

}

void BEMF_B_RISING(){
}

void BEMF_B_FALLING(){
}

void BEMF_C_RISING(){
}

void BEMF_C_FALLING(){
}

void AH_BL_CN(){//this shall be the funktion to start the cycle with
	ACTIVATE_B_LOW_SIDE;
	ACTIVATE_A_HIGH_SIDE;
	DEACTIVATE_C_HIGH_SIDE;
	DEACTIVATE_C_LOW_SIDE;
	//TCCR1A =  0;            // Turn pin 11 (OC2A) PWM ON (pin 9 & pin 10 OFF)
	//TCCR2A =  0x81;         //
}

void AH_CL_BN(){
	ACTIVATE_C_LOW_SIDE;
	DEACTIVATE_B_LOW_SIDE;
	//TCCR1A =  0;            // Turn pin 11 (OC2A) PWM ON (pin 9 & pin 10 OFF)
	//TCCR2A =  0x81;         //t
}

void BH_CL_AN(){
	ACTIVATE_B_HIGH_SIDE;
	DEACTIVATE_A_HIGH_SIDE;
	//TCCR2A =  0;            // Turn pin 10 (OC1B) PWM ON (pin 9 & pin 11 OFF)
	///TCCR1A =  0x21;         //
}
void BH_AL_CN(){
	ACTIVATE_A_LOW_SIDE;
	DEACTIVATE_B_LOW_SIDE;
	//TCCR2A =  0;            // Turn pin 10 (OC1B) PWM ON (pin 9 & pin 11 OFF)
	//TCCR1A =  0x21;         //
}

void CH_AL_BN(){
	ACTIVATE_C_HIGH_SIDE;
	DEACTIVATE_B_LOW_SIDE;
	//TCCR2A =  0;            // Turn pin 9 (OC1A) PWM ON (pin 10 & pin 11 OFF)
	//TCCR1A =  0x81;         //
}

void CH_BL_AN(){
	ACTIVATE_B_LOW_SIDE;
	DEACTIVATE_A_LOW_SIDE;
	//TCCR2A =  0;            // Turn pin 9 (OC1A) PWM ON (pin 10 & pin 11 OFF)
	//TCCR1A =  0x81;         //
}

void bldc_move_star_winding_1(){        // BLDC motor commutation function
	switch(commutation_step){
		case 0:
		AH_BL_CN();
		BEMF_C_RISING();
		break;
		case 1:
		AH_CL_BN();
		BEMF_B_FALLING();
		break;
		case 2:
		BH_CL_AN();
		BEMF_A_RISING();
		break;
		case 3:
		BH_AL_CN();
		BEMF_C_FALLING();
		break;
		case 4:
		CH_AL_BN();
		BEMF_B_RISING();
		break;
		case 5:
		CH_BL_AN();
		BEMF_A_FALLING();
		break;
	}
}


void bldc_move_star_winding(uint16_t * neutral_phase_ptr, uint16_t* phase_a, uint16_t* phase_b, uint16_t* phase_c) {        // BLDC motor commutation function
    //DL_GPIO_togglePins(GPIO_GRP_0_PORT, 1<<17);
	switch(commutation_step){
		case 0:
		//AH_BL_CN();
		current_phase = ah_bl_cn;
		set_hb_a_high();
		//c_high = b_high = 0;
		set_hb_b_low();
		set_hb_c_neutral();
		neutral_phase_ptr = phase_c;		
		//BEMF_C_RISING();
		break;
		case 1:
		//AH_CL_BN();
		current_phase = ah_bn_cl;
		set_hb_a_high();
		set_hb_b_neutral();
		set_hb_c_low();
		neutral_phase_ptr = phase_b;
		//BEMF_B_FALLING();
		break;
		case 2:
		//BH_CL_AN();
		current_phase = an_bh_cl;
		set_hb_a_neutral();
		set_hb_b_high();
		set_hb_c_low();
		neutral_phase_ptr = phase_a;
		//BEMF_A_RISING();
		break;
		case 3:
		//BH_AL_CN();
		current_phase = al_bh_cn;
		set_hb_a_low();
		set_hb_b_high();
		set_hb_c_neutral();
		neutral_phase_ptr = phase_c;
		//BEMF_C_FALLING();
		break;
		case 4:
		//CH_AL_BN();
		current_phase = al_bn_ch;
		set_hb_a_low();
		set_hb_b_neutral();
		set_hb_c_high();
		neutral_phase_ptr = phase_b;
		//BEMF_B_RISING();
		break;
		case 5:
		//CH_BL_AN();
		current_phase = an_bl_ch;
		set_hb_a_neutral();
		set_hb_b_low();
		set_hb_c_high();
		neutral_phase_ptr = phase_a;
		//BEMF_A_FALLING();
		break;
	}
	curr_commutation_step = commutation_step;
}

  /*bldc_move_star_winding(); //TODO: das noch durch die entsprechende delta winding funktion ersetzen, wenn der motor entsprechend konfiguriert ist
  ++bldc_step;
  bldc_step %= 6;*///TODO: ausgeklammert zum debuggen, alles in diesem multicomment dem code wieder hinzufügen
//}

void activate_A() {
	//PORTB |= (1<<PINB3);
}

void deactivate_A() {
	//PORTB &= ~(1<<PINB3);
}

//let the bootstrap capacitors charge
void set_all_hb_low() {
	set_hb_a_low();
	set_hb_b_low();
	set_hb_c_low();
	for (volatile uint32_t i = 0; i<10000; ++i);
	set_hb_a_neutral();
	set_hb_b_neutral();
	set_hb_c_neutral();
}

void open_loop(uint32_t runs, uint32_t wait) {
	start_up(wait);
	open_loop_run(runs, wait);
}

void open_loop_run(uint32_t runs, uint32_t wait) {
	uint16_t outer_idx, inner_idx;
	outer_idx = inner_idx = 0;
	while(outer_idx < runs) {
		inner_idx = 0;
		while (inner_idx<runs) {
			//if (inner_idx == (runs - 1)) zero_crossings = 0;//der open-loop run ist zuende und die bemf-steuerung folgt
			//alle zero crossings bisjetzt zählen nicht
			//if ((inner_idx == (runs-1)) && (outer_idx == (runs-1))) DL_GPIO_togglePins(GPIO_GRP_0_PORT, 1<<25);
			bldc_move_star_winding(neutral_phase_val_ptr, &phase_a_val, &phase_b_val, &phase_c_val);
			commutation_step++;
			commutation_step %= 6;
			for (volatile uint32_t i = 0; i<wait; ++i);
			//_delay_ms(20);
			++inner_idx;
		}
		++outer_idx;
	}
	//set_hb_a_neutral();
	//set_hb_b_neutral();
	//set_hb_c_neutral();
	current_phase = no_phase;
}

//TODO: Open-Loop mode noch hier reinbauen, für den motorstart wenn BEMF feedback nicht vorhanden ist.
//TODO: noch herausfinden was der beste zeitpunkt ist die phasen umzuschalten.

void start_up(volatile uint32_t wait) {
  volatile uint32_t wait_time = 10*5500;
  volatile float wait_time_f = wait_time;
  // Motor start
  while(wait_time>wait) {
	//_delay_ms(1);
    bldc_move_star_winding(neutral_phase_val_ptr, &phase_a_val, &phase_b_val, &phase_c_val);
	//_delay_ms(1);
	wait_time_f *= 0.995f;
	wait_time = (uint32_t) wait_time_f;
	//wait_time -= wait;
	for (volatile uint32_t j = 0; j<wait_time; ++j);
    commutation_step++;
    commutation_step %= 6;
  }
  //ACSR |= 0x08;                    // Enable analog comparator interrupt
  /*while(1) {
    while(!(digitalRead(SPEED_UP)) && motor_speed < PWM_MAX_DUTY){
      motor_speed++;
      SET_PWM_DUTY(motor_speed);
      _delay_ms(100);
    }
    while(!(digitalRead(SPEED_DOWN)) && motor_speed > PWM_MIN_DUTY){
      motor_speed--;
      SET_PWM_DUTY(motor_speed);
      _delay_ms(100);
    }
  }*/
}
