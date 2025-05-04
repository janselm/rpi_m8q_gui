/**
 * @file        gps_setup.c
 * @author      Joshua Anselm
 * @date        2025-05-04
 * @version     1.0
 * @brief       GPS communication and UBX protocol handling over SPI.
 *
 * @details     Implements configuration, polling, and data reading functions for a u-blox GPS
 *              module using the UBX binary protocol via SPI on a Raspberry Pi. This file handles:
 *              - Sending configuration and polling commands to the GPS module
 *              - Parsing UBX responses such as NAV-PVT, CFG-RATE, and CFG-MSG
 *              - Verifying ACK/NACK responses to configuration commands
 *              - Spawning a GPS readout thread that buffers and prints positional data
 *              - Integrating with a GUI via idle callbacks for display updates
 *
 *              Functions support both startup polling and continuous runtime parsing.
 *              Communication is managed using the BCM2835 SPI library.
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

atomic_bool *gpsRunning;

//////////////// POLLING MESSAGES //////////////////

/**
 * @brief Sends a UBX command to poll the NAV-PVT message settings.
 *
 * Expected to receive a response containing the configuration of
 * the NAV-PVT message enablement, typically with a payload of 8 bytes.
 */
void pollNavPVT() {
  printf("Polling NAV-PVT configuration...\n");
  uint8_t pollNavPVT[] = {0xB5, 0x62, 0x06, 0x01, 0x02, 0x00, 0x01, 0x07, 0x11, 0x3A};
  bcm2835_spi_transfern((char*)pollNavPVT, sizeof(pollNavPVT));
}

/**
 * @brief Sends a UBX command to poll the module's navigation rate configuration.
 *
 * Expected to receive a response describing the solution and measurement rates,
 * with a payload of 6 bytes.
 */
void pollRate() {
  printf("Polling nav measurement and solution rate...\n");
  uint8_t pollRate[] = {0xB5, 0x62, 0x06, 0x08, 0x00, 0x00, 0x0E, 0x30};
  bcm2835_spi_transfern((char*)pollRate, sizeof(pollRate));
}

//////////////// CONFIGURATION MESSAGES //////////////////

/**
 * @brief Configures the GPS module to use UBX protocol only over SPI.
 *
 * Sends a CFG-PRT message to restrict input/output to UBX only,
 * targeting the SPI interface settings.
 */
void setProtocol_UBX() {
  uint8_t cfg_ubx_only[] = {0xb5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x94};
  bcm2835_spi_transfern((char *)cfg_ubx_only, sizeof(cfg_ubx_only));
  printf("SET PROTOCOL UBX: SENT\n");
}

/**
 * @brief Enables periodic NAV-PVT messages from the GPS module.
 *
 * Sends a CFG-MSG command to enable NAV-PVT output over the SPI interface.
 */
void enable_navPVT() {
  uint8_t config_navpt_on[] = {0xb5, 0x62, 0x06, 0x01, 0x08, 0x00, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0xde};
  bcm2835_spi_transfern((char *)config_navpt_on, sizeof(config_navpt_on));
  printf("UBX NAV-PVT ON: SENT\n");
}

/**
 * @brief Sets the GPS module to output 2 messages per second.
 *
 * Configures 4Hz measurement rate and 2Hz navigation solution rate.
 * This results in two measurements per message, output every 500ms.
 */
void setRate_4x2() {
  uint8_t config_rate_4x2hz[] = {0xb5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xfa, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10, 0x98};
  bcm2835_spi_transfern((char *)config_rate_4x2hz, sizeof(config_rate_4x2hz));
  printf("RATE CONFIG 2hz: SENT\n");
}

/**
 * @brief Sets the GPS module to output 1 message per second.
 *
 * Configures 2Hz measurement rate and 1Hz navigation solution rate.
 * Output uses UTC as the time reference.
 */
