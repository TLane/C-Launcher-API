/**
 * @file ml_controller.c
 * @brief 
 * @author Travis Lane
 * @version 0.0.2
 * @date 2013-12-31
 */

#include "libmissilelauncher_internal.h"

int16_t ml_init_library(){
  int init_result;
  int16_t failed = 0;

  pthread_mutex_lock(&ml_main_controller_mutex);
  if(ml_main_controller != NULL) {
    TRACE("Main launch control was not null, possibly already initialized\n");
    pthread_mutex_unlock(&ml_main_controller_mutex);
    return ML_LIBRARY_ALREADY_INIT;
  }
  // Allocate space for the main controller, calloc so everything is null.
  ml_main_controller = calloc(sizeof(ml_controller_t), 1);
  if(ml_main_controller == NULL) {
    TRACE("Could not allocate memory for a new launch control\n");
    pthread_mutex_unlock(&ml_main_controller_mutex);
    return ML_CONTROL_ALLOC_FAIL;
  }
  // Initialize libusb 
  init_result = libusb_init(NULL);
  if(init_result < 0) {
    TRACE("libusb failed with code: %d\n", init_result);
    free(ml_main_controller);
    ml_main_controller = NULL;
    pthread_mutex_unlock(&ml_main_controller_mutex);
    return ML_LIBUSB_INIT_FAILED;
  }
  // Initialize the main controller
  failed = _ml_init_controller(ml_main_controller);
  if(failed) {
    if(ml_main_controller->launchers != NULL) {
      free(ml_main_controller->launchers);
    }
    free(ml_main_controller);
    ml_main_controller = NULL;
  }
  pthread_mutex_unlock(&ml_main_controller_mutex);
  return failed;
}

int16_t _ml_init_controller(ml_controller_t *controller) {
  if(controller->control_initialized) {
    TRACE("Controller already initialized\n");
    return ML_CONTROL_ALREADY_INIT;
  }
  // Setup the array
  controller->launchers = calloc(sizeof(ml_arr_launcher_t),
                                 ML_INITIAL_LAUNCHER_ARRAY_SIZE);
  if(controller->launchers == NULL) {
    TRACE("Failed to initialize library. Launcher array was null.\n");
    return ML_CONTROL_ARR_ALLOC_FAIL;
  }
  // Set default variables
  controller->poll_rate_seconds   = ML_DEFAULT_POLL_RATE;
  controller->launcher_array_size = ML_INITIAL_LAUNCHER_ARRAY_SIZE;
  controller->launcher_count = 0;
  // Initialize mutexes and locks
  pthread_rwlock_init(&(controller->poll_rate_lock), NULL);
  pthread_mutex_init(&(controller->poll_control_mutex), NULL);
  // Good to go!
  controller->control_initialized = 1;
  return ML_OK;
}

int16_t ml_cleanup_library() { 
  int16_t failed = 0;
  pthread_mutex_lock(&ml_main_controller_mutex);
  if(ml_main_controller == NULL) {
    TRACE("Could not clean up library. Library not initialized.\n");
    return ML_LIBRARY_NOT_INIT;
  }
  failed = _ml_cleanup_controller(ml_main_controller);
  free(ml_main_controller);
  ml_main_controller = NULL;
  libusb_exit(NULL);
  pthread_mutex_unlock(&ml_main_controller_mutex);
  return failed;
}

int16_t _ml_cleanup_controller(ml_controller_t *controller) {
  if(controller == NULL) {
    TRACE("Could not clean up, control was null.\n");
    return ML_CONTROL_FREE_NULL;
  }
  if(controller->launchers == NULL) {
    TRACE("Could not clean up, launcher array was null.\n");
    return ML_CONTROL_ARR_NULL;
  }
  free(controller->launchers);
  controller->launchers = NULL;
  controller->launcher_array_size = 0;
  controller->launcher_count = 0;
  controller->control_initialized = 0;

  // Cleanup mutexes
  pthread_mutex_destroy(&(controller->poll_control_mutex));
  pthread_rwlock_destroy(&(controller->poll_rate_lock));
  return ML_OK;
}

