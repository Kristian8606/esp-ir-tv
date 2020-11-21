#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <string.h>
#include <etstimer.h>
#include <esplibs/libmain.h>
#include <sysparam.h>
#include "ir_code.h"
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <ir/ir.h>
#include "ir.h"
#include <ir/raw.h>
#include "button.h"
#include <lwip/netdb.h>
#include <ping.h>

#define LED_GPIO 2
#define IR_RX_GPIO 12
#define POWERED_ON_GPIO 0
#define IR_TX_GPIO 14
bool ir_tx_inv = false;
int is_power = 0, temp = 0;
bool is_config_mode = false, change_timer_bool = false, ping_bool = false, ping_flag = false;
TimerHandle_t xPing_Timer = NULL;
uint32_t targrt_time = 0;
char ch_command_name[14][15] = {"power", "volume_up", "volume_down",
								"back","right","left",
								"up","down","select",
								"play_pause", "information", "hdmi_1",
								"hdmi_2","hdmi_3"};
 /*
	"power", 	0
	"volume_up", 	1
	"volume_down",	2
	"back",		3
	"right",	4
	"left",		5
	"up",		6	
	"down",		7	
	"select",	8
	"play_pause",	9 
	"information", 	10
	"hdmi_1"	11
	"hdmi_2"	12
	"hdmi_3"	13


*/

 char ch_command [14][600];

void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void led_init() {
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false);
}


void tv_identify_task(void *_args) {
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            led_write(true);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            led_write(false);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    led_write(false);

    vTaskDelete(NULL);
}

void tv_identify(homekit_value_t _value) {
    xTaskCreate(tv_identify_task, "TV identify", 128, NULL, 2, NULL);
}


void ir_read_remote_task(void* pvParameters) {

int index_key = (int)pvParameters;
printf("%d Dump task Run\n",index_key);
 is_config_mode = true;
   ir_rx_init(IR_RX_GPIO, 1024);
    ir_decoder_t *raw_decoder = ir_raw_make_decoder();
	uint8_t count = 0;
    printf("Read IR commands\n");

     uint16_t buffer_size = sizeof(int16_t) * 1024;
    int16_t *buffer = malloc(buffer_size);
    while (1) {
        // vTaskDelay(50 / portTICK_PERIOD_MS);
         int size = ir_recv(raw_decoder, 0, buffer, buffer_size);
        // vTaskDelay(50 / portTICK_PERIOD_MS);
        if (size <= 0)
            continue;
	
	count++;
        printf("Decoded packet (size = %d):\n", size);
        for (int i=0; i < size; i++) {
            printf("%5d ", buffer[i]);
            if (i % 16 == 15)
                printf("\n");
        }
          char temp[1024] = "";
                printf("\nHAA RAW Format\n");
                for (int i = 0; i < size; i++) {
                    char haa_code[] = "00";
                   // char* a = malloc(2*sizeof(char));
                    haa_code[0] = baseRaw_dic[(buffer[i] / IR_CODE_SCALE) / IR_CODE_LEN];
                    haa_code[1] = baseRaw_dic[(buffer[i] / IR_CODE_SCALE) % IR_CODE_LEN];
                    strcat(temp, haa_code);
                    
                    
                }
             //    vTaskDelay(100 / portTICK_PERIOD_MS);
               // char* raw_code = malloc(strlen(temp) + 1);
                //strcpy(raw_code, temp);
                strcpy(ch_command[index_key], temp);
                memset(&temp[0], '\0',sizeof(temp));
				printf("Comand name %s %s\n", ch_command_name[index_key], ch_command[index_key]);
                //char* index_i = (char*)index_key;
                sysparam_set_string(ch_command_name[index_key], ch_command[index_key]);
                
				// vTaskDelay(50 / portTICK_PERIOD_MS);
               

        if (size % 16)
            printf("\n");
            
        	if(count >= 5){
      		  printf("Settings Mode Stop!\n");
      		  is_config_mode = false;
      		  free(buffer);
      		  for (int i=0; i<3; i++) {
      		  	led_write(true);
      		  	vTaskDelay(100 / portTICK_PERIOD_MS);
      		 	 led_write(false);
      		 	 vTaskDelay(100 / portTICK_PERIOD_MS);
       		  }
        	vTaskDelete(NULL);
        }
       // vTaskDelete(NULL);
    }
}

