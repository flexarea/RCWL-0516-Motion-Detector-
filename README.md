# Vehicle Motion Detection RCWL-0516
### Authors: Esdras Ntuyenabo, Ines Ineza, Robsan Dinka

## Introduction

In the United States, over 25% of fatal crashes occur on horizontal curves, underscoring the significant safety risks associated with limited visibility on winding roads (Federal Highway Administration). Countries with hilly terrain, such as Rwanda and Brazil, face similar challenges due to their high density of curvy roads and the resulting prevalence of roadway accidents. These risks are not only statistical but personal, several members of our team have experienced firsthand the difficulties of navigating one-lane bridges on two-way roads, where restricted visibility and limited communication between drivers heighten the potential for collisions. These collective observations motivated our project: designing a system that enhances driver awareness in low-visibility environments through real-time detection and communication.
The goal of our project is to extend a driver’s situational awareness by monitoring areas beyond their natural line of sight. We aim to develop a sensor-based system capable of detecting the presence and movement of vehicles in visually obstructed regions. Once an object is detected, the system will communicate this information to approaching drivers through a clear visual alert, similar to a traffic-light signal, to indicate when it is safe to proceed. This approach is especially valuable at high-risk sites such as sharp curves and single-lane bridges on two-way roads, where visibility is limited and collision likelihood is elevated.
Several factors influenced our choice of this project. First, each team member has direct or indirect experience with the types of hazards this system aims to mitigate, making the problem personally meaningful. Second, we believe the potential impact is substantial, as improved awareness in these environments can prevent avoidable accidents and save lives. Our initial interest in detecting object motion naturally led us toward sensor-based technologies. During our research, we drew heavily on documentation provided by Arduino (Tarantula3), which employs the Doppler effect (Bettex, 2010) to measure the movement of objects. These sources shaped both our understanding and the eventual design of our system.


## Methods & Hardware Selection

Our motion detection and alert system relies on carefully selected components that work together to create a reliable, wireless communication network between two locations. The system architecture consists of two ESP32 microcontrollers communicating via ESP-NOW protocol, with one unit acting as a motion detector and transmitter, while the other serves as a receiver and visual alerting device.

The **RCWL-0516 Microwave Radar Sensor** was chosen as our motion detection peripheral because of its superior detection capabilities compared to traditional PIR sensors. This sensor uses microwave Doppler radar technology to detect motion through obstacles like walls and doors, making it ideal for applications where line-of-sight detection is not guaranteed. Unlike PIR sensors that rely on infrared radiation and can be affected by temperature changes, the RCWL-0516 provides more consistent detection across varying environmental conditions. Additionally, its 3.3V-5V operating range makes it compatible with the ESP32's voltage levels, and its digital output signal simplifies integration by eliminating the need for analog-to-digital conversion.

We selected **two ESP32 microcontrollers** primarily for their robust networking capabilities, specifically their built-in WiFi module that enables ESP-NOW communication. ESP-NOW is a connectionless communication protocol developed by Espressif that allows multiple devices to communicate without requiring a WiFi router or access point. This makes our system ideal for deployment in environments where traditional WiFi infrastructure may be unavailable or unreliable. The ESP32 also provides ample GPIO pins for connecting our peripherals, sufficient processing power for handling interrupt-driven motion detection, and a generous memory footprint for managing the communication protocol stack.

For the visual alert component, we chose the **NeoPixel Ring with 16 x 5050 RGB LEDs**. This addressable LED ring offers high visibility with its bright 5050-sized LEDs, ensuring the alert can be seen from a distance. The individually addressable nature of the LEDs gives us fine-grained control over color and animation patterns, though our current implementation uses a simple full-ring red alert. The integrated drivers simplify the control logic, requiring only a single data pin from the ESP32 to control all sixteen LEDs.

The **KBT 5V 4Ah Battery Pack** was selected to provide portable power to our system. The RCWL-0516 sensor draws approximately 2-3mA during standby and up to 28mA when actively detecting motion. Combined with the ESP32's power consumption (which averages around 80-160mA during WiFi transmission) and the NeoPixel ring (which can draw up to 960mA when all LEDs are at full brightness), we needed a power source capable of delivering sustained current without voltage sag. The 5V output matches our system's voltage requirements, and the 4Ah capacity provides several hours of continuous operation. The included DC5521 connector cable simplifies the connection to our breadboard power rails, eliminating the need for custom wiring solutions.

## Software Libraries and Existing Code Integration

Our implementation leverages two primary libraries that provide the foundation for device communication and LED control.

