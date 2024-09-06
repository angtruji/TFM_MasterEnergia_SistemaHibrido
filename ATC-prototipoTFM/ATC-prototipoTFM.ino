/*
- Todos los valores de potencias están en KW.
- La potencia de consumo se simula como la lectura de un sensor externo.
- Como estado inicial se asume que el reservorio superior está lleno.
- Para los equipos controlados por salidas de tipo PWM se considera:
    valorPWM = 0 → Equipo apagado
    valorPWM > 0 → Equipo encendido
*/

#include "Timer.h"

// pines de entrada/salida
#define PIN_BOMBA_SUP 11         // Digital Out → PWM
#define PIN_TURBINA 10           // Digital Out → PWM
#define PIN_BATERIA_CARGA 6      // Digital Out → PWM
#define PIN_BATERIA_DESCARGA 5   // Digital Out → PWM
#define PIN_RESERV_SUP_LLENO 13  // Digital In  → 0-1c:\Users\Carlos\Mi unidad\Angie\2024-08-27 TFM código para Arduino\ATC-prototipoTFM\Timer.cpp c:\Users\Carlos\Mi unidad\Angie\2024-08-27 TFM código para Arduino\ATC-prototipoTFM\Timer.h
#define PIN_BATERIA_LLENA 4      // Digital In  → 0-1
#define PIN_ANENOMETRO 0         // Analog In   → 0-255
#define PIN_POTENCIA_CONSUMO 1   // Analog In   → 0-255
// constantes
#define BOMBA_SUP_POTENCIA_MAX 6.09
#define BATERIA_POTENCIA_CARGA_MAX 0.1667
#define BATERIA_POTENCIA_DESCARGA_MAX 0.053
#define VELOCIDAD_VIENTO_MAX 30
#define VELOCIDAD_VIENTO_MIN 2
#define TURBINA_POTENCIA_MAX 3.2
#define POTENCIA_CONSUMO_MAX 3.2
#define POTENCIA_EOLICA_MAX 10
#define DENSIDAD_AIRE 1000
#define AREA_BARRIDO 75.4
#define COEFICIENTE_POTENCIA 0.48
#define TIEMPO_TRANSITORIO_MINUTOS 1
// variables
float potenciaConsumo, potenciaDisponible, potenciaEolica, velocidadViento;
float potenciaHidraulicaNecesaria, potenciaBombaSup;
float potenciaBateriaCarga, potenciaBateriaDescarga;
float pwmBombaSup, pwmTurbina, pwmBateriaCarga, pwmBateriaDescarga, valorSensor;
boolean reservSupLleno, bateriaLlena;
Timer timerBateria;
// estados del sistema
enum Estado { LECTURA_SENSORES,
              ANALISIS_BOMBA_SUP,
              ANALISIS_CARGA_BATERIA,
              ANALISIS_POTENCIA_HIDRAULICA_NECESARIA };
Estado estado;

void setup() {
  // se definen los pines de entrada
  pinMode(PIN_RESERV_SUP_LLENO, INPUT);
  pinMode(PIN_BATERIA_LLENA, INPUT);
  pinMode(PIN_ANENOMETRO, INPUT);
  pinMode(PIN_POTENCIA_CONSUMO, INPUT);
  // se definen los pines de salida
  pinMode(PIN_BOMBA_SUP, OUTPUT);
  pinMode(PIN_TURBINA, OUTPUT);
  pinMode(PIN_BATERIA_CARGA, OUTPUT);
  pinMode(PIN_BATERIA_DESCARGA, OUTPUT);
  // estados iniciales de los puertos de salida
  analogWrite(PIN_BOMBA_SUP, 0);
  analogWrite(PIN_TURBINA, 0);
  analogWrite(PIN_BATERIA_CARGA, 0);
  analogWrite(PIN_BATERIA_DESCARGA, 0);
  // valores iniciales de variables
  velocidadViento = 0;
  potenciaConsumo = 0;
  potenciaEolica = 0;
  potenciaDisponible = 0;
  potenciaHidraulicaNecesaria = 0;
  potenciaBombaSup = 0;
  potenciaBateriaCarga = 0;
  potenciaBateriaDescarga = 0;
  pwmBateriaCarga = 0;
  pwmBateriaDescarga = 0;
  pwmBombaSup = 0;
  pwmTurbina = 0;
  valorSensor = 0;
  reservSupLleno = true;
  // estado inicial del sistema
  estado = LECTURA_SENSORES;
}

