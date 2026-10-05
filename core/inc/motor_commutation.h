

#ifndef MOTOR_COMMUTATION_H
#define MOTOR_COMMUTATION_H

#include <stdint.h>

void bldc_move_star_winding(uint16_t*, uint16_t *,uint16_t*, uint16_t *);
void set_hb_a_high();
//c_high = b_high = 0;
void set_hb_b_low();
void set_hb_c_low();
void set_hb_c_neutral();
void set_hb_c_high();
void set_hb_b_neutral();
void set_hb_b_high();
void set_hb_a_low();
void set_hb_a_neutral();
void set_hb_a_high();
void start_up(uint32_t wait);
void open_loop_run(uint32_t, uint32_t);
void open_loop(uint32_t, uint32_t);
void set_all_hb_low();
void set_hb_b_low();

#endif
