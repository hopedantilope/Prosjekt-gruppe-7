// ------------------------------ //
//        Sensorer soverom       //
// ---------------------------- //

#include <CircusESP32Lib.h>// Inkluder ESP32/Circus bibloteket
#include <analogWrite.h>
#include <Servo.h>

// ------------------------------------------------ //
//      Sette inn CircusESP32Lib egenskapene       //
// ---------------------------------------------- //

char ssid[] = "Martin Router King 2GHz"; //  WIFI SSID 
char password[] = "karlsittinternett"; // WIFI password 
char server[] = "www.circusofthings.com";
char key_temp[] = "24273"; // Setter inn Key til Circus Signal som ESP32 skal høre på. 
char key_LED_Varme[] = "14964"; // Setter inn Key til Circus Signal som ESP32 skal høre på. 
char key_setTemp[] = "27206"; // Setter inn Key til Circus Signal som ESP32 skal høre på. 
char key_Lys[]= "9444";
char key_Dimmer[] = "28261";
char token[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1MTkwIn0.-xszm_AFkXDZTnevdjFy9Ivvozdp02EwBSulSWwiOHg"; // Den personlige token som liker opp til signalene dine
CircusESP32Lib circusESP32(server,ssid,password);

// -------------------------- //
//     Konstante verdier     //
// ------------------------ //

// Temp konstanter //
const int LED_Varme = 26;
const int Temp_Pin = 39;
float volts1;    // variabel for lagring av spenningen 
float temp1;     // Tempratur variabel
float tempVal1;    // Temperatur sensorens rådata
float tempVerdiFunksjon1; // Variabel for lagring av temp verdier som skal legges sammen
float tempVerdiResultat1; // Variabel for tempraturen etter snittutregning
float tempset1;
int LED_state1; // Variabel for å se om led er "AV" eller "PÅ"


// Lys konstanter //
const int LED_Lys = 25;
const int Lux_Pin = 36;
float lysVerdiResultat1;
float lysVerdiFunksjon1;
int buttonState;

// Servo konstanter //
Servo myservo; 
int servopos1 = 0;


// ---------------------------------------- //
//              Void Setup                 //
// -------------------------------------- //
 
void setup() {
  Serial.begin(115200);
  pinMode(LED_Varme, OUTPUT); // Definerer LEDPIN til OUTPUT
  pinMode(LED_Lys, OUTPUT); // Definerer LEDPIN til OUTPUT
  myservo.attach(13);  // attaches the servo on pin 13 to the servo object
  circusESP32.begin(); // Starter Circus, så setter den opp en SSL/Secure connection
}
 
// ---------------------------------------- //
//              Void Loop                  //
// -------------------------------------- //
 
void loop(){
  Serial.println("------ CoT - ESP32 - Soverom 1 ----");
  Brytere();
  termostat1();
} 

// ----------------------------------------------- //
//    Funksjon for å finne ut snitt tempratur     //
// --------------------------------------------- //
 
float tempSnitt1(){
  tempVerdiFunksjon1 = 0; // Setter samlevariabelen til 0 
  for (int i=0;i<=4;i++) { // For loop som tar fem målinger
    tempVal1 = analogRead(Temp_Pin); // Leser av tempPin
    volts1 = tempVal1/1023.0; // Normaliser med maksimal temperatur råavlesningsområdet
    temp1 = (volts1 - 0.5) * 100 ; // Beregner temperatur i celsius fra spenning i henhold til ligningen som er funnet på sensorens spesifikasjonsark.
    tempVerdiFunksjon1 += temp1; // Legger hver måling inn i tempVerdiFunksjon variabelen
    delay(100);
  }
  return tempVerdiFunksjon1 / 5; // Retunerer tempVerdiFunksjon som da er delt på 5, får å få riktig gjennomsnitts tempratur
}

// ------------------------------------------- //
//      Funksjon for å finne ut snitt lys     //
// ----------------------------------------- //
 
float lysSnitt1(){
  lysVerdiFunksjon1 = 0;
  for (int i=0;i<=4;i++) {
    lysVerdiFunksjon1 += analogRead(Lux_Pin);
    delay(100);
  }
  return lysVerdiFunksjon1 / 5;
}


// ----------------------------------------------- //
//    Funksjon for Termostatfunksjon og Servo     //
// --------------------------------------------- //

void termostat1(){
  tempset1 = circusESP32.read(key_setTemp,token);
  tempVerdiResultat1 = tempSnitt1(); // Setter en variabel til tempSnitt funksjonen
  circusESP32.write(key_temp,tempVerdiResultat1,token); // Reporter om Termostaten har slått på varme til COT.
  if (tempVerdiResultat1 < tempset1){
   LED_state1 = 1;
    digitalWrite(LED_Varme, HIGH); // Setter LEDPIN til High
  }
    else{
      LED_state1 = 0;
      digitalWrite(LED_Varme, LOW);
    } 
  if (tempVerdiResultat1 > 26){
    for (servopos1 = 0; servopos1 <= 45; servopos1 += 1) { // goes from 0 degrees to 180 degrees
      myservo.write(servopos1);              // tell servo to go to position in variable 'pos'                    
    }
  }
  if (tempVerdiResultat1 < 26){
    for (servopos1 = 45; servopos1 >= 0; servopos1 -= 1) { // goes from 180 degrees to 0 degrees
      myservo.write(servopos1);              // tell servo to go to position in variable 'pos'
    }
  }
  Serial.print ("Servoposisjon er:  ");
  Serial.print(servopos1);
  Serial.println("  Grader");
  Serial.print(" Tempraturen er:  "); 
  Serial.print(tempVerdiResultat1); // Printer ut tempraturen                 
  Serial.println(" grader C");
  Serial.println(tempVal1);
  circusESP32.write(key_LED_Varme,LED_state1,token); // Reporter om Termostaten har slått på varme til COT.
}


// ---------------------------------------------------- //
//         Funksjon for Automatisk Lysstyring          //
// -------------------------------------------------- //
 
void lysstyring1 (){
  lysVerdiResultat1 = map(lysVerdiResultat1, 0, 4095, 255, 0); // Setter verdien fra Photoresistor til verdier til LED
  analogWrite(LED_Lys, lysVerdiResultat1);
  Serial.print(" Lysverdien er:  "); 
  Serial.println(lysVerdiResultat1); // Printer Lysverdien
}


// --------------------------------------------------------- //
//         Funksjon for innstilling av  Lysstyring          //
// ------------------------------------------------------- //
 
void Brytere (){
  buttonState = circusESP32.read(key_Lys,token); // Leser av hvilken verdi den får fra CoT
  lysVerdiResultat1 = lysSnitt1(); // Setter en variabel til lysSnitt funksjonen
  if (buttonState == 0){
    analogWrite (LED_Lys, 0); // Skrur av Lyset hvis den får signal 0 fra CoT
    Serial.println("Lys er skrudd av");
  }
  else if (buttonState == 1){ // Setter lyset i dimmemodus
    float dimverdi = circusESP32.read(key_Dimmer,token); // Tar inn dimmeverdi
    analogWrite (LED_Lys, dimverdi); // Setter lyset til den dimmeverdien CoT gir
    Serial.println("Lys er i dimmermodus fra CoT ");
  }
  else if (buttonState == 2){ // Setter lyset i automodus, der lyset styres av lysstyrken i rommet.
    Serial.println("Lys står på Auto");
    lysstyring1(); // Starter lysstyringsfunskjonen 
  }
}