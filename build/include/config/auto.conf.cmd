deps_config := \
	M:\msys32\home\albin\esp-idf\components\app_trace\Kconfig \
	M:\msys32\home\albin\esp-idf\components\aws_iot\Kconfig \
	M:\msys32\home\albin\esp-idf\components\bt\Kconfig \
	M:\msys32\home\albin\esp-idf\components\driver\Kconfig \
	M:\msys32\home\albin\esp-idf\components\esp32\Kconfig \
	M:\msys32\home\albin\esp-idf\components\esp_adc_cal\Kconfig \
	M:\msys32\home\albin\esp-idf\components\esp_event\Kconfig \
	M:\msys32\home\albin\esp-idf\components\esp_http_client\Kconfig \
	M:\msys32\home\albin\esp-idf\components\esp_http_server\Kconfig \
	M:\msys32\home\albin\esp-idf\components\ethernet\Kconfig \
	M:\msys32\home\albin\esp-idf\components\fatfs\Kconfig \
	M:\msys32\home\albin\esp-idf\components\freemodbus\Kconfig \
	M:\msys32\home\albin\esp-idf\components\freertos\Kconfig \
	M:\msys32\home\albin\esp-idf\components\heap\Kconfig \
	M:\msys32\home\albin\esp-idf\components\libsodium\Kconfig \
	M:\msys32\home\albin\esp-idf\components\log\Kconfig \
	M:\msys32\home\albin\esp-idf\components\lwip\Kconfig \
	M:\msys32\home\albin\esp-idf\components\mbedtls\Kconfig \
	M:\msys32\home\albin\esp-idf\components\mdns\Kconfig \
	M:\msys32\home\albin\esp-idf\components\mqtt\Kconfig \
	M:\msys32\home\albin\esp-idf\components\nvs_flash\Kconfig \
	M:\msys32\home\albin\esp-idf\components\openssl\Kconfig \
	M:\msys32\home\albin\esp-idf\components\pthread\Kconfig \
	M:\msys32\home\albin\esp-idf\components\spi_flash\Kconfig \
	M:\msys32\home\albin\esp-idf\components\spiffs\Kconfig \
	M:\msys32\home\albin\esp-idf\components\tcpip_adapter\Kconfig \
	M:\msys32\home\albin\esp-idf\components\unity\Kconfig \
	M:\msys32\home\albin\esp-idf\components\vfs\Kconfig \
	M:\msys32\home\albin\esp-idf\components\wear_levelling\Kconfig \
	M:\msys32\home\albin\esp-idf\components\app_update\Kconfig.projbuild \
	M:\msys32\home\albin\esp-idf\components\bootloader\Kconfig.projbuild \
	M:\msys32\home\albin\esp-idf\components\esptool_py\Kconfig.projbuild \
	M:\msys32\home\albin\esp-idf\projects\a2dp_sink\main\Kconfig.projbuild \
	M:\msys32\home\albin\esp-idf\components\partition_table\Kconfig.projbuild \
	/home/albin/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_TARGET)" "esp32"
include/config/auto.conf: FORCE
endif
ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