void setRate_2x1() {
  uint8_t config_rate_2x1hz[] = {0xb5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xF4, 0x01, 0x02, 0x00, 0x00, 0x00, 0x0B, 0x79};
  bcm2835_spi_transfern((char *)config_rate_2x1hz, sizeof(config_rate_2x1hz));
  printf("RATE CONFIG 2hz: SENT\n");
}

//////////////// READING MESSAGES //////////////////

/**
 * @brief Reads a UBX poll response message during startup.
 *
 * Waits for a valid UBX header, reads class, ID, and payload, then
 * dispatches the message for specific parsing based on type.
 */
void readPollResponse() {
  printf("Reading poll response...\n");

  incomingUBX pollResponse;

  uint16_t header = 0xFFFF;
  while(header != 0xB562) {
    header = (header << 8) | bcm2835_spi_transfer(0xFF);
  }

  pollResponse.msgCls = bcm2835_spi_transfer(0xFF);
  pollResponse.msgID = bcm2835_spi_transfer(0xFF);
  pollResponse.msgLen = bcm2835_spi_transfer(0xFF);
  pollResponse.msgLen |= bcm2835_spi_transfer(0xFF) << 8;

  uint8_t payload[pollResponse.msgLen];
  pollResponse.payload = payload;

  memset(pollResponse.payload, 0xFF, pollResponse.msgLen);
  bcm2835_spi_transfern((char *)pollResponse.payload, pollResponse.msgLen);

  pollResponse.ck_a = bcm2835_spi_transfer(0xFF);
  pollResponse.ck_b = bcm2835_spi_transfer(0xFF);

  printf("Received poll response: class=0x%02X id=0x%02X len=%d\n",
    pollResponse.msgCls, pollResponse.msgID, pollResponse.msgLen);

  if (pollResponse.msgCls == 0x06 && pollResponse.msgID == 0x01) {
    checkConfigMsgSettings(pollResponse.payload);
  } else if (pollResponse.msgCls == 0x06 && pollResponse.msgID == 0x08) {
    checkRateSettings(pollResponse.payload);
  } else {
    printf("Unrecognized poll response: class=0x%02X id=0x%02X\n",
            pollResponse.msgCls, pollResponse.msgID);
  }
}

/**
 * @brief Decodes the CFG-RATE payload to print measurement and navigation rates.
 */
void checkRateSettings(uint8_t *payload) {
  printf("CFG-RATE settings:\n");
  uint16_t measRate = payload[0] | (payload[1] << 8);
  uint16_t navRate  = payload[2] | (payload[3] << 8);
  uint16_t timeRef  = payload[4] | (payload[5] << 8);

  printf("  Measurement rate: %d ms\n", measRate);
  printf("  Navigation rate:  1 every %d cycles\n", navRate);
  printf("  Time reference:   %s\n", timeRef == 0 ? "UTC" : "GPS time");
}

/**
 * @brief Parses CFG-MSG payload to determine if NAV-PVT messages are enabled over SPI.
 */
void checkConfigMsgSettings(uint8_t *payload) {
  uint8_t msgClass = payload[0];
  uint8_t msgID    = payload[1];
  uint8_t rateSPI  = payload[6];

  printf("CFG-MSG settings:\n");
  printf("  Message: Class 0x%02X, ID 0x%02X\n", msgClass, msgID);
  printf("  SPI Output Rate: %d\n", rateSPI);

  if (msgClass == 0x01 && msgID == 0x07 && rateSPI > 0) {
    printf("  => NAV-PVT is ENABLED over SPI\n");
  } else if (msgClass == 0x01 && msgID == 0x07) {
    printf("  => NAV-PVT is DISABLED over SPI\n");
  }
}

/**
 * @brief Reads an ACK or NACK UBX response following a configuration command.
 *
 * Matches expected response class (0x05) and determines ACK/NACK status
 * based on the message ID and payload.
 */
