/** @file
    WIFIOnOFF is an alternative software for the Sonoff S20 which provides
    a web user interface (HTTP) and an internet of things interface (MQTT).
    The device can still be controlled by the normal user button.
    The connection to WiFi will be established by WiFiManager.
    This device can be used in the local network
    without any dependency of a cloud.

    Copyright (C) 2019 Siegfried Schöfer

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    @see http://mqtt.org/
    @see http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.pdf
    @see https://github.com/esp8266/Arduino
    @see https://arduino-esp8266.readthedocs.io/en/latest/
    @see https://media.readthedocs.org/pdf/arduino-esp8266/latest/arduino-esp8266.pdf
    @see https://github.com/espressif/ESP8266_NONOS_SDK
*/

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <MQTTClient.h>
#include "wifionoff_version.h"
#include "WiFiManager/WiFiManager.h"

/**
   @brief Version number - obtained by 'git describe'
*/
String VERSION = VERSION_BY_GIT_DESCRIBE;
/**
   @brief Contains a link to the code repository which is embedded in the rendered HTML files in the function renderFooter().
*/
String REPOSITORY_URL_STRING = "https://github.com/peastone/WIFIOnOff";

/**
   @brief Define SERIAL_PRINTING if you want to enable serial communication
*/
#define SERIAL_PRINTING

/**
   @brief Define NUMERIC_RESPONSE if you want the @link mqttClient @endlink to publish "1" or "0" instead of "on" or "off"
*/
#define NUMERIC_RESPONSE

#ifdef _DOXYGEN_ // only used for Doxygen to recognize and document the #define without setting it in the code
/**
   @brief The state of the relay is visible through the blue LED (connected == LED shines).
          Define OUTPUT_RELAY_STATE_ON_GREEN_STATUS_LED,
          if you want the state of the relay being visible also on the green LED (connected == LED shines).
*/
#define OUTPUT_RELAY_STATE_ON_GREEN_STATUS_LED
#endif

/**
  @brief Define DEV_OTA_UPDATES if you want to perform OTA updates for development with Arduino IDE.
*/
#define DEV_OTA_UPDATES
#ifdef DEV_OTA_UPDATES
#include <ArduinoOTA.h>
#endif

/**
   @brief time in ms to wait until factory reset is triggered when pressing the button (see pressHandler())
*/
#define TRIGGER_TIME_FACTORY_RESET 15000

/**
   @brief standard port used for the HTTP protocol
*/
#define HTTP_PORT 80

/**
   @brief standard port used for the MQTT protocol
*/
#define MQTT_PORT 1883

/**
   @brief definition of the timespan of half a second
*/
#define TIME_HALF_A_SECOND 500

/**
   @brief definition of the timespan of a quarter of a second
*/
#define TIME_QUARTER_SECOND 250

/**
   @brief definition of the time the LED stays on in case of an EEPROM warning (CRC failure, initialization, EEPROM incompatible)
*/
#define TIME_EEPROM_WARNING 7000

/**
   @brief maximum amount of tries to connect to WIFI
*/
#define CTR_MAX_TRIES_WIFI_CONNECTION 60

/**
   @brief Global variable to store the Arduino OTA password.
*/
String arduinoOtaPassword = "";

////////////////////////////////////////////////////////////////////////////////
// EEPROM //////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief EEPROM_length defines the maximum amount of bytes to be stored
        in the EEPROM.
        At the time of writing, the maximum amout of bytes to be stored
        is 4096.
        @see https://git.io/vxYHO <!-- URL too long for Latex PDF, original URL: https://github.com/esp8266/Arduino/blob/3ce888e87b6f2a99f6a1bf0ad0baa7cd1d3dbf70/libraries/EEPROM/EEPROM.cpp#L54 -->
        @see https://git.io/vxYHG <!-- URL too long for Latex PDF, original URL: https://github.com/esp8266/Arduino/blob/a2d16f38d48727bba41befadebc590bdd84588ad/tools/sdk/include/spi_flash.h#L47 -->
*/
#define EEPROM_length 4096
/**
   @brief A CRC is calculated for the EEPROM values to ensure that
          the values are untempered.
          The address of the stored CRC is at the beginning of the EEPROM.
*/
#define EEPROM_version_number 1
/**
   @brief This version number is stored in EEPROM. It must not be longer than one byte.
          The version number defined here and that one stored in EEPROM are compared against each other.
          In case the two numbers are not equal, the EEPROM is re-/initialized.
          This number must change, if you change the EEPROM layout, basically if you change adresses.
*/
#define EEPROM_version_number_address 0
/**
   @brief The EEPROM version number is stored at this address.
*/
#define EEPROM_address_CRC 1
/**
   @brief A CRC is calculated for the EEPROM values to ensure that
          the values are untempered.
          The size of the stored CRC is defined by this macro.
*/
#define EEPROM_size_CRC (sizeof(unsigned long))
/**
   @brief A CRC is calculated for the EEPROM values to ensure that
          the values are untempered.
          The CRC takes some space to be stored in the EEPROM.
          The start address takes this into account.
*/
#define EEPROM_address_start (EEPROM_address_CRC + EEPROM_size_CRC)
/**
   @brief If this magic number is set, a defined functionality is enabled.
*/
#define EEPROM_enabled 0xEB
/**
   @brief The amount of storage needed by @link EEPROM_enabled @endlink in EEPROM.
*/
#define EEPROM_enabled_size 1
/**
   @brief If an init value is set, a defined functionality is disabled.
          The init value is also used to initialize the EEPROM in setupEEPROM().
*/
#define EEPROM_init_value 0x00

/**
   @brief This flag is used to check whether @link mqttServer @endlink has already been
          initialized with the value of the MQTT-Server (DNS name or IP)
*/
#define EEPROM_address_MQTT_server_configured EEPROM_address_start
/**
   @brief The variable @link mqttServer @endlink
          which contains the value of the
          MQTT-Server (DNS name or IP) is restored after startup
          from EEPROM, if MQTT is configured (@link EEPROM_address_MQTT_server_configured @endlink).
          The stored bytes can be found at this address.
*/
#define EEPROM_address_MQTT_server (EEPROM_address_MQTT_server_configured + EEPROM_enabled_size)
/**
   @brief The maximum amount of bytes used to store the MQTT server address.
          This address can be either an IP or a DNS name.
          DNS names are restricted to 255 octets.
          As a string is zero-terminated, one byte more is used.
          For more information, look at RFC 1035
          @see https://www.ietf.org/rfc/rfc1035.txt
*/
#define EEPROM_size_MQTT_server 256

/**
   @brief The OTA password is stored here.
*/
#define EEPROM_address_OTA_password (EEPROM_address_MQTT_server + EEPROM_size_MQTT_server)

/**
  @brief Define length for OTA password.
*/
#define EEPROM_size_OTA_password 256

