#include "Timer.h"

Timer::Timer() {
  tiempoInicio = 0;
  tiempoActual = 0;
  tiempoOn = 0;
  periodo = 0;
  dutyCycle = 0.0;
  running = false;
}

void Timer::iniciar(unsigned long periodo) {
  this->periodo = periodo;
  tiempoInicio = millis();
  running = true;
}


void Timer::update() {
  tiempoActual = millis();
}

bool Timer::periodoFin() {
  return (millis() - tiempoInicio) > periodo;
}

void Timer::stop() {
  periodo = 0;
  dutyCycle = 0.0;
  tiempoInicio = 0;
  tiempoActual = 0;
  tiempoOn = 0;
  running = false;
}

bool Timer::isRunning() {
  return running;
}