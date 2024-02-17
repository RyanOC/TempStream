# TempStream: An IoT Solution for Precise Temperature Monitoring

## Project Introduction

TempStream, a project born from the need to accurately monitor and log temperatures for culinary applications like slow BBQ cooking and Sous Vide preparations, has evolved into a versatile temperature monitoring device. Developed using an ESP32 microcontroller, a precise thermocouple for temperature measurement, and housed within a custom 3D-printed case, TempStream is engineered for reliability and ease of use. Its capability extends beyond cooking, offering potential applications in any scenario where temperature monitoring is critical.

## Technical Specifications

- **Microcontroller**: ESP32, chosen for its Wi-Fi capabilities, enabling TempStream to connect to home networks effortlessly.
- **Temperature Sensor**: A thermocouple, known for its accuracy and wide temperature range, ensures that TempStream provides reliable data.
- **Enclosure**: A durable, 3D-printed case not only protects the hardware but also offers a sleek, professional look.
- **Software Features**: A user-friendly web interface allows for seamless software configuration, making TempStream adaptable to various monitoring needs.

The device seamlessly integrates with home networks by publishing temperature readings to a Mosquitto MQTT Broker installed on a Raspberry Pi. This data is further utilized by Home Assistant to display temperature trends graphically, ensuring that users have clear, actionable insights.

## Future Enhancements

The next phase of development for TempStream includes the introduction of an automated alert system. This feature will notify users if the temperature deviates from predefined safe ranges, adding an extra layer of precaution for food safety.

## Discover More

For a deeper dive into TempStream's hardware construction, including the specifics of the 3D-printed enclosure, please visit [this link](https://makerworld.com/en/models/62492#profileId-85909).

