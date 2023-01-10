/**
 * Ардуина мотокрышки SkyHat
 * 
 * @todo
 * Через INA219 читать напругу до 16 В
 * Если 5 шагов ток 0, то нет питания (уход в ошибку). 
 * /@todo
 * 
 * Подключение:
 * - D9, D10: на DRV8871 - левый мотор
 * - D5, D6: на DRV8871 - правый мотор
 * - A4, A5 (SLA, SCL): I2C датчик тока INA219
 * 
 * Протокол на вход (получение команды).
 * Минимум два байта. Может быть больше (зависит от команды):
 * - первый байт всегда 'c' - это старт-байт команды
 * - второй байт - команда
 * - далее байт(ы) - параметр(ы), если нужны
 * 
 * Команды:
 * 
 * с версии 1.8+
 * - 'l'(3 байта): Light, свет. Параметр: byte - яркость (0 = выключено)
 * - 'o'(3 байта): Open, открыть. Параметр 'l' (левую крышку), 'r' (правую крышку), 'b' (обе крышки), 'a' (авто)
 * - 'c'(3 байта): Close, закрыть. Параметр 'l' (левую крышку), 'r' (правую крышку), 'b' (обе крышки), 'a' (авто)
 * - 'a'(2 байта): Abort. Отмена движения
 * - 'g'(2 байта): get, получить пакета текущих данных
 *        - byte start = 0xEE
 *        - byte, current: Текущий ток в ADU датчика
 *        - byte, timeout: Текущий таймаут в секундах (через сколько секунд мотор вырубится с ошибкой)
 *        - byte, moveLeftTo: Куда движется левая створка ('c', 'o' или 's')
 *        - byte, moveRightTo: Куда движется правая створка ('c', 'o' или 's')
 *        - byte, statusLeft: Статус левой крышки, см. ниже
 *        - byte, statusRight: Статус правой крышки, см. ниже
 *        - byte, light: Включена ли лампочка, её яркость (0-255)
 *        - byte, speedLeft: скорость левой крышки
 *        - byte, speedRight: скорость левой крышки
 *        - byte stop = 0x00
 *         * Для отладки та же информация представлена в виде текстовой строки
 * - 'e'(2 байта): get EEEPROM, получить пакет параметров из EEPROM
 *        - byte start = 0xEE
 *        - byte, part: в авто-режиме (и кнопкой) какая крышка едет. Параметр: 'l' (левая, по-умолчанию), 'r' (правая), 'b' (обе по схеме ниже).
 *        - byte, first: какая крышка едет первой при открытии (при закрытии наоборот). Параметр: 'l' (левая, по-умолчанию) или 'r' (правая).
 *        - byte, timeoutLeft: задание таймаута движения левой крышки в секундах. Параметр: число секунд (3, например)
 *        - byte, timeoutRight: задание таймаута движения правой крышки в секундах. Параметр: число секунд (3, например)
 *        - byte, thresholdLeft: задание порога срабатывания датчика тока левой крышки. Параметр: число порога (25, например)
 *        - byte, thresholdRight: задание порога срабатывания датчика тока правой крышки. Параметр: число порога (25, например)
 *        - byte, maxSpeedLeft: задание максимальной скорости левого мотора в ШИМ 0..255. Параметр: число скорости (255, например)
 *        - byte, maxSpeedRight: задание максимальной скорости правого мотора в ШИМ 0..255. Параметр: число скорости (255, например)
 *        - byte, velocityLeft: ускорение (скорость разгона) левой крышки. Параметр: число ускорения (5, например)
 *        - byte, velocityRight: ускорение (скорость разгона) правой крышки. Параметр: число ускорения (5, например)
 *        - byte, reverseLeft: реверс левого мотора (1) или его нормальная работа (0)
 *        - byte, reverseRight: реверс правого мотора (1) или его нормальная работа (0)
 *        - byte stop = 0x00
 *         * Для отладки та же информация представлена в виде текстовой строки
 * - 's'(12 байт): set EEPROM, установить параметры в EEPROM (part, first, timeoutLeft, timeoutRight, thresholdLeft, thresholdRight, maxSpeedLeft, maxSpeedRight, velocityLeft, velocityRight, reverseLeft, reverseRight)
 * 
 * до v.1.8-
 * - 'l'(3 байта): Light, свет. Параметр: '1' / '0' - включить / выключить
 * - 'o'(3 байта): Open, открыть. Параметр 'l' (левую крышку), 'r' (правую крышку), 'a' (обе крышки)
 * - 'c'(3 байта): Close, закрыть. Параметр 'l' (левую крышку), 'r' (правую крышку), 'a' (обе крышки)
 * - 'a'(2 байта): Abort. Отмена движения
 * - 'g'(2 байта): get, получить пакета текущих данных. Параметр игнорируется (но должен быть задан!)
 *        - byte start = 0xEE
 *        - byte, current: Текущий ток в ADU датчика
 *        - byte, timeout: Текущий таймаут в секундах (через сколько секунд мотор вырубится с ошибкой)
 *        - byte, moveLeftTo: Куда движется левая створка ('c', 'o' или 's')
 *        - byte, moveRightTo: Куда движется правая створка ('c', 'o' или 's')
 *        - byte, statusLeft: Статус левой крышки, см. ниже
 *        - byte, statusRight: Статус правой крышки, см. ниже
 *        - byte, light: Включена ли лампочка (1 или 0)
 *        - byte, speedLeft: скорость левой крышки
 *        - byte, speedRight: скорость левой крышки
 *        - byte stop = 0x00
 *         * Для отладки та же информация представлена в виде текстовой строки
 * - 'e'(2 байта): get EEEPROM, получить пакет параметров из EEPROM
 *        - byte start = 0xEE
 *        - byte, first: какая крышка едет первой при открытии (при закрытии наоборот). Параметр: 'l' (левая, по-умолчанию) или 'r' (правая).
 *        - byte, timeout: задание таймаута движения крышки в секундах. Параметр: число секунд (3, например)
 *        - byte, brightness: яркость EL Panel
 *        - byte, threshold: задание порога срабатывания датчика тока. Параметр: число порога (25, например)
 *        - byte, maxSpeed: задание максимальной скорости мотора в ШИМ 0..255. Параметр: число скорости (255, например)
 *        - byte, velocity: ускорение (скорость разгона). Параметр: число ускорения (5, например)
 *        - byte stop = 0x00
 *         * Для отладки та же информация представлена в виде текстовой строки
 * - 's'(2+6=8 байт): set EEPROM, установить параметры в EEPROM (first, timeout, brightness, threshold, maxSpeed, velocity)
 */

