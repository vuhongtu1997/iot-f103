if(CMAKE_SCRIPT_MODE_FILE)
	file(GLOB srcs
	  "hc/database/*.cpp"
	  "hc/device/*.cpp"
	  "hc/gateway/*.cpp"
	  "hc/json/*.cpp"
	  "hc/object/*.cpp"
	  "hc/rule/rule_input/*.cpp"
	  "hc/rule/rule_output/*.cpp"
	  "hc/rule/*.cpp"
	  "hc/util/*.cpp"
	  "rs485/*.cpp"
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
	  "udp/*.cpp"
	  "*.cpp"
	)
else()
	file(GLOB srcs CONFIGURE_DEPENDS
	  "hc/database/*.cpp"
	  "hc/device/*.cpp"
	  "hc/gateway/*.cpp"
	  "hc/json/*.cpp"
	  "hc/object/*.cpp"
	  "hc/rule/rule_input/*.cpp"
	  "hc/rule/rule_output/*.cpp"
	  "hc/rule/*.cpp"
	  "hc/util/*.cpp"
	  "rs485/*.cpp"
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
	  "udp/*.cpp"
	  "*.cpp"
	)
endif()


set(incs
	"hc"
	"hc/database"
	"hc/device"
	"hc/gateway"
	"hc/json"
	"hc/object"
	"hc/rule"
	"hc/rule/rule_input"
	"hc/rule/rule_output"
	"hc/util"
	"rs485"
	"../../common"
	"button"
	"config"
	"led"
	"log"
	"mqtt"
	"ota"
	"sntp"
	"timer"
	"uart"
	"wifi"
	"udp")

idf_component_register(
    SRCS "main.cpp" "${srcs}" "../../common/protocol.c"
    INCLUDE_DIRS "${incs}"
)

# littlefs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
target_compile_options(${COMPONENT_LIB} PRIVATE
	"-std=gnu++17"
	"$<$<COMPILE_LANGUAGE:CXX>:-fexceptions>")

component_compile_options(-Wno-unused-variable -Wno-unused-function)
component_compile_options(-Wno-missing-field-initializers)
component_compile_options(-Wno-deprecated-declarations)
component_compile_options(-Wno-error=format= -Wno-format)
