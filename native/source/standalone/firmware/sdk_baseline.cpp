/* SDK entry point. Peripheral startup requires the network build profile. */
#include <Arduino.h>
extern "C" {
#include "core.h"
#include "sdk_worker.h"
}
extern "C" const char mttl_firmware_version[] = "2.0.12";
/* Native LG U+ OTA checks this 32-byte field at decoded file offset 0x28.
 * Keep it before executable text via the standalone linker script. */
extern "C" const char mttl_native_ota_hardware[32]
    __attribute__((section(".mttl.native.hardware"), used)) = "LGU+-TAP-HW002";
volatile const char *mttl_version_view;
#ifdef MTTL_NETWORK_TEST
/* Existing build flags select real network/OTA/sensor/relay features.
 * Keep the legacy profile bytes: installed firmware checks them before OTA. */
#ifdef MTTL_OTA_TEST
#include "ota_test_config.h"
extern "C" const char mttl_test_profile[] = "NETWORK_OTA_TEST_RAM_SETTINGS_" MTTL_OTA_BUILD_ID;
#else
extern "C" const char mttl_test_profile[] = "NETWORK_TEST_NO_OTA_RAM_SETTINGS";
#endif
volatile const char *mttl_test_profile_view;
volatile int mttl_network_start_result;
#endif
void setup() {
    mttl_version_view = mttl_firmware_version;
#ifdef MTTL_NETWORK_TEST
    mttl_test_profile_view = mttl_test_profile;
    mttl_network_start_result = mttl_sdk_worker_start();
#endif
}
void loop() {
    mttl_sdk_watchdog_tick();
    mttl_sdk_buttons_poll();
    delay(10);
}
