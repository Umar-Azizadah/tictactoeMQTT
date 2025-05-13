<<<<<<< HEAD
# _Sample project_

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This is the simplest buildable example. The example is used by command `idf.py create-project`
that copies the project to user specified path and set it's name. For more information follow the [docs page](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project)



## How to use example
We encourage the users to use the example as a template for the new projects.
A recommended way is to follow the instructions on a [docs page](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project).

## Example folder contents

The project **sample_project** contains one source file in C language [main.c](main/main.c). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt`
files that provide set of directives and instructions describing the project's source files and targets
(executable, library, or both). 

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   └── main.c
└── README.md                  This is the file you are currently reading
```
Additionally, the sample project contains Makefile and component.mk files, used for the legacy Make based build system. 
They are not used or needed when building with CMake and idf.py.
=======
# ESP32 IoT Tic-Tac-Toe System
An embedded + IoT system that implements a real-time multiplayer Tic-Tac-Toe game.  
This project integrates local game logic on an ESP32 microcontroller with cloud-based player communication via MQTT using a Google Cloud-hosted broker.  
The system allows for human vs. cloud-scripted player modes, demonstrating secure device-to-cloud communication and interactive embedded systems design.

## Key Features
- Developed in C with ESP-IDF
- ESP32-S3 microcontroller platform
- Real-time player input via serial interface
- Automated player move generation using GCP-hosted MQTT scripts
- Cloud-to-device messaging pipeline
- Clean text-based UI and game logic
- Foundation for IoT and cloud-integrated microcontroller projects

## Technologies Used
- ESP32 (ESP-IDF)
- MQTT Protocol
- Google Cloud Compute Engine (GCP)
- C Programming
>>>>>>> 49ea6695e6388f8e3e7536902bc52df6a20ad760
