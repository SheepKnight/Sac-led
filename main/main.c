// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "bt_app_core.h"
#include "bt_app_av.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "driver/i2s.h"

#include <math.h>

#include "esp32_digital_led_lib.h"
#include "esp32_digital_led_lib.h"
#include <string.h>
#include "driver/gpio.h"

#include <max7219.h>
#include "font.h"

typedef unsigned char byte;

#define HOST HSPI_HOST

#define PIN_NUM_MOSI 4
#define PIN_NUM_CLK  2
#define PIN_NUM_CS   15

#define CHECK(expr, msg) \
    while ((res = expr) != ESP_OK) { \
        printf(msg "\n", res); \
        vTaskDelay(250 / portTICK_RATE_MS); \
    }

#define nullptr  NULL

#define HIGH 1
#define LOW 0
#define OUTPUT GPIO_MODE_OUTPUT
#define INPUT GPIO_MODE_INPUT

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

#define min(a, b)  ((a) < (b) ? (a) : (b))
#define max(a, b)  ((a) > (b) ? (a) : (b))
#define floor(a)   ((int)(a))
#define ceil(a)    ((int)((int)(a) < (a) ? (a+1) : (a)))


#define BUF_SIZE (1024)


TaskHandle_t xHandle = NULL;

uint16_t lbuffer_d;
uint16_t rbuffer_d;

uint8_t ledBuffer[48];
char * trackInfo[4];

strand_t STRANDS[] = { // Avoid using any of the strapping pins on the ESP32
	{ .rmtChannel = 0,.gpioNum = 14,.ledType = LED_WS2812B_V3,.brightLimit = 15,.numPixels = 19,
	.pixels = nullptr,._stateVars = nullptr },
	{ .rmtChannel = 1,.gpioNum = 18,.ledType = LED_WS2812B_V3,.brightLimit = 15,.numPixels = 19,
	.pixels = nullptr,._stateVars = nullptr },
};
int STRANDCNT = sizeof(STRANDS) / sizeof(STRANDS[0]);


