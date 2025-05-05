/**
 * @file        gui_setup.c
 * @author      Joshua Anselm
 * @date        2025-05-04
 * @version     1.0
 * @brief       GUI rendering and real-time display for GPS and tank pressure data.
 *
 * @details     This source file implements all graphical user interface logic for a GPS-based 
 *              monitoring system using GTK. It handles window and widget initialization, 
 *              user input via dropdowns and buttons, and periodic updates to reflect real-time 
 *              GPS location, speed, system time, and simulated air tank pressure.
 *
 *              The GUI interfaces with backend data buffers and uses GTK idle callbacks to 
 *              safely refresh visual elements from background threads. It also handles drawing 
 *              a map and positioning a marker based on GPS coordinates.
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


 #include "gui_setup.h"
 #include "gps_setup.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <string.h>
 #include <bcm2835.h>
 #include <arpa/inet.h>
 #include <gtk/gtk.h>
 #include <time.h>
 #include <stdbool.h>
 #include <stdatomic.h>
 
 // Struct of GUI elements
 typedef struct {
   GtkWidget *window;
   GtkWidget *latitudeLabel;
   GtkWidget *longitudeLabel;
   GtkWidget *timeLabel;
   GtkWidget *speedLabel;
   GtkWidget *timeZoneDropdown;
   GtkWidget *closeButton;
   GtkWidget *leftLabel;
   GtkWidget *primaryAirLabel;
   GtkWidget *primaryAirCircle;
   GtkWidget *secondaryAirLabel;
   GtkWidget *secondaryAirCircle;
   GtkWidget *mapArea;
   GtkWidget *scrollWindow;
 } GuiWindow;
 
 // GuiWindow instance
 GuiWindow guiWindow;
 
 // Global buffer and state pointers
 bufferStruct *guiBufferStruct;
 incomingUBX *guiFrontBuffer;
 incomingUBX *guiBackBuffer;
 navpvt_data *navpvt;
 atomic_bool *guiRunning;
 
 // UTC offset in hours (adjustable via dropdown)
 int utcOffset = 0;
 
 // Simulated tank pressure states
 static bool isPrimaryPressureOK = false;
 static bool isSecondaryPressureOK = false;
 
 // Map bounding box (hardcoded to test image)
 #define MAP_LAT_TOP     // lat top val
 #define MAP_LAT_BOTTOM  // lat bottom val
 #define MAP_LON_LEFT   // lon val
 #define MAP_LON_RIGHT  // lon val
 
 /**
  * @brief Updates GUI pressure indicators with colored icons.
  *
  * Called via idle handler to avoid concurrency issues with GTK.
  */
 gboolean updatePressureDisplay(gpointer data) {
   const char *primaryImage = isPrimaryPressureOK ? "green_circle.png" : "red_circle.png";
   const char *secondaryImage = isSecondaryPressureOK ? "green_circle.png" : "red_circle.png";
 
   GdkPixbuf *pix = gdk_pixbuf_new_from_file_at_scale(primaryImage, 50, 50, TRUE, NULL);
   gtk_image_set_from_pixbuf(GTK_IMAGE(guiWindow.primaryAirCircle), pix);
   g_object_unref(pix);
 
   pix = gdk_pixbuf_new_from_file_at_scale(secondaryImage, 50, 50, TRUE, NULL);
   gtk_image_set_from_pixbuf(GTK_IMAGE(guiWindow.secondaryAirCircle), pix);
   g_object_unref(pix);
 
   return G_SOURCE_REMOVE;
 }
 
 /**
  * @brief Background thread simulating air tank pressure state changes.
  *
  * Alternates boolean states every 3 seconds and triggers display update.
  */
 void *simulatePressure(void *arg) {
   while (atomic_load(guiRunning)) {
     isPrimaryPressureOK = !isPrimaryPressureOK;
     isSecondaryPressureOK = !isSecondaryPressureOK;
     g_idle_add(updatePressureDisplay, NULL);
     sleep(3);
   }
   return NULL;
 }
 
 /**
  * @brief Callback for timezone selection dropdown.
  *
  * Sets UTC offset based on user selection.
  */
 void on_time_zone_changed(GtkComboBox *widget, gpointer data) {
   gchar *selectedTimeZone = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget));
   if (selectedTimeZone != NULL) {
     if (strcmp(selectedTimeZone, "Mountain Standard Time (MST)") == 0) {
       utcOffset = -7;
     } else if (strcmp(selectedTimeZone, "Eastern Standard Time (EST)") == 0) {
       utcOffset = -5;
     } else if (strcmp(selectedTimeZone, "Pacific Standard Time (PST)") == 0) {
       utcOffset = -8;
     } else if (strcmp(selectedTimeZone, "Central Standard Time (CST)") == 0) {
       utcOffset = -6;
     } else if (strcmp(selectedTimeZone, "Alaska Standard Time (AKST)") == 0) {
       utcOffset = -9;
     } else if (strcmp(selectedTimeZone, "Hawaii-Aleutian Standard Time (HAST)") == 0) {
       utcOffset = -10;
     }
     g_free(selectedTimeZone);
   }
 }
 
 /**
  * @brief Callback for the CLOSE button. Signals termination and quits GTK loop.
  */
 void on_close_button_clicked(GtkWidget *widget, gpointer data) {
   atomic_store(guiRunning, false);
   usleep(500000);
   gtk_main_quit();
 }
 
 /**
  * @brief Callback for toggling air pressure icons on click.
  *
  * Swaps image between red and green states for testing or debugging.
  */
 gboolean on_circle_clicked(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
   static bool isPrimaryRed = true;
   static bool isSecondaryRed = true;
   GdkPixbuf *pixbuf;
 
   if (widget == guiWindow.primaryAirCircle) {
     pixbuf = gdk_pixbuf_new_from_file_at_scale(
         isPrimaryRed ? "green_circle.png" : "red_circle.png", 50, 50, TRUE, NULL);
     gtk_image_set_from_pixbuf(GTK_IMAGE(guiWindow.primaryAirCircle), pixbuf);
     g_object_unref(pixbuf);
     isPrimaryRed = !isPrimaryRed;
   } else if (widget == guiWindow.secondaryAirCircle) {
     pixbuf = gdk_pixbuf_new_from_file_at_scale(
         isSecondaryRed ? "green_circle.png" : "red_circle.png", 50, 50, TRUE, NULL);
     gtk_image_set_from_pixbuf(GTK_IMAGE(guiWindow.secondaryAirCircle), pixbuf);
     g_object_unref(pixbuf);
     isSecondaryRed = !isSecondaryRed;
   }
 
   return TRUE;
 }
 
 /**
  * @brief Draws background map image and overlays a GPS marker based on position.
  *
  * Called automatically by GTK when drawing area needs repainting.
  */
 gboolean draw_map_and_marker(GtkWidget *widget, cairo_t *cr, gpointer data) {
   GdkPixbuf *map = gdk_pixbuf_new_from_file("testMap.png", NULL);
   if (!map) return FALSE;
   gdk_cairo_set_source_pixbuf(cr, map, 0, 0);
   cairo_paint(cr);
   g_object_unref(map);
 
   double lat = navpvt->lat / 1e7;
   double lon = navpvt->lon / 1e7;
 
   double x = (lon - MAP_LON_LEFT) / (MAP_LON_RIGHT - MAP_LON_LEFT) * gtk_widget_get_allocated_width(widget);
   double y = (MAP_LAT_TOP - lat) / (MAP_LAT_TOP - MAP_LAT_BOTTOM) * gtk_widget_get_allocated_height(widget);
 
   GdkPixbuf *icon = gdk_pixbuf_new_from_file_at_scale("loc_icon.png", 24, 24, TRUE, NULL);
   if (icon) {
     gdk_cairo_set_source_pixbuf(cr, icon, x - 12, y - 24);
     cairo_paint(cr);
     g_object_unref(icon);
   }
 
   return FALSE;
 }
 
 /**
  * @brief Builds and initializes the GUI layout and widgets.
  *
  * Includes map display, time/speed indicators, air status lights,
  * timezone dropdown, and signal hookups.
  */
 void initGUI() {
   guiWindow.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(guiWindow.window), "GPS Data Display");
   gtk_window_set_default_size(GTK_WINDOW(guiWindow.window), 600, 400);
 
   GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
   gtk_container_add(GTK_CONTAINER(guiWindow.window), hbox);
 
   GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
   guiWindow.speedLabel = gtk_label_new("Speed: ");
   guiWindow.timeZoneDropdown = gtk_combo_box_text_new();
   guiWindow.mapArea = gtk_drawing_area_new();
   gtk_widget_set_size_request(guiWindow.mapArea, 2053, 1368);
   g_signal_connect(G_OBJECT(guiWindow.mapArea), "draw", G_CALLBACK(draw_map_and_marker), NULL);
 
   GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
   guiWindow.scrollWindow = scroll;
   gtk_widget_set_size_request(scroll, 300, 300);
   gtk_container_add(GTK_CONTAINER(scroll), guiWindow.mapArea);
   gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
 
   const char *zones[] = {
     "Eastern Standard Time (EST)", "Central Standard Time (CST)",
     "Mountain Standard Time (MST)", "Pacific Standard Time (PST)",
     "Alaska Standard Time (AKST)", "Hawaii-Aleutian Standard Time (HAST)"
   };
   for (int i = 0; i < 6; i++) {
     gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(guiWindow.timeZoneDropdown), zones[i]);
   }
 
   GtkCssProvider *provider = gtk_css_provider_new();
   gtk_css_provider_load_from_data(provider, "label { font-family: Sans; font-size: 14pt; font-weight: bold; }", -1, NULL);
   GtkStyleContext *context = gtk_widget_get_style_context(guiWindow.speedLabel);
   gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
   g_object_unref(provider);
 
   gtk_box_pack_start(GTK_BOX(vbox), guiWindow.timeZoneDropdown, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), guiWindow.speedLabel, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
 
   GtkWidget *rightVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
   guiWindow.timeLabel = gtk_label_new("00:00:00");
   guiWindow.primaryAirLabel = gtk_label_new("Primary Air");
   guiWindow.secondaryAirLabel = gtk_label_new("Secondary Air");
   guiWindow.leftLabel = gtk_label_new("OIL PLACEHOLDER");
   guiWindow.closeButton = gtk_button_new_with_label("CLOSE");
 
   GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale("red_circle.png", 50, 50, TRUE, NULL);
   guiWindow.primaryAirCircle = gtk_image_new_from_pixbuf(pixbuf);
   guiWindow.secondaryAirCircle = gtk_image_new_from_pixbuf(pixbuf);
   g_object_unref(pixbuf);
 
   gtk_widget_set_events(guiWindow.primaryAirCircle, GDK_BUTTON_PRESS_MASK);
   gtk_widget_set_events(guiWindow.secondaryAirCircle, GDK_BUTTON_PRESS_MASK);
   g_signal_connect(guiWindow.primaryAirCircle, "button-press-event", G_CALLBACK(on_circle_clicked), NULL);
   g_signal_connect(guiWindow.secondaryAirCircle, "button-press-event", G_CALLBACK(on_circle_clicked), NULL);
 
   gtk_box_pack_start(GTK_BOX(rightVBox), guiWindow.timeLabel, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(rightVBox), guiWindow.primaryAirLabel, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(rightVBox), guiWindow.primaryAirCircle, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(rightVBox), guiWindow.secondaryAirLabel, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(rightVBox), guiWindow.secondaryAirCircle, FALSE, FALSE, 0);
   gtk_box_pack_end(GTK_BOX(rightVBox), guiWindow.leftLabel, FALSE, FALSE, 0);
   gtk_box_pack_end(GTK_BOX(rightVBox), guiWindow.closeButton, FALSE, FALSE, 0);
 
   gtk_box_pack_start(GTK_BOX(hbox), rightVBox, FALSE, FALSE, 0);
   gtk_widget_show_all(guiWindow.window);
 
   g_signal_connect(G_OBJECT(guiWindow.timeZoneDropdown), "changed", G_CALLBACK(on_time_zone_changed), NULL);
   g_signal_connect(guiWindow.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
   g_signal_connect(guiWindow.closeButton, "clicked", G_CALLBACK(on_close_button_clicked), NULL);
 }
 
 /**
  * @brief Thread entry function for starting the GUI.
  *
  * Initializes global references, builds GUI, and runs GTK main loop.
  */
 void *startGUI(void *arg) {
   guiBufferStruct = (bufferStruct *)arg;
   guiFrontBuffer = guiBufferStruct->fBuffer;
   guiBackBuffer = guiBufferStruct->bBuffer;
   guiRunning = guiBufferStruct->isRunning;
   gtk_init(NULL, NULL);
   initGUI();
   gtk_main();
   return NULL;
 }
 
 /**
  * @brief Updates GUI labels and map based on current GPS data.
  *
  * Accesses front or back buffer based on double-buffering strategy.
  */
 gboolean updateGPSLabels(gpointer data) {
   gboolean useFirstBuffer = GPOINTER_TO_INT(data);
   pthread_mutex_lock(&guiBufferStruct->bufferLock);
   navpvt = (navpvt_data *)(useFirstBuffer ? guiFrontBuffer->payload : guiBackBuffer->payload);
   pthread_mutex_unlock(&guiBufferStruct->bufferLock);
 
   int rawSpeed = navpvt->gSpeed;
   float speed_mph = (float)rawSpeed / 447.0;
   int groundSpeed_mph = (int)(speed_mph + 0.5);
   if (groundSpeed_mph < 3) groundSpeed_mph = 0;
 
   if (GTK_IS_LABEL(guiWindow.timeLabel)) {
     char timeStr[100];
     int adjustedHour = (navpvt->hour + utcOffset) % 24;
     if (adjustedHour < 0) adjustedHour += 24;
     sprintf(timeStr, "%02d:%02d:%02d", adjustedHour, navpvt->min, navpvt->sec);
     gtk_label_set_text(GTK_LABEL(guiWindow.timeLabel), timeStr);
   }
 
   if (GTK_IS_LABEL(guiWindow.latitudeLabel)) {
     char latStr[100];
     sprintf(latStr, "LAT: %d", navpvt->lat);
     gtk_label_set_text(GTK_LABEL(guiWindow.latitudeLabel), latStr);
   }
 
   if (GTK_IS_LABEL(guiWindow.longitudeLabel)) {
     char lonStr[100];
     sprintf(lonStr, "LON: %d", navpvt->lon);
     gtk_label_set_text(GTK_LABEL(guiWindow.longitudeLabel), lonStr);
   }
 
   if (GTK_IS_LABEL(guiWindow.speedLabel)) {
     char speedStr[100];
     sprintf(speedStr, "Speed: %d", groundSpeed_mph);
     gtk_label_set_text(GTK_LABEL(guiWindow.speedLabel), speedStr);
   }
 
   gtk_widget_queue_draw(guiWindow.mapArea);
 
   double lat = navpvt->lat / 1e7;
   double lon = navpvt->lon / 1e7;
   double x = (lon - MAP_LON_LEFT) / (MAP_LON_RIGHT - MAP_LON_LEFT) * gtk_widget_get_allocated_width(guiWindow.mapArea);
   double y = (MAP_LAT_TOP - lat) / (MAP_LAT_TOP - MAP_LAT_BOTTOM) * gtk_widget_get_allocated_height(guiWindow.mapArea);
 
   GtkAdjustment *h_adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(guiWindow.scrollWindow));
   GtkAdjustment *v_adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(guiWindow.scrollWindow));
 
   double center_x = x - gtk_adjustment_get_page_size(h_adj) / 2;
   double center_y = y - gtk_adjustment_get_page_size(v_adj) / 2;
 
   center_x = CLAMP(center_x, gtk_adjustment_get_lower(h_adj), gtk_adjustment_get_upper(h_adj) - gtk_adjustment_get_page_size(h_adj));
   center_y = CLAMP(center_y, gtk_adjustment_get_lower(v_adj), gtk_adjustment_get_upper(v_adj) - gtk_adjustment_get_page_size(v_adj));
 
   gtk_adjustment_set_value(h_adj, center_x);
   gtk_adjustment_set_value(v_adj, center_y);
 
   return G_SOURCE_REMOVE;
 }
 
