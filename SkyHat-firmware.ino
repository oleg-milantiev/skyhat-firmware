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
 * - 'l'(3 байта): Light, свет. Параметр: '1' / '0' - включить / выключить
 * - 'o'(3 байта): Open, открыть. Параметр @todo: 'l' (левую крышку), 'r' (правую крышку), 'a' (обе крышки)
 * - 'c'(3 байта): Close, закрыть. Параметр @todo: 'l' (левую крышку), 'r' (правую крышку), 'a' (обе крышки)
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
//#define MOTOR_LEFT_REVERSE
//#define MOTOR_RIGHT_REVERSE

// ноги ардуины
#define _MOTOR_LEFT_OPEN  9
#define _MOTOR_LEFT_CLOSE 10

// если L298N с инвертором на входе
//#define MOTOR_INVERSE

#define _MOTOR_RIGHT_OPEN  5
#define _MOTOR_RIGHT_CLOSE 6

#define LED_MOSFET 3


// Выбор типа индикации завершения движения. Датчик тока ИЛИ концевики
#define CURRENT_INA219    // i2c датчик тока INA219

// перечисление ног концевиков
//#define ENDSTOP_LEFT_OPEN   8 
//#define ENDSTOP_LEFT_CLOSE  7
//#define ENDSTOP_RIGHT_OPEN  3
//#define ENDSTOP_RIGHT_CLOSE 4

////////////////////////////// КОНЕЦ НАСТРОЕК


#include <avr/eeprom.h>
#define EEPROM_FIRST      0
#define EEPROM_TIMEOUT    1
#define EEPROM_BRIGHTNESS 2
#define EEPROM_THRESHOLD  3
#define EEPROM_MAXSPEED   4
#define EEPROM_VELOCITY   5


#ifdef CURRENT_INA219
  #include <Wire.h>
  #include <Adafruit_INA219.h> //https://github.com/adafruit/Adafruit_INA219.git
  Adafruit_INA219 ina219;
#endif


byte first;       // какая крышка едет первой при открытии (при закрытии наоборот), ['l', 'r']
byte timeoutInit; // таймаут движения крышек, сек
byte brightnessInit; // яркость EL Panel
byte threshold;   // порог срабатывания датчика тока, uint8 ADU MAX471
byte maxSpeed;    // максимальный ШИМ (скорость) моторов, uint8
byte velocity;    // приращение (ускорение) ШИМ моторов, uint8

byte moveLeftTo  = 's';    // 'o' - open, 'c' - close, 's' - stop, 'g' - gap (ждёт очереди на движение)
byte moveRightTo = 's';    // 'o' - open, 'c' - close, 's' - stop, 'g' - gap (ждёт очереди на движение)
byte speedLeft   = 0;  // текущая скорость левого мотора
byte speedRight  = 0;  // текущая скорость правого мотора



#ifdef MOTOR_LEFT_REVERSE
  #define MOTOR_LEFT_OPEN  _MOTOR_LEFT_CLOSE
  #define MOTOR_LEFT_CLOSE _MOTOR_LEFT_OPEN
#else
  #define MOTOR_LEFT_OPEN  _MOTOR_LEFT_OPEN
  #define MOTOR_LEFT_CLOSE _MOTOR_LEFT_CLOSE
#endif
#ifdef MOTOR_RIGHT_REVERSE
  #define MOTOR_RIGHT_OPEN  _MOTOR_RIGHT_CLOSE
  #define MOTOR_RIGHT_CLOSE _MOTOR_RIGHT_OPEN
#else
  #define MOTOR_RIGHT_OPEN  _MOTOR_RIGHT_OPEN
  #define MOTOR_RIGHT_CLOSE _MOTOR_RIGHT_CLOSE
#endif


byte commandStage = 0;
char command = ' ';


