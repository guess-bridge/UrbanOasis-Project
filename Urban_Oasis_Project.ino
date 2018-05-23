/*
 *  U R B A N     O A S I S     P R O J E C T
 *  an arduino project made by Guza Gesian and Bedeschi Federica 
 *  see more at https://sites.google.com/site/projecturbanoasis/ *  
 */
 
//costanti display
#include <LiquidCrystal.h>
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 30, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//costanti bottoni
const int leftButton = 8, rightButton = 3, selectButton = 10;
int leftState = LOW, rightState = LOW, selectState = LOW;

//array orari di innaffiamento
const int numberOfMaxWaterings = 5;
int water[numberOfMaxWaterings * 2];

//costanti menu
const int numberOfPages = 14; //numero di pagine del menu
const char* pages[numberOfPages] = {"Irrigazione", "Umidita' aria", "Umidita' suolo", "Temperatura", "Ventola", "Pompa", "LED", 
                                    "Luminosita'", "Ora", "Auto-Mode", "Luminosita' min", "Temperatura max", "Umidità max", "Durata irrigazione"};
int pageState = 0; //deve essere compreso tra 0 e (numberOfPages - 1)
const int delayMenu = 2000;

//costanti sensore igrometrico
#define hygrometerPin A11
int hygrometerValue = 0;
int hygrometerValueOLD = 0;

//costanti GPS
#include <SoftwareSerial.h>
#include <TinyGPS.h>
long latitude = 0, longitude = 0;

//costanti pompa
const int pumpPin = 7;
int pumpState = LOW;
int pumpDuration = 1000; //durata irrigazione
int lastWater;

//costanti DHT22 = temperatura/umidità
#include "DHT.h"
#define DHTTYPE DHT22 //tipologia di sensore, cambiare con la propria versione
const int DHTPin = 6;
DHT dht(DHTPin, DHTTYPE);
float airHumidity = 0, airTemperature = 0, airHumidityOLD = 0, airTemperatureOLD = 0;

//costanti ventola
const int fanPin = 40;
int fanState = LOW;

//costanti sensore di luminosità
#define brightnessPin A12
int 
+-Value = 0, brightnessValueOLD = 0;

//costanti RTC
#include <Wire.h>
#include "DS1307.h"
DS1307 clock;

//costanti bluetooth
char message;

//costanti ricevitore infrarossi, usato per la decodifica del telecomando della striscia led
#include <boarddefs.h>
#include <IRremote.h>
#include <IRremoteInt.h>
#include <ir_Lego_PF_BitStreamEncoder.h>
int irReceiverPin = 13;      
IRrecv receiver(irReceiverPin);  
decode_results results;     
IRsend mySender;
int ledState = LOW; 

//variabili automode
int fanAuto = 0, pumpAuto = 0, ledAuto = 0; //0 quando auto è spenta, 1 accesa
int autoBrightness = 500, autoTemperature = 22, autoHumidity = 0;
int pumpLastTime;
int value;

void setup() {
  //inizializzazione display
  lcd.begin(16, 2);
  lcd.print("What a wonderful");
  lcd.setCursor(0,1);
  lcd.print("day it is!!!");
  delay(3500);
  lcd.clear();
  lcd.print("Your Urban Oasis");
  lcd.setCursor(0,1);
  lcd.print("greets you :)");
  delay(3500);
  //lcd.cursor();
  
  //transizione presentazione
  for (int i = 0; i < 16; i++) {
    lcd.setCursor(i,0);
    lcd.print("/");
    lcd.setCursor(i,1);
    lcd.print("/");
    delay(100);
  }
  for (int i = 0; i < 16; i++) {
    lcd.scrollDisplayRight();
    delay(100); 
  }
  
  //inizializzazione bottoni
  pinMode(leftButton, INPUT);
  pinMode(rightButton, INPUT);
  pinMode(selectButton, INPUT);

  //inizializzazione monitor seriale
  Serial.begin(9600);
  
  //inizializzazione GPS
  //gpsSerial.begin(9600);

  //inizializzazione pompa
  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, LOW);

  //inizializzazione ventola
  pinMode(fanPin, OUTPUT);
  digitalWrite(fanPin, LOW);
  
  //inizializzazione DHT
  dht.begin();

  //inizializzazione RTC
  clock.begin();
  clock.fillByYMD(2018,05,22);
  clock.fillByHMS(23,30,00);
  clock.fillDayOfWeek(TUE);
  clock.setTime();

  //inizializzazione bluetooth
  Serial3.begin(9600);

  //inizializzazione ricevitore infrarossi
  //receiver.enableIRIn();
}

