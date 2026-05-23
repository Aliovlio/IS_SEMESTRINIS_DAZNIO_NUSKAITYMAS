/*
 * testing.h
 *
 *  Created on: 2026-05-23
 *      Author: Darija
 */

#ifndef INC_TESTING_H_
#define INC_TESTING_H_

#include <stdint.h>
#include <string.h>

extern float test_freqs[];
extern float target_freq;
extern uint32_t sample_count;

void DaznisTest(float target_hz);
void RunFullTest(void);

#endif /* INC_TESTING_H_ */