/**
   @brief This function calculates a CRC checksum for the EEPROM.
          It is taken from: https://www.arduino.cc/en/Tutorial/EEPROMCrc
   @return calculated CRC
   @callergraph
   @callgraph
*/
unsigned long calculate_crc() {
#ifdef SERIAL_PRINTING
  Serial.println("Calculate CRC");
#endif
  const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };
  unsigned long crc = ~0L;
  for (unsigned int i = EEPROM_address_start; i < EEPROM.length(); i++)  {
    crc = crc_table[(crc ^ EEPROM.read(i)) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (EEPROM.read(i) >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc;
}

/**
   @brief This function calculates the CRC value of the values in the
          EEPROM with the help of function calculate_crc().
          The resulting CRC checksum is written to @link EEPROM_address_CRC @endlink.
          The caller must still call EEPROM.commit() afterwards to really
          trigger a write to EEPROM.
   @callergraph
   @callgraph
*/
void storeCRC() {
#ifdef SERIAL_PRINTING
  Serial.println("Store CRC");
#endif
  unsigned long crc = calculate_crc();
  uint8_t * p2crc = (uint8_t *) &crc;
  int j = 0;
  for (unsigned int i = EEPROM_address_CRC; i < EEPROM_address_start; i++) {
    EEPROM.write(i, *(p2crc + j));
    j++;
  }
}

/**
   @brief This function reads the CRC value stored at @link EEPROM_address_CRC @endlink.
   @returns CRC value from EEPROM
   @callergraph
   @callgraph
*/
unsigned long retrieveCRC() {
  unsigned long crc;
  uint8_t * p2crc = (uint8_t *) &crc;
  int j = 0;
  for (unsigned int i = EEPROM_address_CRC; i < EEPROM_address_start; i++) {
    *(p2crc + j) = EEPROM.read(i);
    j++;
  }
  return crc;
}

/**
   @brief This function calculates the CRC value for the bytes stored in the EEPROM
          with the help of function calculate_crc() and compares the result to the
          CRC which was read from EEPROM with the help of function retrieveCRC().
   @returns true if calculated CRC matches stored CRC
   @callergraph
   @callgraph
*/
bool checkCRC() {
#ifdef SERIAL_PRINTING
  Serial.println("Check CRC");
#endif
  return (calculate_crc() == retrieveCRC());
}

/**
   @brief This function stores the @link EEPROM_version_number @endlink in EEPROM.
   @callergraph
   @callgraph
*/
void storeEEPROMVersionNumber() {
#ifdef SERIAL_PRINTING
  Serial.println("Store EEPROM version number");
#endif
  EEPROM.write(EEPROM_version_number_address, EEPROM_version_number);
}

/**
   @brief This function checks the @link EEPROM_version_number @endlink stored in EEPROM against the version number required by the latest EEPROM layout.
   @returns true, if the stored and the required EEPROM version number match, false otherwise
   @callergraph
   @callgraph
*/
bool checkEEPROMVersionNumber() {
#ifdef SERIAL_PRINTING
  Serial.println("Check EEPROM version number");
#endif
  return (EEPROM.read(EEPROM_version_number_address) == EEPROM_version_number);
}

/**
   @brief This function writes default data (@link EEPROM_init_value @endlink) to EEPROM and
          stores CRC and the @link EEPROM_version_number @endlink.
   @callergraph
   @callgraph
*/
void initEEPROM() {
#ifdef SERIAL_PRINTING
  Serial.println("Init EEPROM: Write default data");
#endif
  for (int i = 0; i < EEPROM_length; i++) {
    EEPROM.write(i, EEPROM_init_value);
  }
  storeEEPROMVersionNumber();

  arduinoOtaPassword = getDefaultOtaPassword();
  // storeCRC() and EEPROM.commit() will be done in this function
  saveOtaPasswordToEEPROM();
}

/**
   @brief This function is setting up the EEPROM.
          If the CRC check fails, the EEPROM will be overwritten with
          init values. The CRC check makes only sense at initialization phase.
          Afterwards, the data is buffered by EEPROM lib in RAM.
   @see https://git.io/vxOPf <!-- https://github.com/esp8266/Arduino/blob/master/libraries/EEPROM/EEPROM.cpp#L70 -->
   @callergraph
   @callgraph
*/
void setupEEPROM() {
#ifdef SERIAL_PRINTING
  Serial.println("Setup EEPROM");
#endif
  EEPROM.begin(EEPROM_length);
  // reinitialize EEPROM if CRC check or EEPROM version number check fail
  if (!checkCRC() || !checkEEPROMVersionNumber()) {
#ifdef SERIAL_PRINTING
    if (!checkCRC()) {
      Serial.println("CRC check failed");
    }
    if (!checkEEPROMVersionNumber()) {
      Serial.println("EEPROM version number check failed");
    }
#endif
    initEEPROM();
#ifdef SERIAL_PRINTING
    Serial.println("EEPROM reinitialized");
#endif
  }
}

////////////////////////////////////////////////////////////////////////////////
// LED /////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief Constant to map the green LED of the Sonoff S20.
*/
const int pinLED = 13;

/**
   @brief State to track whether the LED is on.
*/
bool stateLEDOn = false;

/**
   @brief This function is used to switch the LED off. It keeps track of the internal state @link stateLEDOn @endlink.
   @callergraph
   @callgraph
*/
void switchOffLED() {
  stateLEDOn = false;
  digitalWrite(pinLED, HIGH);
}

/**
   @brief This function is used to switch the LED on. It keeps track of the internal state @link stateLEDOn @endlink.
   @callergraph
   @callgraph
*/
void switchOnLED() {
  stateLEDOn = true;
  digitalWrite(pinLED, LOW);
}

/**
   @brief This function is used to toggle the LED. Indirectly, it keeps track of the internal state @link stateLEDOn @endlink.
   @callergraph
   @callgraph
*/
void toggleLED() {
  if (getStateLEDOn()) {
    switchOffLED();
  } else {
    switchOnLED();
  }
}

/**
   @brief This function is used to get the state of the LED. It returns @link stateLEDOn @endlink.
   @returns true, if the LED is on, false otherwise.
   @callergraph
   @callgraph
*/
bool getStateLEDOn() {
  return stateLEDOn;
}

////////////////////////////////////////////////////////////////////////////////
// LED feedback ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
   @brief This function is called to give the user feedback about the menu the user selected.
          It will blink fast for a short period of time. The user has the choice to release and select
          the menu or to wait for the next menu. By counting the feedbackQuickBlink()-events,
          the user can determine which menu is selected, when the button is released now.
   @callergraph
   @callgraph
*/
void feedbackQuickBlink() {
#ifdef SERIAL_PRINTING
  Serial.println("Blink quickly to signal menu selection");
#endif
  toggleLED();
  delay(TIME_QUARTER_SECOND);
  toggleLED();
  delay(TIME_QUARTER_SECOND);
  toggleLED();
  delay(TIME_QUARTER_SECOND);
  toggleLED();
  // the request has been fulfilled
  unsetUserActionFeedbackRequest();
}

/**
   @brief State to track whether the user should be notified that a menu can be selected by releasing the button.
*/
bool userActionFeedbackRequest = false;

/**
   @brief Getter function for @link userActionFeedbackRequest @endlink.
   @returns true, if the user should be notified that a menu can be selected, false otherwise
   @callergraph
   @callgraph
*/
bool getUserActionFeedbackRequest() {
  return userActionFeedbackRequest;
}

/**
   @brief Setter function for @link userActionFeedbackRequest @endlink. The user should be notified that a menu can be selected.
   @callergraph
   @callgraph
*/
void setUserActionFeedbackRequest() {
  userActionFeedbackRequest = true;
}

/**
   @brief Setter function for @link userActionFeedbackRequest @endlink. The user has been notified that a menu can be selected.
   @callergraph
   @callgraph
*/
void unsetUserActionFeedbackRequest() {
  userActionFeedbackRequest = false;
}

////////////////////////////////////////////////////////////////////////////////
// WiFi ////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief Necessary to initialize a MQTTClient object.
*/
WiFiClient wifiClient;

/**
   @brief This function returns the client name used for MQTT, see mqttConnect().
   @returns the client ID: "WIFIOnOff" + MAC address
   @callergraph
   @callgraph
*/
String getClientID() {
  return "WIFIOnOff_" + WiFi.macAddress();
}

/*
   @brief This function returns the default OTA password
   @returns the default OTA password
   @callergraph
   @callgraph
*/
String getDefaultOtaPassword() {
  return "ArduinoOTA" + String(ESP.getChipId());;
}

/**
  @brief Password to protect OTA updates. This password should be changed in WiFiManager.
  @see https://media.readthedocs.org/pdf/arduino-esp8266/latest/arduino-esp8266.pdf
*/
WiFiManagerParameter paramOtaPassword(
  "ota",
  "Arduino OTA Password",
  getDefaultOtaPassword().c_str(),
  EEPROM_size_OTA_password-1,
  ""
);

/**
   @brief This function sets up MDNS.
   @callergraph
   @callgraph
*/
void setupMDNS() {
  String mdnsID = getClientID() + ".local";
#ifdef SERIAL_PRINTING
  Serial.println("Setup MDNS on '" + mdnsID + "': http, mqtt");
#endif
  MDNS.begin(mdnsID.c_str());
  MDNS.addService("http", "tcp", HTTP_PORT);
  MDNS.addService("mqtt", "tcp", MQTT_PORT);
}

/**
   @brief Global variable SSID for WiFi credentials change.
*/
String credentialSsid;

/**
   @brief Global variable SSID for WiFi credentials change.
*/
String credentialPassword;

/** This function changes the WiFi credentials.
   @callergraph
   @callgraph
*/
void changeWiFiCredentials() {
#ifdef SERIAL_PRINTING
  Serial.println("Change WiFi credentials");
#endif
  // disconnect also deletes the WiFi credentials
  // https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFiSTA.cpp#L296
  WiFi.disconnect();
  WiFi.begin(credentialSsid.c_str(), credentialPassword.c_str());

  reboot();
}

/**
   @brief This function checks whether WIFI is connected.
   @returns true, if WiFi is connected, false otherwise
   @callergraph
   @callgraph
*/
bool checkWiFiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}
/**
   @brief State to track whether a change of WiFi credentials has been requested.
*/
bool wifiCredentialChangeRequested = false;

/**
   @brief Getter function for @link wifiCredentialChangeRequested @endlink.
   @returns true, if WiFiManager has been requested, false otherwise.
*/
bool getWiFiCredentialChangeRequested() {
  return wifiCredentialChangeRequested;
}

/**
   @brief Setter function to set @link wifiCredentialChangeRequested @endlink.
   @callergraph
   @callgraph
*/
void setWiFiCredentialChangeRequested() {
  wifiCredentialChangeRequested = true;
}

////////////////////////////////////////////////////////////////////////////////
// Relay ///////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief State to track whether the LED is connected to the mains.
*/
bool stateRelayConnected = false;

/**
   @brief Constant to map the relay of the Sonoff S20.
*/
const int pinRelay = 12;

/**
   @brief This function is used to disconnect the relay from the mains. It keeps track of the internal state @link stateRelayConnected @endlink.
   @callergraph
   @callgraph
*/
void disconnectRelay() {
#ifdef SERIAL_PRINTING
  Serial.println("Disconnect relay");
#endif
  stateRelayConnected = false;
#ifdef OUTPUT_RELAY_STATE_ON_GREEN_STATUS_LED
  switchOffLED();
#endif
  digitalWrite(pinRelay, LOW);
  mqttPublish();
}

/**
   @brief This function is used to connect the relay to the mains. It keeps track of the internal state @link stateRelayConnected @endlink.
   @callergraph
   @callgraph
*/
void connectRelay() {
#ifdef SERIAL_PRINTING
  Serial.println("Connect relay");
#endif
  stateRelayConnected = true;
#ifdef OUTPUT_RELAY_STATE_ON_GREEN_STATUS_LED
  switchOnLED();
#endif
  digitalWrite(pinRelay, HIGH);
  mqttPublish();
}

/**
   @brief This function is used to get the connection state of the relay with the mains. It returns @link stateRelayConnected @endlink.
   @returns true, if the relay is connected to the mains, false otherwise.
   @callergraph
   @callgraph
*/
bool getStateRelayConnected() {
  return stateRelayConnected;
}

/**
   @brief This function is used to toggle the connection of the relay with the mains. Indirectly, it keeps track of the internal state @link stateRelayConnected @endlink.
   @callergraph
   @callgraph
*/
void toggleRelay() {
  if (getStateRelayConnected()) {
    disconnectRelay();
  } else {
    connectRelay();
  }
}
////////////////////////////////////////////////////////////////////////////////
// GPIO configuration //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief This function configures the output pins of the microcontroller.
   @callergraph
   @callgraph
*/
void configureOutputs() {
#ifdef SERIAL_PRINTING
  Serial.println("Configure outputs");
#endif
  pinMode(pinLED, OUTPUT);
  pinMode(pinRelay, OUTPUT);
}

////////////////////////////////////////////////////////////////////////////////
// HTML rendering //////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief This function returns the first part of a HTML file which is reused for
          all responses.
   @returns "<!doctype> ... <body>", look inside the function for more information.
   @see http://www.html.am/templates/css-templates/
   @callergraph
   @callgraph
*/
String renderHeader() {
  return R"(<!doctype html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=0" />
<meta charset="utf-8" />
<title>WIFIOnOff</title>
<style>
  body { background-color: white; font-family: Arial, Helvetica, Sans-Serif; Color: black; }
  h1 {color: white;}
  h2 {color: darkorange;}
  .headline { background:orange;padding:18px;}
  .footer { margin:auto;text-align:center;padding:12px; }
  .footer a { color:orange;text-decoration:none;}
</style>
</head>
<body>)";
}

