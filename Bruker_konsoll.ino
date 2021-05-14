// Importerte bilbioteker
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Pinne deklarering til ST7789 skjerm
#define TFT_MOSI 23 // SDA Pin på ESP32
#define TFT_SCLK 18 // SCL Pin på ESP32
#define TFT_CS 15   // Chip select kontrol pin
#define TFT_DC 2    // Data Command kontrol pin
#define TFT_RST 4   // Reset pin

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

#define uS_TO_S_FACTOR 1000000 /* C onversion factor for micro seconds to seconds */
#define mS_TO_S_FACTOR 1000
#define TIME_TO_SLEEP 60       /* Time ESP32 will go to sleep (in seconds) */
#define SCREEN_WIDTH 128       // OLED display width, in pixels
#define SCREEN_HEIGHT 64       // OLED display height, in pixels
#define TIME_TO_SLEEP_CHECK 60 // Hvor ofte den skal sjekke når den må gå i søvnmodus

// debug = 0 er vanlig opperasjon
// debug = 1 vil den ikke lenger koble seg til MQTT eller internett
// Brukbar for å sparetid under feilsøking
const bool debug = 1;

// Oppsett av internett
const char *ssid = "tormodskjhw";
const char *password = "tormodskjhw";

// Oppsett av MQTT
const char *mqttServer = "192.168.137.85";
const int mqttPort = 1883;
const char *mqttUser = "";
const char *mqttPassword = "";

WiFiClient espClient;
PubSubClient client(espClient);

// Pinout til knapper
const int buttonUp = 32;
const int buttonDown = 33;
const int buttonSelect = 35;
const int buttonBack = 34;

// Tilstander til tilstandsmaskinen
enum States
{
  MAIN,
  BOOKING_NAME,
  BOOKING_TIME,
  BOOKING_LENGTH,
  BOOKING_GUEST,
  BOOKING_ROOM,
  LIGHT,
  TEMPERATURE,
  BOOKING_FEEDBACK,
  ROOM_LIGHT,
  ROOM_TEMPERATURE,
  ROOM_DOOR,
  PASSWORD,
  DOOR_GUEST
};
enum States state;

// Inholdet i disse arrayen er det som vises grafisk på brukergrensersnittet
// Funksjonene disse brukes det vil automastisk kompansere for lengde, så kan kan vilkårlig endre disse
String names[] = {"Lars", "Haavard", "Karl", "Tormod"};
String rooms[] = {"Stue", "Kjokken", "Bad", "Klesvask"};
String rooms2[] = {"Stue", "Kjokken", "Bad", "Soverom"};
String mainMenu[] = {"Booking", "Dor", "Temperatur", "Lys"};

int transmittData[7];                                               // Data som skal sendes via MQTT
int returnVal = 0;                                                  // En verdi som skal brukes til å returne fra diverse funksjoner
int const nameCount = (sizeof(names) / sizeof(names[0]));           // Variabel som lagrer antall beboere i leiligheten
int const mainMenuCount = (sizeof(mainMenu) / sizeof(mainMenu[0])); // Variabel som lagrer hvor mange alternativer det er på hovedmenyen
int const roomCount = (sizeof(rooms) / sizeof(rooms[0]));           // Variabel som lagrer hvor mange alternativer det er på hovedmenyen
int guestMax = 2;                                                   // Max antall tillate gjester per persjon

int passwordDoor[] = {1, 1, 1, 1};                                           // Passordet for å åpne døra,
int const passwordLenght = (sizeof(passwordDoor) / sizeof(passwordDoor[0])); // Variabel som lagrer lengden av passordet
int passwordCheck[passwordLenght];                                           // Array som skal brukes til å sammenligne tastet trykk med passordet for å sjekke om det er riktig
String passwordKeys[] = {"1", "2", "3", "4"};
String message;
char *messageChar;
volatile long buttonTimer = 0;                   // Variabel som lagrer tiden siden sist knappetrykk
int sleepTimer = TIME_TO_SLEEP * mS_TO_S_FACTOR; // Tid i millisekunder til ESPen går i deep sleep modus

hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Timerbassert interrupt
// Etter en sjekker etter en hvis tid hvor lenge det har gått siden sist knappetrykk
// Hvis tiden siden sist knappetrykk er lenger en tiden definert av variabelen sleepTimer går ESPen i søvnmodus
void IRAM_ATTR onTimer()
{
  portENTER_CRITICAL_ISR(&timerMux);
  if (millis() - buttonTimer > sleepTimer)
  {
    esp_deep_sleep_start();
  }
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Intterupt som går av hver gang en knapp trykkes
// Brukes til å holde telling på hvor lang tid det har gått siden sist knappetrykk
void ISRbuttonTimer()
{
  buttonTimer = millis();
}

// Funksjon som gjør om en string til en char array
char *toCharArray(String str)
{
  return &str[0];
}

// Funksjon som sender en string over mqtt til et gitt tema/topic
void publish(String doc, String topic)
{
  client.publish(toCharArray(topic), toCharArray(doc));
}

// Callbackfunksjon kjøres når vi mottar en melding i et tema vi subber til
void callback(char *topic, byte *payload, unsigned int length)
{
  returnVal = -1;
  String payloadString = "";                  //Definerer en tom midlertidig string
  Serial.print("Message arrived in topic: "); //Serialprinter tema vi har mottatt beskjed fra
  Serial.println(topic);
  Serial.print("                 Message: "); //Serialprinter beskjeden (payloaden)
  for (int i = 0; i < length; i++)
  { //Omgjør payloaden til en string og printer den
    payloadString += (char)payload[i];
  }
  Serial.println(payloadString);
  Serial.println("");
  if (payloadString == "True")
  {
    returnVal = 1;
  }
  else if (payloadString == "False")
  {
    returnVal = 0;
  }
  else
  {
    returnVal = -1;
  }
}

// Funksjon for menyen til valg av person som skal booke
int mainBooking()
{
  //Meny oppsett
  returnVal = 0;
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(15, 30);
  tft.print("Booking"); // Overskrift
  tft.fillRect(0, 33, 240, 3, ST77XX_WHITE);
  for (int i = 0; i < nameCount; i++)
  {
    tft.drawRoundRect(10, 40 + i * 45, 220, 35, 3, ST77XX_WHITE);
    tft.setCursor(15, 65 + i * 45);
    tft.print(names[i]);
  }
  tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
  tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
  tft.setCursor(15, 65 + returnVal * 45);
  tft.print(names[returnVal]);

  while (digitalRead(buttonSelect) == false && digitalRead(buttonBack) == false)
  {
    if (digitalRead(buttonUp) == true)
    {
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_BLACK);
      tft.drawRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_WHITE);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(names[returnVal]);
      returnVal--;
      if (returnVal < 0)
      {
        returnVal = 0;
      }
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
      tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(names[returnVal]);
      while (digitalRead(buttonUp) == HIGH)
      {
      }
    }

    if (digitalRead(buttonDown) == true)
    {
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_BLACK);
      tft.drawRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_WHITE);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(names[returnVal]);
      returnVal++;
      if (returnVal > nameCount - 1)
      {
        returnVal--;
      }
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
      tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(names[returnVal]);
      while (digitalRead(buttonDown) == HIGH)
      {
      }
    }
  }
  if (digitalRead(buttonSelect) == HIGH)
  {
    while (digitalRead(buttonSelect) == HIGH)
    {
    }
    return (returnVal);
  }
  if (digitalRead(buttonBack) == HIGH)
  {
    while (digitalRead(buttonBack) == HIGH)
    {
    }
    return (-1);
  }
}

