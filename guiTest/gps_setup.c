#include "gps_setup.h"
#include "gui_setup.h"
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <bcm2835.h>
#include <arpa/inet.h>
#include <pthread.h>

#define HEADER1 0xB5
#define HEADER2 0x62
#define MAX_OBSERVERS 5

// shut down flag
atomic_bool *gpsRunning;

//////////////// POLLING MESSAGES //////////////////

/**
 * Polls the NAV-PVT configuration. Should respond with a payload of 8 bytes.
*/
void pollNavPVT() {
  printf("Polling NAV-PVT configuration...\n");
  uint8_t pollNavPVT[] = {0xB5, 0x62, 0x06, 0x01, 0x02, 0x00, 0x01, 0x07, 0x11, 0x3A};
  bcm2835_spi_transfern((char*)pollNavPVT, sizeof(pollNavPVT));
}

/**
 * Polls the configuration of the modules solution and message rates. 
 * Should respond with a payload of 6 bytes. 
*/
void pollRate() {
  printf("Polling nav measurement and solution rate...\n");
  uint8_t pollRate[] = {0xB5, 0x62, 0x06, 0x08, 0x00, 0x00, 0x0E, 0x30};
  bcm2835_spi_transfern((char*)pollRate, sizeof(pollRate));
}

//////////////// CONFIGURATION MESSAGES //////////////////


/**
 * Sets the comms interface to SPI.
 * Sets the input and output protocol masks to UBX protocol only
 * Modifications are in class and id 0x06 0x00
*/
void setProtocol_UBX() {
  uint8_t cfg_ubx_only[] = {0xb5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x94};
    bcm2835_spi_transfern((char *)cfg_ubx_only, sizeof(cfg_ubx_only));
    printf("SET PROTOCOL UBX: SENT\n");
}

/**
 * enables nav pvt messages
*/
void enable_navPVT() {
  uint8_t config_navpt_on[] = {0xb5, 0x62, 0x06, 0x01, 0x08, 0x00, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0xde};
  bcm2835_spi_transfern((char *)config_navpt_on,sizeof(config_navpt_on));
  printf("UBX NAV-PVT ON: SENT\n");
}

/**
 * Sets the message rate to two messages per second
 * 4hz for the module to take gps measurements. 4 measurements per second.
 * 2hz for the module to output data to the receiver. two measurements for each nav solution. Or, one
 * message for every two measurements/solutions. 
 * Time reference set to 0x00 0x00 = UTC time
 * 
 * 2 measurements per message and messages once every 500ms. 
*/
void setRate_4x2() {
  uint8_t config_rate_4x2hz[] = {0xb5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xfa, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10, 0x98};
  bcm2835_spi_transfern((char *)config_rate_4x2hz, sizeof(config_rate_4x2hz));
  printf("RATE CONFIG 2hz: SENT\n");
}

/**
 * Sets message rate to one message per second
 * 2hz for every measurement taken
 * 1hz for every solution (message sent)
 * Time reference set to 0x00 0x00 = UTC time
*/
void setRate_2x1() {
  uint8_t config_rate_2x1hz[] = {0xb5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xF4, 0x01, 0x02, 0x00, 0x00, 0x00, 0x0B, 0x79};
  bcm2835_spi_transfern((char *)config_rate_2x1hz, sizeof(config_rate_2x1hz));
  printf("RATE CONFIG 2hz: SENT\n");
}

//////////////// READING MESSAGES //////////////////

/**
 * Reads the poll response on startup. Separated from the readUBX function to reduce
 * overhead during normal operation. 
 * Currently set to read messages from
 * Classes: 0x06
 * ID: 0x00, 0x01, 0x08
*/
void readPollResponse() {
  printf("Reading poll response...\n");
  // make incomingUBX struct to store message data
  incomingUBX pollResponse;  

  uint16_t header = 0xFFFF;
  while(header != 0xB562) {
    header = (header << 8) | bcm2835_spi_transfer(0xFF);
  }
  // read class and ID
  pollResponse.msgCls = bcm2835_spi_transfer(0xFF);
  pollResponse.msgID = bcm2835_spi_transfer(0xFF);
  // Read length
  pollResponse.msgLen = bcm2835_spi_transfer(0xFF);
  pollResponse.msgLen |= bcm2835_spi_transfer(0xFF) << 8;

  // Create a an array of msgLen size for the payload data
  uint8_t payload[pollResponse.msgLen]; 
  pollResponse.payload = payload;

  // Allocate memory for payload, then read payload
  memset(pollResponse.payload, 0xFF, pollResponse.msgLen);
  bcm2835_spi_transfern((char *)pollResponse.payload, pollResponse.msgLen);

  // get the checksum
  pollResponse.ck_a = bcm2835_spi_transfer(0xFF);
  pollResponse.ck_b = bcm2835_spi_transfer(0xFF);

  if(pollResponse.msgCls == 0x06 && pollResponse.msgID == 0x01) {
    checkConfigMsgSettings(pollResponse.payload);
  } else if(pollResponse.msgCls == 0x06 && pollResponse.msgID == 0x08) {
    checkConfigMsgSettings(pollResponse.payload);
  } else {
    printf("Unrecognized message type: 0x%02x, 0x%02x\n", pollResponse.msgCls, pollResponse.msgID);
  }
}