/**
   @brief This function returns the middle part of a HTML file to
          - show the state of the relay (disconnected - off / connected - on).
          - manipulate the state of the relay.
   @param[in] stateRelayConnected if true, the relay is displayed as on, otherwise as off
   @returns HTML, look inside the function for more information.
   @callergraph
   @callgraph
*/
String renderRelay(bool stateRelayConnected) {
  return R"(
<div class="headline">
  <h1>WIFIOnOff Control</h1>
</div>
<p>
<form action="/" method="post">
  State of relay:
  <p>
  <select name="relay">
    <option value="on")" + String(stateRelayConnected ? " selected" : "") + R"(>On</option>
    <option value="off")" + String(stateRelayConnected ? "" : " selected") + R"(>Off</option>
  </select>
  <p>
  <input type="submit" value="Change state">
</form>)";
}

/**
   @brief This function returns the middle part of a HTML file to
          - show the settings for the MQTT server
          - manipulate the settings for the MQTT server
          This function could be relevant concerning XSS.
   @param[in] storedServerName DNS name or IP address which is displayed on the HTML site
   @param[in] failureMsg message that is displayed to the user in case of success or failure
   @param[in] stateMQTTActivated shows the user, whether MQTT is activated
   @param[in] stateMQTTConnected shows the user, whether MQTT is connected
   @returns HTML, look inside the function for more information.
   @see https://www.owasp.org/index.php/XSS_(Cross_Site_Scripting)_Prevention_Cheat_Sheet
   @callergraph
   @callgraph
*/
String renderMQTTSettings(String storedServerName, String failureMsg, bool stateMQTTActivated, bool stateMQTTConnected) {
  return R"(
<div class="headline">
  <h1>WIFIOnOff MQTT Settings</h1>
</div>
<p>
<form action="/mqtt.html" method="post">
  MQTT server (DNS name or IP address required):
  <p>
  <input type="text" name="mqttserver" value=")" + storedServerName + R"(">
  <p>
  MQTT state:
  <select name="mqttState">
    <option value="on")" + String(stateMQTTActivated ? " selected" : "") + R"(>On</option>
    <option value="off")" + String(stateMQTTActivated ? "" : " selected") + R"(>Off</option>
  </select>
  <p>
  <input type="submit" value="Change MQTT settings">
</form>
<p>)" + String(stateMQTTConnected ? "MQTT server / broker is connected" : "MQTT server / broker is disconnected") + R"(
<p>)" + failureMsg;
}

