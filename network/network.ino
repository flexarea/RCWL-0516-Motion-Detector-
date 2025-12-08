#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#ifdef __AVR__
  #include <avr/power.h>
#endif

#define RCWL_PIN 25
#define PIN        26
#define NUMPIXELS 16

#define RECEIVING 0
#define SENDING 1

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

volatile int state_flag = RECEIVING;

// A8:03:2A: EA: E8:54
uint8_t broadcastAddress[] = {0xA8, 0x03, 0x2A, 0xEA, 0xE8, 0x54};
//{0xA8, 0x03, 0xEA, 0xE8, 0x54};

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  char a[32];
  int b;
  float c;
  bool d;
} struct_message;

// Create a struct_message called myData
struct_message myData;

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {


    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

/*=== Receiver's logic ===*/
void OnDataRecv(const esp_now_recv_info_t* info, const uint8_t *incomingData, int len){
  setNeo();

  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Char: ");
  Serial.println(myData.a);
  Serial.print("Int: ");
  Serial.println(myData.b);
  Serial.print("Float: ");
  Serial.println(myData.c);
  Serial.print("Bool: ");
  Serial.println(myData.d);
  Serial.println();

}

void setup() {
  // Init Serial Monitor
  Serial.begin(115200);

  /*pixel configuration*/
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif

  pixels.begin();

  /* set interrupt*/
  attachInterrupt(RCWL_PIN, my_callback, RISING);
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  /*send packet info*/
  esp_now_register_send_cb(OnDataSent);
  /*get receive packet info*/
  esp_now_register_recv_cb(OnDataRecv);

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  /*configure RCWL PIN*/
  pinMode(RCWL_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  state_flag = RECEIVING;

}

void ARDUINO_ISR_ATTR my_callback() {
  state_flag = SENDING;
}

void transmit(){
  // Set values to send
  strcpy(myData.a, "Motion detected");
  myData.b = random(1,20);
  myData.c = 1.2;
  myData.d = false;

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }

}

void handle_motion_detected (){
  digitalWrite(LED_BUILTIN, HIGH);
  delay(2000);
  digitalWrite(LED_BUILTIN, LOW);
  transmit();
  state_flag = RECEIVING; // go back to receiving state
}


void setNeo(){
  pixels.clear();

  for(int i=0; i<NUMPIXELS; i++) {

    pixels.setPixelColor(i, pixels.Color(255, 0, 0));
    //pixels.show();
    //delay(DELAYVAL);
  }

  pixels.show();
  delay(6000);
  pixels.clear();
  pixels.show();
}

void loop() {
  if (state_flag == SENDING){
    handle_motion_detected();
  }
}