// Funksjon for meny til valg av rom
int room()
{
  //Meny oppsett
  returnVal = 0;
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(15, 30);
  tft.print("Rom"); // Overskrift
  tft.fillRect(0, 33, 240, 3, ST77XX_WHITE);
  for (int i = 0; i < roomCount; i++)
  {
    tft.drawRoundRect(10, 40 + i * 45, 220, 35, 3, ST77XX_WHITE);
    tft.setCursor(15, 65 + i * 45);
    tft.print(rooms[i]);
  }
  tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
  tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
  tft.setCursor(15, 65 + returnVal * 45);
  tft.print(rooms[returnVal]);

  while (digitalRead(buttonSelect) == false && digitalRead(buttonBack) == false)
  {
    if (digitalRead(buttonUp) == true)
    {
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_BLACK);
      tft.drawRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_WHITE);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(rooms[returnVal]);
      returnVal--;
      if (returnVal < 0)
      {
        returnVal = 0;
      }
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
      tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(rooms[returnVal]);
      while (digitalRead(buttonUp) == HIGH)
      {
      }
    }

    if (digitalRead(buttonDown) == true)
    {
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_BLACK);
      tft.drawRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_WHITE);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(rooms[returnVal]);
      returnVal++;
      if (returnVal > roomCount - 1)
      {
        returnVal--;
      }
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
      tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(rooms[returnVal]);
      while (digitalRead(buttonDown) == HIGH)
      {
      }
    }
  }
  if (digitalRead(buttonSelect) == HIGH)
  {
    while (digitalRead(buttonSelect) == HIGH)
    {
    }
    return (returnVal);
  }
  if (digitalRead(buttonBack) == HIGH)
  {
    while (digitalRead(buttonBack) == HIGH)
    {
    }
    return (-1);
  }
}

// Funksjon for meny til valg av rom
int room2()
{
  //Meny oppsett
  returnVal = 0;
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(15, 30);
  tft.print("Rom"); // Overskrift
  tft.fillRect(0, 33, 240, 3, ST77XX_WHITE);
  for (int i = 0; i < roomCount; i++)
  {
    tft.drawRoundRect(10, 40 + i * 45, 220, 35, 3, ST77XX_WHITE);
    tft.setCursor(15, 65 + i * 45);
    tft.print(rooms2[i]);
  }
  tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
  tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
  tft.setCursor(15, 65 + returnVal * 45);
  tft.print(rooms2[returnVal]);

  while (digitalRead(buttonSelect) == false && digitalRead(buttonBack) == false)
  {
    if (digitalRead(buttonUp) == true)
    {
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_BLACK);
      tft.drawRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_WHITE);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(rooms2[returnVal]);
      returnVal--;
      if (returnVal < 0)
      {
        returnVal = 0;
      }
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
      tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(rooms2[returnVal]);
      while (digitalRead(buttonUp) == HIGH)
      {
      }
    }

    if (digitalRead(buttonDown) == true)
    {
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_BLACK);
      tft.drawRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_WHITE);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(rooms2[returnVal]);
      returnVal++;
      if (returnVal > nameCount - 1)
      {
        returnVal--;
      }
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
      tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(rooms2[returnVal]);
      while (digitalRead(buttonDown) == HIGH)
      {
      }
    }
  }
  if (digitalRead(buttonSelect) == HIGH)
  {
    while (digitalRead(buttonSelect) == HIGH)
    {
    }
    return (returnVal);
  }
  if (digitalRead(buttonBack) == HIGH)
  {
    while (digitalRead(buttonBack) == HIGH)
    {
    }
    return (-1);
  }
}