/**
   @brief This function returns the middle part of a HTML file to
          - change the OTA password
   @param[in] infoMsg signal whether the change of the OTA password was successful or not
   @callergraph
   @callgraph
*/
String renderOtaPasswordChangeForm(String infoMsg) {
  return R"(
<div class="headline">
  <h1>WIFIOnOff OTA Password</h1>
</div>
<p>
<form action="/ota.html" method="post">
  Old OTA password:
  <p>
  <input type="password" name="oldOta">
  <p>
  New OTA password:
  <p>
  <input type="password" name="newOta">
  <p>
  New OTA password repeated:
  <p>
  <input type="password" name="newOtaRep">
  <p>
  <input type="submit" value="Change OTA password">
</form>
<p>)" + infoMsg;
}

/**
   @brief This function returns the middle part of a HTML file to
          - perform a factory reset
   @param[in] infoMsg signal whether the factory reset will be performed
   @callergraph
   @callgraph
*/
String renderFactoryReset(String infoMsg) {
  return R"(
<div class="headline">
  <h1>WIFIOnOff Factory Reset</h1>
</div>
<p>
<form action="/reset.html" method="post">
  Please confirm this action with the OTA password:
  <p>
  <input type="password" name="ota">
  <p>
  <input type="submit" value="Perform factory reset">
</form>
<p>)" + infoMsg;
}

/**
   @brief This function returns the middle part of a HTML file to
          - changeWiFiCredentials
   @param[in] infoMsg signal whether the change of WiFiCredentials will be performed.
   @callergraph
   @callgraph
*/
String renderWiFiCredentialChange(String infoMsg) {
  return R"(
<div class="headline">
  <h1>WIFIOnOff WiFi Credentials</h1>
</div>
<p>
<form action="/wifi.html" method="post">
  Please confirm this action with the OTA password:
  <p>
  <input type="password" name="ota">
  <p>
  SSID:
  <p>
  <input type="text" name="ssid">
  <p>
  WiFi Password:
  <p>
  <input type="password" name="wifiPassword">
  <p>
  WiFi Password Repetition:
  <p>
  <input type="password" name="wifiPasswordRepetition">
  <p>
  <input type="submit" value="Change WiFi credentials">
</form>
<p>)" + infoMsg;
}

/**
   @brief This function returns the middle part of a HTML file to
          - show the version of the WIFIOnOff
          - show the default OTA PW
   @returns HTML, look inside the function for more information.
   @callergraph
   @callgraph
*/
String renderInfo() {
  return R"(
<div class="headline">
  <h1>WIFIOnOff Info </h1>
</div>
<p>
WIFIOnOff version: <p>)" + VERSION + R"(
<p>
Default OTA PW: <p>)" + getDefaultOtaPassword();
}

/**
   @brief This function returns the last part of a HTML file which is reused for
          all responses.
   @returns "</body></html>", look inside the function for more information.
   @callergraph
   @callgraph
*/
String renderFooter() {
  return R"(
<p>
<div class="footer">
<a href="/"> Control </a> &#124; <a href="/mqtt.html"> MQTT </a> &#124; <a href="/ota.html"> OTA </a> &#124; <a href="/wifi.html"> WiFi </a> &#124; <a href="/reset.html"> Factory Reset </a> &#124; <a href="/info.html"> Info </a> &#124; <a href=")" + REPOSITORY_URL_STRING + R"("> Project website </a>
</div>
</body>
</html>)";
}

////////////////////////////////////////////////////////////////////////////////
// MQTT ////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief central object to manage MQTT
   @see MQTT protocol http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.pdf
*/
MQTTClient mqttClient;

/**
   @brief Callback which is called when MQTT receives an incoming topic
   @param[in] topic One of the incoming MQTT topics, which the @link mqttClient @endlink
          has been subscribed to. Currently there is only one connection, see mqttConnect().
          So this parameter is irrelevant.
   @param[in] payload Contains the received payload of the message of the incoming topic.
          If "on" or "1" is received, the relay is connected (connectRelay()),
          else if "off" or "0" is received, the relay is disconnected (disconnectRelay()).
   @callergraph
   @callgraph
*/
void mqttControlRelay(String &topic, String &payload) {
#ifdef SERIAL_PRINTING
  Serial.println("MQTT callback, incoming topic: '" + topic + "', payload: '" + payload + "'");
#endif
  if ((payload == "1") || (payload == "on")) {
    connectRelay();
  } else if ((payload == "0") || (payload == "off")) {
    disconnectRelay();
  }
}

/**
   @brief Global variable to store the DNS name or IP address of the MQTT broker.
*/
String mqttServer = "";

/**
   @brief State to track whether MQTT is configured.
*/
bool stateMQTTConfigured = false;