void setup() {
  pinMode(MOTOR_LEFT_OPEN, OUTPUT);
  pinMode(MOTOR_LEFT_CLOSE, OUTPUT);
  pinMode(MOTOR_RIGHT_OPEN, OUTPUT);
  pinMode(MOTOR_RIGHT_CLOSE, OUTPUT);
  pinMode(LED_MOSFET, OUTPUT);

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
    eeprom_write_byte(EEPROM_FIRST, 'l');
    eeprom_write_byte(EEPROM_TIMEOUT, 20);
    eeprom_write_byte(EEPROM_BRIGHTNESS, 128);
    eeprom_write_byte(EEPROM_THRESHOLD, 100);
    eeprom_write_byte(EEPROM_MAXSPEED, 180);
    eeprom_write_byte(EEPROM_VELOCITY, 10);
  #endif

  first = eeprom_read_byte(EEPROM_FIRST);
  if (first == 255) {
    first = 'l';
  }
  #ifdef DEBUG
    Serial.print("read eeprom first = ");
    Serial.println((char) first);
  #endif

  timeoutInit = eeprom_read_byte(EEPROM_TIMEOUT);
  if (timeoutInit == 255) {
    timeoutInit = 10;
  }
  #ifdef DEBUG
    Serial.print("read eeprom timeout = ");
    Serial.println(timeoutInit, DEC);
  #endif

  brightnessInit = eeprom_read_byte(EEPROM_BRIGHTNESS);

  #ifdef DEBUG
    Serial.print("read eeprom brightness = ");
    Serial.println(brightnessInit, DEC);
  #endif

  threshold = eeprom_read_byte(EEPROM_THRESHOLD);
  if (threshold == 255) {
    threshold = 60;
  }
  #ifdef DEBUG
    Serial.print("read eeprom threshold = ");
    Serial.println(threshold, DEC);
  #endif

  maxSpeed = eeprom_read_byte(EEPROM_MAXSPEED);
  #ifdef DEBUG
    Serial.print("read eeprom maxSpeed = ");
    Serial.println(maxSpeed, DEC);
  #endif

  velocity = eeprom_read_byte(EEPROM_VELOCITY);
  if (velocity == 255) {
    velocity = 10;
  }
  #ifdef DEBUG
    Serial.print("read eeprom velocity = ");
    Serial.println(velocity, DEC);
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

void loop() {

  int current;

  // датчики тока
  #ifdef CURRENT_INA219
    /////////////////////// ЗАМЕР ТОКА ///////////////////
    double sum = 0;

    for (int i = 0; i < 50; i++) {
      sum += ina219.getCurrent_mA();
    }
  
    current = - (sum / 50);
    
    #ifdef DEBUG
      if ( (moveLeftTo != 's') || (moveRightTo != 's') ) {
        Serial.print("INA219 current = ");
        Serial.println(current);
      }
    #endif

  #endif

  // концевики
  #ifdef ENDSTOP_LEFT_OPEN
    current = 0;
  
    switch (moveLeftTo) {
      case 'o':
        #ifdef DEBUG
          Serial.print("left open endstop = ");
          Serial.println(digitalRead(ENDSTOP_LEFT_OPEN));
        #endif
        
        if (digitalRead(ENDSTOP_LEFT_OPEN) == 0) {
          current = threshold + 1;
        }
        break;
        
      case 'c':
        #ifdef DEBUG
          Serial.print("left close endstop = ");
          Serial.println(digitalRead(ENDSTOP_LEFT_CLOSE));
        #endif
        
        if (digitalRead(ENDSTOP_LEFT_CLOSE) == 0) {
          current = threshold + 1;
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
          current = threshold + 1;
        }
        break;
        
      case 'c':
        #ifdef DEBUG
          Serial.print("right close endstop = ");
          Serial.println(digitalRead(ENDSTOP_RIGHT_CLOSE));
        #endif
        
        if (digitalRead(ENDSTOP_RIGHT_CLOSE) == 0) {
          current = threshold + 1;
        }
        break;
    }
  #endif

  ////////////////////// ДВИЖЕНИЕ ЛЕВОГО МОТОРА ////////////////
  if ( (moveLeftTo == 'o') || (moveLeftTo == 'c') ) {
    if (speedLeft < maxSpeed) {
      if ((speedLeft + velocity) >= maxSpeed) {
        speedLeft = maxSpeed;
      }
      else {
        speedLeft += velocity;
      }
    }
  }
  
  switch (moveLeftTo) {
    case 's':
      #ifdef MOTOR_INVERSE
        digitalWrite(MOTOR_LEFT_OPEN, 1);
        digitalWrite(MOTOR_LEFT_CLOSE, 1);
      #else
        digitalWrite(MOTOR_LEFT_OPEN, 0);
        digitalWrite(MOTOR_LEFT_CLOSE, 0);
      #endif
      break;

    case 'c':
      #ifdef MOTOR_INVERSE
        digitalWrite(MOTOR_LEFT_OPEN, 1);
        analogWrite(MOTOR_LEFT_CLOSE, 255 - speedLeft);
      #else
        digitalWrite(MOTOR_LEFT_OPEN, 0);
        analogWrite(MOTOR_LEFT_CLOSE, speedLeft);
      #endif
      
      break;

    case 'o':
      #ifdef MOTOR_INVERSE
        analogWrite(MOTOR_LEFT_OPEN, 255 - speedLeft);
        digitalWrite(MOTOR_LEFT_CLOSE, 1);
      #else
        analogWrite(MOTOR_LEFT_OPEN, speedLeft);
        digitalWrite(MOTOR_LEFT_CLOSE, 0);
      #endif

      break;
  }


  ////////////// ОСТАНОВ ЛЕВОГО МОТОРА ////////////
  if (((moveLeftTo == 'o') || (moveLeftTo == 'c')) && (current > threshold)) {
    #ifdef MOTOR_INVERSE
      digitalWrite(MOTOR_LEFT_OPEN, 1);
      digitalWrite(MOTOR_LEFT_CLOSE, 1);
    #else
      digitalWrite(MOTOR_LEFT_OPEN, 0);
      digitalWrite(MOTOR_LEFT_CLOSE, 0);
    #endif

    #ifdef DEBUG
      Serial.print("stop left by threshold. Current=");
      Serial.print(current);
      Serial.print(", threshold=");
      Serial.println(threshold);
    #endif
    
    statusLeft = moveLeftTo;
    speedLeft  = 0;
    moveLeftTo = 's';

    if (moveRightTo == 'g') { // если было отложено движение правого мотора, запустить его
      moveRightTo = statusLeft;
      timeout = timeoutInit * TIMEOUT_SCALER;

      #ifdef DEBUG
        Serial.println("start right after GAP");
      #endif

      delay(1000); // дать мотору остановиться. Иначе датчик тока показывает больше threshold

      return; // заново к замеру тока. А то правый мотор тут же остановится
    }
  }

  
  ////////////////////// ДВИЖЕНИЕ ПРАВОГО МОТОРА ////////////////
  if ( (moveRightTo == 'o') || (moveRightTo == 'c') ) {
    if (speedRight < maxSpeed) {
      if ((speedRight + velocity) >= maxSpeed) {
        speedRight = maxSpeed;
      }
      else {
        speedRight += velocity;
      }
    }
  }

  switch (moveRightTo) {
    case 's':
      #ifdef MOTOR_INVERSE
        digitalWrite(MOTOR_RIGHT_OPEN, 1);
        digitalWrite(MOTOR_RIGHT_CLOSE, 1);
      #else
        digitalWrite(MOTOR_RIGHT_OPEN, 0);
        digitalWrite(MOTOR_RIGHT_CLOSE, 0);
      #endif
      break;

    case 'c':
      #ifdef MOTOR_INVERSE
        digitalWrite(MOTOR_RIGHT_OPEN, 1);
        analogWrite(MOTOR_RIGHT_CLOSE, 255 - speedRight);
      #else
        digitalWrite(MOTOR_RIGHT_OPEN, 0);
        analogWrite(MOTOR_RIGHT_CLOSE, speedRight);
      #endif
      
      break;

    case 'o':
      #ifdef MOTOR_INVERSE
        analogWrite(MOTOR_RIGHT_OPEN, 255 - speedRight);
        digitalWrite(MOTOR_RIGHT_CLOSE, 1);
      #else
        analogWrite(MOTOR_RIGHT_OPEN, speedRight);
        digitalWrite(MOTOR_RIGHT_CLOSE, 0);
      #endif

      break;
  }


  ////////////// ОСТАНОВ ПРАВОГО МОТОРА ////////////
  if (((moveRightTo == 'o') || (moveRightTo == 'c')) && (current > threshold)) {
    #ifdef MOTOR_INVERSE
      digitalWrite(MOTOR_RIGHT_OPEN, 1);
      digitalWrite(MOTOR_RIGHT_CLOSE, 1);
    #else
      digitalWrite(MOTOR_RIGHT_OPEN, 0);
      digitalWrite(MOTOR_RIGHT_CLOSE, 0);
    #endif

    #ifdef DEBUG
      Serial.print("stop right by threshold. Current=");
      Serial.print(current);
      Serial.print(", threshold=");
      Serial.println(threshold);
    #endif
    
    statusRight = moveRightTo;
    speedRight  = 0;
    moveRightTo = 's';

    if (moveLeftTo == 'g') { // если было отложено движение левого мотора, запустить его
      moveLeftTo = statusRight;
      timeout = timeoutInit * TIMEOUT_SCALER;

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
              Serial.print("first=");
              Serial.print((char)first);
              Serial.print(",timeout=");
              Serial.print(timeoutInit, DEC);
              Serial.print(",brightness=");
              Serial.print(brightnessInit, DEC);
              Serial.print(",threshold=");
              Serial.print(threshold, DEC);
              Serial.print(",maxSpeed=");
              Serial.print(maxSpeed, DEC);
              Serial.print(",velocity=");
              Serial.print(velocity, DEC);
              Serial.println();
            #else
              Serial.write(0xee);
              Serial.write(first);
              Serial.write(timeoutInit);
              Serial.write(brightnessInit);
              Serial.write(threshold);
              Serial.write(maxSpeed);
              Serial.write(velocity);
              Serial.write(0);
            #endif            

            commandStage = 0;
            break;

          case 'a':
            #ifdef MOTOR_INVERSE
              digitalWrite(MOTOR_LEFT_OPEN, 1);
              digitalWrite(MOTOR_LEFT_CLOSE, 1);
              digitalWrite(MOTOR_RIGHT_OPEN, 1);
              digitalWrite(MOTOR_RIGHT_CLOSE, 1);
            #else
              digitalWrite(MOTOR_LEFT_OPEN, 0);
              digitalWrite(MOTOR_LEFT_CLOSE, 0);
              digitalWrite(MOTOR_RIGHT_OPEN, 0);
              digitalWrite(MOTOR_RIGHT_CLOSE, 0);
            #endif
            
            moveLeftTo  = 's';
            moveRightTo = 's';
            speedLeft   = 0;
            speedRight  = 0;
            statusLeft  = 'u';
            statusRight = 'u';
            
            #ifdef DEBUG
              Serial.println("got command ABORT");
            #endif
            
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
              Serial.println((char)ch);
            #endif

            brightness = (ch == '1') ? brightnessInit : 0;
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
                break;
              
              case 'r': // команда "cor" = command open right
                moveLeftTo  = 's';
                moveRightTo = 'o';
                break;
    
              case 'a': // команда "coa" = command open all (по одной створке, с учётом first)
                if (first == 'l') { // начнём открывать с левой
                  moveLeftTo  = 'o';
                  moveRightTo = 'g';
                }
                else {
                  moveLeftTo  = 'g';
                  moveRightTo = 'o';
                }
                break;
            }

            speedLeft  = 0;
            speedRight = 0;
            timeout = timeoutInit * TIMEOUT_SCALER;
            
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
                break;
              
              case 'r': // команда "ccr" = command close right
                moveLeftTo  = 's';
                moveRightTo = 'c';
                break;
    
              case 'a': // команда "cca" = command close all (по одной створке, с учётом first)
                if (first == 'l') { // начнём закрывать с правой
                  moveLeftTo  = 'g';
                  moveRightTo = 'c';
                }
                else {
                  moveLeftTo  = 'c';
                  moveRightTo = 'g';
                }
                break;
            }

            speedLeft  = 0;
            speedRight = 0;
            timeout = timeoutInit * TIMEOUT_SCALER;
            
            #ifdef DEBUG
              Serial.println("got command CLOSE");
            #endif
            
            commandStage = 0;
            break;
            
          case 's':
            first = ch;
            eeprom_write_byte(EEPROM_FIRST, first);
            commandStage = 3;
            break;
            
          default:
            commandStage = 0;
        }
        break; // case stage = 2

      case 3:
        if (command == 's') {
          timeoutInit = ch;
          eeprom_write_byte(EEPROM_TIMEOUT, timeoutInit);
          commandStage = 4;
        }
        break;


      case 4:
        if (command == 's') {
          brightnessInit = ch;
          eeprom_write_byte(EEPROM_BRIGHTNESS, brightnessInit);
          commandStage = 5;
        }
        break;

      case 5:
        if (command == 's') {
          threshold = ch;
          eeprom_write_byte(EEPROM_THRESHOLD, threshold);
          commandStage = 6;
        }
        break;

      case 6:
        if (command == 's') {
          maxSpeed = ch;
          eeprom_write_byte(EEPROM_MAXSPEED, maxSpeed);
          commandStage = 7;
        }
        break;

      case 7:
        if (command == 's') {
          velocity = ch;
          eeprom_write_byte(EEPROM_VELOCITY, velocity);
          commandStage = 0;
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
