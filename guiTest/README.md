compile with: gcc `pkg-config --cflags gtk+-3.0` -o test main.c gui_setup.c gps_setup.c -l bcm2835 `pkg-config --libs gtk+-3.0`


Run with sudo or bcm wont work with gpio pins
correct
