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
} GuiWindow;

// GuiWindow struct 
GuiWindow guiWindow;

// address
bufferStruct *guiBufferStruct;
incomingUBX *guiFrontBuffer;
incomingUBX *guiBackBuffer;
navpvt_data *navpvt;
atomic_bool *guiRunning;

// declare the utc offset variable
int utcOffset = 0; 

/**
 * applies the appropriate utc offset when the user selects the time zone
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
 * handler for the "CLOSE" button. Sets boolean flag in other thread(s) to "false"
 * and quits the gui program. 
*/
void on_close_button_clicked(GtkWidget *widget, gpointer data) {
  atomic_store(guiRunning, false);
  usleep(500000);
  gtk_main_quit();
}

static gboolean draw_red_circle(GtkWidget *widget, cairo_t *cr, gpointer data) {
  guint width, height;
  GdkRGBA color;

  width = gtk_widget_get_allocated_width(widget);
  height = gtk_widget_get_allocated_height(widget);

  // Set color for the circle (Red)
  gdk_rgba_parse(&color, "red");
  gdk_cairo_set_source_rgba(cr, &color);

  // Draw the circle
  cairo_arc(cr, width / 2, height / 2, MIN(width, height) / 2 - 5, 0, 2 * G_PI);
  cairo_fill(cr);

  return FALSE;
}


/**
 * buils/populates the GUI
*/
void initGUI() {
  // Initialize GUI elements
  guiWindow.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(guiWindow.window), "GPS Data Display");
  gtk_window_set_default_size(GTK_WINDOW(guiWindow.window), 600, 400); // Adjust the size as needed

  // Main horizontal box
  GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_container_add(GTK_CONTAINER(guiWindow.window), hbox);

  // Initialize and pack the left side label
  guiWindow.leftLabel = gtk_label_new("OIL PLACEHOLDER");
  gtk_box_pack_start(GTK_BOX(hbox), guiWindow.leftLabel, FALSE, FALSE, 0);

  // Center vertical box (existing content)
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

  // Create and pack existing widgets into vbox
  guiWindow.timeLabel = gtk_label_new("");  // Time label without prefix
  guiWindow.latitudeLabel = gtk_label_new("LAT: ");
  guiWindow.longitudeLabel = gtk_label_new("LON: ");
  guiWindow.speedLabel = gtk_label_new("Speed: ");  // Speed label
  guiWindow.timeZoneDropdown = gtk_combo_box_text_new();
  guiWindow.closeButton = gtk_button_new_with_label("CLOSE");

  // Populate time zone dropdown
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(guiWindow.timeZoneDropdown), "Eastern Standard Time (EST)");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(guiWindow.timeZoneDropdown), "Central Standard Time (CST)");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(guiWindow.timeZoneDropdown), "Mountain Standard Time (MST)");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(guiWindow.timeZoneDropdown), "Pacific Standard Time (PST)");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(guiWindow.timeZoneDropdown), "Alaska Standard Time (AKST)");
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(guiWindow.timeZoneDropdown), "Hawaii-Aleutian Standard Time (HAST)");

  // Pack existing widgets into vbox
  gtk_box_pack_start(GTK_BOX(vbox), guiWindow.timeZoneDropdown, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), guiWindow.timeLabel, TRUE, TRUE, 0);

  // Create a CSS provider and load CSS data for larger, bold font
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider, "label { font-family: Sans; font-size: 14pt; font-weight: bold; }", -1, NULL);
  GtkStyleContext *context = gtk_widget_get_style_context(guiWindow.speedLabel);
  gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  // Pack the speed label under the time label
  gtk_box_pack_start(GTK_BOX(vbox), guiWindow.speedLabel, TRUE, TRUE, 0);

  // Pack other labels (latitude, longitude, etc.) after the speed label
  gtk_box_pack_start(GTK_BOX(vbox), guiWindow.latitudeLabel, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), guiWindow.longitudeLabel, TRUE, TRUE, 0);

  // Don't forget to unreference the provider after use
  g_object_unref(provider);
  gtk_box_pack_start(GTK_BOX(vbox), guiWindow.closeButton, TRUE, TRUE, 0);

  // Add vbox to the main hbox
  gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

  // Right side widgets
  GtkWidget *rightVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

  // Spacer at the top for vertical centering
  GtkWidget *topSpacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(rightVBox), topSpacer, TRUE, FALSE, 0);

  // Primary Air label
  guiWindow.primaryAirLabel = gtk_label_new("Primary Air");
  gtk_box_pack_start(GTK_BOX(rightVBox), guiWindow.primaryAirLabel, FALSE, FALSE, 0);

  // Red circle (Primary Air)
  guiWindow.primaryAirCircle = gtk_drawing_area_new();
  gtk_widget_set_size_request(guiWindow.primaryAirCircle, 50, 50);
  g_signal_connect(G_OBJECT(guiWindow.primaryAirCircle), "draw", G_CALLBACK(draw_red_circle), NULL);
  gtk_box_pack_start(GTK_BOX(rightVBox), guiWindow.primaryAirCircle, FALSE, FALSE, 0);

  // Secondary Air label
  guiWindow.secondaryAirLabel = gtk_label_new("Secondary Air");
  gtk_box_pack_start(GTK_BOX(rightVBox), guiWindow.secondaryAirLabel, FALSE, FALSE, 0);

  // Red circle (Secondary Air)
  guiWindow.secondaryAirCircle = gtk_drawing_area_new();
  gtk_widget_set_size_request(guiWindow.secondaryAirCircle, 50, 50);
  g_signal_connect(G_OBJECT(guiWindow.secondaryAirCircle), "draw", G_CALLBACK(draw_red_circle), NULL);
  gtk_box_pack_start(GTK_BOX(rightVBox), guiWindow.secondaryAirCircle, FALSE, FALSE, 0);

  // Spacer at the bottom for vertical centering
  GtkWidget *bottomSpacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(rightVBox), bottomSpacer, TRUE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), rightVBox, FALSE, FALSE, 0);

  // Show all widgets
  gtk_widget_show_all(guiWindow.window);

  // Connect signals
  g_signal_connect(G_OBJECT(guiWindow.timeZoneDropdown), "changed", G_CALLBACK(on_time_zone_changed), NULL);
  g_signal_connect(guiWindow.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(G_OBJECT(guiWindow.closeButton), "clicked", G_CALLBACK(on_close_button_clicked), NULL);
  g_timeout_add(500, updateGPSLabels, GINT_TO_POINTER(1));  // or 0 for backBuffer

}



