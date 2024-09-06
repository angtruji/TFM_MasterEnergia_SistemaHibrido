#ifndef TIMER_H
#define TIMER_H

#include <Arduino.h>

class Timer {
  private:
    unsigned long tiempoInicio, tiempoActual, tiempoOn;
    unsigned long periodo;
    float dutyCycle;
    bool running;

  public:
    Timer();
    void iniciar(unsigned long periodo);
    void update();
    bool periodoFin();
    void stop();
    bool isRunning();
};

#endif