void readACKResponse(const char *label) {
  uint16_t header = 0xFFFF;
  while (header != 0xB562) {
    header = (header << 8) | bcm2835_spi_transfer(0xFF);
  }

  uint8_t cls = bcm2835_spi_transfer(0xFF);
  uint8_t id = bcm2835_spi_transfer(0xFF);
  uint8_t lenL = bcm2835_spi_transfer(0xFF);
  uint8_t lenH = bcm2835_spi_transfer(0xFF);
  uint8_t payload[2] = {0xFF, 0xFF};

  if ((lenL | (lenH << 8)) == 2) {
    bcm2835_spi_transfern((char *)payload, 2);
  }

  uint8_t ck_a = bcm2835_spi_transfer(0xFF);
  uint8_t ck_b = bcm2835_spi_transfer(0xFF);
  (void) ck_a;
  (void) ck_b;

  if (cls == 0x05 && id == 0x01) {
    printf("ACK received for %s (cls=0x%02X id=0x%02X)\n", label, payload[0], payload[1]);
  } else if (cls == 0x05 && id == 0x00) {
    printf("NACK received for %s (cls=0x%02X id=0x%02X)\n", label, payload[0], payload[1]);
  } else {
    printf("Unexpected response after %s: cls=0x%02X id=0x%02X\n", label, cls, id);
  }
}

//////////////// GPS START //////////////////

/**
 * @brief GPS reader thread entry point.
 *
 * Continuously reads NAV-PVT messages into double-buffered memory.
 * Alternates front/back buffers, prints latitude/longitude,
 * and schedules GUI label updates using GLib idle callbacks.
 *
 * @param arg Pointer to bufferStruct used for synchronization and data sharing
 * @return NULL
 */
void *startGPS(void *arg) {
  bufferStruct *buffers = (bufferStruct *)arg;
  incomingUBX *currentBuffer = buffers->fBuffer;
  navpvt_data *navpvt;
  gpsRunning = buffers->isRunning;
  
  atomic_bool useFrontBuffer = ATOMIC_VAR_INIT(true);
  while(atomic_load(gpsRunning)) {
    pthread_mutex_lock(&buffers->bufferLock);
    readUBX(currentBuffer);
    pthread_mutex_unlock(&buffers->bufferLock);
    navpvt = (navpvt_data*)currentBuffer->payload;
    printf("LAT: %d, LON: %d\n", navpvt->lat, navpvt->lon);
    if(atomic_load(&useFrontBuffer)) {
      currentBuffer = buffers->bBuffer;
      atomic_store(&useFrontBuffer, false);
    } else {
      currentBuffer = buffers->fBuffer;
      atomic_store(&useFrontBuffer, true);
    }
    g_idle_add(updateGPSLabels, GINT_TO_POINTER(atomic_load(&useFrontBuffer)));
    usleep(900000);
  }
  printf("Value of atomic boolean: %s\n", atomic_load(gpsRunning) ? "true" : "false");
  return NULL;
}

/**
 * @brief Reads a UBX message from the GPS module.
 *
 * This version is simplified for NAV-PVT messages only.
 * Reads header, class, ID, length, payload, and checksum.
 *
 * @param msg Pointer to an incomingUBX struct to be populated
 */
void readUBX(incomingUBX *msg) {
  uint16_t header = 0xFFFF;
  while(header != 0xB562) {
    header = (header << 8) | bcm2835_spi_transfer(0xFF);
  }

  msg->msgCls = bcm2835_spi_transfer(0xFF);
  msg->msgID = bcm2835_spi_transfer(0xFF);
  msg->msgLen = bcm2835_spi_transfer(0xFF);
  msg->msgLen |= bcm2835_spi_transfer(0xFF) << 8;
  printf("UBX msg received: class=0x%02X id=0x%02X len=%d\n", msg->msgCls, msg->msgID, msg->msgLen);

  memset(msg->payload, 0xFF, msg->msgLen);
  bcm2835_spi_transfern((char *) msg->payload, msg->msgLen);

  msg->ck_a = bcm2835_spi_transfer(0xFF);
  msg->ck_b = bcm2835_spi_transfer(0xFF);
}