void decode_and_send(void* pvParameters){

int index_key = (int)pvParameters;
char* raw_code = malloc(strlen(ch_command[index_key]) * sizeof(char));
  strcpy(raw_code, ch_command[index_key]);
  
  printf("Command Send %s\n", raw_code);

while(1){
    uint16_t* ir_code = NULL;
    uint16_t ir_code_len = 0;
	const uint16_t json_ir_code_len = strlen(raw_code);
                ir_code_len = json_ir_code_len >> 1;
                
                ir_code = malloc(sizeof(uint16_t) * ir_code_len);
                
                printf("IR packet (%i)\n", ir_code_len);

                uint16_t index, packet;
                for (uint16_t i = 0; i < ir_code_len; i++) {
                    index = i << 1;
                    char* found = strchr(baseRaw_dic, raw_code[index]);
                    packet = (found - baseRaw_dic) * IR_CODE_LEN * IR_CODE_SCALE;
                    
                    found = strchr(baseRaw_dic, raw_code[index + 1]);
                    packet += (found - baseRaw_dic) * IR_CODE_SCALE;

                    ir_code[i] = packet;

                   // printf("%s%5d ", i & 1 ? "-" : "+", packet);
                    if (i % 16 == 15) {
                        printf(" ");
                    }
                }
             //   printf("\n");
               //////////// 
                  uint32_t start;
            const bool ir_true = true ^ ir_tx_inv;
            const bool ir_false = false ^ ir_tx_inv;

                
                taskENTER_CRITICAL();
                
                for (uint16_t i = 0; i < ir_code_len; i++) {
                    if (ir_code[i] > 0) {
                        if (i & 1) {    // Space
                            gpio_write(IR_TX_GPIO, ir_false);
                            sdk_os_delay_us(ir_code[i]);
                        } else {        // Mark
                            start = sdk_system_get_time();
                            while ((sdk_system_get_time() - start) < ir_code[i]) {
                                gpio_write(IR_TX_GPIO, ir_true);
                                sdk_os_delay_us(13);
                                gpio_write(IR_TX_GPIO, ir_false);
                                sdk_os_delay_us(13);
                            }
                        }
                    }
                }
                
                gpio_write(IR_TX_GPIO, ir_false);
				//free(ir_code);
                taskEXIT_CRITICAL();
                
                printf("IR %i sent\n", ir_code_len);
                //vTaskDelay(50 / portTICK_PERIOD_MS);

         if (ir_code) {
                free(ir_code);
                
         }
         free(raw_code);
         vTaskDelete(NULL);
      //vTaskDelay(1000 / portTICK_PERIOD_MS);
 }
      // vTaskDelete(NULL);
    
    
}

void send_command(int key){

 
   if(xTaskGetTickCountFromISR() - targrt_time <= 10){
   	printf("%d - %d\n", xTaskGetTickCountFromISR(), targrt_time);
   		return;
   	}
   	targrt_time = xTaskGetTickCountFromISR();
   	
	if(is_config_mode){
		
		is_config_mode = false;
		if(xTaskCreate(ir_read_remote_task, "ir read remote task", 1024, (void*) key, tskIDLE_PRIORITY, NULL) != pdPASS){
		    printf("Read Remote\n");
		    printf("Press Key %d\n", key); 

		}
	}else{
	is_config_mode = false;
		if(xTaskCreate(decode_and_send, "decode_and_send", 512,(void*) key, 0, NULL) != pdPASS) {
            printf("Creating decode_and_send Task");
        }
	}
}

void on_input_configured_name(homekit_characteristic_t *ch, homekit_value_t value, void *arg);


void on_configured_name(homekit_characteristic_t *ch, homekit_value_t value, void *arg) {
    sysparam_set_string("tv_name", value.string_value);
}

