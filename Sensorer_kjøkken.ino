// ------------------------------------------------------------------ //
//                         Sensorer Kjøkken                          //
// ---------------------------------------------------------------- //
 
 
#include <CircusESP32Lib.h>// Inkluder ESP32/Circus bibloteket
#include <analogWrite.h> // Inkludere analogWrite bibloteket
#include <ezButton.h> // Inkluderer ezButton bibloteket
 
 
// ------------------------------------------------ //
//            CircusESP32Lib egenskapene           //
// ---------------------------------------------- //
 
char ssid[] = "Martin Router King 2GHz"; //  WIFI SSID 
char password[] = "karlsittinternett"; // WIFI password 
char server[] = "www.circusofthings.com";
char key_temp[] = "2368"; // Knyttet til Token 2
char key_LED_Varme[] = "27481"; // Knyttet til Token 2  
char key_setTemp[] = "5429"; // Knyttet til Token 2
char key_dimverdi[] = "11225"; // Knyttet til Token 3
char key_nattsenk[] = "30493"; // Knyttet til Token 1
char key_oppvask_start[] = "22356";
char key_oppvask_pa[] = "27602";
char token1[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1MTkwIn0.-xszm_AFkXDZTnevdjFy9Ivvozdp02EwBSulSWwiOHg"; // Den personlige token som liker opp til signalene dine
char token2[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1OTIyIn0.-QervryV2oRrnpeFFmbx7RGGSLwKmIWhKN3X1mAXCy0"; 
char token3[] = "eyJhbGciOiJIUzI1NiJ9.eyJqdGkiOiI1OTY2In0.jpA9BqhHik3DZACJT-aMcrSunlvQeyid9snyh6WxPTA"; 
CircusESP32Lib circusESP32(server,ssid,password);
 
 
// -------------------------- //
//     Konstante verdier     //
// ------------------------ //
 
// Temp konstanter //
const int LED_Varme = 26; // Setter LED som viser at varme er på til GPIO 26
const int Temp_Pin = 39; // Setter Tem_Pin til GPIO 39
float volts1;    // Variabel for lagring av spenningen som brukes i tempSnitt() funksjonen 
float temp1;     // Tempratur variabel, brukes i tempSnitt() funksjonen 
float tempVal1;    // Temperatur sensorens rådata som blir hentet inn fra GPIO 39
float tempVerdiFunksjon1; // Variabel for lagring av temp verdier som skal legges sammen
float tempVerdiResultat1; // Variabel for tempraturen etter snittutregning
float tempset1; // Variabel som lager tempverdien som er satt i CoT
int LED_state1; // Variabel for å se om led er "AV" eller "PÅ"
int nattsenk; // Variabel som lager verdien til nattsenking hentet fra CoT 
 
 
// Lys konstanter //
const int LED_Lys = 25; // Setter LED som skal representere lyset i rommet til GPIO 25
float dimverdi; // Variabel som lagrer lysverdien som blir hentet fra CoT
 
// Oppvaskmaskin konstanter //
const int buttonPin = 15;
const int LED_Oppvask = 27;
ezButton button(buttonPin);
int buttonState = LOW;
int cotverdi;
 
 
// -------------------------------- //
//     Void Setup / Void Loop      //
// ------------------------------ //
 
void setup() {
  Serial.begin(115200); // Starter Serial
  circusESP32.begin(); // Starter Circus, så setter den opp en SSL/Secure connection
  pinMode(LED_Varme, OUTPUT); // Definerer LEDPIN til OUTPUT
  pinMode(LED_Lys, OUTPUT); // Definerer LEDPIN til OUTPUT
  pinMode(LED_Oppvask, OUTPUT);
  button.setDebounceTime(50);
}
 
 
void loop(){
  Serial.println("------ CoT - ESP32 - KJØKKEN ----"); // Printer startlinje i voidloop
  konsolldimmer(); // Kjører lysstyrings funskjonen 
  termostat(); // Kjører termostatfunksjonen
  oppvaskmaskin(); // Kjører oppvaskmaskin funksjon
  delay(10);
} 
 
//-------------------------------//
//   Funksjon for trykknapp     //
//-----------------------------//
 
void knapp(){
  button.loop();
  if (button.isPressed()){
    Serial.println ("Trykknapp er trykket");
    buttonState = !buttonState;
    if (buttonState = HIGH){
      Serial.println("Oppvaskmaskin klar til å starte");
      }
      else {
        Serial.print("Oppvaskmaskin er ikke klar til å starte. ");
        Serial.println("Trykk på startknapp når maskinen er klar til å starte");
      }         
  }
}
 
 
// --------------------------------//
//   Funksjon for oppvaskmaskin   //
// ----------------------------- //
 
void oppvaskmaskin(){
  float startverdi = circusESP32.read(key_oppvask_start,token2);
  knapp();
  if (startverdi == 1 && buttonState == HIGH){
	cotverdi = 1;
    digitalWrite(LED_Oppvask, HIGH);
    Serial.print("OPPVASKMASKIN STÅR PÅ !!");
    circusESP32.write(key_oppvask_pa,cotverdi,token2);
    }
    else {
      cotverdi = 0;
      digitalWrite(LED_Oppvask, LOW);
      circusESP32.write(key_oppvask_pa,cotverdi,token2);
    } 
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
 
// ----------------------------------------------- //
//    Funksjoner som tar for seg tempstyring.     //
// --------------------------------------------- //
 
// Nattsenk funksjon //
 
void nattsenkFunksjon(){ // Funksjon for strømsparing om kvelden, nattsenk skal være aktiv mellom 23:00 --> 07:00. Styres av CoT
  if (tempVerdiResultat1 < 18){ // Sjekker om romtempraturen er mindre enn hva nattsenktemp er satt til
   LED_state1 = 1; // Setter LED_state til 1, dette er for å vise at varme er på i dashboard på CoT
    digitalWrite(LED_Varme, HIGH); // Skru på diode for å vise at varme står på
  }
    else{
      LED_state1 = 0; // Setter LED_state til 0, dette er for å vise at varme er av i dashboard på CoT
      digitalWrite(LED_Varme, LOW); // Skrur av diode for å vise at det ikke står på varme
    }
  Serial.print(" Tempraturen er:  "); 
  Serial.print(tempVerdiResultat1); // Printer ut tempraturen                 
  Serial.println(" grader C");
  circusESP32.write(key_LED_Varme,LED_state1,token1); // Reporter om Termostaten har slått på varme til COT.
}
 
// Vanlig tempstyrings funskjon //
 void tempstyring_dag(){
   if (tempVerdiResultat1 < tempset1){ // Sjekker om romtempraturen er mindre enn hva tempraturen er satt til
   LED_state1 = 1;
    digitalWrite(LED_Varme, HIGH); // Skru på diode for å vide for at varme står på
  }
    else{
      LED_state1 = 0;
      digitalWrite(LED_Varme, LOW); // Skrur av diode for å vise at det ikke står på varme
    } 
  Serial.print(" Tempraturen er:  "); 
  Serial.print(tempVerdiResultat1); // Printer ut tempraturen                 
  Serial.println(" grader C");
  circusESP32.write(key_LED_Varme,LED_state1,token1); // Reporter om Termostaten har slått på varme til COT.
}
 
// Termostat funksjon //
void termostat(){
  tempset1 = circusESP32.read(key_setTemp,token1);
  tempVerdiResultat1 = tempSnitt1(); // Setter en variabel til tempSnitt funksjonen
  circusESP32.write(key_temp,tempVerdiResultat1,token1); // Reporter om hvor varmt det er i rommet til COT.
  nattsenk = circusESP32.read(key_nattsenk,token1); // Leser av verdien til nattsenk på CoT
  if (nattsenk == 1){
    tempstyring_dag(); // Kjører vanlig tempstyring funksjon
    }
    else{
      nattsenkFunksjon(); // Kjører nattsenk funksjon
    }
 
}
    
 
// --------------------------------------------------------- //
//                Funksjon for Lysstyring                   //
// ------------------------------------------------------- //
 
void konsolldimmer(){
  dimverdi = circusESP32.read(key_dimverdi,token3); // Leser av hvilken verdi den får fra CoT
  dimverdi = map(dimverdi, 0, 100, 0, 255); // Får en verdi mellom 0 og 100 fra CoT, gjør dette om til 0 --> 255
  analogWrite(LED_Lys, dimverdi); // Setter dioden til verdien som er satt på dimmeverdi
}