/**
   @brief Read back MQTT server / broker name from EEPROM and store it
          in global variable @link mqttServer @endlink.
          Read back whether MQTT is configured and store it in global variable @link stateMQTTConfigured @endlink.
   @callergraph
   @callgraph
*/
void restoreMQTTConfigurationFromEEPROM() {
#ifdef SERIAL_PRINTING
  Serial.println("Restore MQTT configuration from EEPROM");
#endif
  // Readback mqttServer
  char mqttServerLocal[EEPROM_size_MQTT_server];
  for (int i = 0; i < EEPROM_size_MQTT_server; i++) {
    mqttServerLocal[i] = EEPROM.read(EEPROM_address_MQTT_server + i);
  }
  setMQTTServer(mqttServerLocal);

  // Readback stateMQTTConfigured
  setStateMQTTConfigured(EEPROM.read(EEPROM_address_MQTT_server_configured) == EEPROM_enabled);
}

/**
   @brief Store MQTT server / broker name (@link mqttServer @endlink) and state (configured or not, @link stateMQTTConfigured @endlink) in EEPROM.
   @callergraph
   @callgraph
*/
void saveMQTTConfigurationToEEPROM() {
  const char * mqttServerLocal = mqttServer.c_str();
  EEPROM.write(EEPROM_address_MQTT_server_configured, (stateMQTTConfigured ? EEPROM_enabled : EEPROM_init_value));
  for (int i = 0; i < EEPROM_size_MQTT_server; i++) {
    EEPROM.write(EEPROM_address_MQTT_server + i, mqttServerLocal[i]);
  }
  storeCRC();
  EEPROM.commit();
}

/**
   @brief Configure new MQTT server / broker.
          This will
          - disconnect the MQTT client if connected
          - set the server / broker name
          - set the callback mqttConrolRelay()
   @callergraph
   @callgraph
*/
void configureMQTT() {
#ifdef SERIAL_PRINTING
  Serial.println("Configure MQTT");
#endif
  mqttClient.disconnect();
  mqttClient.begin(mqttServer.c_str(), wifiClient);
  mqttClient.onMessage(mqttControlRelay);
}

/**
   @brief Setter function for MQTT server / broker @link mqttServer @endlink.
   @param[in] input MQTT server / broker
   @callergraph
   @callgraph
*/
void setMQTTServer(String input) {
  mqttServer = input;
#ifdef SERIAL_PRINTING
  Serial.println("mqttServer='" + mqttServer + "'");
#endif
}

/**
   @brief Getter function for MQTT server / broker @link mqttServer @endlink.
   @returns the latest MQTT server / broker
   @callergraph
   @callgraph
*/
String getMQTTServer() {
  return mqttServer;
}

/**
   @brief Setter function for @link stateMQTTConfigured @endlink.
   @param[in] input true if MQTT is configured, false otherwise
   @callergraph
   @callgraph
*/
void setStateMQTTConfigured(bool input) {
  stateMQTTConfigured = input;
#ifdef SERIAL_PRINTING
  Serial.println("stateMQTTConfigured=" + (stateMQTTConfigured ? String("true") : String("false")));
#endif
}

/**
   @brief Getter function for @link stateMQTTConfigured @endlink.
   @returns true if MQTT is configured, false otherwise
   @callergraph
   @callgraph
*/
bool getStateMQTTConfigured() {
  return stateMQTTConfigured;
}

/**
   @brief The topic @link mqttClient @endlink publishes.
*/
String mqttOutgoingTopic;

/**
   @brief Setter function for @link mqttOutgoingTopic @endlink.
   @param[in] input String to be set
   @callergraph
   @callgraph
*/
void setMQTTOutgoingTopic(String input) {
  mqttOutgoingTopic = input;
}

/**
   @brief Getter function for @link mqttOutgoingTopic @endlink.
   @returns String of outgoing MQTT topic
   @callergraph
   @callgraph
*/
String getMQTTOutgoingTopic() {
  return mqttOutgoingTopic;
}

/**
   @brief The topic which @link mqttClient @endlink subscribes.
*/
String mqttIncomingTopic;

/**
   @brief Setter function for @link mqttIncomingTopic @endlink.
   @param[in] input String to be set
   @callergraph
   @callgraph
*/
void setMQTTIncomingTopic(String input) {
  mqttIncomingTopic = input;
}

/**
   @brief Get MQTT connection state.
   @returns true, if MQTT is connected, false otherwise
   @callergraph
   @callgraph
*/
bool checkMQTTConnected() {
  return mqttClient.connected();
}

/**
   @brief This function connects to broker and
          - subscribes topic wifionoff/getClientID()/set
          - sets last will "disconnected" on wifionoff/getClientID()/get
          - publishes latest state on wifionoff/getClientID()/get
   @callergraph
   @callgraph
*/
void mqttConnect() {
#ifdef SERIAL_PRINTING
  Serial.println("MQTT: Connect to broker");
#endif
  // MQTT needs a unique client ID
  String clientID = getClientID();
  if (mqttClient.connect(clientID.c_str())) {
#ifdef SERIAL_PRINTING
    Serial.println("MQTT: Connected as client: '" + clientID + "'");
#endif
    // subscribe
#ifdef SERIAL_PRINTING
    Serial.println("MQTT: Subscribe");
#endif
    setMQTTIncomingTopic("wifionoff/" + clientID + "/set");
    mqttClient.subscribe(mqttIncomingTopic);

    // set last will
#ifdef SERIAL_PRINTING
    Serial.println("MQTT: Set last will");
#endif
    setMQTTOutgoingTopic("wifionoff/" + clientID + "/get");
    mqttClient.setWill(mqttOutgoingTopic.c_str(), "disconnected");

    // publish state
    mqttPublish();
  } else {
#ifdef SERIAL_PRINTING
    Serial.println("MQTT: connection failed");
#endif
  }
}

