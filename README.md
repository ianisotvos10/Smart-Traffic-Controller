# Smart Traffic Controller & Dual Speed Detection System

This project is an adaptive traffic monitoring and control system built on the **Arduino Uno (ATmega328P)** platform. It demonstrates a transition from passive, fixed-timing traffic lights to an active, sensor-driven architecture suitable for "Smart City" concepts.

# Smart Traffic Controller & Dual Speed Detection System

🚀 **Live Simulation:** [Test the traffic controller logic directly in Tinkercad!](https://www.tinkercad.com/things/bfrtEjRBrl0-glorious-vihelmo-lappi)

This project is an adaptive traffic monitoring and control system built on the **Arduino Uno (ATmega328P)** platform. It demonstrates a transition from passive, fixed-timing traffic lights to an active, sensor-driven architecture suitable for "Smart City" concepts.

## 🚀 Key Features

The system employs a dual-sensor approach to monitor vehicle speed and presence, dynamically adjusting traffic lights and prioritizing safety:

*   **Dual Speed Detection:**
    *   **High-Precision IR Sensors (TCRT5000):** Used in a "Start/Stop gate" configuration on the main lane to calculate exact average speed based on time intervals in microseconds.
    *   **Ultrasonic Sensors (HC-SR04):** Used on secondary lanes to estimate instantaneous speed and monitor queue presence using differential distance sampling.
*   **Adaptive Traffic Logic (Finite State Machine):**
    *   **Automatic Cycling:** Changes phases based on a timer (`INTERVAL_SCHIMBARE_FAZA`).
    *   **Speed Penalty System:** If a vehicle exceeds the speed limit (e.g., 30 km/h), the system interrupts the normal cycle, turning the violator's light red and switching the opposing lane to green.
*   **Pedestrian Priority:** A capacitive touch sensor (`TTP223`) allows pedestrians to interrupt the cycle, setting all vehicle lights to red and allowing safe crossing.
*   **Real-time Dashboard:** An I2C LCD (1602) displays live speed metrics from both IR and Ultrasonic sensors and system status.

## 🛠️ Hardware Architecture

*   **Microcontroller:** Arduino Uno (ATmega328P)
*   **Sensors:** 
    *   2x HC-SR04 (Ultrasonic)
    *   1x TCRT5000 module (IR gate)
    *   1x TTP223 (Capacitive Touch)
*   **Outputs:** 
    *   Traffic Lights (LEDs: Red, Yellow, Green for main and secondary lanes, plus pedestrian crossing).
    *   Active Buzzer for audio alerts during penalties or pedestrian crossings.
    *   LCD 1602 with I2C module (PCF8574).

## 🧠 Software Implementation

The software is an event-driven Finite State Machine (FSM) written in C++.
*   **Non-blocking Code:** Designed to avoid blocking the processor (e.g., using minimal timeouts for `pulseIn`), ensuring the system remains responsive to asynchronous events.
*   **Signal Filtering:** Implements software thresholds to filter sensor noise (e.g., ignoring static fluctuations or distant objects).
*   **Physical Scaling Algorithm:** Translates parameters from a scaled model (short distances, low physical speeds) into realistic traffic parameters (km/h) for accurate simulation.

## 📂 Repository Structure

*   `/src`: Contains the main Arduino sketch (`.ino`).
*   `/docs`: Contains the project documentation and hardware schematics.