/**
 * @file        gui_setup.h
 * @author      Joshua Anselm
 * @date        2025-05-04
 * @version     1.0
 * @brief       Header interface for GUI components in a GPS monitoring system.
 *
 * @details     This file declares the functions used to build and manage the GTK-based GUI
 *              for a GPS monitoring application. It includes interfaces for starting the GUI
 *              loop, updating real-time GPS data on screen, handling user interactions (e.g.,
 *              time zone selection, shutdown), and simulating tank pressure display states.
 *
 *              The GUI integrates with a backend buffer structure shared across threads and 
 *              reflects live data including location, speed, and air system status.
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


#ifndef GUI_SETUP_H
#define GUI_SETUP_H

/**
 * @file gui_setup.h
 * @brief Function declarations for GUI initialization and updates.
 *
 * Defines the interface for managing GTK GUI rendering, event handling,
 * GPS data updates, and simulated pressure visualization.
 */

#include "gui_setup.h"
#include "gps_setup.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <gtk/gtk.h>

/**
 * @brief Starts the GUI in a separate thread.
 *
 * Initializes GTK, sets up layout, and enters the GTK main loop.
 *
 * @param arg Pointer to bufferStruct containing GPS data and sync primitives.
 * @return void* (unused)
 */
void *startGUI(void * arg);

/**
 * @brief Updates GPS labels and map display in the GUI.
 *
 * Invoked periodically from another thread using g_idle_add.
 * Reads buffer data and refreshes text and map elements.
 *
 * @param data Boolean flag (as gpointer) indicating which buffer to read from.
 * @return gboolean Always returns G_SOURCE_REMOVE to stop repeat invocation.
 */
gboolean updateGPSLabels(gpointer data);

/**
 * @brief Handles the close button click.
 *
 * Signals shutdown and exits the GTK main loop.
 *
 * @param widget The button widget (unused)
 * @param data Additional data (unused)
 */
void on_close_button_clicked(GtkWidget *widget, gpointer data);

/**
 * @brief Background thread that simulates tank pressure states.
 *
 * Alternates boolean flags and schedules pressure icon updates.
 *
 * @param arg Unused
 * @return void* (unused)
 */
void *simulatePressure(void *arg);

#endif
