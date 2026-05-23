/*
 * testing.c
 *
 *  Created on: 2026-05-23
 *      Author: Darija
 */
#include "testing.h"
#include "main.h"
#include "sh1106_driver.h"
#include <stdio.h>
#include <stdint.h>

float test_freqs[] = {100.5f, 1000.01f, 10000.2f, 100000.0f};
float target_freq = 0;
uint32_t sample_count = 0;

void DaznisTest(float target_hz)
{
    target_freq  = target_hz;
    sample_count = 0;
    measured_freq = 0;
    char msg[32];
    //Pradzios mygtukas
    SH1106_Fill(SH1106_COLOR_BLACK);
    sprintf(msg, "Testuojame %.0f Hz", target_hz);
    SH1106_GotoXY(5, 10);
    SH1106_Puts(msg, &Font_7x10, SH1106_COLOR_WHITE);
    SH1106_GotoXY(5, 30);
    SH1106_Puts("Pradedama...", &Font_7x10, SH1106_COLOR_WHITE);
    SH1106_UpdateScreen();
    //Duomenu nusistovejimas
    for(int w = 0; w < 3; w++)
    	SignaloApdorojimas();
    //Kartojasi 20 imciu
    while(sample_count < 20)
    {
        SignaloApdorojimas();
        if(timer_done)
        {
        	timer_done = 0;
        	if(measured_freq >= 1.0f)
        	{
        	    float f = measured_freq;
        	    sample_count++;
        	    //Siuntimas per UART duomenu rinkimui
        	    char uart_msg[128];
        	    sprintf(uart_msg, "Test: %.2f Hz, Imtis: %lu, Mat: %.3f Hz\r\n", target_hz, sample_count, f);
        	    HAL_UART_Transmit(&huart2, (uint8_t*)uart_msg, strlen(uart_msg), 100);
        	}
        	//Ekrano atnaujinimas
            SH1106_Fill(SH1106_COLOR_BLACK);
            sprintf(msg, "Test %.0f Hz", target_hz);
            SH1106_GotoXY(5, 5);
            SH1106_Puts(msg, &Font_7x10, SH1106_COLOR_WHITE);
            sprintf(msg, "Freq: %.1f Hz", measured_freq);
            SH1106_GotoXY(5, 20);
            SH1106_Puts(msg, &Font_7x10, SH1106_COLOR_WHITE);
            sprintf(msg, "Imtis: %lu/20", sample_count);
            SH1106_GotoXY(5, 35);
            SH1106_Puts(msg, &Font_7x10, SH1106_COLOR_WHITE);
            SH1106_UpdateScreen();
        }
    }
}

void RunFullTest(void)
{
    DaznisTest(test_freqs[test_freq_idx]);

    //Pranesame kai baigiame matuoti visas imtis
    SH1106_Fill(SH1106_COLOR_BLACK);
    SH1106_GotoXY(10, 25);
    SH1106_Puts("Testavimas baigtas!", &Font_7x10, SH1106_COLOR_WHITE);
    SH1106_UpdateScreen();
    uint8_t sec = 0;
    while(sec < 3)
    {
        if(timer_done)
        {
        	timer_done = 0;
            sec++;
        }
    }
}