int getSoilHumidity() {
  hygrometerValue = analogRead(hygrometerPin); 
  hygrometerValue = constrain(hygrometerValue,400,1023);
  hygrometerValue = map(hygrometerValue,400,1023,100,0); //range 0-100 %
  return hygrometerValue;
}

/* void getCoord() {
  while (gpsSerial.available()) {
    if (gps.encode(gpsSerial.read())) {
      gps.get_position(&lat,&lon);
      Serial.print("Position: ");
      Serial.print("lat: ");
      Serial.print(double(lat)/1000000,4);
      Serial.print(" lon: ");
      Serial.println(double(lon)/1000000,4);
    }
  }
} */

void turnPump(int state) { //cambia lo stato della pompa, se viene accesa la chiude dopo una durata "pumpDuration" definita
  pumpState = state;
  digitalWrite(pumpPin, pumpState);
  if (pumpState == HIGH) {
    delay(pumpDuration);
    pumpState = LOW;
    digitalWrite(pumpPin, pumpState);
  }
}

float getTemperature() {
  airTemperature = dht.readTemperature();
  return airTemperature;
}

float getAirHumidity() {
  airHumidity = dht.readHumidity();
  return airHumidity;
}

void turnFan(int state) {
  fanState = state;
  digitalWrite(fanPin, fanState);  
}

int getBrightness() {
  brightnessValue = analogRead(brightnessPin);
  brightnessValue = map(brightnessValue,0,1000,0,100);
  return brightnessValue;
}

int searchWaterTime() {
  clock.getTime();
  for (int i = 0; i < numberOfMaxWaterings - 1; i++) {
    if (water[i*2] == clock.hour && lastWater < clock.hour)
      if (water[i*2+1] == clock.minute) {
        turnPump(HIGH);
        lastWater = clock.hour;
        return 1; 
      }    
  }
  return 0;
}

void autoMode() {
  //luminosità
  if (ledAuto == 1) {
    if ((brightnessValue = getBrightness()) < autoBrightness) {
      ledStripe(0);
      ledState = HIGH;
    } 
    else if (ledState == HIGH) {
     ledStripe(0);
     ledState = LOW;  
    } 
  }
  
  //temperatura
  if (fanAuto == 1) {
    if ((airTemperature = getTemperature()) > autoTemperature)
      turnFan(HIGH);
    else
      turnFan(LOW); 
  }

  clock.getTime();
  //umidità terreno
  if (pumpAuto == 1) {
    if ((hygrometerValue = getSoilHumidity()) < autoHumidity  && clock.hour > lastWater) //0 per prova, poi sostituire con valore reale
      if (!searchWaterTime()) { //se non è prevista un'irrigazione, irriga lo stesso
        turnPump(HIGH);
        lastWater = clock.hour;
      }
  } 
}

