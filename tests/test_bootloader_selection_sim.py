"""Exercise the installed SDK's real OTA selector with a simulated metadata store.
Not execution of the stock dump, flash-write physics, or the MCU bootloader.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

IDF=Path(os.environ["IDF_PATH"])

class BootloaderSelectionTests(unittest.TestCase):
    def test_rollback_enabled_and_disabled_models(self):
        text=(IDF/"components/bootloader_support/src/bootloader_utility.c").read_text(encoding="utf-8")
        start=text.index("int bootloader_utility_get_selected_boot_partition(")
        end=text.index("\n}\n",start)+3
        selector=text[start:end]
        program=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define ESP_OK 0
#define FACTORY_INDEX -1
#define INVALID_INDEX -99
#define FLASH_SECTOR_SIZE 4096
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
enum {ESP_OTA_IMG_NEW,ESP_OTA_IMG_PENDING_VERIFY,ESP_OTA_IMG_VALID,ESP_OTA_IMG_INVALID,ESP_OTA_IMG_ABORTED};
typedef struct {uint32_t offset,size;} esp_partition_pos_t;
typedef struct {esp_partition_pos_t ota_info,factory;uint32_t app_count;} bootloader_state_t;
typedef struct {uint32_t ota_seq,ota_state,crc;} esp_ota_select_entry_t;
static esp_ota_select_entry_t flash[2];
static bool ota_has_initial_contents,fail_write;
static uint32_t bootloader_common_ota_select_crc(const esp_ota_select_entry_t *e) {return e->ota_seq;}
static bool bootloader_common_ota_select_invalid(const esp_ota_select_entry_t *e) {
    return e->ota_seq==UINT32_MAX || e->crc!=e->ota_seq || e->ota_state==ESP_OTA_IMG_INVALID || e->ota_state==ESP_OTA_IMG_ABORTED;
}
static int bootloader_common_get_active_otadata(esp_ota_select_entry_t *e) {
    bool a=!bootloader_common_ota_select_invalid(e),b=!bootloader_common_ota_select_invalid(e+1);
    return a && b ? (e[1].ota_seq>e[0].ota_seq) : a?0:b?1:-1;
}
static int bootloader_common_read_otadata(const esp_partition_pos_t *p,esp_ota_select_entry_t *out) {memcpy(out,flash,sizeof(flash));return ESP_OK;}
static bool esp_flash_encryption_enabled(void) {return false;}
static int write_otadata(esp_ota_select_entry_t *e,uint32_t offset,bool encrypted) {
    if(fail_write)return -1;
    flash[(offset-0xe000)/4096]=*e;return ESP_OK;
}
''' + selector + r'''
static void reset_metadata(void) {
    flash[0]=(esp_ota_select_entry_t){1,ESP_OTA_IMG_VALID,1};
    flash[1]=(esp_ota_select_entry_t){2,ESP_OTA_IMG_NEW,2};
}
int main(void) {
    bootloader_state_t bs={.ota_info={0xe000,8192},.app_count=2};
    reset_metadata();
    assert(bootloader_utility_get_selected_boot_partition(&bs)==1);
#ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    assert(flash[1].ota_state==ESP_OTA_IMG_PENDING_VERIFY);
    assert(bootloader_utility_get_selected_boot_partition(&bs)==0); // crash before confirmation
    assert(flash[1].ota_state==ESP_OTA_IMG_ABORTED);
    reset_metadata();assert(bootloader_utility_get_selected_boot_partition(&bs)==1);
    flash[1].ota_state=ESP_OTA_IMG_VALID; // model application confirmation
    assert(bootloader_utility_get_selected_boot_partition(&bs)==1);
    reset_metadata();fail_write=true;assert(bootloader_utility_get_selected_boot_partition(&bs)==1);
    assert(flash[1].ota_state==ESP_OTA_IMG_NEW); // failed metadata write is NOT durable
    fail_write=false;assert(bootloader_utility_get_selected_boot_partition(&bs)==1);
    assert(bootloader_utility_get_selected_boot_partition(&bs)==0);
    puts("SDK selector: unconfirmed -> fallback; confirmed -> retained; failed metadata write -> retry");
#else
    assert(flash[1].ota_state==ESP_OTA_IMG_NEW);
    assert(bootloader_utility_get_selected_boot_partition(&bs)==1);
    puts("OBSERVATION: without rollback support an unconfirmed NEW image is selected again");
#endif
    return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix="shu1-boot-model-") as tmp:
            src=Path(tmp)/"model.c";src.write_text(program,encoding="utf-8")
            for enabled in (True,False):
                with self.subTest(rollback=enabled):
                    exe=Path(tmp)/"model.exe"
                    cmd=[os.environ.get("CC","clang"),"-std=c11"]
                    if enabled:cmd.append("-DCONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1")
                    r=subprocess.run(cmd+[str(src),"-o",str(exe)],capture_output=True,text=True)
                    self.assertEqual(r.returncode,0,r.stderr)
                    r=subprocess.run([str(exe)],capture_output=True,text=True)
                    self.assertEqual(r.returncode,0,r.stdout+r.stderr)
                    print(r.stdout.strip())
