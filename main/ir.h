#pragma once

#include <stdint.h>

struct Remote{
    char* ir_tv_1; 
    char* ir_tv_2;
    char* ir_tv_3;
    char* ir_tv_4;
    char* ir_tv_5;
    char* ir_tv_6;
    char* ir_tv_7;
    char* ir_tv_8;
    char* ir_tv_9;
    char* ir_tv_0;
    char* ir_tv_power; // 0
    char* ir_tv_volume_up; // 1
    char* ir_tv_volume_down; //2 
    char* ir_tv_mute; // 3
    char* ir_tv_next_channel; // 4
    char* ir_tv_prev_channel; //5
    char* ir_tv_menu; // 6
    char* ir_tv_exit; // 7
    char* ir_tv_right; //8
    char* ir_tv_left; // 9
    char* ir_tv_up; // 10
    char* ir_tv_down; // 11
    char* ir_tv_enter; // 12
    char* ir_tv_home; // 13
    char* ir_tv_picture; // 14
} remote_comand;


typedef enum {
    ir_hdmi_switch_input_1 = 0x02,
    ir_hdmi_switch_input_2 = 0x04,
    ir_hdmi_switch_input_3 = 0x06,
    ir_hdmi_switch_next_input = 0x08,
} ir_hdmi_switch_command_t;

//void ir_tv_send(ir_tv_command_t command);
ir_decoder_t *ir_tv_make_decoder();

void ir_hdmi_switch_send(ir_hdmi_switch_command_t command);