/*
 * Что сейчас делает левая (правая) крышка:
 * - 'u': неизвестно (начальный статус)
 * - 'g': ждёт начала движения второй крышки (реализует GAP)
 * - 'c': закрыта
 * - 'o': открыта
 * - 't': таймаут
 */
byte statusLeft  = 'u'; 
byte statusRight = 'u'; 


//////////////////////////////// НАСТРОЙКИ

//#define DEBUG
//#define INIT_EEPROM

// ноги ардуины
#define MOTOR_LEFT_OPEN  9
#define MOTOR_LEFT_CLOSE 10

#define MOTOR_RIGHT_OPEN  6
#define MOTOR_RIGHT_CLOSE 5

#define LED_MOSFET 3

// нога кнопки открыть / закрыть
#define TRIGGER_BUTTON 2
byte triggerButtonState = 1; // 1 = сначала не нажата

// Выбор типа индикации завершения движения. Датчик тока ИЛИ концевики
#define CURRENT_INA219    // i2c датчик тока INA219

// перечисление ног концевиков
//#define ENDSTOP_LEFT_OPEN   8 
//#define ENDSTOP_LEFT_CLOSE  7
//#define ENDSTOP_RIGHT_OPEN  3
//#define ENDSTOP_RIGHT_CLOSE 4

////////////////////////////// КОНЕЦ НАСТРОЕК


#include <avr/eeprom.h>

#define EEPROM_PART            0
#define EEPROM_FIRST           1
#define EEPROM_TIMEOUT_LEFT    2
#define EEPROM_TIMEOUT_RIGHT   3
#define EEPROM_THRESHOLD_LEFT  4
#define EEPROM_THRESHOLD_RIGHT 5
#define EEPROM_MAXSPEED_LEFT   6
#define EEPROM_MAXSPEED_RIGHT  7
#define EEPROM_VELOCITY_LEFT   8
#define EEPROM_VELOCITY_RIGHT  9
#define EEPROM_REVERSE_LEFT    10
#define EEPROM_REVERSE_RIGHT   11

#define EEPROM_STATUS_LEFT     12
#define EEPROM_STATUS_RIGHT    13


#ifdef CURRENT_INA219
  #include <Wire.h>
  #include <Adafruit_INA219.h> //https://github.com/adafruit/Adafruit_INA219.git
  Adafruit_INA219 ina219;
#endif


byte part;             // в авто-режиме (и кнопкой) какая крышка едет ['l', 'r', 'b']
byte first;            // какая крышка едет первой при открытии (при закрытии наоборот), ['l', 'r']
byte timeoutInitLeft;  // таймаут движения левого мотора, сек
byte timeoutInitRight; // таймаут движения правого мотора, сек
byte thresholdLeft;    // порог срабатывания датчика тока при движении левого мотора, uint8 ADU датчика
byte thresholdRight;   // порог срабатывания датчика тока при движении правого мотора, uint8 ADU датчика
byte maxSpeedLeft;     // максимальный ШИМ (скорость) левого мотора, uint8
byte maxSpeedRight;    // максимальный ШИМ (скорость) правого мотора, uint8
byte velocityLeft;     // приращение (ускорение) ШИМ левого мотора, uint8
byte velocityRight;    // приращение (ускорение) ШИМ правого мотора, uint8
byte reverseLeft;      // реверс левого мотора, bool
byte reverseRight;     // реверс правого мотора, bool