long map(long x, long in_min, long in_max, long out_min, long out_max)
{
	x = min(in_max, x);
	x = max(in_min, x);

	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/* event for handler "bt_av_hdl_stack_up */
enum {
    BT_APP_EVT_STACK_UP = 0,
};

/* handler for bluetooth stack enabled events */
static void bt_av_hdl_stack_evt(uint16_t event, void *p_param);

void gpioSetup(int gpioNum, int gpioMode, int gpioVal) {
#if defined(ARDUINO) && ARDUINO >= 100
	pinMode(gpioNum, gpioMode);
	digitalWrite(gpioNum, gpioVal);
#elif defined(ESP_PLATFORM)
	gpio_num_t gpioNumNative = (gpio_num_t)(gpioNum);
	gpio_mode_t gpioModeNative = (gpio_mode_t)(gpioMode);
	gpio_pad_select_gpio(gpioNumNative);
	gpio_set_direction(gpioNumNative, gpioModeNative);
	gpio_set_level(gpioNumNative, gpioVal);
#endif
}


static const uint64_t symbols[] = {
	0x383838fe7c381000, // arrows
	0x10387cfe38383800,
	0x10307efe7e301000,
	0x1018fcfefc181000,
	0x10387cfefeee4400, // heart
	0x105438ee38541000, // sun

	0x7e1818181c181800, // digits
	0x7e060c3060663c00,
	0x3c66603860663c00,
	0x30307e3234383000,
	0x3c6660603e067e00,
	0x3c66663e06663c00,
	0x1818183030667e00,
	0x3c66663c66663c00,
	0x3c66607c66663c00,
	0x3c66666e76663c00
};

void resetScreen() {
	memset(ledBuffer, 0, 48);
}

void shiftLeft() {
	memcpy(ledBuffer , ledBuffer+1, 47);
}

void putChar(uint8_t pos, uint8_t index) {

	pos = max(0, pos);
	pos = min(40, pos);
	memcpy(ledBuffer + pos, cp437_font + index, 8);

}

void putToScreen(max7219_t * dev) {
	
	for (byte offset = 0; offset < 4; offset++) {
		byte m[8] = { 0 }; 
		for (byte col = 0; col < 8; col++) { 
			byte b = ledBuffer[(4-offset) * 8 + (7-col)];
			for (byte bit = 0; bit < 8; bit++) { 
				if (b & 1) m[bit] |= (128 >> col); 
				b >>= 1; 
			} 
		} 
		for (byte col = 0; col < 8; col++) 
			max7219_set_digit(dev, (3-offset)*8 + col, m[col]);
			
	}
	
}
char * inputText = nullptr;
int currentChar = 0;
void displayText(char * track, char * artist, char * disc, char * style) {
	/*free(trackInfo[0]);
	free(trackInfo[1]);
	free(trackInfo[2]);
	free(trackInfo[3]);
	*/

	trackInfo[0] = track;
	trackInfo[1] = artist;
	trackInfo[2] = disc;
	trackInfo[3] = style;
	
	ESP_LOGE(BT_AV_TAG, "0,%s\n", trackInfo[0]);
	ESP_LOGE(BT_AV_TAG, "1,%s\n", trackInfo[1]);
	ESP_LOGE(BT_AV_TAG, "2,%s\n", trackInfo[2]);
	ESP_LOGE(BT_AV_TAG, "3,%s\n", trackInfo[3]);
	currentChar = 1;
	resetScreen();
	putChar(40, (uint8_t)artist[0]);
	
	inputText = malloc((4 + strlen(trackInfo[0]) + strlen(trackInfo[1])) * sizeof(char));
	strcpy(inputText, trackInfo[1]);
	strcat(inputText, " - ");
	strcat(inputText, trackInfo[0]);
	ESP_LOGE(BT_AV_TAG, "finale : %s\n", inputText);
}

void workLed(void * pvParameters) {
	float log10MaxDisplaySegments = log10(18);
	uint16_t LMSBL = 65535;
	uint16_t LMSBR = 65535;
	
	esp_err_t res;

	// Configure SPI bus
	spi_bus_config_t cfg = {
		.mosi_io_num = PIN_NUM_MOSI,
		.miso_io_num = -1,
		.sclk_io_num = PIN_NUM_CLK,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 0,
		.flags = 0
	};
	CHECK(spi_bus_initialize(HOST, &cfg, 1),
		"Could not initialize SPI bus: %d");

	// Configure device
	max7219_t dev = {
		.cascade_size = 4,
		.digits = 0,
		.mirrored = true
	};
	CHECK(max7219_init_desc(&dev, HOST, PIN_NUM_CS),
		"Could not initialize MAX7129 descriptor: %d");
	CHECK(max7219_init(&dev),
		"Could not initialize MAX7129: %d");
	int decalage = 0;
	putChar(40, 60);
	for (;;) {
		if (rbuffer_d != LMSBR || lbuffer_d != LMSBL) {
			
			uint16_t rBuffer = rbuffer_d;
			uint16_t lBuffer = lbuffer_d;
			LMSBL = lBuffer;
			LMSBR = rBuffer;
			if (rBuffer > 32768) {
				rBuffer = 65535-rBuffer;
			}
			if (lBuffer > 32768) {
				lBuffer = 65535-lBuffer;
			}
			lBuffer = map(lBuffer*8, 0, 2048, 0, 18);
			rBuffer = map(rBuffer*8, 0, 2048, 0, 18);
			
			lBuffer = ((log10(lBuffer + 1) / log10MaxDisplaySegments * 18));
			rBuffer = ((log10(rBuffer + 1) / log10MaxDisplaySegments * 18));
				


			strand_t * pStrand = &STRANDS[0];
			memset(pStrand->pixels, 0, pStrand->numPixels * sizeof(pixelColor_t));

			int toDisp = min(lBuffer, pStrand->numPixels);
			int i = 0;
			for (i = 0; i < toDisp; i++) {
				pStrand->pixels[18-i] = pixelFromRGB(15 * i, 25, 25);
				
			}
			digitalLeds_updatePixels(pStrand);


			strand_t * pStrandR = &STRANDS[1];
			memset(pStrandR->pixels, 0, pStrandR->numPixels * sizeof(pixelColor_t));

			toDisp = min(rBuffer, pStrandR->numPixels);

			for (i = 0; i < toDisp; i++) {
				pStrandR->pixels[18-i] = pixelFromRGB(15 * i, 25, 25);
				
			}
			digitalLeds_updatePixels(pStrandR);
			shiftLeft();
			if (currentChar !=0) {
				
				if (decalage++ == 8) {
					putChar(40, (uint8_t)inputText[currentChar]);
					currentChar++;
					decalage = 0;
					if (currentChar == strlen(inputText)) {
						currentChar = 0;
						free(trackInfo[0]);
						free(trackInfo[1]);
						free(trackInfo[2]);
						free(trackInfo[3]);
						free(inputText);
					}
				}

				
			}
			putToScreen(&dev);
			
		}
		
	}
}

void app_main()
{
	/* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    i2s_config_t i2s_config = {
#ifdef CONFIG_A2DP_SINK_OUTPUT_INTERNAL_DAC
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
#else
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,                                  // Only TX
#endif
        .sample_rate = 44100,
        .bits_per_sample = 16,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           //2-channels
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .dma_buf_count = 6,
        .dma_buf_len = 60,
        .intr_alloc_flags = 0,                                                  //Default interrupt priority
        .tx_desc_auto_clear = true                                              //Auto clear tx descriptor on underflow
    };


    i2s_driver_install(0, &i2s_config, 0, NULL);
#ifdef CONFIG_A2DP_SINK_OUTPUT_INTERNAL_DAC
    i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
    i2s_set_pin(0, NULL);
#else
    i2s_pin_config_t pin_config = {
        .bck_io_num = CONFIG_I2S_BCK_PIN,
        .ws_io_num = CONFIG_I2S_LRCK_PIN,
        .data_out_num = CONFIG_I2S_DATA_PIN,
        .data_in_num = -1                                                       //Not used
    };

    i2s_set_pin(0, &pin_config);
#endif



    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((err = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    if ((err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    if ((err = esp_bluedroid_init()) != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    if ((err = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(BT_AV_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(err));
        return;
    }

    /* create application task */
    bt_app_task_start_up();

    /* Bluetooth device name, connection mode and profile set up */
    bt_app_work_dispatch(bt_av_hdl_stack_evt, BT_APP_EVT_STACK_UP, NULL, 0, NULL);

#if (CONFIG_BT_SSP_ENABLED == true)
    /* Set default parameters for Secure Simple Pairing */
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
#endif

    /*
     * Set default parameters for Legacy Pairing
     * Use fixed pin code
     */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code;
    pin_code[0] = '1';
    pin_code[1] = '2';
    pin_code[2] = '3';
    pin_code[3] = '4';
    esp_bt_gap_set_pin(pin_type, 4, pin_code);

	gpioSetup(14, OUTPUT, LOW);
	gpioSetup(18, OUTPUT, LOW);
	if (digitalLeds_initStrands(STRANDS, STRANDCNT)) {
		ets_printf("Init FAILURE\n");
	}
	setupBuf( &rbuffer_d, &lbuffer_d, &displayText);
	
	
	xTaskCreatePinnedToCore(workLed, "LIGHT", 4096, nullptr, tskIDLE_PRIORITY, xHandle, 0);

	

	/*
	uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	uart_param_config(UART_NUM_1, &uart_config);
	uart_set_pin(UART_NUM_1, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);
	uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);*/

}


void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT: {
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(BT_AV_TAG, "authentication success: %s", param->auth_cmpl.device_name);
            esp_log_buffer_hex(BT_AV_TAG, param->auth_cmpl.bda, ESP_BD_ADDR_LEN);
        } else {
            ESP_LOGE(BT_AV_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
        }
        break;
    }

#if (CONFIG_BT_SSP_ENABLED == true)
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %d", param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%d", param->key_notif.passkey);
        break;
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;
#endif

    default: {
        ESP_LOGI(BT_AV_TAG, "event: %d", event);
        break;
    }
    }
    return;
}
static void bt_av_hdl_stack_evt(uint16_t event, void *p_param)
{
    ESP_LOGD(BT_AV_TAG, "%s evt %d", __func__, event);
    switch (event) {
    case BT_APP_EVT_STACK_UP: {
        /* set up device name */
        char *dev_name = "LED_SPEAKER";
        esp_bt_dev_set_device_name(dev_name);

        esp_bt_gap_register_callback(bt_app_gap_cb);
        /* initialize A2DP sink */
        esp_a2d_register_callback(&bt_app_a2d_cb);
        esp_a2d_sink_register_data_callback(bt_app_a2d_data_cb);
        esp_a2d_sink_init();

        /* initialize AVRCP controller */
        esp_avrc_ct_init();
        esp_avrc_ct_register_callback(bt_app_rc_ct_cb);

        /* set discoverable and connectable mode, wait to be connected */
        esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
        break;
    }
    default:
        ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}
