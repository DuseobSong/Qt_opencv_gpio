#include "led_thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>

#define LED_GPIO_DIR "/sys/class/gpio"

LED_thread::LED_thread()
{

}


int blink(int gpio){
    int value, i;
    char buf[256];
    FILE *led_gpio;

    snprintf(buf, sizeof(buf), "%s/export", LED_GPIO_DIR);
    led_gpio = fopen(buf, "w");
    fprintf(led_gpio, "%d\n", gpio);
    fclose(led_gpio);

    snprintf(buf, sizeof(buf), "%s/gpio%d/direction", LED_GPIO_DIR, gpio);
    led_gpio = fopen(buf, "w");
    fprintf(led_gpio, "out\n");
    fclose(led_gpio);

    snprintf(buf, sizeof(buf), "%s/gpio%d/value", LED_GPIO_DIR, gpio);
    led_gpio = fopen(buf, "w");


}