void on_active(homekit_characteristic_t *ch, homekit_value_t value, void *arg) {
		if(value.int_value != is_power){
			is_power = value.int_value;
		
			if(ping_flag){
				ping_flag = false;
				return;
			}
			
			if(value.int_value){
			  printf("ON\n");

			}else{
			printf("OFF\n");
			}
			
			
			xTimerChangePeriod(xPing_Timer, 20000/portTICK_PERIOD_MS, 100 );
			change_timer_bool = true;
			
			send_command(0);
		}
  
}
/*
void on_pwr_mode(homekit_characteristic_t *ch, homekit_value_t value, void *arg) {
		//send_command(0);
		printf("POWER_MODE_SELECTION  %d\n", value.int_value);
  
}
homekit_characteristic_t tv_power = HOMEKIT_CHARACTERISTIC_(POWER_MODE_SELECTION, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_pwr_mode));
*/

void on_active_identifier(homekit_characteristic_t *ch, homekit_value_t value, void *arg) {
    switch (value.int_value) {
    case 1:
       // ir_hdmi_switch_send(ir_hdmi_switch_input_1);
		send_command(11);
        break;
    case 2:
       // ir_hdmi_switch_send(ir_hdmi_switch_input_2);
       send_command(12);
        break;
    case 3:
       // ir_hdmi_switch_send(ir_hdmi_switch_input_3);
       send_command(13);
        break;
    default:
        printf("Unknown active identifier: %d", value.int_value);
    }
}

void on_remote_key(homekit_characteristic_t *ch, homekit_value_t value, void *arg) {
    switch (value.int_value) {
    case HOMEKIT_REMOTE_KEY_ARROW_UP:
		send_command(6);
        break;
    case HOMEKIT_REMOTE_KEY_ARROW_DOWN:
		send_command(7);
        break;
    case HOMEKIT_REMOTE_KEY_ARROW_LEFT:
		send_command(5);
        break;
    case HOMEKIT_REMOTE_KEY_ARROW_RIGHT:
       send_command(4);
        break;
    case HOMEKIT_REMOTE_KEY_SELECT:
		send_command(8);
		break;
    case HOMEKIT_REMOTE_KEY_BACK:
		send_command(3);
        break;
    case HOMEKIT_REMOTE_KEY_PLAY_PAUSE:
		send_command(9);
        break;
    case HOMEKIT_REMOTE_KEY_INFORMATION:
		send_command(10);
        break;
    default:
        printf("Unsupported remote key code: %d", value.int_value);
    }
}


void on_mute(homekit_characteristic_t *ch, homekit_value_t value, void *arg) {
        printf("Mute button: %d", value.int_value);
}

void on_volume_selector(homekit_characteristic_t *ch, homekit_value_t value, void *arg) {
 if(value.int_value == 0){
 	send_command(1);
 	//printf("%d\n", value.int_value);
 }else{
    send_command(2);
   // printf("%d\n", value.int_value);
 }
   
}


homekit_characteristic_t input_source1_name = HOMEKIT_CHARACTERISTIC_(
    CONFIGURED_NAME, "HDMI 1",
    .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_input_configured_name)
);

homekit_service_t input_source1 =
    HOMEKIT_SERVICE_(INPUT_SOURCE, .characteristics=(homekit_characteristic_t*[]){
        HOMEKIT_CHARACTERISTIC(NAME, "hdmi1"),
        HOMEKIT_CHARACTERISTIC(IDENTIFIER, 1),
        &input_source1_name,
        HOMEKIT_CHARACTERISTIC(INPUT_SOURCE_TYPE, HOMEKIT_INPUT_SOURCE_TYPE_HDMI),
        HOMEKIT_CHARACTERISTIC(IS_CONFIGURED, true),
        HOMEKIT_CHARACTERISTIC(CURRENT_VISIBILITY_STATE, HOMEKIT_CURRENT_VISIBILITY_STATE_SHOWN),
        NULL
    });

homekit_characteristic_t input_source2_name = HOMEKIT_CHARACTERISTIC_(
    CONFIGURED_NAME, "HDMI 2",
    .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_input_configured_name)
);

