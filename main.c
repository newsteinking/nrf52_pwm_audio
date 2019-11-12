/**
 *  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 *  @file     main.c
 *  @author   Tamas Harczos
 *  @since    2019-10-01
 *  @version  0.1.0
 *  @licence  GNU LGPL v3 (https://www.gnu.org/licenses/lgpl-3.0.txt)
 * 
 *  @brief    Application to demonstrate PWM audio functionality on nRF52.
 *  
 *  Project homepage:  https://nrf52-pwm-audio.sourceforge.io/
 *  Youtube video:     https://youtu.be/_m4-pH_Yw3M
 *
 *  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 *  This program will demonstrate the functionality of the nrf_pwm_audio
 *  module. Upon start, it will play back a sentence "Please use the buttons
 *  on the DSK!". By pressing buttons 1, 2, or 3 the program will play back
 *  "Sample one" (sampled at 7812 Hz), "Sample two" (sampled at 15625 Hz),
 *  or "Sample three" (sampled at 31250 Hz), respectively. By pressing button
 *  4 any current playback will be stopped.
 *
 *  To hear the output, you should connect a headphone between Vdd (NOT GND!)
 *  and the output pin (by default: GPIO pin 3). Possibilities to reduce
 *  static noise:
 *  
 *  - power the board from battery,
 *  - detach USB cable (disable programming/debug interface),
 *  - disable logging (NRF_LOG_ENABLED=0 in sdk_config.h),
 *  - add an electrolytic capacitor between Vdd and Gnd (1000 uF does well),
 *  - on nRF52840 DSK set SW6 to "nRF ONLY".
 *
 *  To use own audio clips, convert any digital audio to 8-bit signed integer
 *  format at any of the supported sampling rates, save as headerless RAW
 *  data and convert to .c/.h using e.g. bin2c. For the audio part you may
 *  want to use the free software Audacity.
 *
 *  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
**/


#include <stdio.h>
#include <string.h>

#include "nrf_pwm_audio.h"

#include "audio/intro.h"
#include "audio/sample1.h"
#include "audio/sample2.h"
#include "audio/sample3.h"

#include "app_util_platform.h"
#include "app_timer.h"

#include "nrf_drv_clock.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "boards.h"
#include "bsp.h"

/// SD Card
#include "ff.h"
#include "diskio_blkdev.h"
#include "nrf_block_dev_sdc.h"


#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"



// -------------------------------------------------------------------------------------------------------------------
#define SDC_SCK_PIN     ARDUINO_13_PIN  ///< SDC serial clock (SCK) pin.
#define SDC_MOSI_PIN    ARDUINO_11_PIN  ///< SDC serial data in (DI) pin.
#define SDC_MISO_PIN    ARDUINO_12_PIN  ///< SDC serial data out (DO) pin.
#define SDC_CS_PIN      ARDUINO_10_PIN  ///< SDC chip select (CS) pin.





static void bsp_evt_handler(bsp_event_t evt)
{
  NRF_LOG_INFO("Button pressed (%d).", evt);

  switch (evt)
  {
    case BSP_EVENT_KEY_0:
      nrf_pwm_audio_playback(sample1_8K_raw, sizeof(sample1_8K_raw), NRF_PWM_AUDIO_SAMPLERATE_8K, 1.0, 0);
    break;

    case BSP_EVENT_KEY_1:
      nrf_pwm_audio_playback(sample2_16K_raw, sizeof(sample2_16K_raw), NRF_PWM_AUDIO_SAMPLERATE_16K, 1.0, 0);
    break;

    case BSP_EVENT_KEY_2:
      nrf_pwm_audio_playback(sample3_31K_raw, sizeof(sample3_31K_raw), NRF_PWM_AUDIO_SAMPLERATE_31K, 1.0, 0);
    break;

    case BSP_EVENT_KEY_3:
    nrf_pwm_audio_playback(sample3_31K_raw, sizeof(sample3_31K_raw), NRF_PWM_AUDIO_SAMPLERATE_31K, 1.0, 0);
      nrf_pwm_audio_stop();
    break;

    default:
    return;
  }
}
//====================================================================================================================
// SD Card
#define FILE_NAME   "NORDIC.TXT"
#define TEST_STRING "SD card example."

/**
 * @brief  SDC block device definition
 * */
NRF_BLOCK_DEV_SDC_DEFINE(
        m_block_dev_sdc,
        NRF_BLOCK_DEV_SDC_CONFIG(
                SDC_SECTOR_SIZE,
                APP_SDCARD_CONFIG(SDC_MOSI_PIN, SDC_MISO_PIN, SDC_SCK_PIN, SDC_CS_PIN)
         ),
         NFR_BLOCK_DEV_INFO_CONFIG("Nordic", "SDC", "1.00")
);

