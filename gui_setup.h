#ifndef GUI_SETUP_H
#define GUI_SETUP_H

#include "gui_setup.h"
#include "gps_setup.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *startGUI(void * arg);

void updateGUI(navpvt_data *data);

#endif