homekit_service_t input_source2 =
    HOMEKIT_SERVICE_(INPUT_SOURCE, .characteristics=(homekit_characteristic_t*[]){
        HOMEKIT_CHARACTERISTIC(NAME, "hdmi2"),
        HOMEKIT_CHARACTERISTIC(IDENTIFIER, 2),
        &input_source2_name,
        HOMEKIT_CHARACTERISTIC(INPUT_SOURCE_TYPE, HOMEKIT_INPUT_SOURCE_TYPE_HDMI),
        HOMEKIT_CHARACTERISTIC(IS_CONFIGURED, true),
        HOMEKIT_CHARACTERISTIC(CURRENT_VISIBILITY_STATE, HOMEKIT_CURRENT_VISIBILITY_STATE_SHOWN),
        NULL
    });

homekit_characteristic_t input_source3_name = HOMEKIT_CHARACTERISTIC_(
    CONFIGURED_NAME, "HDMI 3",
    .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_input_configured_name)
);

homekit_service_t input_source3 =
    HOMEKIT_SERVICE_(INPUT_SOURCE, .characteristics=(homekit_characteristic_t*[]){
        HOMEKIT_CHARACTERISTIC(NAME, "hdmi3"),
        HOMEKIT_CHARACTERISTIC(IDENTIFIER, 3),
        &input_source3_name,
        HOMEKIT_CHARACTERISTIC(INPUT_SOURCE_TYPE, HOMEKIT_INPUT_SOURCE_TYPE_HDMI),
        HOMEKIT_CHARACTERISTIC(IS_CONFIGURED, true),
        HOMEKIT_CHARACTERISTIC(CURRENT_VISIBILITY_STATE, HOMEKIT_CURRENT_VISIBILITY_STATE_SHOWN),
        NULL
    });

homekit_characteristic_t tv_name = HOMEKIT_CHARACTERISTIC_(
    CONFIGURED_NAME, "My TV",
    .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_configured_name)
);

homekit_characteristic_t tv_active = HOMEKIT_CHARACTERISTIC_(ACTIVE, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_active));

void powered_on_notify(int val) {
   	tv_active.value = HOMEKIT_UINT8(val);
    ping_flag = true;
    homekit_characteristic_notify(&tv_active, tv_active.value);
    printf("DEBUG Notify\n");
}

void print_ip_info() {
    struct ip_info info;
    sdk_wifi_get_ip_info(STATION_IF, &info);
    printf("Got DHCP IP: %s\n", ipaddr_ntoa(&info.ip));
    printf("Got GW IP: %s\n", ipaddr_ntoa(&info.gw));
    printf("Got Netmask: %s\n", ipaddr_ntoa(&info.netmask));
}

ip_addr_t get_gw_ip() {
    struct ip_info info;
    sdk_wifi_get_ip_info(STATION_IF, &info);
    ip_addr_t gw_ip;
    gw_ip.addr = info.gw.addr;
    return gw_ip;
}

int do_dns_resolve(const char *hostname, ip_addr_t *target_ip) {
    const struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype =
            SOCK_RAW };
    struct addrinfo *res;

    int err = getaddrinfo(hostname, NULL, &hints, &res);

    if (err != 0 || res == NULL) {
        printf("DNS lookup failed err=%d res=%p\r\n", err, res);
        if (res)
            freeaddrinfo(res);
        return -1;
    } else {
        struct sockaddr *sa = res->ai_addr;
        if (sa->sa_family == AF_INET) {
            struct in_addr ipv4_inaddr = ((struct sockaddr_in*) sa)->sin_addr;
            memcpy(target_ip, &ipv4_inaddr, sizeof(*target_ip));
            //printf("DNS lookup succeeded. IP=%s\r\n", inet_ntoa(target_ip));
        }
#if LWIP_IPV6
		if (sa->sa_family == AF_INET6) {
			struct in_addr ipv6_inaddr = ((struct sockaddr_in6 *)sa)->sin6_addr;
			memcpy(target_ip, &ipv6_inaddr, sizeof(*target_ip));
			//printf("DNS lookup succeeded. IP=%s\r\n", inet6_ntoa(target_ip));
		}
#endif
        freeaddrinfo(res);
        return 0;
    }
}