/**
 * @brief Function for demonstrating FAFTS usage.
 */
static void fatfs_example()
{
    static FATFS fs;
    static DIR dir;
    static FILINFO fno;
    static FIL file;

    uint32_t bytes_written;
    FRESULT ff_result;
    DSTATUS disk_state = STA_NOINIT;

    // Initialize FATFS disk I/O interface by providing the block device.
    static diskio_blkdev_t drives[] =
    {
            DISKIO_BLOCKDEV_CONFIG(NRF_BLOCKDEV_BASE_ADDR(m_block_dev_sdc, block_dev), NULL)
    };

    diskio_blockdev_register(drives, ARRAY_SIZE(drives));

    NRF_LOG_INFO("Initializing disk 0 (SDC)...");
    for (uint32_t retries = 3; retries && disk_state; --retries)
    {
        disk_state = disk_initialize(0);
    }
    if (disk_state)
    {
        NRF_LOG_INFO("Disk initialization failed.");
        return;
    }

    uint32_t blocks_per_mb = (1024uL * 1024uL) / m_block_dev_sdc.block_dev.p_ops->geometry(&m_block_dev_sdc.block_dev)->blk_size;
    uint32_t capacity = m_block_dev_sdc.block_dev.p_ops->geometry(&m_block_dev_sdc.block_dev)->blk_count / blocks_per_mb;
    NRF_LOG_INFO("Capacity: %d MB", capacity);

    NRF_LOG_INFO("Mounting volume...");
    ff_result = f_mount(&fs, "", 1);
    if (ff_result)
    {
        NRF_LOG_INFO("Mount failed.");
        return;
    }

    NRF_LOG_INFO("\r\n Listing directory: /");
    ff_result = f_opendir(&dir, "/");
    if (ff_result)
    {
        NRF_LOG_INFO("Directory listing failed!");
        return;
    }

    do
    {
        ff_result = f_readdir(&dir, &fno);
        if (ff_result != FR_OK)
        {
            NRF_LOG_INFO("Directory read failed.");
            return;
        }

        if (fno.fname[0])
        {
            if (fno.fattrib & AM_DIR)
            {
                NRF_LOG_RAW_INFO("   <DIR>   %s",(uint32_t)fno.fname);
            }
            else
            {
                NRF_LOG_RAW_INFO("%9lu  %s", fno.fsize, (uint32_t)fno.fname);
            }
        }
    }
    while (fno.fname[0]);
    NRF_LOG_RAW_INFO("");

    NRF_LOG_INFO("Writing to file " FILE_NAME "...");
    ff_result = f_open(&file, FILE_NAME, FA_READ | FA_WRITE | FA_OPEN_APPEND);
    if (ff_result != FR_OK)
    {
        NRF_LOG_INFO("Unable to open or create file: " FILE_NAME ".");
        return;
    }

    ff_result = f_write(&file, TEST_STRING, sizeof(TEST_STRING) - 1, (UINT *) &bytes_written);
    if (ff_result != FR_OK)
    {
        NRF_LOG_INFO("Write failed\r\n.");
    }
    else
    {
        NRF_LOG_INFO("%d bytes written.", bytes_written);
    }

    (void) f_close(&file);
    return;
}


// ===================================================================================================================
int main(void)
{
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // init stuff (note: define NRF_LOG_ENABLED=1 in sdk_config.h first to enable logging)
  APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
  NRF_LOG_DEFAULT_BACKENDS_INIT();

  nrf_drv_clock_init();
  nrf_drv_clock_lfclk_request(NULL);

  app_timer_init();

  bsp_init(BSP_INIT_BUTTONS, bsp_evt_handler);
  bsp_buttons_enable();
      
  NRF_LOG_INFO("PWM Audio example started.");

  //================================================================================================================
  // SD Card


  fatfs_example();



  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // initialize the PWM audio module
  uint8_t const pwm_audio_pin   = 3;        ///< GPIO pin to use for output
  bool const pwm_audio_highdrive= false;    ///< whether to use GPIO pin in highdrive mode to increase volume

  nrf_pwm_audio_init(pwm_audio_pin, pwm_audio_highdrive);   

  // play back intro sound
  nrf_pwm_audio_playback(intro_16K_raw, sizeof(intro_16K_raw), NRF_PWM_AUDIO_SAMPLERATE_16K, 1.0, 0);

  // wait for playback to end
  while (nrf_pwm_audio_is_playing()) { __WFE(); NRF_LOG_FLUSH(); }

  // loop for eternity (but in the meantime, button presses will be evaluated)
  while (1) { __WFE(); NRF_LOG_FLUSH(); }

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // uninitialize PWM audio module (we will never reach this line in this program)
  nrf_pwm_audio_destroy();
}

