#include <CircusESP32Lib.h> // Include all the particular coding for ESP32/Circus, so that you only will use  "read and "write" functions

char ssid[] = "test"; // Place your wifi SSID here
char password[] =  "testtest"; // Place your wifi password here
char server[] = "www.circusofthings.com";
char order_key[] = "27170"; // Type the Key of the Circus Signal you want the ESP32 listen to.
char token[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1OTY2In0.jpA9BqhHik3DZACJT-aMcrSunlvQeyid9snyh6WxPTA"; // Place your token, find it in 'account' at Circus. It will identify you.
CircusESP32Lib circusESP32(server, ssid, password); // The object representing an ESP32 to whom you can order to Write or Read

const int trigPin_1 = 23;
const int echoPin_1 = 22;

const int trigPin_2 = 21;
const int echoPin_2 = 19;

const int distanceThreshold = 10; // Avstanden til konsollen et objekt må være før den blir triggra
const int LEDPin = 14;
const int maxCount = 5; // Maks antall personer tillat i kollektiver

long duration;
int distance;

int count;
int prevMillis;
int prevMillisUpload;

void setup() {
  pinMode(trigPin_1, OUTPUT);
  pinMode(echoPin_1, INPUT);
  pinMode(trigPin_2, OUTPUT);
  pinMode(echoPin_2, INPUT);
  pinMode(LEDPin, OUTPUT);
  Serial.begin(9600); // Starts the serial communication
  circusESP32.begin(); // Let the Circus object set up itself for an SSL/Secure connection
}

void loop() {
  // Sjekker om sensormodul 1 blir trigget først
  // Hvis denne blir trigget først går noen inn i kollektivet og verdien count økes da med 1
  if ( distanceRead(trigPin_1, echoPin_1, distanceThreshold)) {
    prevMillis = millis();
    while ((millis() - prevMillis) < 1000) {
      if (distanceRead(trigPin_2, echoPin_2, distanceThreshold)) {
        count++;
        digitalWrite(LEDPin, HIGH);
        delay(200);
        digitalWrite(LEDPin, LOW);
        delay(500);
      }
    }
  }
  // Sjekker om sensormodul 2 blir trigget først
  // Hvis denne blir trigget først går noen ut av kollektivet og verdien count reduseres da med 1
  if ( distanceRead(trigPin_2, echoPin_2, distanceThreshold)) {
    prevMillis = millis();
    while ((millis() - prevMillis) < 1000) {
      if (distanceRead(trigPin_1, echoPin_1, distanceThreshold)) {
        count--;
        if (count < 0) {
          count = 0;
        }
        digitalWrite(LEDPin, HIGH);
        delay(200);
        digitalWrite(LEDPin, LOW);
        delay(500);

      }
    }
  }

  // Oppdaterer CoT hvert 30sekund med hvor mange som er i kollektivet
  if (( millis() - prevMillisUpload) > 30000) {
    circusESP32.write(order_key, count, token);
    prevMillisUpload = millis();
  }
  // Skur på en LED hvis det er for mange folk i kollektivet
  if (count > maxCount) {
    digitalWrite(LEDPin, HIGH);
  }
  else {
    digitalWrite(LEDPin, LOW);
  }
  Serial.println(count);
}

// Funksjon som sjekker om noen har gått forbi sensoren
bool distanceRead(int trigPin, int echoPin, int threshold) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  if (distance < threshold) {
    return (true);
  }
  else {
    return (false);
  }
}
