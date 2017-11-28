#include <Arduino.h>

int analog_read_avg(const int sensor_pin, const int nb_samples, const long time_gap) {
    static int currentSample;
    static int currentValue = 0;

    // Perform multiple reads on the given pin and return an average
    for (int i = 0; i < nb_samples; i++) {
        currentSample = analogRead(sensor_pin);
        currentValue += currentSample;
        delay(time_gap);
    }

    return (currentValue / nb_samples);
}
