/* Arduino-based replica of the Back to the Future DeLorean Speedometer
   Copyright (C) 2017  Baptiste Candellier

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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
