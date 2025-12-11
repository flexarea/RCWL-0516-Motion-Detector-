# Vehicle Motion Detection RCWL-0516
### Authors: Esdras Ntuyenabo, Ines Ineza, Robsan Dinka

## Introduction

In the United States, over 25% of fatal crashes occur on horizontal curves, underscoring the significant safety risks associated with limited visibility on winding roads (Federal Highway Administration). Countries with hilly terrain, such as Rwanda and Brazil, face similar challenges due to their high density of curvy roads and the resulting prevalence of roadway accidents[1]. 

These risks are not only statistical but personal, several members of our team have experienced firsthand the difficulties of navigating one-lane bridges on two-way roads, where restricted visibility and limited communication between drivers heighten the potential for collisions. These collective observations motivated our project: designing a system that enhances driver awareness in low-visibility environments through real-time detection and communication.

The goal of our project is to extend a driver’s situational awareness by monitoring areas beyond their natural line of sight. We aim to develop a sensor-based system capable of detecting the presence and movement of vehicles in visually obstructed regions. Once an object is detected, the system will communicate this information to approaching drivers through a clear visual alert, similar to a traffic-light signal, to indicate when it is safe to proceed. This approach is especially valuable at high-risk sites such as sharp curves and single-lane bridges on two-way roads, where visibility is limited and collision likelihood is elevated.

Several factors influenced our choice of this project. First, each team member has direct or indirect experience with the types of hazards this system aims to mitigate, making the problem personally meaningful. Second, we believe the potential impact is substantial, as improved awareness in these environments can prevent avoidable accidents and save lives. Our initial interest in detecting object motion naturally led us toward sensor-based technologies. During our research, we drew heavily on documentation provided by Arduino (Tarantula3), which employs the Doppler effec to measure the movement of objects [2]. These sources shaped both our understanding and the eventual design of our system.

Figure 1  
![Horizontal curve sketch](./assets/horizontal-curve.png "horizontal curve sketch")  

Figure 2  
![Tunnel](./assets/tunnel.png "Tunnel image")  

## Methods & Hardware Selection

Our motion detection and alert system relies on carefully selected components that work together to create a reliable, wireless communication network between two locations. The system architecture consists of two ESP32 microcontrollers communicating via ESP-NOW protocol, with one unit acting as a motion detector and transmitter, while the other serves as a receiver and visual alerting device.

The **RCWL-0516 Microwave Radar Sensor** was chosen as our motion detection peripheral because of its superior detection capabilities compared to traditional PIR sensors. This sensor uses microwave Doppler radar technology to detect motion through obstacles like walls and doors, making it ideal for applications where line-of-sight detection is not guaranteed. Unlike PIR sensors that rely on infrared radiation and can be affected by temperature changes, the RCWL-0516 provides more consistent detection across varying environmental conditions. Additionally, its 3.3V-5V operating range makes it compatible with the ESP32's voltage levels, and its digital output signal simplifies integration by eliminating the need for analog-to-digital conversion [3].

We selected **two ESP32 microcontrollers** primarily for their robust networking capabilities, specifically their built-in WiFi module that enables ESP-NOW communication. ESP-NOW is a connectionless communication protocol developed by Espressif that allows multiple devices to communicate without requiring a WiFi router or access point. This makes our system ideal for deployment in environments where traditional WiFi infrastructure may be unavailable or unreliable. The ESP32 also provides ample GPIO pins for connecting our peripherals, sufficient processing power for handling interrupt-driven motion detection, and a generous memory footprint for managing the communication protocol stack.

For the visual alert component, we chose the **NeoPixel Ring with 16 x 5050 (5.0mm x5.0mm) RGB LEDs** [4]. This addressable LED ring offers high visibility with its consistant ~18mA per LED, ensuring the alert can be seen from a distance. The individually addressable nature of the LEDs gives us fine-grained control over color and animation patterns, though our current implementation uses a simple full-ring red alert. The integrated drivers simplify the control logic, requiring only a single data pin from the ESP32 to control all sixteen LEDs.

The **KBT 5V 4Ah Battery Pack** was selected to provide portable power to our system [5]. The RCWL-0516 sensor draws approximately 2-3mA during standby and up to 28mA when actively detecting motion. Combined with the ESP32's power consumption (which averages around 80-160mA during WiFi transmission) and the NeoPixel ring (which can draw up to 288mA (16*18mA) when all LEDs are at full brightness), we needed a power source capable of delivering sustained current without voltage sag. The 5V output matches our system's voltage requirements, and the 4Ah capacity provides several hours of continuous operation. The included DC5521 connector cable simplifies the connection to our breadboard power rails, eliminating the need for custom wiring solutions.