void checkRateSettings(uint8_t *payload) {
  printf("Polled nav and measurement rate settings (0x06 0x08)\n add code to display rate settings...\n");
}

void checkConfigMsgSettings(uint8_t *payload) {
  printf("Polled message config settings: \n");
  if(payload[0] == 0x01 && payload[1] == 0x07) {
    printf("Configured for NAV-PVT\n");
  } else {
    printf("Unknown configuration. Check payload: \n");
  }
}

//////////////// GPS START //////////////////

/**
 * Function called by thread to start reading gps data from module
*/
void *startGPS(void *arg) {
  // while loop to read gps data from module
  bufferStruct *buffers = (bufferStruct *)arg;
  incomingUBX *currentBuffer = buffers->fBuffer;
  navpvt_data *navpvt;
  gpsRunning = buffers->isRunning;
  
  atomic_bool useFrontBuffer = ATOMIC_VAR_INIT(true);
  while(atomic_load(gpsRunning)) {
    pthread_mutex_lock(&buffers->bufferLock);
    readUBX(currentBuffer);
    pthread_mutex_unlock(&buffers->bufferLock);
    navpvt = (navpvt_data*)currentBuffer->payload;  // loaded to print values to terminal
    printf("LAT: %d, LON: %d\n", navpvt->lat, navpvt->lon);
    if(atomic_load(&useFrontBuffer)) {
      currentBuffer = buffers->bBuffer;
      atomic_store(&useFrontBuffer, false);
    } else {
      currentBuffer = buffers->fBuffer;
      atomic_store(&useFrontBuffer, true);
    }
    usleep(900000);
  }
  printf("Value of atomic boolean: %s\n", atomic_load(gpsRunning) ? "true" : "false");
  return NULL;
}

// Original readUBX Function:
// /**
//  * Reads the UBX message from the module
//  * right now, only reading nav pvt, so pretty easy
// */
void readUBX(incomingUBX *msg) {
  // consider dynamic allocation for payload sizes in case you want different message types
  // consider error checking and handling for checksums
  uint16_t header = 0xFFFF;
  while(header != 0xB562) {
    header = (header << 8) | bcm2835_spi_transfer(0xFF);
  }
  // Read class and ID
  msg->msgCls = bcm2835_spi_transfer(0xFF);
  msg->msgID = bcm2835_spi_transfer(0xFF);
  // Read length
  msg->msgLen = bcm2835_spi_transfer(0xFF);
  msg->msgLen |= bcm2835_spi_transfer(0xFF) << 8;
  // Allocate memory for payload
  memset(msg->payload, 0xFF, msg->msgLen);
  bcm2835_spi_transfern((char *) msg->payload, msg->msgLen);
  // Read checksum
  msg->ck_a = bcm2835_spi_transfer(0xFF);
  msg->ck_b = bcm2835_spi_transfer(0xFF);
}


/**
 * Reads UBX messages from the module.
 * Currently reads ACK/NACK and NAVPVT messages only. 
*/
// void readUBX(incomingUBX *msg) {
//   uint16_t header = 0xFFFF;
//   while(header != 0xB562) {
//     header = (header << 8) | bcm2835_spi_transfer(0xFF);
//   }
//   // read class and ID
//   msg->msgCls = bcm2835_spi_transfer(0xFF);
//   msg->msgID = bcm2835_spi_transfer(0xFF);
//   // Read length
//   msg->msgLen = bcm2835_spi_transfer(0xFF);
//   msg->msgLen |= bcm2835_spi_transfer(0xFF) << 8;
//   // check what message type is received
//   if(msg->msgCls == 0x01 && msg->msgID == 0x07) {
//     // it's a nav pvt message
//     // Allocate memory for payload, then read payload
//     memset(msg->payload, 0xFF, msg->msgLen);
//     bcm2835_spi_transfern((char *)msg->payload, msg->msgLen);
//   } else if(msg->msgCls == 0x05) {
//     if(msg->msgID == 0x01) {
//       // this is an ACK
//       uint8_t ackedClass = bcm2835_spi_transfer(0xFF);
//       uint8_t ackedID = bcm2835_spi_transfer(0xFF);
//       printf("Received ACK for message Class: 0x%02X, ID: 0x%02X\n", ackedClass, ackedID);
//     } else if(msg->msgID == 0x00) {
//       // this is a nack
//       uint8_t nackedClass = bcm2835_spi_transfer(0xFF);
//       uint8_t nackedID = bcm2835_spi_transfer(0xFF);
//       printf("Received NACK for message Class: 0x%02X, ID: 0x%02X\n", nackedClass, nackedID);
//     }
//   }
//   // read checksum
//   msg->ck_a = bcm2835_spi_transfer(0xFF);
//   msg->ck_b = bcm2835_spi_transfer(0xFF);
// }