// Funksjon for meny for valg av hvor lenge man skal booke et rom
int len()
{
  returnVal = 1;
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(15, 30);
  tft.print("Booking lengde"); // Overskrift
  tft.fillRect(0, 33, 240, 3, ST77XX_WHITE);
  tft.setCursor(15, 130);
  tft.setTextSize(2);

  tft.fillRect(0, 40, 240, 50, ST77XX_BLACK);
  tft.print(returnVal / 12);
  tft.print("H  ");
  tft.print((returnVal * 5) - (12 * (returnVal / 12)));
  tft.print("M");
  while (digitalRead(buttonSelect) == LOW && digitalRead(buttonBack) == LOW)
  {
    if (digitalRead(buttonUp) == HIGH)
    {
      returnVal++;
      tft.fillRect(0, 40, 240, 100, ST77XX_BLACK);
      tft.setCursor(15, 130);
      tft.print(returnVal / 12);
      tft.print("H  ");
      tft.print((returnVal * 5) - (60 * (returnVal / 12)));
      tft.print("M");
      while (digitalRead(buttonUp) == HIGH)
      {
      }
    }
    else if (digitalRead(buttonDown) == HIGH)
    {
      returnVal--;
      if (returnVal < 1)
      {
        returnVal = 1;
      }
      tft.fillRect(0, 40, 240, 100, ST77XX_BLACK);
      tft.setCursor(15, 130);
      tft.print(returnVal / 12);
      tft.print("H  ");
      tft.print((returnVal * 5) - (60 * (returnVal / 12)));
      tft.print("M");
      while (digitalRead(buttonDown) == HIGH)
      {
      }
    }
  }
  tft.setTextSize(1);
  if (digitalRead(buttonSelect) == HIGH)
  {
    while (digitalRead(buttonSelect) == HIGH)
    {
    }
    return (returnVal);
  }
  if (digitalRead(buttonBack) == HIGH)
  {
    while (digitalRead(buttonBack) == HIGH)
    {
    }
    return (-1);
  }
}

// Funksjon for meny for valg av antall gjester
int guestBooking()
{
  returnVal = 0;
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(15, 30);
  tft.print("Antall gjester"); // Overskrift
  tft.fillRect(0, 33, 240, 3, ST77XX_WHITE);
  tft.setCursor(15, 130);
  tft.setTextSize(2);
  tft.fillRect(0, 40, 240, 50, ST77XX_BLACK);
  tft.print(returnVal);
  while (digitalRead(buttonSelect) == LOW && digitalRead(buttonBack) == LOW)
  {
    if (digitalRead(buttonUp) == HIGH)
    {
      returnVal++;
      if (returnVal > guestMax)
      {
        returnVal = guestMax;
      }
      tft.fillRect(0, 40, 240, 100, ST77XX_BLACK);
      tft.setCursor(15, 130);
      tft.print(returnVal);
      while (digitalRead(buttonUp) == HIGH)
      {
      }
    }
    else if (digitalRead(buttonDown) == HIGH)
    {
      returnVal--;
      if (returnVal < 0)
      {
        returnVal = 0;
      }
      tft.fillRect(0, 40, 240, 100, ST77XX_BLACK);
      tft.setCursor(15, 130);
      tft.print(returnVal);
      while (digitalRead(buttonDown) == HIGH)
      {
      }
    }
  }
  tft.setTextSize(1);
  if (digitalRead(buttonSelect) == HIGH)
  {
    while (digitalRead(buttonSelect) == HIGH)
    {
    }
    return (returnVal);
  }
  if (digitalRead(buttonBack) == HIGH)
  {
    while (digitalRead(buttonBack) == HIGH)
    {
    }
    return (-1);
  }
}

// Funksjon for når man skal booke et rom
int when()
{
  returnVal = 1;
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(15, 30);
  tft.print("Booking tid"); // Overskrift
  tft.fillRect(0, 33, 240, 3, ST77XX_WHITE);
  tft.setCursor(15, 130);
  tft.setTextSize(2);

  tft.fillRect(0, 40, 240, 50, ST77XX_BLACK);
  tft.print(returnVal / 12);
  tft.print("H  ");
  tft.print((returnVal * 5) - (12 * (returnVal / 12)));
  tft.print("M");
  while (digitalRead(buttonSelect) == LOW && digitalRead(buttonBack) == LOW)
  {
    if (digitalRead(buttonUp) == HIGH)
    {
      returnVal++;
      tft.fillRect(0, 40, 240, 100, ST77XX_BLACK);
      tft.setCursor(15, 130);
      tft.print(returnVal / 12);
      tft.print("H  ");
      tft.print((returnVal * 5) - (60 * (returnVal / 12)));
      tft.print("M");
      while (digitalRead(buttonUp) == HIGH)
      {
      }
    }
    else if (digitalRead(buttonDown) == HIGH)
    {
      returnVal--;
      if (returnVal < 0)
      {
        returnVal = 0;
      }
      tft.fillRect(0, 40, 240, 100, ST77XX_BLACK);
      tft.setCursor(15, 130);
      tft.print(returnVal / 12);
      tft.print("H  ");
      tft.print((returnVal * 5) - (60 * (returnVal / 12)));
      tft.print("M");
      while (digitalRead(buttonDown) == HIGH)
      {
      }
    }
  }
  tft.setTextSize(1);
  if (digitalRead(buttonSelect) == HIGH)
  {
    while (digitalRead(buttonSelect) == HIGH)
    {
    }
    return (returnVal);
  }
  if (digitalRead(buttonBack) == HIGH)
  {
    while (digitalRead(buttonBack) == HIGH)
    {
    }
    return (-1);
  }
}

