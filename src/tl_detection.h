/**
 * @file tl_detection.h
 * @brief 
 * @author Travis Lane
 * @version 0.0.1
 * @date 2013-12-18
 */

#ifndef TL_DETECTION_H
#define TL_DETECTION_H

#include "tl_common.h"

#define TL_STD_VENDOR_ID 8483
#define TL_STD_PRODUCT_ID 4112

typedef enum tl_launcher_type {
  TL_NOT_LAUNCHER,
  TL_STANDARD_LAUNCHER
} tl_launcher_type;

int16_t initialize_library();
int16_t cleanup_library();

int16_t _initialize_control(launch_control *init_control);
int16_t _cleanup_control(launch_control *clean_control);


int16_t start_continuous_poll();
int16_t stop_continuous_poll();

int16_t _start_continuous_control_poll(launch_control *target_control);
int16_t _stop_continuous_control_poll(launch_control *target_control);

int16_t set_poll_rate(uint8_t new_rate);
uint8_t get_poll_rate();

int16_t _set_control_poll_rate(launch_control *target_control, uint8_t new_rate);
uint8_t _get_control_poll_rate(launch_control *target_control);
 

void *_poll_control_for_launcher(void *target_control);
void _poll_control_for_launcher_cleanup(void *target_control);

uint8_t is_device_launcher(struct libusb_device_descriptor *desc);
int16_t _control_mount_launcher(launch_control *control, struct libusb_device *device);

#endif