// void initGUI() {
//   // Initialize GUI elements
//   guiWindow.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
//   gtk_window_set_title(GTK_WINDOW(guiWindow.window), "GPS Data Display");
//   gtk_window_set_default_size(GTK_WINDOW(guiWindow.window), 300, 200);

//   // Create labels for GPS data
//   guiWindow.timeLabel = gtk_label_new("");  // Time label without prefix
//   guiWindow.latitudeLabel = gtk_label_new("LAT: ");
//   guiWindow.longitudeLabel = gtk_label_new("LON: ");
//   guiWindow.speedLabel = gtk_label_new("Speed: ");  // Speed label
//   guiWindow.timeZoneDropdown = gtk_combo_box_text_new();
//   guiWindow.closeButton = gtk_button_new_with_label("CLOSE");
//   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(guiWindow.timeZoneDropdown), "Eastern Standard Time (EST)");
//   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(guiWindow.timeZoneDropdown), "Central Standard Time (CST)");
//   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(guiWindow.timeZoneDropdown), "Mountain Standard Time (MST)");
//   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(guiWindow.timeZoneDropdown), "Pacific Standard Time (PST)");
//   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(guiWindow.timeZoneDropdown), "Alaska Standard Time (AKST)");
//   gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(guiWindow.timeZoneDropdown), "Hawaii-Aleutian Standard Time (HAST)");

//   // Layout
//   GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
//   gtk_container_add(GTK_CONTAINER(guiWindow.window), vbox);