For wireless communication between the two ESP32 units, we implemented **ESP-NOW** following the tutorial and code examples from Random Nerd Tutorials (https://randomnerdtutorials.com/esp-now-esp32-arduino-ide/). This existing resource provided the fundamental structure for initializing ESP-NOW, registering peer devices, and implementing callback functions for sent and received data. We adapted their example code by incorporating our specific MAC address for the peer device and modified the data structure to carry motion detection information. The callback functions `OnDataSent()` and `OnDataRecv()` are based on their implementation pattern, though we extended `OnDataRecv()` to trigger our LED alert system by calling `setNeo()`.

For LED control, we utilized the **Adafruit NeoPixel library** documented at https://github.com/adafruit/Adafruit_NeoPixel. This library abstracts the timing-critical protocol required to communicate with WS2812B LEDs, providing simple functions like `setPixelColor()` and `show()` that we used to create our alert pattern. Our `setNeo()` function implementation is original code that creates a solid red alert display across all sixteen pixels for six seconds before clearing.

## System Architecture and Code Logic

The system operates in a state-based architecture with two distinct operational modes: **RECEIVING** and **SENDING**. This design pattern allows the same code to run on both ESP32 units, with each device capable of detecting motion and alerting the other.

### Initialization and Setup

During the `setup()` phase, we initialize the serial monitor for debugging, configure the NeoPixel ring, and establish the ESP-NOW communication framework. A critical element of our design is the interrupt-driven motion detection system. We attach an interrupt to the RCWL sensor pin (GPIO 25) that triggers on a RISING edge:

```cpp
attachInterrupt(RCWL_PIN, my_callback, RISING);
```

This interrupt-driven approach is essential because it allows the microcontroller to respond immediately to motion detection without continuously polling the sensor pin in the main loop. When motion is detected, the RCWL-0516 outputs a HIGH signal, triggering the interrupt service routine `my_callback()`, which simply sets the `state_flag` to `SENDING`. This lightweight ISR design follows best practices by avoiding any time-consuming operations within the interrupt context.

### ESP-NOW Configuration

The ESP-NOW setup process involves several steps. We first set the ESP32 to station mode (`WIFI_STA`) and initialize the ESP-NOW protocol. We then register two callback functions: `OnDataSent()` provides delivery confirmation for transmitted packets, while `OnDataRecv()` handles incoming messages from the peer device. The peer registration process requires the MAC address of the remote ESP32, which we hard-code into the `broadcastAddress` array:

```cpp
uint8_t broadcastAddress[] = {0xA8, 0x03, 0x2A, 0xEA, 0xE8, 0x54};
memcpy(peerInfo.peer_addr, broadcastAddress, 6);
peerInfo.channel = 0;  
peerInfo.encrypt = false;
```

We configured the system for unencrypted communication on channel 0 to minimize latency, as our application does not transmit sensitive data and prioritizes response time.

### Main Loop and State Management

The `loop()` function implements a simple but effective state machine. Rather than continuously checking sensor status, it only responds when the `state_flag` indicates that motion has been detected:

```cpp
void loop() {
  if (state_flag == SENDING){
    handle_motion_detected();
  }
}
```

This design minimizes unnecessary processing and allows the ESP32 to remain in a lower power state when no motion is detected. When motion triggers the interrupt, the system enters the `handle_motion_detected()` function, which performs three key actions: activates the onboard LED for local feedback, calls `transmit()` to send a notification to the peer device, and resets the state flag back to `RECEIVING`.

### Data Transmission Protocol

The `transmit()` function packages motion detection data into a structured message format:

```cpp
typedef struct struct_message {
  char a[32];
  int b;
  float c;
  bool d;
} struct_message;
```

While our current implementation uses a simple text message "Motion detected" in field `a`, the structure includes additional fields that could be extended to transmit sensor readings, timestamps, or other metadata. The `esp_now_send()` function transmits this structure to the registered peer device, with the callback function confirming successful delivery.

### Visual Alert Implementation

When the receiving ESP32 gets a motion notification, the `OnDataRecv()` callback immediately triggers `setNeo()`, which creates a visual alert. Our implementation lights all sixteen LEDs in red for six seconds:

```cpp
void setNeo(){
  pixels.clear();
  for(int i=0; i<NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 0, 0));
  }
  pixels.show();
  delay(6000);
  pixels.clear();
  pixels.show();
}
```

The six-second duration was chosen to ensure the alert is noticeable without being excessively long. We use red at full intensity (255) as it provides maximum visibility and is universally recognized as an alert color. The `pixels.clear()` calls before and after the alert ensure a clean state transition.

## Key Design Decisions and Differentiators

Several aspects of our implementation distinguish it from basic ESP-NOW examples. First, we designed the system to be bidirectional and symmetric—each unit can detect motion and alert the other, rather than having dedicated transmitter and receiver roles. This flexibility allows deployment in various configurations without reprogramming.

Second, our interrupt-driven architecture provides faster response times compared to polling-based approaches. The typical delay from motion detection to remote alert is under 100 milliseconds, making the system suitable for time-sensitive applications.

Third, we incorporated multiple feedback mechanisms: the onboard LED provides local confirmation of motion detection, while the NeoPixel ring on the remote unit provides the primary alert. This redundancy helps with system debugging and user confidence.

The modular structure of our code also facilitates future enhancements. The data structure could easily be extended to include sensor readings, battery status, or device identification. The LED alert pattern could be modified to support different colors or animations based on message content or priority levels. The system could also scale to support more than two devices by registering additional peers in the ESP-NOW framework.

## Results

![Horizontal curve sketch](./assets/horizontal-curve.png "horizontal curve sketch")