void pingTask() {
    ping_result_t res;
	if(change_timer_bool){
		change_timer_bool = false;
		xTimerChangePeriod(xPing_Timer, 5000/portTICK_PERIOD_MS, 100 );
	}
    //get GW IP
    ip_addr_t to_ping = get_gw_ip();
    //or use hostname
    const char *hostname = "192.168.0.122";
    //set to true if you want to ping the gateway.
    bool ping_gateway = false;

    if (!ping_gateway) {
        if (do_dns_resolve(hostname, &to_ping) < 0) {
          //  printf("DNS resolve of \"%s\" failed\n", hostname);
           // vTaskDelete(NULL);
        }
       // printf("Pinging hostname %s at IP %s\n", hostname,ipaddr_ntoa(&to_ping));
    } else {
       // printf("Pinging gateway at IP %s\n", ipaddr_ntoa(&to_ping));
    }

        ping_ip(to_ping, &res);

        if (res.result_code == PING_RES_ECHO_REPLY) {
            printf("good ping from %s %u ms\n", ipaddr_ntoa(&res.response_ip), res.response_time_ms);
          ping_bool = true;
        } else {
            printf("bad ping err %d\n", res.result_code);
          ping_bool = false;

        }
        
        homekit_value_t active_state = tv_active.value;
      //  printf("%i\n", active_state.int_value);
        
        if(ping_bool == true && active_state.int_value == 0){
        	powered_on_notify(1);
        
        }else if(ping_bool == false && active_state.int_value == 1){
        	powered_on_notify(0);
        }
      
}
/////
homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Remote Control");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_television, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
			&name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "HaPK"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "1"),
            HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266-1"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, tv_identify),
            NULL
        }),
        HOMEKIT_SERVICE(TELEVISION, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Television"),
            &tv_active,
            HOMEKIT_CHARACTERISTIC(
                ACTIVE_IDENTIFIER, 1,
                .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_active_identifier)
            ),
            &tv_name,
            HOMEKIT_CHARACTERISTIC(
                SLEEP_DISCOVERY_MODE,
                HOMEKIT_SLEEP_DISCOVERY_MODE_ALWAYS_DISCOVERABLE
            ),
            HOMEKIT_CHARACTERISTIC(
                REMOTE_KEY, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_remote_key)
            ),
            HOMEKIT_CHARACTERISTIC(PICTURE_MODE, HOMEKIT_PICTURE_MODE_STANDARD),
            HOMEKIT_CHARACTERISTIC(POWER_MODE_SELECTION),
            NULL
            
        }, .linked=(homekit_service_t*[]) {
            &input_source1,
            &input_source2,
            &input_source3,
            NULL
        }),
        &input_source1,
        &input_source2,
        &input_source3,
        HOMEKIT_SERVICE(TELEVISION_SPEAKER, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(MUTE, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_mute)),
            HOMEKIT_CHARACTERISTIC(ACTIVE, true),
            HOMEKIT_CHARACTERISTIC(VOLUME_CONTROL_TYPE, HOMEKIT_VOLUME_CONTROL_TYPE_RELATIVE),
            HOMEKIT_CHARACTERISTIC(VOLUME_SELECTOR, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_volume_selector)),
            NULL
        }),
        NULL
    }),
    NULL
};


void on_input_configured_name(homekit_characteristic_t *ch, homekit_value_t value, void *arg) {
    const char *input_param_name = NULL;
    if (ch == &input_source1_name) {
        input_param_name = "input1_name";
    } else if (ch == &input_source2_name) {
        input_param_name = "input2_name";
    } else if (ch == &input_source3_name) {
        input_param_name = "input3_name";
    }

    if (!input_param_name)
        return;

    sysparam_set_string(input_param_name, value.string_value);
}


homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
    };