uint8_t ml_is_library_init() {
  if(ml_main_controller == NULL) {
    TRACE("Library not initialized.\n");
    return 0;
  } 
  return 1;
}

int16_t ml_start_continuous_poll() {
  int thread_code = 0;
  if(!ml_is_library_init()) return ML_LIBRARY_NOT_INIT;

  pthread_mutex_lock(&(ml_main_controller->poll_control_mutex));
  if(ml_main_controller->currently_polling) {
    pthread_mutex_unlock(&(ml_main_controller->poll_control_mutex));
    return ML_OK;
  }

  thread_code = pthread_create(&(ml_main_controller->poll_thread), NULL,
                   _ml_poll_for_launchers, (void *) ml_main_controller);
  ml_main_controller->currently_polling = 1;
  pthread_mutex_unlock(&(ml_main_controller->poll_control_mutex));
  if(thread_code != 0) {
    return ML_FAILED_POLL_START;
  }
  return ML_OK;
}

int16_t ml_stop_continuous_poll() {
  if(!ml_is_library_init()) return ML_LIBRARY_NOT_INIT;

  pthread_mutex_lock(&(ml_main_controller->poll_control_mutex));
  if(!(ml_main_controller->currently_polling)) {
    pthread_mutex_unlock(&(ml_main_controller->poll_control_mutex));
    return ML_OK;
  }
  pthread_cancel(ml_main_controller->poll_thread);

#ifndef NDEBUG
  pthread_join(ml_main_controller->poll_thread, NULL);
#endif
  ml_main_controller->currently_polling = 0;
  pthread_mutex_unlock(&(ml_main_controller->poll_control_mutex));
  return ML_OK;
}


void *_ml_poll_for_launchers(void *target_arg) {
  int device_count = 0, desc_result = 0;
  uint8_t poll_rate = 0;
  int16_t failed = 0;

  libusb_device **devices = NULL;
  libusb_device *cur_device = NULL;

  if(!ml_is_library_init()) return NULL;
  TRACE("Polling\n");
  poll_rate = ml_get_poll_rate();

  for(;;) {
    // The cancellation point 
    pthread_testcancel();
    TRACE("loop\n");
    // Get devices  
    device_count = libusb_get_device_list(NULL, &devices);
    _ml_update_launchers(devices, device_count); 
    //TODO: Scan and add to array if new. Else continue

    // Free
    libusb_free_device_list(devices, 1);
    // Sleep and update poll rate
    ml_second_sleep(poll_rate);
    poll_rate = ml_get_poll_rate();
  }
}

uint8_t ml_is_polling() {
  uint8_t polling = 0;

  if(!ml_is_library_init()) return ML_LIBRARY_NOT_INIT;

  pthread_mutex_lock(&(ml_main_controller->poll_control_mutex));
  polling = ml_main_controller->currently_polling;
  pthread_mutex_unlock(&(ml_main_controller->poll_control_mutex));
  return polling;
}

/**
 * @brief 
 *
 * @return 
 */
uint8_t ml_get_poll_rate() {
  uint8_t poll_rate;

  if(!ml_is_library_init()) return ML_LIBRARY_NOT_INIT;

  pthread_rwlock_rdlock(&(ml_main_controller->poll_rate_lock));
  poll_rate = ml_main_controller->poll_rate_seconds;
  pthread_rwlock_unlock(&(ml_main_controller->poll_rate_lock));
  return poll_rate;
}

/**
 * @brief Sets a new poll rate for the library to use.
 *
 * @param poll_rate_seconds The new pollrate in seconds equal to 
 * or between ML_MAX_POLL_RATE and ML_MIN_POLL_RATE or
 * 0 to reset to the default poll rate
 *
 * @return A result code, 0 (OK),  ML_LIBRARY_NOT_INIT, or ML_INVALID_POLL_RATE
 */
