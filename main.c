/**
 * @file        main.c
 * @author      Joshua Anselm
 * @date        2025-05-04
 * @version     1.0
 * @brief       Entry point for GPS system initialization and execution.
 *
 * @details     This file contains the `main()` function and orchestrates system initialization 
 *              for a GPS monitoring application on a Raspberry Pi. It:
 *              - Initializes the SPI interface and BCM2835 library
 *              - Sets up double-buffered memory for UBX GPS data
 *              - Sends configuration messages to the GPS module
 *              - Polls initial GPS settings for verification
 *              - Starts two threads: one for GPS polling and another for simulating pressure data
 *              - Launches the GTK-based GUI in the main thread
 *
 *              Upon exit, it performs cleanup of threads, SPI state, mutexes, and allocated memory.
 *              The GPS module is configured to communicate using UBX protocol over SPI.
 *
 * @license     MIT License
 *              Copyright (c) 2025 Joshua Anselm
 *
 *              Permission is hereby granted, free of charge, to any person obtaining a copy
 *              of this software and associated documentation files (the "Software"), to deal
 *              in the Software without restriction, including without limitation the rights
 *              to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *              copies of the Software, and to permit persons to whom the Software is
 *              furnished to do so, subject to the following conditions:
 *
 *              The above copyright notice and this permission notice shall be included in
 *              all copies or substantial portions of the Software.
 *
 *              THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *              IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *              FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *              AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *              LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *              OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *              SOFTWARE.
 */

#include "gps_setup.h"
#include "gui_setup.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <bcm2835.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#define I2C_ADDRESS 0x42
#define SPI_BASE_CLOCK_SPEED 500000000
#define SPI_BAUD_RATE 115200

/**
 * @brief Polls the GPS module for configuration status.
 *
 * This function sends requests to retrieve:
 *  - The current message rate configuration
 *  - The current NAV-PVT message status
 *
 * Short delays are added between commands to allow the module to respond.
 */
void pollModule() {
  pollRate();
  usleep(10000);
  readPollResponse();

  pollNavPVT();
  usleep(10000);
  readPollResponse();
}


/**
 * @brief Sends a set of configuration commands to the GPS module.
 *
 * Configures the module to:
 *  - Use the UBX protocol exclusively over SPI
 *  - Set the message rate to 2 Hz with a solution rate of 4 Hz
 *  - Enable periodic output of NAV-PVT messages
 *
 * Temporary buffers are allocated to capture ACK/NACK responses from the module.
 * These buffers are freed after configuration is complete.
 */
void sendConfig() {
  incomingUBX *tempBuffer = (incomingUBX *)malloc(sizeof(incomingUBX));
  navpvt_data *tempPayload = (navpvt_data *)malloc(sizeof(navpvt_data));
  tempBuffer->payload = (uint8_t*)tempPayload;

  setProtocol_UBX();
  usleep(10000);
  readACKResponse("setProtocol_UBX");
  
  setRate_2x1();
  usleep(10000);
  readACKResponse("setRate_2x1");
  
  enable_navPVT();
  usleep(10000);
  readACKResponse("enable_navPVT");  

  free(tempBuffer->payload);
  free(tempBuffer);
}


/**
 * @brief Main application entry point.
 *
 * Initializes SPI and BCM2835 libraries, prepares double buffers for GPS data,
 * and creates worker threads for GPS reading and pressure simulation.
 *
 * The GUI is launched in the main thread and interacts with the shared buffer structure.
 * Proper cleanup of threads, memory, mutexes, and SPI state is performed before exit.
 *
 * @return int Exit status code
 */
int main(void) {
  const char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (xdg_runtime_dir) {
    printf("XDG_RUNTIME_DIR: %s\n", xdg_runtime_dir);
  } else {
    printf("XDG_RUNTIME_DIR is not set.\n");
  }

  atomic_bool atomic_bool_isRunning = ATOMIC_VAR_INIT(true);

  incomingUBX *frontBuffer = (incomingUBX *)malloc(sizeof(incomingUBX));
  incomingUBX *backBuffer = (incomingUBX *)malloc(sizeof(incomingUBX));
  navpvt_data *frontPayload = (navpvt_data *)malloc(sizeof(navpvt_data));
  navpvt_data *backPayload = (navpvt_data *)malloc(sizeof(navpvt_data));
  
  frontBuffer->payload = (uint8_t*)frontPayload;
  backBuffer->payload = (uint8_t*)backPayload;

  bufferStruct buffers;
  buffers.fBuffer = frontBuffer;
  buffers.bBuffer = backBuffer;
  buffers.isRunning = &atomic_bool_isRunning;

  pthread_t gps_thread;
  pthread_t pressure_thread;

  uint32_t divider = (SPI_BASE_CLOCK_SPEED / SPI_BAUD_RATE);

  if (!bcm2835_init()) {
    printf("Error: failed to initialize bcm2835\n");
    return 1;
  }
  printf("BCM2835 Initialized\n");

  bcm2835_spi_begin();
  printf("SPI STARTED\n");
  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
  printf("Bit order set...\n");
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
  printf("SPI data mode set\n");
  bcm2835_spi_setClockDivider(divider);
  printf("Clock divider set...\n");
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
  printf("Chip Select pin set\n");
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
  printf("GPIO and SPI Configured\n\n");

  sendConfig();
  usleep(50000);
  pollModule();
  
  pthread_mutex_init(&buffers.bufferLock, NULL);

  if (pthread_create(&gps_thread, NULL, startGPS, (void*)&buffers)) {
    printf("Error: Failed to create GPS thread\n");
    return -1;
  }
  if (pthread_create(&pressure_thread, NULL, simulatePressure, NULL)) {
    printf("Error: Failed to create pressure simulation thread\n");
    return -1;
  }

  startGUI((void*)&buffers);

  pthread_join(gps_thread, NULL);
  pthread_join(pressure_thread, NULL);
  pthread_mutex_destroy(&buffers.bufferLock);

  free(frontBuffer->payload);
  free(backBuffer->payload);
  free(frontBuffer);
  free(backBuffer);
  bcm2835_spi_end();
  bcm2835_close();

  return 0;
}
