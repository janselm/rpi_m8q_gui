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

// define all necessary configuration for all serial communication protocols. 
// modify as necessary
#define I2C_ADDRESS 0x42
#define SPI_BASE_CLOCK_SPEED 500000000
#define SPI_BAUD_RATE 115200

/**
 * Polls the module for the current configuration of rate and 
 * UBX protocol NAV-PVT status. 
*/
void pollModule() {
  pollRate();
  readPollResponse();
  pollNavPVT();
  readPollResponse();
}

/**
 * Sends configuration messages to the module. Configured to set:
 * - UBX protocol only over SPI
 * - Solution rate of 4hz and message rate of 2hz
 * - Enable UBX NAV-PVT messages
*/
void sendConfig() {
// send configuration if necessary
  incomingUBX *tempBuffer = (incomingUBX *)malloc(sizeof(incomingUBX));
  navpvt_data *tempPayload = (navpvt_data *)malloc(sizeof(navpvt_data));
  tempBuffer->payload = (uint8_t*)tempPayload;

  // Send configuration messages and read ACK/NACK into tempBuffer
  setProtocol_UBX();
  usleep(10000);  // Delay for 10ms

  setRate_2x1();
  usleep(10000);  // Delay for 10ms

  enable_navPVT();
  usleep(10000);  // Delay for 10ms

  // Free the temporary buffer
  free(tempBuffer->payload);
  free(tempBuffer);
}


int main(void) {
    const char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (xdg_runtime_dir) {
        printf("XDG_RUNTIME_DIR: %s\n", xdg_runtime_dir);
    } else {
        printf("XDG_RUNTIME_DIR is not set.\n");
    }



  // shutdown flag for threads
  atomic_bool atomic_bool_isRunning = ATOMIC_VAR_INIT(true);
  // double buffers for gps data
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

  uint32_t divider = (SPI_BASE_CLOCK_SPEED/SPI_BAUD_RATE);

  if(!bcm2835_init()) {
    printf("Error: falied to initialize bcm2835\n");
    return 1;
  }
  printf("BCM2835 Initialized\n");

  bcm2835_spi_begin();
  printf("SPI STARTED\n");
  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
  printf("Bit order set...\n");
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
  printf("SPI data mode set\n");
  bcm2835_spi_setClockDivider(divider); // For example, this will set the SPI speed to 250 KHz
  printf("Clock divider set...\n");
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0); // Use the default chip select pin (CE0)
  printf("Chip Selct pin set\n");
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
  printf("GPIO and SPI Configured\n\n");

  // poll the module for configuration. Requires modified readUBX function
  // to read and interpret configuration data.
  // pollModule();

  // send configuration message to the module. 
  // may not be necessary if the pollModule function is properly implemented
  // to check the config on startup. 
  sendConfig();

  pthread_mutex_init(&buffers.bufferLock, NULL);

  if(pthread_create(&gps_thread, NULL, startGPS, (void*)&buffers)) {
    printf("Error: Failed to create GPS thread\n");
    return -1;
  }

  // GUI must run in main thread
  startGUI((void*)&buffers);

  pthread_join(gps_thread, NULL);
  pthread_mutex_destroy(&buffers.bufferLock);

  free(frontBuffer->payload);
  free(backBuffer->payload);
  free(frontBuffer);
  free(backBuffer);
  bcm2835_spi_end();
  bcm2835_close();

  return 0;
}