int16_t ml_set_poll_rate(uint8_t poll_rate_seconds) {

  if(!ml_is_library_init()) return ML_LIBRARY_NOT_INIT;

  if(poll_rate_seconds == 0) {
    pthread_rwlock_wrlock(&(ml_main_controller->poll_rate_lock));
    ml_main_controller->poll_rate_seconds = ML_DEFAULT_POLL_RATE;
    pthread_rwlock_unlock(&(ml_main_controller->poll_rate_lock));
    return ML_OK;
  }

  if(poll_rate_seconds > ML_MAX_POLL_RATE ||
      poll_rate_seconds < ML_MIN_POLL_RATE) {
    return ML_INVALID_POLL_RATE;
  }
  pthread_rwlock_wrlock(&(ml_main_controller->poll_rate_lock));
  ml_main_controller->poll_rate_seconds = poll_rate_seconds;
  pthread_rwlock_unlock(&(ml_main_controller->poll_rate_lock));
  return ML_OK;
}

uint8_t _ml_catagorize_device(struct libusb_device_descriptor *desc) {
  if(desc->idProduct == ML_STD_PRODUCT_ID && 
     desc->idVendor == ML_STD_VENDOR_ID) {
    TRACE("found std launcher\n");
    return ML_STANDARD_LAUNCHER;
  }
  return ML_NOT_LAUNCHER;
}

int16_t _ml_update_launchers(struct libusb_device **devices, int device_count) {
  libusb_device *found_device = NULL;
  libusb_device **found_launchers = NULL;
  ml_launcher_t *known_device = NULL;
  int16_t launchers_found = 0;
  
  // Make a new array of devices
  for(int i = 0; i < device_count && (found_device = devices[i]) != NULL; i++) {
    struct libusb_device_descriptor device_descriptor;
    libusb_get_device_descriptor(found_device, &device_descriptor);

    // Check if the device is a launcher
    if(_ml_catagorize_device(&device_descriptor) != ML_NOT_LAUNCHER) {
      // Device is launcher
      launchers_found += 1;
      found_launchers = realloc(found_launchers, 
                          (launchers_found + 1) * sizeof(libusb_device *));
      if(found_launchers == NULL) {
        TRACE("realloc failed. _ml_update_launchers\n");
        return ML_FAILED_DEV_ARR_REALLOC;
      }
      TRACE("Device found, temp index: %d\n", launchers_found - 1);
      
      found_launchers[launchers_found - 1] = found_device;
      found_launchers[launchers_found] = NULL;
    } 
  }
  TRACE("Found launchers: %d\n", launchers_found);
  // Update the main array

  pthread_rwlock_wrlock(&(ml_main_controller->launcher_array_lock));
  
  // Check to see if we need to remove any devices 
  for(uint16_t known_it = 0; known_it < ml_main_controller->launcher_array_size &&
      (known_device = ml_main_controller->launchers[known_it]) != NULL; known_it++){
    uint8_t found = 0;
    pthread_mutex_lock(&(known_device->main_lock));
    for(uint16_t found_it = 0; found_it < launchers_found && 
        (found_device = found_launchers[found_it]) != NULL && found == 0; found_it++) {
      if(known_device->usb_device == found_device){
        TRACE("Found existing device.\n");
        found = 1;
      }
    }
    known_device->device_connected = found;
    if(found == 0) {
      TRACE("Device removed.\n");
    } else {
      TRACE("Device still mounted.\n");
    }
    pthread_mutex_unlock(&(known_device->main_lock));
    if(known_device->device_connected == 0 &&
       known_device->ref_count == 0) {
      TRACE("Removing old device.\n");
      // No one is refrencing the device, so we can free it.
      _ml_remove_launcher_index(known_it);
      _ml_cleanup_launcher(&known_device);
    }
  }

  for(uint16_t found_it = 0; found_it < launchers_found &&
        (found_device = found_launchers[found_it]) != NULL; found_it++){
    uint8_t found = 0;
    for(uint16_t known_it = 0; known_it < ml_main_controller->launcher_array_size &&
        (known_device = ml_main_controller->launchers[known_it]) != NULL && found == 0; known_it++) {
      TRACE("k: %p f: %p\n", known_device->usb_device, found_device);
      if(known_device->usb_device == found_device){ 
        found = 1;
      }
    }
    if(found == 0) {
      ml_launcher_t *new_launcher = calloc(sizeof(ml_launcher_t), 1);
      TRACE("adding device.\n");
      if(new_launcher == NULL) {
        TRACE("Calloc failed...\n");
      } else {
        _ml_init_launcher(new_launcher, found_device);
        _ml_add_launcher(new_launcher);
      }
    }
  }
  free(found_launchers);
  pthread_rwlock_unlock(&(ml_main_controller->launcher_array_lock));
  return ML_NOT_IMPLEMENTED;
}