byte moveLeftTo  = 's';    // 'o' - open, 'c' - close, 's' - stop, 'g' - gap (ждёт очереди на движение)
byte moveRightTo = 's';    // 'o' - open, 'c' - close, 's' - stop, 'g' - gap (ждёт очереди на движение)
byte speedLeft   = 0;  // текущая скорость левого мотора
byte speedRight  = 0;  // текущая скорость правого мотора

byte commandStage = 0;
char command = ' ';

byte motor_left_open, motor_left_close;
byte motor_right_open, motor_right_close;


void reverseInit()
{
  if (reverseLeft == 0) {
    motor_left_open = MOTOR_LEFT_OPEN;
    motor_left_close = MOTOR_LEFT_CLOSE;
  }
  else {
    motor_left_open = MOTOR_LEFT_CLOSE;
    motor_left_close = MOTOR_LEFT_OPEN;
  }

  if (reverseRight == 0) {
    motor_right_open = MOTOR_RIGHT_OPEN;
    motor_right_close = MOTOR_RIGHT_CLOSE;
  }
  else {
    motor_right_open = MOTOR_RIGHT_CLOSE;
    motor_right_close = MOTOR_RIGHT_OPEN;
  }
}


void setup() {
  pinMode(MOTOR_LEFT_OPEN, OUTPUT);
  pinMode(MOTOR_LEFT_CLOSE, OUTPUT);
  pinMode(MOTOR_RIGHT_OPEN, OUTPUT);
  pinMode(MOTOR_RIGHT_CLOSE, OUTPUT);
  pinMode(LED_MOSFET, OUTPUT);

  #ifdef TRIGGER_BUTTON
    pinMode(TRIGGER_BUTTON, INPUT_PULLUP);
  #endif

  Serial.begin(115200);

  #ifdef CURRENT_INA219
    ina219.begin();
    ina219.setCalibration_16V_400mA();
  #endif

  #ifdef ENDSTOP_LEFT_OPEN
    pinMode(ENDSTOP_LEFT_OPEN,   INPUT);
    pinMode(ENDSTOP_LEFT_CLOSE,  INPUT);
    pinMode(ENDSTOP_RIGHT_OPEN,  INPUT);
    pinMode(ENDSTOP_RIGHT_CLOSE, INPUT);
  #endif

  #ifdef DEBUG
    Serial.println("start");
  #endif

  #ifdef INIT_EEPROM
    eeprom_write_byte(EEPROM_PART, 'r');
    eeprom_write_byte(EEPROM_FIRST, 'r');
    eeprom_write_byte(EEPROM_TIMEOUT_LEFT, 20);
    eeprom_write_byte(EEPROM_TIMEOUT_RIGHT, 20);
    eeprom_write_byte(EEPROM_THRESHOLD_LEFT, 150);
    eeprom_write_byte(EEPROM_THRESHOLD_RIGHT, 150);
    eeprom_write_byte(EEPROM_MAXSPEED_LEFT, 220);
    eeprom_write_byte(EEPROM_MAXSPEED_RIGHT, 220);
    eeprom_write_byte(EEPROM_VELOCITY_LEFT, 5);
    eeprom_write_byte(EEPROM_VELOCITY_RIGHT, 5);
    eeprom_write_byte(EEPROM_REVERSE_LEFT, 0);
    eeprom_write_byte(EEPROM_REVERSE_RIGHT, 0);
    eeprom_write_byte(EEPROM_STATUS_LEFT, 'u');
    eeprom_write_byte(EEPROM_STATUS_RIGHT, 'u');
  #endif

  part = eeprom_read_byte(EEPROM_PART);
  if (part == 255) {
    part = 'r';
  }

  first = eeprom_read_byte(EEPROM_FIRST);
  if (first == 255) {
    first = 'r';
  }

  timeoutInitLeft = eeprom_read_byte(EEPROM_TIMEOUT_LEFT);
  if (timeoutInitLeft == 255) {
    timeoutInitLeft = 20;
  }

  timeoutInitRight = eeprom_read_byte(EEPROM_TIMEOUT_RIGHT);
  if (timeoutInitRight == 255) {
    timeoutInitRight = 20;
  }

  thresholdLeft = eeprom_read_byte(EEPROM_THRESHOLD_LEFT);
  if (thresholdLeft == 255) {
    thresholdLeft = 150;
  }

  thresholdRight = eeprom_read_byte(EEPROM_THRESHOLD_RIGHT);
  if (thresholdRight == 255) {
    thresholdRight = 150;
  }

  maxSpeedLeft = eeprom_read_byte(EEPROM_MAXSPEED_LEFT);
  maxSpeedRight = eeprom_read_byte(EEPROM_MAXSPEED_RIGHT);

  velocityLeft = eeprom_read_byte(EEPROM_VELOCITY_LEFT);
  if (velocityLeft == 255) {
    velocityLeft = 10;
  }

  velocityRight = eeprom_read_byte(EEPROM_VELOCITY_RIGHT);
  if (velocityRight == 255) {
    velocityRight = 10;
  }

  reverseLeft = eeprom_read_byte(EEPROM_REVERSE_LEFT);
  if (reverseLeft == 255) {
    reverseLeft = 0;
  }

  reverseRight = eeprom_read_byte(EEPROM_REVERSE_RIGHT);
  if (reverseRight == 255) {
    reverseRight = 0;
  }

  reverseInit();

  statusLeft = eeprom_read_byte(EEPROM_STATUS_LEFT);
  if (statusLeft == 255) {
    statusLeft = 'u';
  }

  statusRight = eeprom_read_byte(EEPROM_STATUS_RIGHT);
  if (statusRight == 255) {
    statusRight = 'u';
  }

  #ifdef DEBUG
    Serial.print("read eeprom part = ");
    Serial.println((char) part);
    Serial.print("read eeprom first = ");
    Serial.println((char) first);
    Serial.print("read eeprom timeout left = ");
    Serial.println(timeoutInitLeft, DEC);
    Serial.print("read eeprom timeout right = ");
    Serial.println(timeoutInitRight, DEC);
    Serial.print("read eeprom threshold left = ");
    Serial.println(thresholdLeft, DEC);
    Serial.print("read eeprom threshold right = ");
    Serial.println(thresholdRight, DEC);
    Serial.print("read eeprom maxSpeed left = ");
    Serial.println(maxSpeedLeft, DEC);
    Serial.print("read eeprom maxSpeed right = ");
    Serial.println(maxSpeedRight, DEC);
    Serial.print("read eeprom velocity left = ");
    Serial.println(velocityLeft, DEC);
    Serial.print("read eeprom velocity right = ");
    Serial.println(velocityRight, DEC);
    Serial.print("read eeprom reverse left = ");
    Serial.println(reverseLeft, DEC);
    Serial.print("read eeprom reverse right = ");
    Serial.println(reverseRight, DEC);
    Serial.print("read eeprom status left = ");
    Serial.println(statusLeft);
    Serial.print("read eeprom status right = ");
    Serial.println(statusRight);
  #endif

  // ушёл от прерывайний таймера, т.к. они схавали ноги 9 и 10. Надо менять схему на 3 и 11 (timer2)
  // // ежесекундный таймер. 
  // TCCR1A = 0;
  // TCCR1B = 0;
  // TCNT1 = 34286;   
  // TCCR1B |= (1 << CS12);    
  // TIMSK1 |= (1 << TOIE1);   
  // interrupts();             
}

