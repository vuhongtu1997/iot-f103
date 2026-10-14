if(CMAKE_SCRIPT_MODE_FILE)
	file(GLOB srcs
	  "hc/database/*.cpp"
	  "hc/device/*.cpp"
	  "hc/gateway/*.cpp"
	  "hc/group/*.cpp"
	  "hc/json/*.cpp"
	  "hc/object/*.cpp"
	  "hc/room/*.cpp"
	  "hc/rule/rule_input/*.cpp"
	  "hc/rule/rule_output/*.cpp"
	  "hc/rule/*.cpp"
	  "hc/scene/*.cpp"
	  "hc/util/*.cpp"
	  "button/*.cpp"
	  "config/*.cpp"
	  "led/*.cpp"
	  "log/*.cpp"
		"mqtt/Mqtt.cpp"
	  "ota/*.cpp"
	  "sntp/*.cpp"
	  "sntp/*.c"
	  "timer/*.cpp"
	  "uart/*.cpp"
	  "wifi/*.cpp"
	  "*.cpp"
	)
else()
	file(GLOB srcs CONFIGURE_DEPENDS
	  "hc/database/*.cpp"
	  "hc/device/*.cpp"
	  "hc/gateway/*.cpp"
	  "hc/group/*.cpp"
	  "hc/json/*.cpp"
	  "hc/object/*.cpp"
	  "hc/room/*.cpp"
	  "hc/rule/rule_input/*.cpp"
	  "hc/rule/rule_output/*.cpp"
	  "hc/rule/*.cpp"
	  "hc/scene/*.cpp"
	  "hc/util/*.cpp"
	  "button/*.cpp"
	  "config/*.cpp"
	  "led/*.cpp"
	  "log/*.cpp"
		"mqtt/Mqtt.cpp"
	  "ota/*.cpp"
	  "sntp/*.cpp"
	  "sntp/*.c"
	  "timer/*.cpp"
	  "uart/*.cpp"
	  "wifi/*.cpp"
	  "*.cpp"
	)
endif()

set(incs
	"hc"
	"hc/database"
	"hc/device"
	"hc/gateway"
	"hc/group"
	"hc/json"
	"hc/object"
	"hc/room"
	"hc/rule"
	"hc/rule/rule_input"
	"hc/rule/rule_output"
	"hc/scene"
	"hc/util"
	"button"
	"config"
	"led"
	"log"
	"mqtt"
	"ota"
	"sntp"
	"timer"
	"uart"
	"wifi")

if(CONFIG_ENABLE_BLE)
	if(CMAKE_SCRIPT_MODE_FILE)
		file(GLOB_RECURSE BLE_SRCS
			"hc/device/ble/*.cpp"
	  		"hc/device/ble/device/*.cpp"
	  		"hc/device/ble/module/*.cpp"
			"hc/protocol/ble/*.cpp")
	else()
		file(GLOB_RECURSE BLE_SRCS CONFIGURE_DEPENDS
			"hc/device/ble/*.cpp"
	  		"hc/device/ble/device/*.cpp"
	  		"hc/device/ble/module/*.cpp"
			"hc/protocol/ble/*.cpp")
	endif()
	list(APPEND BLE_SRCS
		"hc/device/ble/device/DeviceBleIR.cpp"
		"hc/device/ble/device/DeviceBleLightWhite.cpp"
		"hc/device/ble/module/ModuleLightOnOff.cpp"
		"hc/device/ble/module/ModuleRawData.cpp")
	list(REMOVE_DUPLICATES BLE_SRCS)
	list(APPEND srcs ${BLE_SRCS})
	list(APPEND incs
		"hc/device/ble"
  		"hc/device/ble/device"
		"hc/protocol/ble")
endif()

idf_component_register(
    SRCS "main.cpp" "${srcs}"
    INCLUDE_DIRS "${incs}"
)

# littlefs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
target_compile_options(${COMPONENT_LIB} PRIVATE "-std=gnu++17")

component_compile_options(-Wno-unused-variable -Wno-unused-function)
component_compile_options(-Wno-missing-field-initializers)
component_compile_options(-Wno-deprecated-declarations)
component_compile_options(-Wno-error=format= -Wno-format)
