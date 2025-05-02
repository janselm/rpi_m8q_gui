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

#define MAX_LABEL_LENGTH 30

static GtkWidget *label;
static GtkWidget *timeLabel;

extern atomic_bool running;

static void on_window_destroy(GtkWidget *widget, gpointer user_data) {
  atomic_store(&running, false); // stop GPS loop
  g_application_quit(G_APPLICATION(user_data)); // stop GTK main loop
}

static void activate(GtkApplication *app, gpointer user_data) {
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "Window");
  gtk_window_set_default_size(GTK_WINDOW(window), 300, 300);

  GtkWidget *label_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add(GTK_CONTAINER(window), label_box);

  label = gtk_label_new("Lat: 0.0\nLon: 0.0");
  gtk_box_pack_start(GTK_BOX(label_box), label, TRUE, TRUE, 0);

  timeLabel = gtk_label_new("Time: 00:00:00");
  gtk_box_pack_start(GTK_BOX(label_box), timeLabel, TRUE, TRUE, 0);

  g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), app);

  gtk_widget_show_all(window);
}

void *startGUI(void *arg) {
  GtkApplication *app;
  int status;

  app = gtk_application_new ("org.gtk.example", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  status = g_application_run (G_APPLICATION (app), 0, NULL);
  g_object_unref (app);

  return NULL;
}

gboolean gui_update_handler(gpointer user_data) {
  navpvt_data *data = (navpvt_data *)user_data;

  double lat = data->lat / 1e7;
  double lon = data->lon / 1e7;

  gchar label_text[64];
  snprintf(label_text, sizeof(label_text), "Lat: %.7f\nLon: %.7f", lat, lon);
  gtk_label_set_text(GTK_LABEL(label), label_text);

  gchar time_text[64];
  snprintf(time_text, sizeof(time_text), "Time: %02d:%02d:%02d", data->hour, data->min, data->sec);
  gtk_label_set_text(GTK_LABEL(timeLabel), time_text);

  free(data);
  return G_SOURCE_REMOVE; // Remove source after execution
}