#define TIMEOUT_SCALER 10
int timeout = 0;     // таймаут движения крышек, сек * TIMEOUT_SCALER

int brightness = 0;  // текущая яркость лампочки (для реализации ШИМ analogRead)

/*
// каждую секунду уменьшаю счётчик таймаута и, если он наступил, генерю ошибку
ISR(TIMER1_OVF_vect)
{
  interrupts();

  TCNT1 = 34286;

  if ( (moveLeftTo == 'c') || (moveLeftTo == 'o') ) {
    if (--timeout == 255) {
      statusLeft  = 't';
      speedLeft   = 0;
      moveLeftTo  = 's';
      moveRightTo = 's';

      #ifdef DEBUG
        Serial.println("got timeout left motor");
      #endif

      return;
    }
  }
  
  if ( (moveRightTo == 'c') || (moveRightTo == 'o') ) {
    if (--timeout == 255) {
      statusRight = 't';
      speedRight  = 0;
      moveRightTo = 's';
      moveLeftTo  = 's';

      #ifdef DEBUG
        Serial.println("got timeout right motor");
      #endif

      return;
    }
  }
}
*/

void startMoveAuto(byte dir)
{
  switch (part) {
    case 'l':
      moveLeftTo  = dir;
      moveRightTo = 's';
      timeout = timeoutInitLeft * TIMEOUT_SCALER;
      break;

    case 'r':
      moveLeftTo  = 's';
      moveRightTo = dir;
      timeout = timeoutInitRight * TIMEOUT_SCALER;
      break;

    case 'b':
      if (dir == 'o') {
        if (first == 'l') { // начнём открывать с левой
          moveLeftTo  = dir;
          moveRightTo = 'g';
          timeout = timeoutInitLeft * TIMEOUT_SCALER;
        }
        else {
          moveLeftTo  = 'g';
          moveRightTo = dir;
          timeout = timeoutInitRight * TIMEOUT_SCALER;
        }
      }
      else {
        if (first == 'r') { // начнём закрывать с правой
          moveLeftTo  = dir;
          moveRightTo = 'g';
          timeout = timeoutInitLeft * TIMEOUT_SCALER;
        }
        else {
          moveLeftTo  = 'g';
          moveRightTo = dir;
          timeout = timeoutInitRight * TIMEOUT_SCALER;
        }
      }
      break;
  }
}