// Funksjonen for hovedmenyen
int main()
{
  returnVal = 0;
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(15, 30);
  tft.print("Hovedmeny"); // Overskrift
  tft.fillRect(0, 33, 240, 3, ST77XX_WHITE);
  for (int i = 0; i < mainMenuCount; i++)
  {
    tft.drawRoundRect(10, 40 + i * 45, 220, 35, 3, ST77XX_WHITE);
    tft.setCursor(15, 65 + i * 45);
    tft.print(mainMenu[i]);
  }
  tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
  tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
  tft.setCursor(15, 65 + returnVal * 45);
  tft.print(mainMenu[returnVal]);
  while (digitalRead(buttonSelect) == false && digitalRead(buttonBack) == false)
  {
    if (digitalRead(buttonUp) == true)
    {
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_BLACK);
      tft.drawRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_WHITE);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(mainMenu[returnVal]);
      returnVal--;
      if (returnVal < 0)
      {
        returnVal = 0;
      }
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
      tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(mainMenu[returnVal]);
      while (digitalRead(buttonUp) == HIGH)
      {
      }
    }

    if (digitalRead(buttonDown) == true)
    {
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_BLACK);
      tft.drawRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_WHITE);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(mainMenu[returnVal]);
      returnVal++;
      if (returnVal > mainMenuCount - 1)
      {
        returnVal--;
      }
      tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
      tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
      tft.setCursor(15, 65 + returnVal * 45);
      tft.print(mainMenu[returnVal]);
      while (digitalRead(buttonDown) == HIGH)
      {
      }
    }
  }
  if (digitalRead(buttonSelect) == HIGH)
  {
    while (digitalRead(buttonSelect) == HIGH)
    {
    }
    return (returnVal);
  }
  if (digitalRead(buttonBack) == HIGH)
  {
    while (digitalRead(buttonBack) == HIGH)
    {
    }
    return (-1);
  }
}

