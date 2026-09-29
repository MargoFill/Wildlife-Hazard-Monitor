#include <WiFiS3.h>
#include <SoftwareSerial.h>

#define RX 3
#define TX 2
SoftwareSerial LoRaSerial(RX,TX);

// WiFi info
const char* ssid = "OnePlus 12R"; // WiFi name
const char* password = "g88fqaii"; // WiFi password
const char* serverIP = "192.168.172.241"; // Laptop local IP
const int serverPort = 5000; // Localhost port for data transfer

WiFiClient client;
void setup() {
  Serial.begin(9600);
  LoRaSerial.begin(9600);

  // connect to wifi
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected!");

  // Set LoRa frequency & parameters
  LoRaSerial.println("AT+BAND=869525000\r\n");  // Use 869.525 MHz for a 10% duty cycle
  LoRaSerial.println("AT+PARAMETER=7,125,4,6\r\n"); // SF7, 125 kHz BW, CR 4/6, Preamble 6
  LoRaSerial.println("AT+CRFOP=14\r\n");
  delay(2000);
}

void loop() {
  if (LoRaSerial.available()) {
    String response = LoRaSerial.readString();
    response.trim(); // Remove extra spaces or newlines
    Serial.println("Received: " + response);

    // Check if response contains LoRa metadata
    if (response.startsWith("+RCV=")) {
      int firstComma = response.indexOf(','); 
      int secondComma = response.indexOf(',', firstComma + 1); 
      int lastComma = response.lastIndexOf(','); 
      int secondLastComma = response.lastIndexOf(',', lastComma - 1);

      // Extract only the GPS data (removes first two and last two values)
      String gpsData = response.substring(secondComma + 1, secondLastComma);

      Serial.println(gpsData);

      // Ensure it's valid GPS data
      if (gpsData.indexOf(',') != -1) { 
        sendToServer(gpsData);
      }
    }
  }
}


void sendToServer(String data) {
  if (client.connect(serverIP, serverPort)) {
    Serial.println("Sending data to server...");
    client.println("POST /data HTTP/1.1");
    client.println("Host: " + String(serverIP));
    client.println("Content-Type: text/plain");
    client.println("Content-Length: " + String(data.length()));
    client.println();
    client.print(data);  
    
    client.stop();
  } else {
    Serial.println("Failed to connect to server.");
  }
}