//   gtk_box_pack_start(GTK_BOX(vbox), guiWindow.timeZoneDropdown, TRUE, TRUE, 0);
//   gtk_box_pack_start(GTK_BOX(vbox), guiWindow.timeLabel, TRUE, TRUE, 0);  // Pack time label
//   gtk_box_pack_start(GTK_BOX(vbox), guiWindow.latitudeLabel, TRUE, TRUE, 0);  // Pack latitude label
//   gtk_box_pack_start(GTK_BOX(vbox), guiWindow.longitudeLabel, TRUE, TRUE, 0);  // Pack longitude label
//   gtk_box_pack_start(GTK_BOX(vbox), guiWindow.speedLabel, TRUE, TRUE, 0);  // Pack speed label
//   gtk_box_pack_start(GTK_BOX(vbox), guiWindow.closeButton, TRUE, TRUE, 0);

//   // Show all widgets
//   gtk_widget_show_all(guiWindow.window);
  
//   g_signal_connect(G_OBJECT(guiWindow.timeZoneDropdown), "changed", G_CALLBACK(on_time_zone_changed), NULL);

//   // Connect the destroy signal (when the window is closed)
//   g_signal_connect(guiWindow.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
//   g_signal_connect(G_OBJECT(guiWindow.closeButton), "clicked", G_CALLBACK(on_close_button_clicked), NULL);
// }


void *startGUI(void *arg) {
  // set global variables with address of gps buffers
  guiBufferStruct = (bufferStruct *)arg;
  guiFrontBuffer = guiBufferStruct->fBuffer;
  guiBackBuffer = guiBufferStruct->bBuffer;
  guiRunning = guiBufferStruct->isRunning;
  // Initialize GTK
  gtk_init(NULL, NULL);
  // Initialize GUI elements
  initGUI();
  // Start the GTK main loop
  gtk_main();
  return NULL;
}

/**
 * updates the GUI labels with the current data
*/
gboolean updateGPSLabels(gpointer data) {
  // Convert the gpointer back to a boolean value
  gboolean useFirstBuffer = GPOINTER_TO_INT(data);
  pthread_mutex_lock(&guiBufferStruct->bufferLock);
  if (useFirstBuffer) {
      navpvt = (navpvt_data *)guiFrontBuffer->payload;
  } else {
      navpvt = (navpvt_data *)guiBackBuffer->payload;
  }
  pthread_mutex_unlock(&guiBufferStruct->bufferLock);
  
  // gSpeed is given in mm/s. need to convert to mph. divide mm/s by 447
  int rawSpeed = navpvt->gSpeed;
  float speed_mph = (float)rawSpeed / 447.0;
  // to get accurate integer values for mph, round to nearest whole and cast to int
  int groundSpeed_mph = (int)(speed_mph + 0.5);
  // Update time
  if (GTK_IS_LABEL(guiWindow.timeLabel)) {
    char timeStr[100];
    int adjustedHour = (navpvt->hour + utcOffset) % 24;
    if (adjustedHour < 0) adjustedHour += 24;
    sprintf(timeStr, "%02d:%02d:%02d", adjustedHour, navpvt->min, navpvt->sec);
    gtk_label_set_text(GTK_LABEL(guiWindow.timeLabel), timeStr);
  }
  // Update latitude
  if (GTK_IS_LABEL(guiWindow.latitudeLabel)) {
    char latStr[100];
    sprintf(latStr, "LAT: %d", navpvt->lat);
    gtk_label_set_text(GTK_LABEL(guiWindow.latitudeLabel), latStr);
  }
  // Update longitude
  if (GTK_IS_LABEL(guiWindow.longitudeLabel)) {
    char lonStr[100];
    sprintf(lonStr, "LON: %d", navpvt->lon);
    gtk_label_set_text(GTK_LABEL(guiWindow.longitudeLabel), lonStr);
  }
  // Update speed
  if (GTK_IS_LABEL(guiWindow.speedLabel)) {
    char speedStr[100];
    sprintf(speedStr, "Speed: %d", groundSpeed_mph);
    gtk_label_set_text(GTK_LABEL(guiWindow.speedLabel), speedStr);
  }
  return G_SOURCE_REMOVE;
}