/**
   @brief With the call of this function, @link mqttClient @endlink
          publishes the state of the relay (getStateRelayConnected()) on
          the outgoing topic (getMQTTOutgoingTopic()) to the MQTT server / broker.
          The macro @link NUMERIC_RESPONSE @endlink is taken into account.
   @callergraph
   @callgraph
*/
void mqttPublish() {
  if (checkMQTTConnected()) {
    if (getStateRelayConnected()) {
#ifdef SERIAL_PRINTING
      Serial.println("MQTT: Publish 'on' or '1'");
#endif
#ifdef NUMERIC_RESPONSE
      mqttClient.publish(getMQTTOutgoingTopic(), "1");
#else
      mqttClient.publish(getMQTTOutgoingTopic(), "on");
#endif
    } else {
#ifdef SERIAL_PRINTING
      Serial.println("MQTT: Publish: 'off' or '0'");
#endif
#ifdef NUMERIC_RESPONSE
      mqttClient.publish(getMQTTOutgoingTopic(), "0");
#else
      mqttClient.publish(getMQTTOutgoingTopic(), "off");
#endif
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Webserver ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief This is the webserver object. It is used to serve the user interface
          over HTTP. Per default, port 80 is used.
*/
ESP8266WebServer webserver;

/**
   @brief This function is used for filtering out XSS attacks.
          This is done by whitelisting.
          - IPv4 addresses consist of numbers and dots (0..9 | .).
          - IPv6 addresses consist of hexadeximal numbers, double dots or brackets (0..9 | a..f | A..F | [ | ] )
          - DNS names consist of letters, digits and hypen (0..9 | a..z | A..Z | - ).
          This function is not validating DNS, IPv4, IPv6. Rubbish DNS names can still pass.
          But characters which could be used for XSS, like <, >, &, " are blocked.
   @param[in] c character to check
   @see https://www.ietf.org/rfc/rfc1035.txt
   @see https://wonko.com/post/html-escaping
   @see https://www.owasp.org/index.php/XSS_(Cross_Site_Scripting)_Prevention_Cheat_Sheet
   @returns true, if the argument character is not whitelisted, false otherwise.
   @callergraph
   @callgraph
*/
bool isNotWhitelisted(char c) {
  if (('a' <= c) && (c <= 'z')) {
    return false;
  }
  if (('A' <= c) && (c <= 'Z')) {
    return false;
  }
  if (('0' <= c) && (c <= '9')) {
    return false;
  }
  switch (c) {
    case '-':
    case '.':
    case ':':
    case '[':
    case ']':
      return false;
    default:
      return true;
  }
}

/**
   @brief This function is used to check whether mqttServer is valid.
          Therefore, it is checked that the
          - length of mqttServer is less or equal than @link EEPROM_size_MQTT_server @endlink
          - all characters are whitelisted (see isNotWhitelisted())
   @param[in] mqttServer String to check for validity
   @returns true, if mqttServer is valid, false otherwise.
   @callergraph
   @callgraph
*/
bool mqttServerValid(String mqttServer) {
  if (mqttServer.length() >= EEPROM_size_MQTT_server) {
    return false;
  }
  for (unsigned int i = 0; i < mqttServer.length(); i++) {
    if (isNotWhitelisted(mqttServer.charAt(i))) {
      return false;
    }
  }
  return true;
}

/**
   @brief This function is used configure the webserver.
          The webserver is configured by defining callback functions for handling incoming requests on
          - /
          - /settings.html
   @callergraph
   @callgraph
*/
void configureWebServer() {
#ifdef SERIAL_PRINTING
  Serial.println("Configure webserver");
#endif
  //////////////////////////////////////////////////////////////////////////////
  // / /////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  webserver.on("/", []() {
#ifdef SERIAL_PRINTING
    Serial.println("HTTP: /");
#endif
    // perform action based on post argument
    String userrequest = webserver.arg("relay");
#ifdef SERIAL_PRINTING
    Serial.println("relay: '" + userrequest + "'");
#endif
    if (userrequest == "on") {
      connectRelay();
    } else if (userrequest == "off") {
      disconnectRelay();
    } else {
      // userrequest == "" if "relay" is not in request => do nothing
      // this happens if the user requests "/" the first time
    }
    // generate rendering and send answer back
    String response = renderHeader() + \
                      renderRelay(getStateRelayConnected()) + \
                      renderFooter();
    webserver.send(200, "text/html", response);
  });
  //////////////////////////////////////////////////////////////////////////////
  // /info.html ////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  webserver.on("/info.html", []() {
#ifdef SERIAL_PRINTING
    Serial.println("HTTP: /info.html");
#endif
    // generate rendering and send answer back
    String response = renderHeader() + \
                      renderInfo() + \
                      renderFooter();
    webserver.send(200, "text/html", response);
  });
  //////////////////////////////////////////////////////////////////////////////
  // /mqtt.html ////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  webserver.on("/mqtt.html", []() {
#ifdef SERIAL_PRINTING
    Serial.println("HTTP: /mqtt.html");
#endif
    // query whether MQTT should be activated
    String mqttState = webserver.arg("mqttState");
    bool localStateMQTTConfigured;
#ifdef SERIAL_PRINTING
    Serial.println("mqttState: '" + mqttState + "'");
#endif
    if (mqttState == "on") {
      // enable mqtt
      localStateMQTTConfigured = true;
    } else if (mqttState == "off") {
      // disable mqtt
      localStateMQTTConfigured = false;
    } else {
      // use old value
      localStateMQTTConfigured = getStateMQTTConfigured();
    }
    // query MQTT server
    String localMqttServer = webserver.arg("mqttserver");
#ifdef SERIAL_PRINTING
    Serial.println("mqttServer: '" + localMqttServer + "'");
#endif
    String failureMsg = "";
    if (localMqttServer != "") {
      if (mqttServerValid(localMqttServer)) {
        // localMqttServer is valid => send accpeted
        failureMsg = "Input accepted.";
      } else {
        // localMqttServer is valid => send rejected
        failureMsg = "Input rejected, due to XSS mitigation.";
      }
    } else {
      // localMqttServer == "" if "mqttserver" is not in request => use stored server, no failure message
      // this happens if the user requests "/settings.html" the first time
      localMqttServer = getMQTTServer();
    }

    // configure MQTT if something changed
    if ((localMqttServer != getMQTTServer()) || (localStateMQTTConfigured != getStateMQTTConfigured())) {
      setMQTTServer(localMqttServer);
      setStateMQTTConfigured(localStateMQTTConfigured);
      saveMQTTConfigurationToEEPROM();
      configureMQTT();
    }

    // generate rendering
    String response = renderHeader() + \
                      renderMQTTSettings(localMqttServer, failureMsg, localStateMQTTConfigured, checkMQTTConnected()) + \
                      renderFooter();

    // send answer back
    webserver.send(200, "text/html", response);
  });
  //////////////////////////////////////////////////////////////////////////////
  // /ota.html /////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  webserver.on("/ota.html", []() {
#ifdef SERIAL_PRINTING
    Serial.println("HTTP: /ota.html");
#endif
    // query whether MQTT should be activated
    String oldOta = webserver.arg("oldOta");
#ifdef SERIAL_PRINTING
    Serial.println("Old OTA: " + oldOta);
#endif
    String newOta = webserver.arg("newOta");
#ifdef SERIAL_PRINTING
    Serial.println("New OTA: " + newOta);
#endif
    String newOtaRep = webserver.arg("newOtaRep");
#ifdef SERIAL_PRINTING
    Serial.println("New OTA repeated: " + newOtaRep);
#endif
    bool willReset = false;
    String info = "";
    if (oldOta.equals("") &&
        newOta.equals("") &&
        newOtaRep.equals("")) {
        info = "";
    } else {
      if (oldOta.equals(arduinoOtaPassword)) {
        if (newOta.equals("")) {
          info = "New password may not be empty.";
        } else if (newOta.equals(newOtaRep)) {
          if (newOta.length() >= EEPROM_size_OTA_password) {
            info = "New password too long!";
          } else {
            arduinoOtaPassword = newOta;
            saveOtaPasswordToEEPROM();
            willReset = true;
            info = "Password successfully changed. WIFIOnOff will reset.";
#ifdef SERIAL_PRINTING
    Serial.println("Password will be changed.");
#endif
          }
        } else {
          info = "New password does not match repetition.";
        }
      } else {
        info = "Old OTA password does not match!";
      }
    }

    // generate rendering
    String response = renderHeader() + \
                      renderOtaPasswordChangeForm(info) + \
                      renderFooter();

    // send answer back
    webserver.send(200, "text/html", response);
    if (willReset) {
      reboot();
    }
  });
  //////////////////////////////////////////////////////////////////////////////
  // /reset.html ///////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  webserver.on("/reset.html", []() {
