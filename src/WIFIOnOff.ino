/** @file
    WIFIOnOFF is an alternative software for the Sonoff S20 which provides
    a web user interface (HTTP) and an internet of things interface (MQTT).
    The device can still be controlled by the normal user button.
    The connection to WiFi will be established by WPS for reasons of a
    better user experience. This device can be used in the local network
    without any dependency of a cloud.

    Copyright (C) 2018 Siegfried Schöfer

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


/**
   @brief Version number - obtained by 'git describe'
*/
String VERSION = "DUMMY_VERSION_DUMMY";

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
  @brief Define the password to protect OTA with DEV_OTA_PASSWD. This is not a high security procedure.
  @todo Despite the fact that over-the-air (OTA) updates are not highly secure, it is highly recommended to change the password.
        This is just a very bad default.
  @see https://media.readthedocs.org/pdf/arduino-esp8266/latest/arduino-esp8266.pdf
*/
#define DEV_OTA_PASSWD "CwpvVzR33gKY"

/**
   @brief time in ms to wait until WPS request is triggered when pressing the button (see pressHandler())
*/
#define TRIGGER_TIME_WPS 3000

/**
   @brief time in ms to wait until the WIFI configuration reset
*/
#define TRIGGER_TIME_WIFI_DATA_RESET 10000

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
#define EEPROM_version_number 0
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
   @brief WPS is the only choice to connect to WIFI. This shall improve
          usability. The state of WPS is stored in EEPROM at this address.
          The value 0x00 was chosen as it terminates Strings, which leads to perfect default data.
*/
#define EEPROM_address_WPS_configured EEPROM_address_start
/**
   @brief This flag is used to check whether @link mqttServer @endlink has already been
          initialized with the value of the MQTT-Server (DNS name or IP)
*/
#define EEPROM_address_MQTT_server_configured (EEPROM_address_WPS_configured + EEPROM_enabled_size)
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
  storeCRC();
  EEPROM.commit();
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
    // display warning to the user
    feedbackEEPROMInit();
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
   @brief This function is called to give the user feedback that the EEPROM has been re-/initialized.
          This means that all the user-entered data is gone and the device needs to be reconfigured.
   @callergraph
   @callgraph
*/
void feedbackEEPROMInit() {
#ifdef SERIAL_PRINTING
  Serial.println("Long pulse on LED to signal that the EEPROM got re/-initialized");
#endif
  // backup LED state
  bool backupStateLEDOn = getStateLEDOn();

  // switch off LED for a short period of time, so that the user recognizes the HIGH pulse following
  switchOffLED();
  delay(TIME_HALF_A_SECOND);

  // switch on the LED as warning light
  switchOnLED();
  delay(TIME_EEPROM_WARNING);

  // restore LED state
  if (backupStateLEDOn == true) {
    switchOnLED();
  } else {
    switchOffLED();
  }
}

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
   @brief This function is called to give the user feedback that WIFI is about to connect.
          It will blink at a medium rate.
   @callergraph
   @callgraph