void on_wifi_event(wifi_config_event_t event) {
    if (event == WIFI_CONFIG_CONNECTED) {
        homekit_server_init(&config);
         xPing_Timer = xTimerCreate("Ping Timer Task",(5000/portTICK_PERIOD_MS),pdTRUE,0, pingTask);
         xTimerStart(xPing_Timer, 1000/portTICK_PERIOD_MS );
    }
}




void settings_mode() {
    //Flash the LED first before we start the reset
    for (int i=0; i<3; i++) {
        led_write(true);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        led_write(false);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
   // xTaskCreate(ir_dump_task, "IR receiver", 512, NULL, 0, NULL);

    printf("Starting settings mode\n");
    
   // wifi_config_reset();
    
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
   // printf("Resetting HomeKit Config\n");
    
   // homekit_server_reset();
    
    //vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    //printf("Restarting\n");
    
   // sdk_system_restart();
    is_config_mode = true;
    vTaskDelete(NULL);
}

void set_mode() {
    xTaskCreate(settings_mode, "settings_mode", 256, NULL, 2, NULL);
}

void button_callback(uint8_t gpio, button_event_t event) {
    switch (event) {
        case button_event_single_press:
            printf("Toggling button\n");
          //  switch_on.value.bool_value = !switch_on.value.bool_value;
            //  xTaskCreate(mainTask, "mainTask", 512, NULL, 0, NULL);

            break;
        case button_event_long_press:
            set_mode();
            break;
        default:
            printf("Unknown button event: %d\n", event);
    }
}

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    int name_len = snprintf(NULL, 0, "Remote-%02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Remote-%02X%02X%02X",
             macaddr[3], macaddr[4], macaddr[5]);
    
    name.value = HOMEKIT_STRING(name_value);
}

void user_init(void) {
    uart_set_baud(0, 115200);
	create_accessory_name();

    led_init();
    ir_tx_init();

    wifi_config_init2("Remote-tv", NULL, on_wifi_event);

    gpio_enable(POWERED_ON_GPIO, GPIO_INPUT);
    //tv_active.value = HOMEKIT_UINT8(gpio_read(POWERED_ON_GPIO));
    //gpio_set_interrupt(POWERED_ON_GPIO, GPIO_INTTYPE_EDGE_ANY, powered_on_intr_callback);
    
    gpio_enable(IR_TX_GPIO, GPIO_OUTPUT);
    
    if (button_create(POWERED_ON_GPIO, 0, 5000, button_callback)) {
        printf("Failed to initialize button\n");
    }
    
   

    char *s = NULL;
    if (sysparam_get_string("tv_name", &s) == SYSPARAM_OK) {
        homekit_value_destruct(&tv_name.value);
        tv_name.value = HOMEKIT_STRING(s);
    }

    if (sysparam_get_string("input1_name", &s) == SYSPARAM_OK) {
        homekit_value_destruct(&input_source1_name.value);
        input_source1_name.value = HOMEKIT_STRING(s);
    }

    if (sysparam_get_string("input2_name", &s) == SYSPARAM_OK) {
        homekit_value_destruct(&input_source2_name.value);
        input_source2_name.value = HOMEKIT_STRING(s);
    }

    if (sysparam_get_string("input3_name", &s) == SYSPARAM_OK) {
        homekit_value_destruct(&input_source3_name.value);
        input_source3_name.value = HOMEKIT_STRING(s);
    }
    int size = sizeof(ch_command_name)/sizeof(ch_command_name[0]);
    
    for(int i = 0; i< size; i++){
        char* name = malloc(sizeof(char)* 25);
        strcpy(name, ch_command_name[i]);
        
       // printf("command name %s\n", name);
       
   		 if (sysparam_get_string(name, &s) == SYSPARAM_OK) {
   		 strcpy(ch_command[i], s);
       // ir_tv_power = malloc(strlen(s)+1);
       	 printf("read sysparm %s - %s\n",ch_command_name[i], ch_command[i]);
       // printf("read sysparm %s\n", s);
        
       
   		 }else{
   		 	sysparam_set_string(name, "LO");
   		 }
   		 //printf("read sysparm %s - %s\n",ch_command_name[i], ch_command[i]);
    }
    
    free(s);
    
    if(homekit_is_paired()){
    }

}
