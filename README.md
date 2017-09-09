# Neato Scheduler
Neato Scheduler is a framework built on an ESP8266 Wifi serial adapter, that allows you to interface wirelessly to your neato botvac (65, 70e, 75, 80, 85, D75, D80 and D85) and provides you a few important features that even newer models don't have. You won't have to use that clunky neato interface on your botvac ever again. https://www.neatoscheduler.com

_A video guide can be found [here](https://www.neatoscheduler.com/home.php?action=videos)_
_This project was tested with a Neato Botvac 85 and an ESP8266 ESP-12F._

## Table of contents
* [Setup](#setup)
  * [Hardware](#hardware)
  * [Software](#software)
* [Usage](#usage)
  * [Websocket](#websocket)
  * [HTML Interface](#html-interface)
  * [Command Syntax](#command-syntax)
* [Known Issues and Limitations](#known-issues-and-limitations)


## Setup
### Hardware
Any ESP8266 with 1MB or larger flash can be used with this program.
There is a card edge connector on each side of the main board (labelled P7 on top side and P25 on the bottom side) at the front right corner (when viewed from the front of the bot) of the bot. The pins are as follows:

Top:
* Serial Transmit
* Ground
* Serial Receive

Bottom:
* +3.3 volt
* not connected
* Ground
![Botvac Serial Connector](/img/botvac-pins.jpg)

The following connections need to be made between the bot and the ESP module:
* TX bot to RX ESP
* RX bot to TX ESP
* GND bot to GND ESP
* 3.3V bot to VCC ESP (Note: If you are using an ESP module that has a built-in 5V -> 3.3V power regulator (like the Witty Cloud Module), you will need to solder the 3.3V power connection directly to the ESP module as the VCC pin of the ESP Module. This is because the power input of the ESP Module is connected to the 5V of the power regulator and not directly to the 3.3V VCC of the ESP module itself)

The 3.3V connection on the bot is handled by the power regulation circuitry, so there **should** be no risk of the module completely draining the battery. The module power draw is also included in the GetCharger Discharge_mAH total. When powered by a bench power supply, the module I'm using had a maximum power draw of 80mA to 100mA.

The wires should be long enough so they don't interfere with the opening and closing of the top case. I would also add some connectors to the wires so the ESP module can be disconnected from the bot for easier firmware updates. I used Dupont connectors.

The module is glued with strong double sided to the top case on the opposite side of the display:
![Botvac Serial Connector](/img/installed-module.jpg)

### Software
Required:
* Arduino IDE: https://www.arduino.cc/en/Main/Software
* ESP8266 core for Arduino: https://github.com/esp8266/Arduino
* TimedAction: https://playground.arduino.cc/Code/TimedAction
* rBASE64: https://github.com/boseji/rBASE64
* arduinoWebSockets: https://github.com/Links2004/arduinoWebSockets

Install all software according to the documentation. Depending on what module/programming adaptor you are using you might also need to install the appropriate drives.

On initial bootup, a Wifi AP will show up with the SSID "neato". You will need to connect to it and naviage to http://neato.local. Replace the XXX with your WiFi SSID and password and press submit. Your ESP will restart and connect to your specified SSID. You can now access your ESP by connecting to your specified SSID and navigating to http://neato.local

## Usage
The software includes multicast DNS (mDNS), so you should be able to connect to the bot with http://neato.local for a simple websocket interface or use ws://neata.local:81 for a direct websocket connection. Alternatively you can check your router IP page to find out the IP address of the bot or use a tool like nmao (https://nmap.org/).

# Linking your device to neato scheduler
* Step 1: Login and navigate to "my profile"
* Step 2: Click "add device" and input your device information (serial number, model, location)
* Step 3: Click "add device"

For convenience and safety purposes, "TestMode off" is always send to the bot if a user disconnects.

### Websocket
The websocket server is listening on port 81. Everything that you send via the websocket connection is directly sent to the serial interface and the results from the serial interface are returned via the websocket connection (see also limitations).

### HTML Interface
The software also includes a simple HTML interface for the websocket connection. Connect with a browser to the bot to see it. The websocket connection is automatically established in the background. Type a command and press enter to send the command. The results are automatically displayed. You can disconnect by typing /disconnect or clear the screen by typing /clear.

### Command Syntax
Send help to the bot to see an overview of all commands or send help [command name] to get help for a specific command. Neato also offers a programmers manual: https://www.neatorobotics.com/resources/programmersmanual_20140305.pdf

## Known Issues and Limitations
* Only one user can be connected at the same time. If a new user connects, the old user is disconnected. This is intentional and by design to prevent issues with the bot and data corruption with multiple users.
* The websocket responses contain only ASCII characters. All special characters get replaced by a underscore. This is only relevant for the text of non-english error messages. The error code is transmitted unchanged. This a limitation of the microcontroller/IDE.
* ~~Responses that are to big to fit into a single TCP packet are split up into multiple websocket messages. Always check for the response for the end of file marker (ctrl-z; \x1A). This a workaround for a possible bug in the network APIs (see also https://github.com/Links2004/arduinoWebSockets/issues/85).~~ **Resolved in latest version**

## Updating ESP8266 firmware
* Step 1: Navigate to https://www.neatoscheduler.com and log in
* Step 2: Navigate to "my profile"
* Step 3: Click the button labeled as "Latest Firmware" and the firmware will download
* Step 4: Navigate to http://neato.local:82/updateServer
* Step 5: Choose and upload your downloaded firmware in Step 3
* Step 6: Click the button labeled as "Update" and the firmware will upload
* Step 7: Wait until ESP comes back online

**The (sadly) necessary disclaimer:**

THE SOFTWARE AND INFORMATION IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE, INFORMATION OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