void loop() {

  int current;

  // датчики тока
  #ifdef CURRENT_INA219
    /////////////////////// ЗАМЕР ТОКА ///////////////////
    double sum = 0;

    for (int i = 0; i < 50; i++) {
      sum += ina219.getCurrent_mA();
    }
  
    current = - (sum / 100);
    
    #ifdef DEBUG
      if ( (moveLeftTo != 's') || (moveRightTo != 's') ) {
        Serial.print("INA219 current = ");
        Serial.println(current);
      }
    #endif

  #endif


  // триггер-кнопка срабатывает по отпусканию
  #ifdef TRIGGER_BUTTON
  if (digitalRead(TRIGGER_BUTTON) == 0) {
    // нажата
    triggerButtonState = 0;
  }

  if ( (digitalRead(TRIGGER_BUTTON) == 1) && (triggerButtonState == 0) ) {
    // отжата
    triggerButtonState = 1;

    // если куда-то бежал и нажата кнопка, остановить
    if ((moveLeftTo == 'o') || (moveLeftTo == 'c') || (moveRightTo == 'o') || (moveRightTo == 'c')) {
      digitalWrite(motor_left_open, 0);
      digitalWrite(motor_left_close, 0);
      digitalWrite(motor_right_open, 0);
      digitalWrite(motor_right_close, 0);
  
      #ifdef DEBUG
        Serial.println("stop move by trigger button");
      #endif
      
      statusLeft = 'u';
      statusRight = 'u';
      eeprom_write_byte(EEPROM_STATUS_LEFT, statusLeft);
      eeprom_write_byte(EEPROM_STATUS_RIGHT, statusRight);
      
      speedLeft  = 0;
      speedRight = 0;

      moveLeftTo  = 's';
      moveRightTo = 's';
    }
    else {
      // если стоял и нажата кнопка - ехать!
      // но куда? Если что не ясно, то закрывать!
      byte dir = 'c';

      // кроме как, если ясно, что закрыто. Тогда открывать!
      switch (part) {
        case 'l':
          if (statusLeft == 'c') {
            dir = 'o';
          }
          break;

        case 'r':
          if (statusRight == 'c') {
            dir = 'o';
          }
          break;

        case 'b':
          if ((statusLeft == 'c') && (statusRight == 'c')) {
            dir = 'o';
          }
          break;
      }

      #ifdef DEBUG
        Serial.print("start move by trigger button: ");
        Serial.println(dir);
      #endif

      startMoveAuto(dir);
    }
  }
  #endif

  
  ////////// КОНЦЕВИКИ !!!!!!!!!!!!!!!!
  #ifdef ENDSTOP_LEFT_OPEN
    current = 0;
  
    switch (moveLeftTo) {
      case 'o':
        #ifdef DEBUG
          Serial.print("left open endstop = ");
          Serial.println(digitalRead(ENDSTOP_LEFT_OPEN));
        #endif
        
        if (digitalRead(ENDSTOP_LEFT_OPEN) == 0) {
          current = thresholdLeft + 1;
        }
        break;
        
      case 'c':
        #ifdef DEBUG
          Serial.print("left close endstop = ");
          Serial.println(digitalRead(ENDSTOP_LEFT_CLOSE));
        #endif
        
        if (digitalRead(ENDSTOP_LEFT_CLOSE) == 0) {
          current = thresholdLeft + 1;
        }
        break;
    }
    
    switch (moveRightTo) {
      case 'o':
        #ifdef DEBUG
          Serial.print("right open endstop = ");
          Serial.println(digitalRead(ENDSTOP_RIGHT_OPEN));
        #endif
        
        if (digitalRead(ENDSTOP_RIGHT_OPEN) == 0) {
          current = thresholdRight + 1;
        }
        break;
        
      case 'c':
        #ifdef DEBUG
          Serial.print("right close endstop = ");
          Serial.println(digitalRead(ENDSTOP_RIGHT_CLOSE));
        #endif
        
        if (digitalRead(ENDSTOP_RIGHT_CLOSE) == 0) {
          current = thresholdRight + 1;
        }
        break;
    }
  #endif

  
  ////////////////////// ДВИЖЕНИЕ ЛЕВОГО МОТОРА ////////////////
  if ( (moveLeftTo == 'o') || (moveLeftTo == 'c') ) {
    if (speedLeft < maxSpeedLeft) {
      if ((speedLeft + velocityLeft) >= maxSpeedLeft) {
        speedLeft = maxSpeedLeft;
      }
      else {
        speedLeft += velocityLeft;
      }
    }
  }
  
  switch (moveLeftTo) {
    case 's':
      digitalWrite(motor_left_open, 0);
      digitalWrite(motor_left_close, 0);
      break;

    case 'c':
      digitalWrite(motor_left_open, 0);
      analogWrite(motor_left_close, speedLeft);
      break;

    case 'o':
      analogWrite(motor_left_open, speedLeft);
      digitalWrite(motor_left_close, 0);
      break;
  }


  ////////////// ОСТАНОВ ЛЕВОГО МОТОРА ////////////
  if (((moveLeftTo == 'o') || (moveLeftTo == 'c')) && (current > thresholdLeft)) {
    digitalWrite(motor_left_open, 0);
    digitalWrite(motor_left_close, 0);

    #ifdef DEBUG
      Serial.print("stop left by threshold. Current=");
      Serial.print(current);
      Serial.print(", threshold=");
      Serial.println(thresholdLeft);
    #endif
    
    statusLeft = moveLeftTo;
    eeprom_write_byte(EEPROM_STATUS_LEFT, statusLeft);
    
    speedLeft  = 0;
    moveLeftTo = 's';

    if (moveRightTo == 'g') { // если было отложено движение правого мотора, запустить его
      moveRightTo = statusLeft;
      timeout = timeoutInitRight * TIMEOUT_SCALER;

      #ifdef DEBUG
        Serial.println("start right after GAP");
      #endif

      delay(1000); // дать мотору остановиться. Иначе датчик тока показывает больше threshold

      return; // заново к замеру тока. А то правый мотор тут же остановится
    }
  }

  
  ////////////////////// ДВИЖЕНИЕ ПРАВОГО МОТОРА ////////////////
  if ( (moveRightTo == 'o') || (moveRightTo == 'c') ) {
    if (speedRight < maxSpeedRight) {
      if ((speedRight + velocityRight) >= maxSpeedRight) {
        speedRight = maxSpeedRight;
      }
      else {
        speedRight += velocityRight;
      }
    }
  }

  switch (moveRightTo) {
    case 's':
      digitalWrite(motor_right_open, 0);
      digitalWrite(motor_right_close, 0);
      break;

    case 'c':
      digitalWrite(motor_right_open, 0);
      analogWrite(motor_right_close, speedRight);
      break;

    case 'o':
      analogWrite(motor_right_open, speedRight);
      digitalWrite(motor_right_close, 0);
      break;
  }


  ////////////// ОСТАНОВ ПРАВОГО МОТОРА ////////////
  if (((moveRightTo == 'o') || (moveRightTo == 'c')) && (current > thresholdRight)) {
    digitalWrite(motor_right_open, 0);
    digitalWrite(motor_right_close, 0);

    #ifdef DEBUG
      Serial.print("stop right by threshold. Current=");
      Serial.print(current);
      Serial.print(", threshold=");
      Serial.println(thresholdRight);
    #endif
    
    statusRight = moveRightTo;
    eeprom_write_byte(EEPROM_STATUS_RIGHT, statusRight);

    speedRight  = 0;
    moveRightTo = 's';

    if (moveLeftTo == 'g') { // если было отложено движение левого мотора, запустить его
      moveLeftTo = statusRight;
      timeout = timeoutInitLeft * TIMEOUT_SCALER;

      #ifdef DEBUG
        Serial.println("start left after GAP");
      #endif
    
      delay(1000); // дать мотору остановиться. Иначе датчик тока показывает больше threshold

      return; // заново к замеру тока. А то левый мотор тут же остановится
    }
  }


  ////////////////////// ЧТЕНИЕ КОМАНД ИЗ SERIAL ////////////////
  if (Serial.available()) {
    byte ch = Serial.read();

    switch (commandStage) {
      case 0:
        if (ch == 'c') {
          commandStage = 1;
        }

        break;

      case 1:
        command = ch;

        switch (command) {
          case 'g':
            #ifdef DEBUG
              Serial.print("current=");
              Serial.print(current, DEC);
              Serial.print(",timeout=");
              Serial.print(timeout, DEC);
              Serial.print(",moveLeftTo=");
              Serial.print((char)moveLeftTo);
              Serial.print(",moveRightTo=");
              Serial.print((char)moveRightTo);
              Serial.print(",statusLeft=");
              Serial.print((char)statusLeft);
              Serial.print(",statusRight=");
              Serial.print((char)statusRight);
              Serial.print(",light=");
              Serial.print(brightness, DEC);
              Serial.print(",speedLeft=");
              Serial.print(speedLeft, DEC);
              Serial.print(",speedRight=");
              Serial.print(speedRight, DEC);
              Serial.println();
            #else
              Serial.write(0xee);
              Serial.write(current);
              Serial.write(timeout);
              Serial.write(moveLeftTo);
              Serial.write(moveRightTo);
              Serial.write(statusLeft);
              Serial.write(statusRight);
              Serial.write(brightness);
              Serial.write(speedLeft);
              Serial.write(speedRight);
              Serial.write(0);
              // @todo checksum?
            #endif            

            commandStage = 0;
            break;

          case 'e':
            #ifdef DEBUG
              Serial.print("part=");
              Serial.print((char)part);
              Serial.print(",first=");
              Serial.print((char)first);
              Serial.print(",timeoutLeft=");
              Serial.print(timeoutInitLeft, DEC);
              Serial.print(",timeoutRight=");
              Serial.print(timeoutInitRight, DEC);
              Serial.print(",thresholdLeft=");
              Serial.print(thresholdLeft, DEC);
              Serial.print(",thresholdRight=");
              Serial.print(thresholdRight, DEC);
              Serial.print(",maxSpeedLeft=");
              Serial.print(maxSpeedLeft, DEC);
              Serial.print(",maxSpeedRight=");
              Serial.print(maxSpeedRight, DEC);
              Serial.print(",velocityLeft=");
              Serial.print(velocityLeft, DEC);
              Serial.print(",velocityRight=");
              Serial.print(velocityRight, DEC);
              Serial.print(",reverseLeft=");
              Serial.print(reverseLeft, DEC);
              Serial.print(",reverseRight=");
              Serial.print(reverseRight, DEC);
              Serial.println();
            #else
              Serial.write(0xee);
              Serial.write(part);
              Serial.write(first);
              Serial.write(timeoutInitLeft);
              Serial.write(timeoutInitRight);
              Serial.write(thresholdLeft);
              Serial.write(thresholdRight);
              Serial.write(maxSpeedLeft);
              Serial.write(maxSpeedRight);
              Serial.write(velocityLeft);
              Serial.write(velocityRight);
              Serial.write(reverseLeft);
              Serial.write(reverseRight);
              Serial.write(0);
            #endif            

            commandStage = 0;
            break;

          case 'a':
            #ifdef DEBUG
              Serial.println("got command ABORT");
            #endif
            
            if ( (moveLeftTo != 's') || (speedLeft > 0) ) {
              #ifdef DEBUG
                Serial.println("ABORT left motor movement");
              #endif
              
              digitalWrite(motor_left_open, 0);
              digitalWrite(motor_left_close, 0);
              statusLeft  = 'u';
              moveLeftTo  = 's';
              speedLeft   = 0;
            }
          
            if ( (moveRightTo != 's') || (speedRight > 0) ) {
              #ifdef DEBUG
                Serial.println("ABORT right motor movement");
              #endif
              
              digitalWrite(motor_right_open, 0);
              digitalWrite(motor_right_close, 0);
              statusRight = 'u';
              moveRightTo = 's';
              speedRight  = 0;
            }
            
            commandStage = 0;
            break;

          default:
            commandStage = 2;
        }
        
        break;

      case 2:
        switch (command) {
          case 'l':
            #ifdef DEBUG
              Serial.print("got command LIGHT: ");
              Serial.println(ch, DEC);
            #endif

            brightness = ch;
            analogWrite(LED_MOSFET, brightness);
            
            commandStage = 0;
            break;
            
          case 'o':
            digitalWrite(LED_MOSFET, 0); // погасить лампаду
            brightness = 0;

            switch (ch) {
              case 'l': // команда "col" = command open left
                moveLeftTo  = 'o';
                moveRightTo = 's';
                timeout = timeoutInitLeft * TIMEOUT_SCALER;
                break;
              
              case 'r': // команда "cor" = command open right
                moveLeftTo  = 's';
                moveRightTo = 'o';
                timeout = timeoutInitRight * TIMEOUT_SCALER;
                break;
    
              case 'a': // команда "coa" = command open auto
                startMoveAuto('o');
                break;
    
              case 'b': // команда "cob" = command open both (по одной створке, с учётом first)
                if (first == 'l') { // начнём открывать с левой
                  moveLeftTo  = 'o';
                  moveRightTo = 'g';
                  timeout = timeoutInitLeft * TIMEOUT_SCALER;
                }
                else {
                  moveLeftTo  = 'g';
                  moveRightTo = 'o';
                  timeout = timeoutInitRight * TIMEOUT_SCALER;
                }
                break;
            }

            speedLeft  = 0;
            speedRight = 0;
            
            #ifdef DEBUG
              Serial.println("got command OPEN");
            #endif
            
            commandStage = 0;
            break;
            
          case 'c':
            digitalWrite(LED_MOSFET, 0); // погасить лампаду
            brightness = 0;

            switch (ch) {
              case 'l': // команда "ccl" = command close left
                moveLeftTo  = 'c';
                moveRightTo = 's';
                timeout = timeoutInitLeft * TIMEOUT_SCALER;
                break;
              
              case 'r': // команда "ccr" = command close right
                moveLeftTo  = 's';
                moveRightTo = 'c';
                timeout = timeoutInitRight * TIMEOUT_SCALER;
                break;
    
              case 'a': // команда "cca" = command close auto
                startMoveAuto('c');
                break;
    
              case 'b': // команда "ccb" = command close both (по одной створке, с учётом first)
                if (first == 'l') { // начнём закрывать с правой
                  moveLeftTo  = 'g';
                  moveRightTo = 'c';
                  timeout = timeoutInitLeft * TIMEOUT_SCALER;
                }
                else {
                  moveLeftTo  = 'c';
                  moveRightTo = 'g';
                  timeout = timeoutInitRight * TIMEOUT_SCALER;
                }
                break;
            }

            speedLeft  = 0;
            speedRight = 0;
            
            #ifdef DEBUG
              Serial.println("got command CLOSE");
            #endif
            
            commandStage = 0;
            break;
            
          case 's':
            part = ch;
            eeprom_write_byte(EEPROM_PART, part);
            commandStage = 3;
            break;
            
          default:
            commandStage = 0;
        }
        break; // case stage = 2

      case 3:
        if (command == 's') {
          first = ch;
          eeprom_write_byte(EEPROM_FIRST, first);
          commandStage = 4;
        }
        break;

      case 4:
        if (command == 's') {
          timeoutInitLeft = ch;
          eeprom_write_byte(EEPROM_TIMEOUT_LEFT, timeoutInitLeft);
          commandStage = 5;
        }
        break;

      case 5:
        if (command == 's') {
          timeoutInitRight = ch;
          eeprom_write_byte(EEPROM_TIMEOUT_RIGHT, timeoutInitRight);
          commandStage = 6;
        }
        break;

      case 6:
        if (command == 's') {
          thresholdLeft = ch;
          eeprom_write_byte(EEPROM_THRESHOLD_LEFT, thresholdLeft);
          commandStage = 7;
        }
        break;

      case 7:
        if (command == 's') {
          thresholdRight = ch;
          eeprom_write_byte(EEPROM_THRESHOLD_RIGHT, thresholdRight);
          commandStage = 8;
        }
        break;

      case 8:
        if (command == 's') {
          maxSpeedLeft = ch;
          eeprom_write_byte(EEPROM_MAXSPEED_LEFT, maxSpeedLeft);
          commandStage = 9;
        }
        break;

      case 9:
        if (command == 's') {
          maxSpeedRight = ch;
          eeprom_write_byte(EEPROM_MAXSPEED_RIGHT, maxSpeedRight);
          commandStage = 10;
        }
        break;

      case 10:
        if (command == 's') {
          velocityLeft = ch;
          eeprom_write_byte(EEPROM_VELOCITY_LEFT, velocityLeft);
          commandStage = 11;
        }
        break;

      case 11:
        if (command == 's') {
          velocityRight = ch;
          eeprom_write_byte(EEPROM_VELOCITY_RIGHT, velocityRight);
          commandStage = 12;
        }
        break;

      case 12:
        if (command == 's') {
          reverseLeft = ch;
          eeprom_write_byte(EEPROM_REVERSE_LEFT, reverseLeft);
          commandStage = 13;
        }
        break;

      case 13:
        if (command == 's') {
          reverseRight = ch;
          eeprom_write_byte(EEPROM_REVERSE_RIGHT, reverseRight);
          commandStage = 0;

          reverseInit();
        }
        break;

    } // switch stage
  } // if serial.available
  
  delay(50);

  
  //////////// Timeout
  if ( (moveLeftTo == 'c') || (moveLeftTo == 'o') ) {
    if (--timeout < 0) {
      statusLeft  = 't';
      speedLeft   = 0;
      moveLeftTo  = 's';
      moveRightTo = 's';

      #ifdef DEBUG
        Serial.println("got timeout left motor");
      #endif

      return;
    }
  }
  
  if ( (moveRightTo == 'c') || (moveRightTo == 'o') ) {
    if (--timeout < 0) {
      statusRight = 't';
      speedRight  = 0;
      moveRightTo = 's';
      moveLeftTo  = 's';

      #ifdef DEBUG
        Serial.println("got timeout right motor");
      #endif

      return;
    }
  }
  
}
