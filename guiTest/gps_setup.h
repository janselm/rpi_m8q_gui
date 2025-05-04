#ifndef GPS_SETUP_H
#define GPS_SETUP_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdatomic.h>
#include <pthread.h>  

// NAV PVT STRUCT
typedef struct navpvt_data {
  uint32_t iTOW; // GPS time of week of the navigation epoch: ms
  uint16_t year; // Year (UTC)
  uint8_t month; // Month, range 1..12 (UTC)
  uint8_t day;   // Day of month, range 1..31 (UTC)
  uint8_t hour;  // Hour of day, range 0..23 (UTC)
  uint8_t min;   // Minute of hour, range 0..59 (UTC)
  uint8_t sec;   // Seconds of minute, range 0..60 (UTC)
  union {
    uint8_t all;
    struct {
      uint8_t validDate : 1;     // 1 = valid UTC Date
      uint8_t validTime : 1;     // 1 = valid UTC time of day
      uint8_t fullyResolved : 1; // 1 = UTC time of day has been fully resolved (no seconds uncertainty).
      uint8_t validMag : 1;      // 1 = valid magnetic declination
    } bits;
  } valid;
  uint32_t tAcc;   // Time accuracy estimate (UTC): ns
  int32_t nano;    // Fraction of second, range -1e9 .. 1e9 (UTC): ns
  uint8_t fixType; // GNSSfix Type:
                   // 0: no fix
                   // 1: dead reckoning only
                   // 2: 2D-fix
                   // 3: 3D-fix
                   // 4: GNSS + dead reckoning combined
                   // 5: time only fix
  union {
    uint8_t all;
    struct {
      uint8_t gnssFixOK : 1; // 1 = valid fix (i.e within DOP & accuracy masks)
      uint8_t diffSoln : 1;  // 1 = differential corrections were applied
      uint8_t psmState : 3;
      uint8_t headVehValid : 1; // 1 = heading of vehicle is valid, only set if the receiver is in sensor fusion mode
      uint8_t carrSoln : 2;     // Carrier phase range solution status:
                                // 0: no carrier phase range solution
                                // 1: carrier phase range solution with floating ambiguities
                                // 2: carrier phase range solution with fixed ambiguities
    } bits;
  } flags;
  union {
    uint8_t all;
    struct {
      uint8_t reserved : 5;
      uint8_t confirmedAvai : 1; // 1 = information about UTC Date and Time of Day validity confirmation is available
      uint8_t confirmedDate : 1; // 1 = UTC Date validity could be confirmed
      uint8_t confirmedTime : 1; // 1 = UTC Time of Day could be confirmed
    } bits;
  } flags2;
  uint8_t numSV;    // Number of satellites used in Nav Solution
  int32_t lon;      // Longitude: deg * 1e-7
  int32_t lat;      // Latitude: deg * 1e-7
  int32_t height;   // Height above ellipsoid: mm
  int32_t hMSL;     // Height above mean sea level: mm
  uint32_t hAcc;    // Horizontal accuracy estimate: mm
  uint32_t vAcc;    // Vertical accuracy estimate: mm
  int32_t velN;     // NED north velocity: mm/s
  int32_t velE;     // NED east velocity: mm/s
  int32_t velD;     // NED down velocity: mm/s
  int32_t gSpeed;   // Ground Speed (2-D): mm/s
  int32_t headMot;  // Heading of motion (2-D): deg * 1e-5
  uint32_t sAcc;    // Speed accuracy estimate: mm/s
  uint32_t headAcc; // Heading accuracy estimate (both motion and vehicle): deg * 1e-5
  uint16_t pDOP;    // Position DOP * 0.01
  union {
    uint8_t all;
    struct {
      uint8_t invalidLlh : 1; // 1 = Invalid lon, lat, height and hMSL
    } bits;
  } flags3;
  uint8_t reserved1[5];
  int32_t headVeh; // Heading of vehicle (2-D): deg * 1e-5
  int16_t magDec;  // Magnetic declination: deg * 1e-2
  uint16_t magAcc; // Magnetic declination accuracy: deg * 1e-2
} navpvt_data;


typedef struct incomingUBX {
  uint8_t sync1;
  uint8_t sync2;
  uint8_t msgCls;
  uint8_t msgID;
  uint16_t msgLen;
  uint8_t *payload;
  uint8_t ck_a;
  uint8_t ck_b;
} incomingUBX;


typedef struct bufferStruct {
  incomingUBX *fBuffer;
  incomingUBX *bBuffer;
  atomic_bool *isRunning;
  pthread_mutex_t bufferLock;   
} bufferStruct;


void pollConfig();

void pollProtocol();

void pollNavPVT();

void pollRate();

void setProtocol_UBX();

void enable_navPVT();

void setRate_4x2();

void setRate_2x1();

void readUBX(incomingUBX *msg);

void *startGPS(void *arg);

void readPollResponse();

void checkRateSettings(uint8_t *payload);

void checkConfigMsgSettings(uint8_t *payload);


#endif