// Funksjonen for passordmenyen
bool passwordMenu()
{
  //Meny oppsett
  returnVal = 0;
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(15, 30);
  tft.print("Passord"); // Overskrift
  tft.fillRect(0, 33, 240, 3, ST77XX_WHITE);
  tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
  tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
  tft.setCursor(15, 65 + returnVal * 45);
  tft.print(passwordKeys[returnVal]);
  for (int i = 0; i < passwordLenght; i++)
  {
    tft.drawRoundRect(10, 40 + i * 45, 220, 35, 3, ST77XX_WHITE);
    tft.setCursor(15, 65 + i * 45);
    tft.print(passwordKeys[i]);
  }

  for (int i = 0; i < passwordLenght; i++)
  {
    // Triangle_1(5, 15 * returnVal);
    while (digitalRead(buttonSelect) == false && digitalRead(buttonBack) == false)
    {
      if (digitalRead(buttonUp) == true)
      {
        tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_BLACK);
        tft.drawRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_WHITE);
        tft.setCursor(15, 65 + returnVal * 45);
        tft.print(passwordKeys[returnVal]);
        returnVal--;
        if (returnVal < 0)
        {
          returnVal = 0;
        }
        tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
        tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
        tft.setCursor(15, 65 + returnVal * 45);
        tft.print(passwordKeys[returnVal]);
        while (digitalRead(buttonUp) == HIGH)
        {
        }
      }

      if (digitalRead(buttonDown) == true)
      {
        tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_BLACK);
        tft.drawRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_WHITE);
        tft.setCursor(15, 65 + returnVal * 45);
        tft.print(passwordKeys[returnVal]);
        returnVal++;
        if (returnVal > passwordLenght - 1)
        {
          returnVal--;
        }
        tft.fillRoundRect(10, 40 + returnVal * 45, 220, 35, 3, ST77XX_GREEN);
        tft.fillRoundRect(14, 44 + returnVal * 45, 212, 27, 3, ST77XX_BLACK);
        tft.setCursor(15, 65 + returnVal * 45);
        tft.print(passwordKeys[returnVal]);
        while (digitalRead(buttonDown) == HIGH)
        {
        }
      }
    }
    passwordCheck[i] = returnVal + 1;
    while (digitalRead(buttonSelect) == HIGH)
    {
    }
    if (digitalRead(buttonBack) == HIGH)
    {
      while (digitalRead(buttonBack) == HIGH)
      {
      }
      return (-1);
    }
    delay(200);
  }

  //passordSjekk
  for (int i = 0; i < passwordLenght; i++)
  {
    if (passwordDoor[i] != passwordCheck[i])
    {
      return (0);
      break;
    }
    else if (i == passwordLenght - 1)
    {
      return (1);
      break;
    }
    else
    {
    }
  }
}

// Funksjonen for meny for valg av lysstyrke
int light()
{
  returnVal = 1;
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(15, 30);
  tft.print("Lysstyrke"); // Overskrift
  tft.fillRect(0, 33, 240, 3, ST77XX_WHITE);
  tft.setCursor(15, 130);
  tft.setTextSize(2);
  tft.fillRect(0, 40, 240, 50, ST77XX_BLACK);
  tft.print(returnVal);
  tft.print("%");
  while (digitalRead(buttonSelect) == LOW && digitalRead(buttonBack) == LOW)
  {
    if (digitalRead(buttonUp) == HIGH)
    {
      returnVal++;
      if (returnVal > 100)
      {
        returnVal = 100;
      }
      tft.fillRect(0, 40, 240, 100, ST77XX_BLACK);
      tft.setCursor(15, 130);
      tft.print(returnVal);
      tft.print("%");
      while (digitalRead(buttonUp) == HIGH)
      {
      }
    }
    else if (digitalRead(buttonDown) == HIGH)
    {
      returnVal--;
      if (returnVal < 1)
      {
        returnVal = 1;
      }
      tft.fillRect(0, 40, 240, 100, ST77XX_BLACK);
      tft.setCursor(15, 130);
      tft.print(returnVal);
      tft.print("%");
      while (digitalRead(buttonDown) == HIGH)
      {
      }
    }
  }
  tft.setTextSize(1);
  if (digitalRead(buttonSelect) == HIGH)
  {
    while (digitalRead(buttonSelect) == HIGH)
    {
    }
    return (returnVal);
  }
  if (digitalRead(buttonBack) == HIGH)
  {
    while (digitalRead(buttonBack) == HIGH)
    {
    }
    return (-1);
  }
}

// Funksjonen for meny for valg av temperatur
int temperature()
{
  returnVal = 1;
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(15, 30);
  tft.print("Temperature"); // Overskrift
  tft.fillRect(0, 33, 240, 3, ST77XX_WHITE);
  tft.setCursor(15, 130);
  tft.setTextSize(2);
  tft.fillRect(0, 40, 240, 50, ST77XX_BLACK);
  tft.print(returnVal);
  tft.print(" C");
  while (digitalRead(buttonSelect) == LOW && digitalRead(buttonBack) == LOW)
  {
    if (digitalRead(buttonUp) == HIGH)
    {
      returnVal++;
      if (returnVal > 100)
      {
        returnVal = 100;
      }
      tft.fillRect(0, 40, 240, 100, ST77XX_BLACK);
      tft.setCursor(15, 130);
      tft.print(returnVal);
      tft.print(" C");
      while (digitalRead(buttonUp) == HIGH)
      {
      }
    }
    else if (digitalRead(buttonDown) == HIGH)
    {
      returnVal--;
      if (returnVal < 1)
      {
        returnVal = 1;
      }
      tft.fillRect(0, 40, 240, 100, ST77XX_BLACK);
      tft.setCursor(15, 130);
      tft.print(returnVal);
      tft.print(" C");
      while (digitalRead(buttonDown) == HIGH)
      {
      }
    }
  }
  tft.setTextSize(1);
  if (digitalRead(buttonSelect) == HIGH)
  {
    while (digitalRead(buttonSelect) == HIGH)
    {
    }
    return (returnVal);
  }
  if (digitalRead(buttonBack) == HIGH)
  {
    while (digitalRead(buttonBack) == HIGH)
    {
    }
    return (-1);
  }
}