void pageSelect(int openPage) { 
  int timing[4] = {12, 00}; //formato: 00:00, perciò 2 numeri necessari
  int numberTimes = 0; //numeri di orari selezionati
  int i;
  
  switch (openPage) {
    case 0: //MODIFICA ORARIO
      lcd.clear();
      lcd.print("Num. Orari:"); //28 caratteri occupati su 32
      delay(delayMenu/2);

      //annullamento stato bottoni
      leftState = 0;
      rightState = 0;
      selectState = 0;
      
      //selezione numero di orari
      while (selectState == LOW) {
        lcd.setCursor(0,1);
        lcd.print(numberTimes);
        leftState = digitalRead(leftButton);
        rightState = digitalRead(rightButton);
        selectState = digitalRead(selectButton);
        if (leftState == HIGH && numberTimes > 0)
          --numberTimes;
        else if (rightState == HIGH && numberTimes < numberOfMaxWaterings)
          ++numberTimes;
        else if (leftState == HIGH && rightState == HIGH)
          return;
        delay(100);
      }

      //inserimento orari
      for (i = 0; i < numberTimes; i++) {
        lcd.clear();
        lcd.print("Orario "); lcd.print(i+1); lcd.print(":");

        //richiesta ora
        leftState = 0;
        rightState = 0;
        selectState = 0;
        while (selectState == LOW) {
          //stampa ora
          lcd.setCursor(0,1);
          if (timing[0] > 9) {
            lcd.print(timing[0]);
            lcd.print(":"); 
          }
          else {
            lcd.print(0);
            lcd.print(timing[0]);
            lcd.print(":");
          }          
          //stampa minuti
          if (timing[1] > 9)
            lcd.print(timing[1]);
          else {
            lcd.print(0);
            lcd.print(timing[1]);
          }
          
          //modifica valori
          leftState = digitalRead(leftButton);
          rightState = digitalRead(rightButton);
          selectState = digitalRead(selectButton);
          if (leftState == HIGH && timing[0] > 0)
            --timing[0];
          else if (rightState == HIGH && timing[0] < 24)
            ++timing[0];
          delay(100);
        }

        delay(500); //tempo di pressione del tasto select necessario a dare l'invio
        
        //richiesta minuti
        leftState = 0;
        rightState = 0;
        selectState = 0;
        //modifica valori
        while (selectState == LOW) {
          //stampa ora
          lcd.setCursor(0,1);
          if (timing[0] > 9) {
            lcd.print(timing[0]);
            lcd.print(":"); 
          }
          else {
            lcd.print(0);
            lcd.print(timing[0]);
            lcd.print(":");
          }
        
          //stampa minuti
          if (timing[1] > 9)
            lcd.print(timing[1]);
          else {
            lcd.print(0);
            lcd.print(timing[1]);
          }

          leftState = digitalRead(leftButton);
          rightState = digitalRead(rightButton);
          selectState = digitalRead(selectButton);
          if (leftState == HIGH && timing[1] > 0)
            --timing[1];
          else if (rightState == HIGH && timing[1] < 59)
            ++timing[1];
          delay(100);
        }

        //resetta visuale dopo orario ricevuto
        lcd.clear();
        lcd.print("Orario Ricevuto!");
        lcd.setCursor(0,1);
        if (i < numberTimes - 1)
          lcd.print("Inserire succes."); 
        delay(delayMenu);
        
        //salvataggio orari
        water[i*2] = timing[0]; //ora
        water[i*2+1] = timing[1]; //minuti
     }

     //le celle dell'array da non considerare contengono il valore 99
     for (i; i < numberOfMaxWaterings; i++) {
        water[i*2] = 99;
        water[i*2+1] = 99;
     }
     
      //conclusione processo
      lcd.clear();
      lcd.print("Tutti gli orari");
      lcd.setCursor(0,1);
      lcd.print("ricevuti!");
      delay(delayMenu);
      lcd.clear();
      lcd.print("Orari inseriti:");
      for (i = 0; i < numberOfMaxWaterings && water[i*2] <= 24; i++) {
        lcd.setCursor(0,1);
        lcd.print(i+1);
        lcd.print(") ");
        if (water[i*2] > 9)
          lcd.print(water[i*2]);
        else {
          lcd.print(0);
          lcd.print(water[i*2]);
        }
        lcd.print(":");
        if (water[i*2+1] > 9)
          lcd.print(water[i*2+1]);
        else {
          lcd.print(0);
          lcd.print(water[i*2+1]);
        }
        delay(3000);
      }
      break;

    case 1: //UMIDITA' DELL'ARIA
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Umidità aria:");
      lcd.setCursor(0,1);
      lcd.print(getAirHumidity());
      lcd.print("%");
      break;

    case 2: //UMIDITA' DEL TERRENO
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Umidità terreno:");
      lcd.setCursor(0,1);
      lcd.print(getSoilHumidity());
      lcd.print("%");
      break;

    case 3: //TEMPERATURA
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Temperatura:");
      lcd.setCursor(0,1);
      lcd.print(getTemperature());
      lcd.print(" C");
      break;

    case 4: //VENTOLA MANUALE
      lcd.clear();
      lcd.setCursor(0,0);
      if (fanState == LOW)
        lcd.print("Ventola accesa");
      else
        lcd.print("Ventola spenta");
      turnFan(!fanState);
      fanAuto == 0;
      break;

    case 5: //POMPA MANUALE
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Irrigazione in");
      lcd.setCursor(0,1);
      lcd.print("corso...");
      turnPump(HIGH);
      pumpAuto == 0;

    case 6: //LED
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Premi Select per");
      lcd.setCursor(0,1);
      lcd.print("acc/spe luci");
      delay(50); mySender.sendNEC(0xFF02FD, 32);
      ledAuto = 0;
      break;

    case 7: //LUMINOSITA'
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Luminosita':");
      lcd.setCursor(0,1);
      lcd.print(getBrightness());
      lcd.print("%");
      break;

    case 8: //ORA
      clock.getTime();
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Ora:");
      lcd.setCursor(0,1);
      lcd.print(clock.hour, DEC);
      lcd.print(":");
      lcd.print(clock.minute, DEC);
      lcd.print(":");
      lcd.print(clock.second, DEC);  
      break;

    case 9: //AUTO-MODE ON-OFF
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Automode On-Off:");
      lcd.setCursor(0,1);
      fanAuto = !fanAuto;
      ledAuto = !ledAuto;
      pumpAuto = !pumpAuto;
      if (fanAuto == 1)
        lcd.print("On");
      else
        lcd.print("Off");
      break;

    case 10: //LUMINOSITA' MIN
      while (selectState == LOW) {
          value = 0;
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Valore min%:");
          lcd.setCursor(0,1);
          lcd.print(value);
          leftState = digitalRead(leftButton);
          rightState = digitalRead(rightButton);
          selectState = digitalRead(selectButton);
          if (leftState == HIGH && timing[1] > 0)
            --value;
          else if (rightState == HIGH && timing[1] < 100)
            ++value;
          delay(100);
        }
      autoBrightness = value;

    case 11: //TEMPERATURA MAX
      while (selectState == LOW) {
          value = 0;
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Valore min%:");
          lcd.setCursor(0,1);
          lcd.print(value);
          leftState = digitalRead(leftButton);
          rightState = digitalRead(rightButton);
          selectState = digitalRead(selectButton);
          if (leftState == HIGH && timing[1] > 0)
            --value;
          else if (rightState == HIGH && timing[1] < 100)
            ++value;
          delay(100);
        }
      autoTemperature = value;  

    case 12: //UMIDITA' MAX
      while (selectState == LOW) {
          int value = 0;
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Valore min%:");
          lcd.setCursor(0,1);
          lcd.print(value);
          leftState = digitalRead(leftButton);
          rightState = digitalRead(rightButton);
          selectState = digitalRead(selectButton);
          if (leftState == HIGH && timing[1] > 0)
            --value;
          else if (rightState == HIGH && timing[1] < 100)
            ++value;
          delay(100);
        }
      autoHumidity = value;

    case 13: //DURATA IRRIGAZIONE
      while (selectState == LOW) {
          int value = 0;
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Valore min%:");
          lcd.setCursor(0,1);
          lcd.print(value);
          leftState = digitalRead(leftButton);
          rightState = digitalRead(rightButton);
          selectState = digitalRead(selectButton);
          if (leftState == HIGH && timing[1] > 0)
            --value;
          else if (rightState == HIGH && timing[1] < 5000)
            ++value;
          delay(100);
        }
      pumpDuration = value;
    
    default:
      break;
  }
}