## Software Libraries and Existing Code Integration

Our implementation leverages two primary libraries that provide the foundation for device communication and LED control.

For wireless communication between the two ESP32 units, we implemented **ESP-NOW** following the tutorial and code examples from Random Nerd Tutorials [6]. This existing resource provided the fundamental structure for initializing ESP-NOW, registering peer devices, and implementing callback functions for sent and received data. We adapted their example code by incorporating our specific MAC address for the peer device and modified the data structure to carry motion detection information. The callback functions `OnDataSent()` and `OnDataRecv()` are based on their implementation pattern, though we extended `OnDataRecv()` to trigger our LED alert system by calling `setNeo()`.

For LED control, we utilized the **Adafruit NeoPixel library** [7]. This library abstracts the timing-critical protocol required to communicate with Neopixel Ring, providing simple functions like `setPixelColor()` and `show()` that we used to create our alert pattern. Our `setNeo()` function implementation is original code that creates a solid red alert display across all sixteen pixels for six seconds before clearing.

## System Architecture and Code Logic

The system operates in a state-based architecture with two distinct operational modes: **RECEIVING** and **SENDING**. This design pattern allows the same code to run on both ESP32 units, with each device capable of detecting motion and alerting the other.

### Initialization and Setup

During the `setup()` phase, we initialize the serial monitor for debugging, configure the NeoPixel ring, and establish the ESP-NOW communication framework. A critical element of our design is the interrupt-driven motion detection system. We attach an interrupt to the RCWL sensor pin (GPIO 25) that triggers on a RISING edge:

```cpp
attachInterrupt(RCWL_PIN, my_callback, RISING);
```

This interrupt-driven approach is essential because it allows the microcontroller to respond immediately to motion detection without continuously polling the sensor pin in the main loop. When motion is detected, the RCWL-0516 outputs a HIGH signal, triggering the interrupt service routine `my_callback()`, which simply sets the `state_flag` to `SENDING`.

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

When a car approaches one of our units, it signals the other unit to display a red light to oncoming traffic, notifying them that another vehicle is approaching through the narrow passage. Our product communicates to drivers where they should use increased caution in an easy-to-digest format. One possible point of failure for our devices is that since we use a microwave sensor, any movement will trigger a reaction. This could result in an increase of false positive readings cause by weather conditions or non-traffic motion near the sign.
It is important to note that our product should not be a stand-alone signal and that it should be supplemented with a traffic sign that reflects the layout of the road or promotes caution. 


Detecting Part
![off](./assets/off.jpeg "Off")

Alert Part
![On](./assets/on.jpeg "On ")

## Accessibility
Since our project incorporates color signals for drivers, it can be challenging for drivers with color blindness or low-vision impairments to interpret the signal. To mitigate this, redundant cues can help adjust the type of signal given to the driver. An optimal solution would be to communicate with the driver directly and alert them of incoming vehicles, but since we cannot establish such communication at this time, we will instead have the light blink four times at a high brightness level to ensure it is noticeable. Additionally, the device is only on when there is incoming traffic and off otherwise; therefore, regardless of the color, the presence of light always indicates incoming traffic.
We also considered drivers who might find it difficult to interpret alerts quickly, which is why we kept the system simple; there is light if there is incoming traffic and no light otherwise. The signal will be visible from afar, helping drivers recognize it even during fog or undesirable weather. We will ensure the device is mounted at a height that maximizes visibility while remaining safe for maintenance workers. Since the device uses rechargeable batteries, it will not require external electricity, making it accessible for communities that need it most. In the future, we hope to have the device harness solar energy to power itself.

We have encountered multiple issues in this project. First, we had a hard time mounting everything on the board. Watching tutorials helped us gain foundational knowledge about where everything goes and how to program it. We were able to have the ESP32 communicate with the microwave sensor and read the results. What we were initially unable to do was have the ESP32 turn the Neopixels on. This slowed us down, but we later discovered that we had to connect the ESP32 and the Neopixels to the same ground, quite literally. This setback affected our schedule, but it did not affect the original goal.
Another issue we faced was the latency between the time the receiver Neopixel turns on and when the sender ESP32 sends a signal. There is about a two-second delay. This created concerns regarding how quickly the device could notify the driver of an incoming vehicle. To solve this, we decided to change where we mount the devices on the road. This adjustment did not affect our schedule significantly, and it did not affect our original goal.