int16_t ml_get_launcher_array(ml_launcher_t ***new_arr) {
  
  return ML_NOT_IMPLEMENTED;
}

int16_t ml_free_launcher_array(ml_launcher_t **free_arr) {

  return ML_NOT_IMPLEMENTED;
}


/**
 * @brief Removes a launcher from the array. Be sure to lock the array for writing first
 *
 * @param launcher The launcher to remove
 *
 * @return A status value, ML_OK if it is okay.
 */
int16_t _ml_remove_launcher(ml_launcher_t * launcher) {
  /* This function is not thread safe, please lock the array first */
  for(int16_t i = 0; i < ml_main_controller->launcher_count; i++) {
    if(ml_main_controller->launchers[i] == launcher) {
      return _ml_remove_launcher_index(i);
    }
  }
  return ML_NOT_FOUND;
}

/**
 * @brief Removes a launcher from the array. Be sure to lock the array for writing first
 *
 * @param index The index of the item to remove
 *
 * @return A status value, ML_OK if it is okay.
 */
int16_t _ml_remove_launcher_index(int16_t index) {
  /* This function is not thread safe, please lock the array first */
  if(ml_main_controller->launchers[index] == NULL) {
    TRACE("Addition position was null.\n");
    return ML_POSITION_WAS_NULL;
  }
  if(index >= ml_main_controller->launcher_array_size || index < 0) {
    TRACE("Index out of bounds.\n");
    return ML_INDEX_OUT_OF_BOUNDS;
  }
  if(ml_main_controller->launcher_count <= 0) {
    TRACE("Nothing to remove.\n");
    return ML_COUNT_ZERO;
  }
  ml_main_controller->launcher_count -= 1;
  ml_main_controller->launchers[index] = NULL;
  return ML_OK;
}

/**
 * @brief adds a launcher to the array. Be sure to lock the array for writing first
 *
 * @param launcher The item to add
 *
 * @return A status value, ML_OK if it is okay
 */
int16_t _ml_add_launcher(ml_launcher_t *launcher) {
  /* This function is not thread safe, please lock the array first */
  for(int16_t i = 0; i < ml_main_controller->launcher_array_size; i++) {
    if(ml_main_controller->launchers[i] == NULL) {
      TRACE("adding launcher to index: %d\n", i);
      return _ml_add_launcher_index(launcher, i);
    }
  }
  // No freespace found, needs a resize.
  // Not yet implemented...
  return ML_NOT_IMPLEMENTED;
}

/**
 * @brief adds a launcher to the array. Be sure to lock the array for writing first
 *
 * @param launcher The launcher to add
 * @param index The index to insert it in
 *
 * @return A status code, ML_OK if it is okay
 */
int16_t _ml_add_launcher_index(ml_launcher_t *launcher, int16_t index) {
  /* This function is not thread safe, please lock the array first */
  if(ml_main_controller->launchers[index] != NULL) {
    TRACE("Addition position was not null.\n");
    return ML_POSITION_WAS_NOT_NULL;
  }
  ml_main_controller->launcher_count += 1;
  ml_main_controller->launchers[index] = launcher;
  return ML_NOT_IMPLEMENTED;
}

