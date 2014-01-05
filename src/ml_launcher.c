/**
 * @file ml_launcher.c
 * @brief 
 * @author Travis Lane
 * @version 0.0.2
 * @date 2014-01-02
 */

#include "libmissilelauncher_internal.h"


/**
 * @brief 
 *
 * @param launcher
 *
 * @return 
 */
int16_t _ml_init_launcher(ml_launcher_t *launcher, libusb_device *device) {
  if(launcher == NULL) {
    TRACE("Launcher was null. _ml_init_launcher\n");
    return ML_NULL_LAUNCHER;
  }
  launcher->usb_device = device;
  launcher->ref_count = 0;
  launcher->device_connected = 1;
  libusb_ref_device(device);
  pthread_mutex_init(&(launcher->main_lock), NULL);
  return ML_OK;
}

/**
 * @brief 
 *
 * @param launcher
 *
 * @return 
 */
int16_t _ml_cleanup_launcher(ml_launcher_t **launcher) {
  if((*launcher) == NULL) {
    TRACE("Launcher was null. _ml_cleanup_launcher\n");
    return ML_NULL_LAUNCHER;
  }
  libusb_unref_device((*launcher)->usb_device);
  pthread_mutex_destroy(&((*launcher)->main_lock));
  free((*launcher));
  launcher = NULL;
  return ML_OK;
}

/**
 * @brief 
 *
 * @param launcher
 *
 * @return 
 */
int16_t ml_refrence_launcher(ml_launcher_t *launcher) {
  if(launcher == NULL) {
    TRACE("Launcher was null. ml_refrence_launcher\n");
    return ML_NULL_LAUNCHER;
  }
  pthread_mutex_lock(&(launcher->main_lock));
  launcher->ref_count += 1; 
  pthread_mutex_unlock(&(launcher->main_lock));
  return ML_NOT_IMPLEMENTED;
}

/**
 * @brief 
 *
 * @param launcher
 *
 * @return 
 */
int16_t ml_derefrence_launcher(ml_launcher_t *launcher) {
  if(launcher == NULL) {
    TRACE("Launcher was null. ml_derefrence_launcher\n");
    return ML_NULL_LAUNCHER;
  }
  pthread_mutex_lock(&(launcher->main_lock));
  launcher->ref_count -= 1;
  if(launcher->ref_count == 0 && launcher->device_connected == 0) {
    // Not connected and not refrenced
    _ml_remove_launcher(launcher);
    _ml_cleanup_launcher(&launcher);
  }
  pthread_mutex_unlock(&(launcher->main_lock));
  return ML_NOT_IMPLEMENTED;
}