*/
void feedbackWIFIisConnecting() {
#ifdef SERIAL_PRINTING
  Serial.print(".");
#endif
  toggleLED();
  delay(TIME_HALF_A_SECOND);
#ifdef SERIAL_PRINTING
  Serial.print(".");
#endif
  toggleLED();
  delay(TIME_HALF_A_SECOND);
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
   @brief This function checks whether WPS has already been performed
          once. In this case, one can directly connect to WiFi.
   @returns true if WPS has already been performed, false otherwise
   @callergraph
   @callgraph
 **/
bool checkWiFiConfigured() {
  return EEPROM.read(EEPROM_address_WPS_configured) == EEPROM_enabled;
}

/**
   @brief This function is a setter function which sets WiFi to
          configured, which means that WPS has already been
          performed successfully. This function is only called in
          performWPS().
          The configuration is saved to EEPROM.
   @callergraph
   @callgraph
*/
void setWiFiConfigured() {
#ifdef SERIAL_PRINTING
  Serial.println("Set WiFi configured");
#endif
  EEPROM.write(EEPROM_address_WPS_configured, EEPROM_enabled);
  storeCRC();
  EEPROM.commit();
}

/**
   @brief This function is a setter function which sets WiFi to not
          configured, which means that WPS is necessary before
          connecting to WIFI.
          The configuration is saved to EEPROM.
   @callergraph
   @callgraph
*/
void unsetWiFiConfigured() {
#ifdef SERIAL_PRINTING
  Serial.println("Delete WiFi configuration");
#endif
  // disconnect also deletes the WiFi credentials
  // https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFiSTA.cpp#L296
  WiFi.disconnect();
#ifdef SERIAL_PRINTING
#endif
  EEPROM.write(EEPROM_address_WPS_configured, EEPROM_init_value);
  storeCRC();
  EEPROM.commit();
  unsetWifiResetRequested();
}

/**
   @brief This function is used to trigger WPS. If WPS has been
          successful, setWiFiConfigured() is executed and unsetWPSRequest() is called.
   @callergraph
   @callgraph
*/
void performWPS() {
#ifdef SERIAL_PRINTING
  Serial.println("Perform WPS");
#endif
  // This step is very important. If the WiFi configuration is not unset,
  // loop() will try to connect endlessly, even if WPS fails.
  unsetWiFiConfigured();
  // the actual WPS push button procedure
  WiFi.beginWPSConfig();
  if (connectToWiFi()) {
    // WPS is only proven to be successful, if WiFi connection is successful
#ifdef SERIAL_PRINTING
    Serial.println("WiFi connection successful. WPS successful.");
#endif
    setWiFiConfigured();
  } else {
#ifdef SERIAL_PRINTING
    Serial.println("WiFi connection not successful. WPS not successful.");
#endif
  }
  // one WPS procedure has been performed, unset request
  unsetWPSRequest();
}

/**
   @brief This function returns the client name used for MQTT, see mqttConnect().
   @returns the client ID: "WIFIOnOff" + MAC address
   @callergraph
   @callgraph
*/
String getClientID() {
  return "WIFIOnOff_" + WiFi.macAddress();
}

/**
   @brief This function tries to connect to WIFI.
   @returns true, if the connection was successful, false otherwise.
   @callergraph
   @callgraph
*/
bool connectToWiFi() {
#ifdef SERIAL_PRINTING
  Serial.println("Connect to WIFI");
#endif
  // put WIFI to station mode
  WiFi.mode(WIFI_STA);
  // enable auto reconnect
  WiFi.setAutoReconnect(true);
  // set SSID and PSK
  WiFi.begin(WiFi.SSID().c_str(), WiFi.psk().c_str());
#ifdef SERIAL_PRINTING
  Serial.println("Try to connect to WiFi SSID: " + WiFi.SSID());
#endif
  // polling: leave when WiFi connected
  for (int i = 0; (i < CTR_MAX_TRIES_WIFI_CONNECTION) && (WiFi.status() != WL_CONNECTED); i++) {
    feedbackWIFIisConnecting();
  }
  if (WiFi.status() != WL_CONNECTED) {
#ifdef SERIAL_PRINTING
    Serial.println("\nWiFi failed to connect.");
#endif
    return false;
  }

#ifdef SERIAL_PRINTING
  // return value of WiFi.localIP() is of type IPAddress
  // https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/ESP8266WiFiSTA.h#L61
  // IPAddress needs to be converted to String
  // https://github.com/esp8266/Arduino/blob/f4c391032aff382bcd243bb038d69feb136a96b5/cores/esp8266/IPAddress.cpp#L108
  Serial.println("\nConnected, IP: " + WiFi.localIP().toString());
#endif

  return true;
}

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
   @brief This function checks whether WIFI is connected.
   @returns true, if WiFi is connected, false otherwise
   @callergraph
   @callgraph
*/
bool checkWiFiConnected() {
  return (WiFi.status() == WL_CONNECTED);
}

/**
   @brief State to track whether the WPS has been requested.
*/
bool wpsRequested = false;

/**
   @brief Getter function for @link wpsRequested @endlink.
   @returns true, if WPS has been requested, false otherwise.
*/
bool getWPSRequest() {
  return wpsRequested;
}

/**
   @brief Setter function to unset @link wpsRequested @endlink.
   @callergraph
   @callgraph
*/
void unsetWPSRequest() {
  wpsRequested = false;
}

/**
   @brief Setter function to set @link wpsRequested @endlink.
   @callergraph
   @callgraph
*/
void setWPSRequest() {
  wpsRequested = true;
}

/**
   @brief State to track whether the deletion of WIFI data has been requested.
*/
bool wifiResetRequested = false;

/**
   @brief Getter function for @link wifiResetRequested @endlink.
   @returns true, if a deletion of WIFI data has been requested, false otherwise.
*/
bool getWifiResetRequested() {
  return wifiResetRequested;
}

/**
   @brief Setter function to set @link wifiResetRequested @endlink.
   @callergraph
   @callgraph
*/
void setWifiResetRequested() {
  wifiResetRequested = true;
}

/**
   @brief Setter function to unsset @link wifiResetRequested @endlink.
   @callergraph
   @callgraph
*/
void unsetWifiResetRequested() {
  wifiResetRequested = false;
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
String renderMQTTServerSettings(String storedServerName, String failureMsg, bool stateMQTTActivated, bool stateMQTTConnected) {
  return R"(
<div class="headline">
  <h1>WIFIOnOff Settings</h1>
</div>
<p>
<form action="/settings.html" method="post">
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
  <input type="submit" value="Change settings">
</form>
<p>)" + String(stateMQTTConnected ? "MQTT server / broker is connected" : "MQTT server / broker is disconnected") + R"(
<p>)" + failureMsg + R"(
<p>
WIFIOnOff version: )" + VERSION;
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
<a href="/"> Control </a> &#124; <a href="/settings.html"> Settings </a> &#124; <a href=")" + REPOSITORY_URL_STRING + R"("> Project website </a>
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
  char mqttServerLocal[256];
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
  webserver.on("/settings.html", []() {
#ifdef SERIAL_PRINTING
    Serial.println("HTTP: /settings.html");
#endif
    // query whether MQTT should be activated
    String mqttState = webserver.arg("mqttState");
    bool stateMQTTConfigured;
#ifdef SERIAL_PRINTING
    Serial.println("mqttState: '" + mqttState + "'");
#endif
    if (mqttState == "on") {
      // enable mqtt
      stateMQTTConfigured = true;
    } else if (mqttState == "off") {
      // disable mqtt
      stateMQTTConfigured = false;
    } else {
      // use old value
      stateMQTTConfigured = getStateMQTTConfigured();
    }
    // query MQTT server
    String mqttServer = webserver.arg("mqttserver");
#ifdef SERIAL_PRINTING
    Serial.println("mqttServer: '" + mqttServer + "'");
#endif
    String failureMsg = "";
    if (mqttServer != "") {
      if (mqttServerValid(mqttServer)) {
        // mqttServer is valid => send accpeted
        failureMsg = "Input accepted.";
      } else {
        // mqttServer is valid => send rejected
        failureMsg = "Input rejected, due to XSS mitigation.";
      }
    } else {
      // mqttServer == "" if "mqttserver" is not in request => use stored server, no failure message
      // this happens if the user requests "/settings.html" the first time
      mqttServer = getMQTTServer();
    }

    // configure MQTT if something changed
    if ((mqttServer != getMQTTServer()) || (stateMQTTConfigured != getStateMQTTConfigured())) {
      setMQTTServer(mqttServer);
      setStateMQTTConfigured(stateMQTTConfigured);
      saveMQTTConfigurationToEEPROM();
      configureMQTT();
    }

    // generate rendering
    String response = renderHeader() + \
                      renderMQTTServerSettings(mqttServer, failureMsg, stateMQTTConfigured, checkMQTTConnected()) + \
                      renderFooter();

    // send answer back
    webserver.send(200, "text/html", response);
  });
  webserver.begin();
#ifdef SERIAL_PRINTING
  Serial.println("HTTP server started");
#endif
}

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
  unsetWiFiConfigured();
  setMQTTServer("");
  setStateMQTTConfigured(false);
  initEEPROM();
  unsetFactoryResetRequested();
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

/**
   @brief Setter function to unset @link factoryResetRequested @endlink.
   @callergraph
   @callgraph
*/
void unsetFactoryResetRequested() {
  factoryResetRequested = false;
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
          - WPS button method request
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
    if ((pastTime > TRIGGER_TIME_WPS) && (selectionState < TRIGGER_TIME_WPS)) {
#ifdef SERIAL_PRINTING
      Serial.println("WPS selected if released now");
#endif
      selectionState = TRIGGER_TIME_WPS;
      if (!checkWiFiConfigured()) {
        setUserActionFeedbackRequest();
      } else {
#ifdef SERIAL_PRINTING
        Serial.println("WPS was already performed successfully, wait for WIFI DATA RESET and repeat.");
#endif
      }
    }
    if ((pastTime > TRIGGER_TIME_WIFI_DATA_RESET) && (selectionState < TRIGGER_TIME_WIFI_DATA_RESET)) {
#ifdef SERIAL_PRINTING
      Serial.println("WIFI DATA RESET selected if released now");
#endif
      selectionState = TRIGGER_TIME_WIFI_DATA_RESET;
      setUserActionFeedbackRequest();
    }
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
    } else if (pastTime > TRIGGER_TIME_WIFI_DATA_RESET) {
      // delete WiFi configuration data
      setWifiResetRequested();
    } else if (pastTime > TRIGGER_TIME_WPS) {
      if (!checkWiFiConfigured()) {
        // set WPS request
        setWPSRequest();
      } else {
        // do nothing WPS has already been performed
      }
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
  // connect to WIFI ///////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  if (checkWiFiConfigured()) {
    connectToWiFi();
  }

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
  ArduinoOTA.setPassword(DEV_OTA_PASSWD);
#ifdef SERIAL_PRINTING
  Serial.println("Start Arduino OTA");
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
          - to perform WPS (performWPS())
          - to delete the WiFi configuration (unsetWiFiConfigured())
          - to perform a factory reset (performFactoryReset())

          It is also checked for requests to inform the user about actions
          with the LED. These requests are triggered programmatically.

          After all requests are handled, it is cyclically checked that WPS has been
          performed and that the WiFi is connected.
          If WPS has not been done or WiFi is not connected,
          it does not make any sense to check for HTTP, ArduinoOTA or MQTT.
          For MQTT to be checked, it is also required, that it was enabled by the user.

          If MQTT is not connected, try to connect, otherwise handle MQTT.
   @callergraph
   @callgraph
*/
void loop(void) {
  //////////////////////////////////////////////////////////////////////////////
  // perform WPS if requested //////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  if (getWPSRequest()) {
    performWPS();
  }

  //////////////////////////////////////////////////////////////////////////////
  // blink LEDs to inform user about menu selection ////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  if (getUserActionFeedbackRequest()) {
    feedbackQuickBlink();
  }

  //////////////////////////////////////////////////////////////////////////////
  // perform WIFI reset, if requested //////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  if (getWifiResetRequested()) {
    unsetWiFiConfigured();
  }

  //////////////////////////////////////////////////////////////////////////////
  // perform factory reset, if requested ///////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  if (getFactoryResetRequested()) {
    performFactoryReset();
  }

  //////////////////////////////////////////////////////////////////////////////
  // continue only if WiFi is configured ///////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  if (checkWiFiConfigured()) {
    ////////////////////////////////////////////////////////////////////////////
    // continue only if WiFi connected /////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    if (checkWiFiConnected()) {
#ifdef DEV_OTA_UPDATES
      //////////////////////////////////////////////////////////////////////////
      // handle development OTA updates (Arduino IDE) //////////////////////////
      //////////////////////////////////////////////////////////////////////////
      ArduinoOTA.handle();
#endif
      //////////////////////////////////////////////////////////////////////////
      // handle webserver //////////////////////////////////////////////////////
      //////////////////////////////////////////////////////////////////////////
      webserver.handleClient();
      ////////////////////////////////////////////////////////////////////////
      // continue only if MQTT configured  ///////////////////////////////////
      ////////////////////////////////////////////////////////////////////////
      if (getStateMQTTConfigured()) {
        if (checkMQTTConnected()) {
          ////////////////////////////////////////////////////////////////
          // handle MQTT /////////////////////////////////////////////////
          ////////////////////////////////////////////////////////////////
          mqttClient.loop();
        } else {
          ////////////////////////////////////////////////////////////////
          // connect to MQTT broker //////////////////////////////////////
          ////////////////////////////////////////////////////////////////
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
  } else {
    // do nothing (WiFi needs to be configured by the user
    // with the help of the button: doubleclick for WPS )
  }
}