#ifdef SERIAL_PRINTING
    Serial.println("HTTP: /reset.html");
#endif
    String otaPassword = webserver.arg("ota");
    String info = "";

    if (otaPassword.equals(arduinoOtaPassword)) {
      info = "Factory reset will be performed. Device will reset.";
#ifdef SERIAL_PRINTING
    Serial.println("Factory reset will be performed.");
#endif
      setFactoryResetRequested();
    } else if (otaPassword.equals("")) {
      // do nothing (no password inserted)
    } else {
      // wrong password
      info = "OTA password does not match. Factory reset will not be performed.";
    }
    // generate rendering
    String response = renderHeader() + \
                      renderFactoryReset(info) + \
                      renderFooter();

    // send answer back
    webserver.send(200, "text/html", response);
  });
  //////////////////////////////////////////////////////////////////////////////
  // /wifi.html ///////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  webserver.on("/wifi.html", []() {
#ifdef SERIAL_PRINTING
    Serial.println("HTTP: /wifi.html");
#endif
    String otaPassword = webserver.arg("ota");
    String ssid = webserver.arg("ssid");
    String wifiPassword = webserver.arg("wifiPassword");
    String wifiPasswordRepetition = webserver.arg("wifiPasswordRepetition");
    String info = "";

    if (otaPassword.equals(arduinoOtaPassword)) {
      if (ssid.equals("")) {
        info = "SSID is empty!";
      } else {
        if (!wifiPassword.equals(wifiPasswordRepetition)) {
          info = "WiFi passwords do not match!";
        } else {
          if (wifiPassword.equals("")) {
            info = "WiFi password is empty!";
          } else {
#ifdef SERIAL_PRINTING
    Serial.println("WiFi credentials will change.");
#endif
            info = "WiFi credentials will change. Device will reset.";
            credentialPassword = wifiPassword;
            credentialSsid = ssid;
            setWiFiCredentialChangeRequested();
          }
        }
      }
    } else if (otaPassword.equals("")) {
      // do nothing (no password inserted)
    } else {
      // wrong password
      info = "OTA password does not match. WiFi credentials will not change.";
    }
    // generate rendering
    String response = renderHeader() + \
                      renderWiFiCredentialChange(info) + \
                      renderFooter();

    // send answer back
    webserver.send(200, "text/html", response);
  });
  webserver.begin();
#ifdef SERIAL_PRINTING
  Serial.println("HTTP server started");
#endif
}

/**
   @brief This object is a handle for the captive portal / WiFiManager.
*/
WiFiManager wifiManager;

////////////////////////////////////////////////////////////////////////////////
// Factory Reset ///////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief Perform factory reset
          - reset WiFi
          - reset MQTT
          - reset EEPROM
   @callergraph
   @callgraph
*/
void performFactoryReset() {
#ifdef SERIAL_PRINTING
  Serial.println("Perform factory reset");
#endif
  wifiManager.resetSettings();
  setMQTTServer("");
  setStateMQTTConfigured(false);
  initEEPROM();
  reboot();
}

/**
   @brief Perform a reboot.
*/
void reboot() {
#ifdef SERIAL_PRINTING
  Serial.println("Reboot");
#endif
  delay(1000);
  ESP.restart();
}

/**
   @brief State to track whether a factory reset has been requested.
*/
bool factoryResetRequested = false;

/**
   @brief Getter function for @link factoryResetRequested @endlink.
   @returns true, if factory reset is requested, false otherwise.
*/
bool getFactoryResetRequested() {
  return factoryResetRequested;
}

/**
   @brief Setter function to set @link factoryResetRequested @endlink.
   @callergraph
   @callgraph
*/
void setFactoryResetRequested() {
  factoryResetRequested = true;
}

////////////////////////////////////////////////////////////////////////////////
// Button //////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief Constant to map the hardware button of the Sonoff S20.
*/
const int buttonPin = 0;

/**
   @brief State to track whether the button was pressed since pressHandler() was called the last time.
*/
bool buttonLastPressed = false;

/**
   @brief State to track the time since the button was pressed.
*/
unsigned long timeSincebuttonPressed = 0;
/**
   @brief State to track which menu the user selects, if the button is released.
*/
unsigned long selectionState = 0;

/**
   @brief This function triggers
          - the toggling of the relay
          - factory reset request.
          As this function is called regularly with @link ticker @endlink,
          it should be quick, otherwise the ESP8266 might crash.
   @see https://arduino-esp8266.readthedocs.io/en/latest/faq/a02-my-esp-crashes.html
   @callergraph
   @callgraph
*/
void pressHandler() {
  // check whether the button is pressed at the moment
  bool buttonPressed = (digitalRead(buttonPin) == LOW);

  // the button has not been pressed before and is pressed now
  // store the latest time in timeSincebuttonPressed
  if ((buttonLastPressed == false) && (buttonPressed == true)) {
    timeSincebuttonPressed = millis();
  }

  // the button has been pressed before and is pressed now
  // => this will only trigger a blinking LED to inform the user
  if ((buttonLastPressed == true) && (buttonPressed == true)) {
    unsigned long pastTime = millis() - timeSincebuttonPressed;
    if ((pastTime > TRIGGER_TIME_FACTORY_RESET) && (selectionState < TRIGGER_TIME_FACTORY_RESET)) {
#ifdef SERIAL_PRINTING
      Serial.println("Factory reset selected if released now");
#endif
      selectionState = TRIGGER_TIME_FACTORY_RESET;
      setUserActionFeedbackRequest();
    }
  }

  // button has been released => this will trigger the real action
  if ((buttonLastPressed == true) && (buttonPressed == false)) {
    unsigned long pastTime = millis() - timeSincebuttonPressed;
    // if the past time is longer than TRIGGER_TIME_FACTORY_RESET
    // then perform a factory reset
    if (pastTime > TRIGGER_TIME_FACTORY_RESET) {
      // set factory reset reqest
      setFactoryResetRequested();
    } else {
      // toggle connection state of relay
      toggleRelay();
    }
    // reset selectionState as button has been released
    selectionState = 0;
  }
  // save last button state
  buttonLastPressed = buttonPressed;
}

/**
   @brief This object is used to trigger the function press frequently.
          The button handler is implemented there.
*/
Ticker ticker;

////////////////////////////////////////////////////////////////////////////////
// OTA /////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief Read back OTA password from EEPROM and store it
          in global variable @link arduinoOtaPassword @endlink.
   @callergraph
   @callgraph