void loop() {
  switch (estado) {
    case LECTURA_SENSORES:
      // se lee el valor del sensor analógico
      valorSensor = analogRead(PIN_ANENOMETRO);
      // se ajusta el valor leído a la escala correspondiente
      velocidadViento = map(valorSensor, 0, 1023, 0, VELOCIDAD_VIENTO_MAX);
      // se limita la velocidad de viento a un valor mayor o igual a 2m/s
      if (velocidadViento < VELOCIDAD_VIENTO_MIN) {
        velocidadViento = 0;
      }
      // se lee el valor del sensor analógico
      valorSensor = analogRead(PIN_POTENCIA_CONSUMO);
      // se ajusta el valor leído a la escala correspondiente
      potenciaConsumo = map(valorSensor, 0, 1023, 0, POTENCIA_CONSUMO_MAX);
      // se calcula la potencia eólica en función a la velocidad del viento
      potenciaEolica = 0.5 * DENSIDAD_AIRE * AREA_BARRIDO * pow(velocidadViento, 3) * COEFICIENTE_POTENCIA;
      // se limita la potencia eólica a un valor menor o igual a 10KW
      if (potenciaEolica > POTENCIA_EOLICA_MAX) {
        potenciaEolica = POTENCIA_EOLICA_MAX;
      }
      // de acuerdo al nivel de potencia eólica generada se evalúa el siguiente estado
      if (potenciaEolica > potenciaConsumo) {
        /* si la turbina está encendida se apaga porque ya no es necesario producir
        potencia hidraúlica */
        if (pwmTurbina > 0) {
          potenciaHidraulicaNecesaria = 0;
          pwmTurbina = 0;
          analogWrite(PIN_TURBINA, pwmTurbina);
          /* si la batería todavía se estaba descargando entonces se detiene la descarga,
          esto significa que hubo un aumento de la velocidad del viento cuando la turbina
          estaba encendida y la batería estaban descargándose durante el estado transitorio */
          if (timerBateria.isRunning()) {
            timerBateria.stop();
            potenciaBateriaDescarga = 0;
            pwmBateriaDescarga = 0;
            analogWrite(PIN_BATERIA_DESCARGA, 255);
          }
        }
        estado = ANALISIS_BOMBA_SUP;
      } else {
        /* como la potencia eólica no es suficiente se deben apagar los equipos secundarios
        que hayan estado encendidos hasta ese momento 
        si la bomba superior estaba encendida se apaga */
        if (pwmBombaSup > 0) {
          potenciaBombaSup = 0;
          pwmBombaSup = 0;
          analogWrite(PIN_BOMBA_SUP, pwmBombaSup);
        }
        // si se estaba cargando la batería se detiene la carga
        if (pwmBateriaCarga > 0) {
          potenciaBateriaCarga = 0;
          pwmBateriaCarga = 0;
          analogWrite(PIN_BATERIA_CARGA, pwmBateriaCarga);
        }
        estado = ANALISIS_POTENCIA_HIDRAULICA_NECESARIA;
      }
      break;
    case ANALISIS_BOMBA_SUP:
      // se calcula la potencia disponible actual
      potenciaDisponible = potenciaEolica - potenciaConsumo;
      // si analiza que tanta potencia disponible se puede destinar a la bomba
      if (potenciaDisponible > BOMBA_SUP_POTENCIA_MAX) {
        /*  si la potencia disponible es superior al máximo de potencia de la bomba
        se enciende al 100% de su capacidad, la bomba tiene la primera prioridad */
        potenciaBombaSup = BOMBA_SUP_POTENCIA_MAX;
        pwmBombaSup = 255;
        analogWrite(PIN_BOMBA_SUP, pwmBombaSup);
        // se evalúa si el reservorio superior está lleno o no
        reservSupLleno = digitalRead(PIN_RESERV_SUP_LLENO);
        if (reservSupLleno) {
          // si se llenó el reservorio superior se apaga la bomba
          potenciaBombaSup = 0;
          pwmBombaSup = 0;
          analogWrite(PIN_BOMBA_SUP, pwmBombaSup);
        }
        estado = ANALISIS_CARGA_BATERIA;
      } else {
        /* se enciende la bomba con toda la potencia disponible así no llegue al
        100% de su capacidad */
        potenciaBombaSup = potenciaDisponible;
        pwmBombaSup = 255 * potenciaBombaSup / BOMBA_SUP_POTENCIA_MAX;
        analogWrite(PIN_BOMBA_SUP, pwmBombaSup);
        // se evalúa si el reservorio superior está lleno o no
        reservSupLleno = digitalRead(PIN_RESERV_SUP_LLENO);
        if (reservSupLleno) {
          // si se llenó el reservorio superior se apaga la bomba
          potenciaBombaSup = 0;
          pwmBombaSup = 0;
          analogWrite(PIN_BOMBA_SUP, pwmBombaSup);
        }
        estado = LECTURA_SENSORES;
      }
      break;
    case ANALISIS_CARGA_BATERIA:
      // se calcula la potencia disponible actual
      potenciaDisponible = potenciaEolica - potenciaConsumo - potenciaBombaSup;
      if (potenciaDisponible > BATERIA_POTENCIA_CARGA_MAX) {
        /* si la potencia disponible es superior al máximo de potencia de carga
        de la batería se carga al 100% de su capacidad de carga*/
        potenciaBateriaCarga = BATERIA_POTENCIA_CARGA_MAX;
        pwmBateriaCarga = 255;
        analogWrite(PIN_BATERIA_CARGA, pwmBateriaCarga);
      } else {
        /* se carga la batería con toda la potencia disponible así no llegue al
        100% de su capacidad de carga */
        potenciaBateriaCarga = potenciaDisponible;
        pwmBateriaCarga = 255 * potenciaBateriaCarga / BATERIA_POTENCIA_CARGA_MAX;
        analogWrite(PIN_BATERIA_CARGA, pwmBateriaCarga);
      }
      // se evalúa si la batería está al máximo de su capacidad de carga o no
      bateriaLlena = digitalRead(PIN_BATERIA_LLENA);
      if (bateriaLlena) {
        // si la batería está al máximo de su capacidad se detiene la carga
        potenciaBateriaCarga = 0;
        pwmBateriaCarga = 0;
        analogWrite(PIN_BATERIA_CARGA, pwmBateriaCarga);
      }
      estado = LECTURA_SENSORES;
      break;
    case ANALISIS_POTENCIA_HIDRAULICA_NECESARIA:
      // se calcula la potencia hidraúlica necesaria
      potenciaHidraulicaNecesaria = potenciaConsumo - potenciaEolica;
      // si la turbina está apagada se enciende junto con la descarga de la batería
      if (pwmTurbina == 0) {
        // se activa la descarga de la batería al máximo de su capacidad
        potenciaBateriaDescarga = BATERIA_POTENCIA_DESCARGA_MAX;
        pwmBateriaDescarga = 255;
        analogWrite(PIN_BATERIA_DESCARGA, pwmBateriaDescarga);
        // se activa la turbina en proporción a la potencia hidráulica necesaria
        pwmTurbina = 255 * potenciaHidraulicaNecesaria / TURBINA_POTENCIA_MAX;
        analogWrite(PIN_TURBINA, pwmTurbina);
        // se inicia la cuenta del estado transitorio
        timerBateria.iniciar(TIEMPO_TRANSITORIO_MINUTOS * 60 * 1000);
      }
      estado = LECTURA_SENSORES;
      break;
    default:
      break;
  }
  // se evalúa si ya se llegó al fin del estado transitorio
      if (timerBateria.periodoFin()) {
        // se detiene la descarga de la batería
        potenciaBateriaDescarga = 0;
        pwmBateriaDescarga = 0;
        analogWrite(PIN_BATERIA_DESCARGA, pwmBateriaDescarga);
        // se detiene el timer
        timerBateria.stop();
      }
  // se actualiza el estado del timer para que lea el tiempo actual
  if (timerBateria.isRunning()) {
    timerBateria.update();
  }
}