// Meny for sending over MQTT
void transmitt()
{
  publish("-1", "booking/esp/pub");
  Serial.println("-1");
  delay(1);
  for (byte i = 0; i < (sizeof(transmittData) / sizeof(transmittData[0])); i++)
  {
    String transmittString = String(transmittData[i]);
    Serial.println(transmittString);
    publish(transmittString, "booking/esp/pub");
    delay(1);
  }
}

// Oppsett
void setup()
{
  Serial.begin(115200);
  if (debug != 1) // Når debug = 1 vil alt av internett og MQTT oppsett hoppes over
  {
    // Kobling til WIFI
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.println("wifi status: ");
      Serial.println(WiFi.status());
    }
    Serial.println("Connected to the WiFi network");

    // Kobling til MQTT
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);
    while (!client.connected())
    {
      Serial.println("Connecting to MQTT...");
      if (client.connect("ESP32Client", mqttUser, mqttPassword))
      {
        Serial.println("connected");
      }
      else
      {
        Serial.print("failed with state ");
        Serial.print(client.state());
        delay(2000);
      }
    }
    client.subscribe("booking/esp/sub");
  }

  // Oppsett av pinner
  pinMode(buttonUp, INPUT);
  pinMode(buttonDown, INPUT);
  pinMode(buttonSelect, INPUT);
  pinMode(buttonBack, INPUT);

  // Oppsett av skjerm
  tft.init(240, 240, SPI_MODE2); // Initialisering av ST7789 skjerm 240x240 pixler
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextSize(1);

  // Oppsett for timer basert interrupt
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // Setter opp interrupts som brukes til å måle tiden siden sist knappetrykk
  // Nødvendig for strømsparingsmodus
  attachInterrupt(buttonSelect, ISRbuttonTimer, RISING);
  attachInterrupt(buttonBack, ISRbuttonTimer, RISING);
  attachInterrupt(buttonDown, ISRbuttonTimer, RISING);
  attachInterrupt(buttonUp, ISRbuttonTimer, RISING);

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_35, 1); // Setter selectknappen til å kunne vekke ESPen fra søvnmodus

  // For å holde styr over hvor mange som er i leiligheten kan man sjekke hvor mange konsoller som er tilkoblet MQTT serveren
  // Men siden den ikke kan være tilkoblet når den er i søvnmodus må den vekkes etter en hvis tid definert av TIME_TO_SLEEP
  // Hvis konsollen blir vekt av knappen vil den opperere som vanlig
  // Hvis den blir vekt av timer vil den pinge nettverket og gå tilbake til søvnmodus

  esp_sleep_wakeup_cause_t wakeup_reason; // Finner ut hva som vekte ESPen fra søvnmodus
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0: // Hvis den ble vekket av knapp oppererer den som vanlig
      Serial.println("Wakeup:Button");
      break;
    case ESP_SLEEP_WAKEUP_TIMER: // Hvis den ble vekket av timer går den tilbake til søvnmodus
      Serial.println("Wakeup:Timer");
      esp_deep_sleep_start();
      break;
  }
  state = MAIN;
}