*/
void restoreOtaPasswordFromEEPROM() {
#ifdef SERIAL_PRINTING
  Serial.println("Restore OTA configuration from EEPROM");
#endif
  // Readback OTA password
  char otaPassword[EEPROM_size_OTA_password];
  for (int i = 0; i < EEPROM_size_OTA_password; i++) {
    otaPassword[i] = EEPROM.read(EEPROM_address_OTA_password + i);
  }
  arduinoOtaPassword = String(otaPassword);
}

/**
   @brief Store OTA password in EEPROM.
   @callergraph
   @callgraph
*/
void saveOtaPasswordToEEPROM() {
  const char * otaPassword = arduinoOtaPassword.c_str();
  for (int i = 0; i < EEPROM_size_OTA_password; i++) {
    EEPROM.write(EEPROM_address_OTA_password + i, otaPassword[i]);
  }
  storeCRC();
  EEPROM.commit();
}

////////////////////////////////////////////////////////////////////////////////
// WiFi Manager ////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief This function is called when a connection has been established
   @callergraph
   @callgraph
*/
void callbackConfigSuccessful() {
  arduinoOtaPassword = String(paramOtaPassword.getValue());
#ifdef SERIAL_PRINTING
  Serial.println("OTA password:" + arduinoOtaPassword);
#endif
  saveOtaPasswordToEEPROM();
}

/**
   @brief This function is called when WiFiManager is in config mode
   @callergraph
   @callgraph
*/
void callbackConfigMode(WiFiManager *myWiFiManager) {
  switchOnLED();
}

////////////////////////////////////////////////////////////////////////////////
// Setup ///////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief This function is executed once at the startup of the microcontroller.
          - The serial communication is setup with 115200 bauds. It is usefull for debugging or just information.
          - The output pins are configured.
          - The EEPROM is setup.
          - The MQTT configuration is restored from EEPROM.
          - MQTT is configured.
          - The webserver is configured.
          - MDNS is set up.
          - The LED is switched off.
          - The relay is disconnected. This is thought to be the natural experience, if you plug in a socket.
          - The button handler is started.
   @callergraph
   @callgraph
*/
void setup(void) {
  //////////////////////////////////////////////////////////////////////////////
  // start serial console //////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
#ifdef SERIAL_PRINTING
  Serial.begin(115200);
  Serial.println("WIFIOnOff started");
#endif

  //////////////////////////////////////////////////////////////////////////////
  // configure output pins /////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  configureOutputs();

  //////////////////////////////////////////////////////////////////////////////
  // setup EEPROM //////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  setupEEPROM();

  //////////////////////////////////////////////////////////////////////////////
  // restore MQTT configuration from EEPROM ////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  restoreMQTTConfigurationFromEEPROM();

  //////////////////////////////////////////////////////////////////////////////
  // Configure MQTT ////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  configureMQTT();

  //////////////////////////////////////////////////////////////////////////////
  // connect to WIFI ///////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////

  // add custom parameter for OTA password
  wifiManager.addParameter(&paramOtaPassword);

  // set callback for successful connection
  wifiManager.setSaveConfigCallback(callbackConfigSuccessful);

  // set callback for config mode
  wifiManager.setAPCallback(callbackConfigMode);

  // start captive portal
  wifiManager.autoConnect("WIFIOnOff");

  //////////////////////////////////////////////////////////////////////////////
  // enable auto reconnect /////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  WiFi.setAutoReconnect(true);

  //////////////////////////////////////////////////////////////////////////////
  // configure webserver ///////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  configureWebServer();

  //////////////////////////////////////////////////////////////////////////////
  // configure MDNS ////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  setupMDNS();

  //////////////////////////////////////////////////////////////////////////////
  // initialize LED ////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  switchOffLED();

  //////////////////////////////////////////////////////////////////////////////
  // start with the relay to the mains disabled ////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  disconnectRelay();

  //////////////////////////////////////////////////////////////////////////////
  // set button handler ////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
#ifdef SERIAL_PRINTING
  Serial.println("Start listening for user interaction ...");
#endif
  ticker.attach(0.1, pressHandler);

#ifdef DEV_OTA_UPDATES
  //////////////////////////////////////////////////////////////////////////////
  // setup OTA updates for development (Arduino IDE) ///////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  // example for doing OTA:
  // https://github.com/esp8266/Arduino/blob/master/libraries/ArduinoOTA/examples/BasicOTA/BasicOTA.ino
#ifdef SERIAL_PRINTING
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("OTA: Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA: End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA: Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA: Error[%u]\n: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("OTA: Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("OTA: Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("OTA: Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("OTA: Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("OTA: End Failed");
  });
#endif
  restoreOtaPasswordFromEEPROM();
  ArduinoOTA.setPassword(arduinoOtaPassword.c_str());
#ifdef SERIAL_PRINTING
  Serial.println("OTA PW: " + arduinoOtaPassword);
#endif
  ArduinoOTA.begin();
#endif
}

////////////////////////////////////////////////////////////////////////////////
// Loop ////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/**
   @brief This function is executed in a loop after setup(void) has been called.

   At first it checks for incoming user requests:
   - to change WiFi credentials
   - to perform a factory reset

   It is also checked for requests to inform the user about actions
   with the LED. These requests are triggered programmatically.

   After all requests are handled, it is cyclically checked that that the WiFi is connected.
   If WiFi is not connected, it does not make any sense to check for HTTP, ArduinoOTA or MQTT.
   For MQTT to be checked, it is also required, that it was enabled by the user.

   If MQTT is not connected, try to connect, otherwise handle MQTT.

   @callergraph
   @callgraph
*/
void loop(void) {
  //////////////////////////////////////////////////////////////////////////////
  // blink LEDs to inform user about menu selection ////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  if (getUserActionFeedbackRequest()) {
    feedbackQuickBlink();
  }
  //////////////////////////////////////////////////////////////////////////////
  // change WIFI credentials, if requested /////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  if (getWiFiCredentialChangeRequested()) {
    changeWiFiCredentials();
  }

  //////////////////////////////////////////////////////////////////////////////
  // perform factory reset, if requested ///////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  if (getFactoryResetRequested()) {
    performFactoryReset();
  }

  //////////////////////////////////////////////////////////////////////////////
  // continue only if WiFi connected ///////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  if (checkWiFiConnected()) {
#ifdef DEV_OTA_UPDATES
    ////////////////////////////////////////////////////////////////////////////
    // handle development OTA updates (Arduino IDE) ////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    ArduinoOTA.handle();
#endif
    ////////////////////////////////////////////////////////////////////////////
    // handle webserver ////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    webserver.handleClient();
    ////////////////////////////////////////////////////////////////////////////
    // continue only if MQTT configured  ///////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    if (getStateMQTTConfigured()) {
      if (checkMQTTConnected()) {
        ////////////////////////////////////////////////////////////////////////
        // handle MQTT /////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////
        mqttClient.loop();
      } else {
        ////////////////////////////////////////////////////////////////////////
        // connect to MQTT broker //////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////
        mqttConnect();
      }
    } else {
      // do nothing (MQTT needs to be configured by the user
      // with the help of the web user interface)
    }
  } else {
    // do nothing,
    // autoreconnect is enabled for WIFI
  }

}