## Ethical Implication

Although this system is meant to save lives by preventing accidents on horizontal curve roads, it could also yield potential problems. One ethical concern involves the risk of over-reliance: as drivers grow to trust and depend on the system’s alerts, the sudden removal of the system or any malfunction may create a false sense of security and increase accident risk. Another ethical consideration relates to equity in deployment. If such safety systems are installed only in well-funded or high-traffic regions, communities with similar hazards but fewer resources may be left without comparable protection, unintentionally increasing safety disparities. 

## Schedule

In our original proposal, we planned to complete all technical development before beginning the presentation and final report. The final week was intended solely for preparing these deliverables, keeping the building and documentation phases clearly separated. Our initial schedule was: 

* Week 1: Project proposal, identify hardware needs, begin learning hardware and building the scenario model

* Week 2: Work with components, continue physical model development, assemble embedded system, begin coding

* Week 3: Troubleshoot and debug, add final touches to the system

* Week 4: Create the 3D model of the scenario environment  

In practice, our timeline shifted during the final weeks. We realized the presentation and report required more time than expected, so we began both earlier and developed them alongside the remaining technical tasks. This change allowed us to manage our workload more effectively and avoid rushing at the end. Our adjusted schedule became:  

* Week 1: Project proposal, hardware needs, begin learning hardware and building the scenario model

* Week 2: Work with components, continue physical model development, assemble embedded system, write code

* Week 3: Troubleshoot, debug, finalize system details, begin report

* Week 4: Create 3D casing model, finalize report, complete presentation  

## Issues

This is where you discuss the issues you encountered during the project development and what you did to resolve them. What did you change? Did they affect your schedule? Did the affect your original goal?

We have encountered multiple issues in this project. First, we had a hard time mounting everything on the board. Watching tutorials helped us gain foundational knowledge about where everything goes and how to program it. We were able to have the ESP32 communicate with the microwave sensor and read the results. What we were initially unable to do was have the ESP32 turn the Neopixels on. This slowed us down, but we later discovered that we had to connect the ESP32 and the Neopixels to the same ground, quite literally. This setback affected our schedule, but it did not affect the original goal.
Another issue we faced was the latency between the time the receiver Neopixel turns on and when the sender ESP32 sends a signal. There is about a two-second delay. This created concerns regarding how quickly the device could notify the driver of an incoming vehicle. To solve this, we decided to change where we mount the devices on the road. This adjustment did not affect our schedule significantly, and it did not affect our original goal.

 
## Future Work
Given more time and a bigger budget, there are several things we would improve to make a better product.

Firstly, we would use a different sensor with more functionality. Since we are working with microwave sensors, our product detects all types of motion. If we had more time and a bigger budget, we would use a different sensor that would be able to detect the difference between a vehicle and other moving objects. Using a different sensor, such as a LIDAR sensor, would help reduce false positives caused by non-traffic-related objects. 

Secondly, if we had more time, we would use solar energy to power the batteries in our product. This improvement would extend battery life and minimize maintenance of our product while sourcing from clean energy. 

Finaly, we would write a custom transmission network protocol, that is faster and acknowledge packet receipt.  

## References
[1] <https://highways.dot.gov/safety/rwd/keep-vehicles-road/horizontal-curve-safety> 
[2] <https://news.mit.edu/2010/explained-doppler-0803>  
[3] <https://github.com/jdesbonnet/RCWL-0516?tab=readme-ov-file>  
[4] <https://learn.adafruit.com/adafruit-neopixel-uberguide/downloads>    
[5] <https://ibspot.com/us/products/kbt-5v-4ah-battery-pack-with-charger-dc5521-connector-cable-for-5volt-devices-rc-car-robot-diy-led-light-strip> 
[6] <https://randomnerdtutorials.com/esp-now-esp32-arduino-ide/> 
[7] <https://github.com/adafruit/Adafruit_NeoPixel> 
[-] <https://projecthub.arduino.cc/tarantula3/all-about-rcwl-0516-microwave-radar-motion-sensor-5aa86d>  
