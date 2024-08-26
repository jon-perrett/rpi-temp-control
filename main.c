#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdbool.h>

#include <pigpio.h>

#define MAX_GPIOS 32

static int target_t = 0;
static int gpio_pin = 0;

void usage() {
	fprintf(stderr, "\n" \
			"Usage: ./temp_control [OPTION] \n" \
			"	-t value, setpoint temperature\n" \
			"	-p value, GPIO pin for fan control, must be less than %d"\
			"\n",
			MAX_GPIOS
	       );

}

void fatal(int show_usage, char *fmt, ...) {
	char buf[128];
	
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	fprintf(stderr, "%s\n", fmt);
	if (show_usage) usage();
	fflush(stderr);
	exit(EXIT_FAILURE);
}

static int initOpts(int argc, char *argv[]) {
	int i, opt;

	while ((opt = getopt(argc, argv, "t:p:")) != -1) {
		i = -1;
		switch (opt) {
			case 't':
				target_t = atoi(optarg);
				break;
			case 'p':
				gpio_pin = atoi(optarg);
				if ((gpio_pin<0) || (gpio_pin > MAX_GPIOS)) {
					fatal(1, "invalid pin selection (%d)", i);
				}
				break;

			default:
				usage();
				exit(-1);
				
		}
	}
	return optind;
}

int getTemp() {
	float systemp, millideg;
	FILE *thermal;
	thermal = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
	fscanf(thermal, "%f", &millideg);
	fclose(thermal);
	systemp = millideg / 1000;

	int temp = (int)(systemp < 0 ? (systemp - 0.5) : (systemp + 0.5));
	return temp;

}

int main(int argc, char *argv[]) {
	initOpts(argc, argv);
	if (gpioInitialise() < 0) return 1;
	int hysteresis = 5;
	fprintf(stdout, "Target temeperature %d\nGPIO Pin: %d\n", target_t, gpio_pin);
	int modeSetStatus = gpioSetMode(gpio_pin, PI_OUTPUT);
	if (modeSetStatus) {
		fprintf(stderr, "Unable to set GPIO pin %d to output", gpio_pin);
	}
	bool pin_set = gpioRead(gpio_pin);
	while (1) {
		if ((getTemp() > (target_t+hysteresis)) && !pin_set) {
			fprintf(stdout, "Setting pin on!\n");

			int status = gpioWrite(gpio_pin, 1);
			if (status) {
				fprintf(stderr, "Failed to set pin with status %d", status);
			}
			pin_set = true;
			continue;
		} else if ((getTemp() < (target_t - hysteresis)) & pin_set) {
			fprintf(stdout, "Setting pin off!\n");
			int status = gpioWrite(gpio_pin, 0);
			if (status) {
				fprintf(stderr, "Failed to set pin with status %d", status);
			}
			pin_set = false;
			continue;
		}
		fprintf(stdout, "Not doing anything. Temp: %d, Target Temp: %d, Hysteresis: %d\n", getTemp(), target_t , hysteresis);
		sleep(1);
	}
	
}
