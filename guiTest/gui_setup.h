#ifndef GUI_SETUP_H
#define GUI_SETUP_H

#include "gui_setup.h"
#include "gps_setup.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <gtk/gtk.h>

void *startGUI(void * arg);

// void updateGUI(navpvt_data *data);

gboolean updateGPSLabels(gpointer data);

void on_close_button_clicked(GtkWidget *widget, gpointer data);

// void on_time_zone_changed(GtkComboBox *widget, gpointer data);

#endif