// Hovedløkke og tilstandsmaskin
void loop()
{
  switch (state)
  {
    case MAIN:
      memset(transmittData, 0, sizeof(transmittData));
      switch (main())
      {
        case (0):
          state = BOOKING_NAME;
          transmittData[0] = 0;
          break;

        case (1):
          state = DOOR_GUEST;
          transmittData[0] = 1;
          break;

        case (2):
          state = ROOM_TEMPERATURE;
          transmittData[0] = 2;
          break;

        case (3):
          state = ROOM_LIGHT;
          transmittData[0] = 3;
          break;
      }
      break;

    case BOOKING_NAME:
      transmittData[1] = mainBooking();
      if (transmittData[1] >= 0)
      {
        state = BOOKING_ROOM;
      }
      else
      {
        state = MAIN;
      }
      break;

    case BOOKING_ROOM:
      transmittData[2] = room();
      if (transmittData[2] >= 0)
      {
        state = BOOKING_TIME;
      }
      else
      {
        state = BOOKING_NAME;
      }
      break;

    case BOOKING_TIME:
      transmittData[3] = when();
      if (transmittData[3] >= 0)
      {
        state = BOOKING_LENGTH;
      }
      else
      {
        state = BOOKING_NAME;
      }
      break;

    case BOOKING_LENGTH:
      transmittData[4] = len() + transmittData[3];
      if (transmittData[4] >= 0)
      {
        state = BOOKING_GUEST;
      }
      else
      {
        state = BOOKING_TIME;
      }
      break;

    case BOOKING_GUEST:
      transmittData[5] = guestBooking();
      if (transmittData[5] >= 0)
      {
        state = BOOKING_FEEDBACK;
        transmittData[6] = -2;
      }
      else
      {
        state = BOOKING_LENGTH;
      }
      break;

    case BOOKING_FEEDBACK:
      state = MAIN;
      if (debug == false)
      {
        transmitt();
        tft.fillScreen(ST77XX_BLACK);
        tft.setCursor(0, 20);
        tft.print("Waiting....");
        returnVal = -1;
        while (returnVal == -1)
        {
          client.loop();
          delay(100);
        }
        if (returnVal == 0)
        {
          tft.fillScreen(ST77XX_BLACK);
          tft.setCursor(0, 20);
          tft.print("Declined :(");

          delay(2000);
        }
        else if (returnVal == 1)
        {
          tft.fillScreen(ST77XX_BLACK);
          tft.setCursor(0, 20);
          tft.print("Approved!");
          delay(2000);
        }
        else
        {
        }
      }
      break;

    case DOOR_GUEST:
      transmittData[0] = 1;
      transmittData[1] = guestBooking();
      if (transmittData[1] >= 0)
      {
        state = PASSWORD;
      }
      else
      {
        state = MAIN;
      }
      break;

    case PASSWORD:
      transmittData[2] = passwordMenu();
      if (transmittData[2] >= 0)
      {
        state = MAIN;
        transmittData[3] = -2;
        if (transmittData[1] == 1)
        {
          transmitt();
        }
      }
      else
      {
        state = DOOR_GUEST;
      }
      break;

    case ROOM_TEMPERATURE:
      transmittData[0] = 2;
      transmittData[1] = room2();
      if (transmittData[1] >= 0)
      {
        state = TEMPERATURE;
      }
      else
      {
        state = MAIN;
      }
      break;

    case TEMPERATURE:
      transmittData[2] = temperature();
      transmittData[3] = -2;
      if (transmittData[1] >= 0)
      {
        state = MAIN;
      }
      else
      {
        state = ROOM_TEMPERATURE;
      }
      transmitt();
      break;

    case ROOM_LIGHT:
      transmittData[0] = 3;
      transmittData[1] = room2();
      if (transmittData[1] >= 0)
      {
        state = LIGHT;
      }
      else
      {
        state = MAIN;
      }
      break;

    case LIGHT:
      transmittData[2] = light();
      transmittData[3] = -2;
      if (transmittData[2] >= 0)
      {
        transmitt();
        state = MAIN;
      }
      else
      {
        state = ROOM_LIGHT;
      }
      break;
  }
}
