#include <Arduino.h>

#define LED_PIN 40
#define PIR_PIN 5

boolean isPeopleDetected()
{
    int sensorValue = digitalRead(PIR_PIN);
    if(sensorValue == HIGH)//if the sensor value is HIGH?
    {
        return true;
    }
    else
    {
        return false;
    }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(LED_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  Serial.println();
  Serial.println("Plain LED blinking on GPIO40.");
}

void loop() {
  
  if (isPeopleDetected()) //There is a problem that the loop only has time to run as long as isPeopleDetected is high
  {
    Serial.println("Detection Sequence commencing!");
    for (int i=0;i<3;){
      digitalWrite(LED_PIN, HIGH);   // on
      delay(200);                    // <-- wait 1/4 of a second
      digitalWrite(LED_PIN, LOW);    // off  
      i++;
    }
    Serial.println("Sequence completed"); 
  }
  else
  {
    digitalWrite(LED_PIN, LOW);    // off  
    Serial.println("no detection?");
  }                  // <-- and again
  Serial.println("Running");

  delay(250);
}