void menu() {
  //movimento tra le pagine del menu
  if (leftState == HIGH && pageState > 0) {
    --pageState;
    lcd.clear(); //resetta display
    lcd.print(pages[pageState]); //visualizza titoli pagina sul display
  } 
  else if (rightState == HIGH && pageState < (numberOfPages - 1)) {
    ++pageState;
    lcd.clear();
    lcd.print(pages[pageState]); 
  }

  //selezione pagina  
  if (selectState == HIGH)
    pageSelect(pageState);
}

void loop() {
  //lettura stato bottoni
  leftState = digitalRead(leftButton);
  rightState = digitalRead(rightButton);
  selectState = digitalRead(selectButton);    
  delay(75);
  digitalWrite(pumpPin, LOW);
  //visualizzazione pagina del menu
  menu();
  //controllo sensori ed adattamento
  autoMode();
  //comunicazione bluetooth
  Bluetooth(); 
  //codice da utilizzare per ricavare i segnali mandati dal proprio telecomando
  /*if(receiver.decode(&results)) {             
    Serial.println(results.value, HEX);
    receiver.resume();                        
  }*/  
}

int Bluetooth() {
  if (Serial3.available())
    message = char(Serial3.read());
  else {
    if (message != '\0') {
      Serial.println(message);
      switch(message) {
        //ventola
        case 'F': turnFan(HIGH); break;
        case 'f': turnFan(LOW); break;
        //pompa
        case 'P': turnPump(HIGH); break;
        case 'p': turnPump(LOW); break;
        //auto
        case 'A': fanAuto = 1; pumpAuto = 1; ledAuto = 1; break;
        case 'a': fanAuto = 0; pumpAuto = 0; ledAuto = 0; break;
        //accendi led
        case '0': delay(50); mySender.sendNEC(0xFF02FD, 32); break;
       
        case '1': delay(50); mySender.sendNEC(0xFF1AE5, 32); break;

        case '2': delay(50); mySender.sendNEC(0xFF9A65, 32); break;

        case '3': delay(50); mySender.sendNEC(0xFFA25D, 32); break;

        case '4': delay(50); mySender.sendNEC(0xFF3AC5, 32); break;

        case '5': delay(50); mySender.sendNEC(0xFFBA45, 32); break;

        case '6': delay(50); mySender.sendNEC(0xFF609F, 32); break;

        case '7': delay(50); mySender.sendNEC(0xFFC837, 32); break;

        case '8': delay(50); mySender.sendNEC(0xFFE817, 32); break;

        case '9': delay(50); mySender.sendNEC(0xFF8A75, 32); break;
      }
      message='\0';
    }
    if (brightnessValue != brightnessValueOLD)
      Serial3.print("*L"); Serial3.print(brightnessValue); Serial3.println("*");
    delay(100);
    
    if (airTemperature != airTemperatureOLD)
      Serial3.print("*T"); Serial3.print(airTemperature); Serial3.println("*"); 
    delay(100);
    
    if (hygrometerValue != hygrometerValueOLD)
      Serial3.print("*H"); Serial3.print(hygrometerValue); Serial3.println("*"); 
    delay(100);
    
    if (airHumidity != airHumidityOLD) {
      Serial3.print("*A"); Serial3.print(airHumidity); Serial3.println("*");
    }
  }
  return 0;
}
