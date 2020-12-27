///////////////////////////////////////////////////////////////////////////////////
//
//  Complex example to demonstrate the Usage of the MWICheck library
//
//  All connection details are entered through WiFiManager.
//  The built-in LED is set, if new messages are waiting.
//  A MQTT message is sent.
//
//  Author: Th. Frey <tom_frey@web.de>
//  Date:   26.12.2020
//
//  Copyright (c) 2020 Thomas Frey. All rights reserved.
//
//  Published under GNU GPL V3.0 license, check LICENSE for more information.
//  All text above must be included in any redistribution.
//
///////////////////////////////////////////////////////////////////////////////////

#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

//#define DEBUGLOG // Use this to see (a lot of) status information
#include <MWI.h>

// Sip parameters
String SipIP = "";       // IP of the FRITZ!Box
uint16_t SipPORT = 5060; // SIP port of the FRITZ!Box
String SipUSER = "";     // SIP-Call username at the FRITZ!Box
String SipPW = "";       // SIP-Call password at the FRITZ!Box

String mqtt_server = "";   // MQTT server address
uint16_t mqtt_port = 1883; // MQTT server port

// Instantiation of MWI class
MWI myMWI;

//flag for saving data
bool shouldSaveConfig = false;

WiFiClient espClient;               // WiFi client for the MQTT client...
PubSubClient mqttClient(espClient); // MQTT client, obviously

// MQTT reconnection
void mqttConnect()
{
  // Loop until we're reconnected
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");

    // Create a client ID
    String clientId = "MWICheckClient-";
    clientId += WiFi.macAddress().substring(8);

    // Attempt to connect
    if (mqttClient.connect(clientId.c_str()))
    {
      Serial.println("connected");
    }
    else // try again
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

//callback notifying us of the need to save config
void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

////////////////////////////////////////////////////////////////
void setup()
{
  Serial.begin(115200);

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (LittleFS.begin())
  {
    Serial.println("Mounted file system.");
    if (LittleFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("Reading config file");
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

#if ARDUINOJSON_VERSION_MAJOR < 6
#warning "ARDUINOJSON library must be Version 6 or higher!"
#endif

        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if (!deserializeError)
        {
          Serial.println("\nparsed json");
          mqtt_server = json["mqtt_server"].as<String>();
          mqtt_port = json["mqtt_port"];
          SipIP = json["sip_ip"].as<String>();
          SipPORT = json["sip_port"];
          SipUSER = json["sip_user"].as<String>();
          SipPW = json["sip_pw"].as<String>();
        }
        else
        {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  }
  else
  {
    Serial.println("ERROR: Failed to mount FS!");
  }
  //end read

  //WiFiManager
  WiFiManager wifiManager;

  //DEBUG: reset settings - for testing
  //wifiManager.resetSettings();

  //DEBUG: deactivate WifiManager messages
  //wifiManager.setDebugOutput(false);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // The extra parameters to be configured (can be either global or just in the setup)
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("mqttserver", "MQTT server", mqtt_server.c_str(), 40);
  WiFiManagerParameter custom_mqtt_port("mqttport", "MQTT port", String(mqtt_port).c_str(), 6);
  WiFiManagerParameter custom_sip_ip("sipip", "SIP IP", SipIP.c_str(), 40);
  WiFiManagerParameter custom_sip_port("sipport", "SIP port", String(SipPORT).c_str(), 6);
  WiFiManagerParameter custom_sip_user("sipuser", "SIP user", SipUSER.c_str(), 40);
  WiFiManagerParameter custom_sip_pw("sippw", "SIP password", SipPW.c_str(), 40);

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_sip_ip);
  wifiManager.addParameter(&custom_sip_port);
  wifiManager.addParameter(&custom_sip_user);
  wifiManager.addParameter(&custom_sip_pw);

  if (!wifiManager.autoConnect("WMICheckAP"))
  {
    Serial.println("Failed to connect and hit timeout.");
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  Serial.println("\r\nWiFi connected.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  //read updated parameters
  mqtt_server = custom_mqtt_server.getValue();
  mqtt_port = String(custom_mqtt_port.getValue()).toInt();
  SipIP = custom_sip_ip.getValue();
  SipPORT = String(custom_sip_port.getValue()).toInt();
  SipUSER = custom_sip_user.getValue();
  SipPW = custom_sip_pw.getValue();

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    Serial.println("Saving config");
    DynamicJsonDocument json(1024);

    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["sip_ip"] = SipIP;
    json["sip_port"] = SipPORT;
    json["sip_user"] = SipUSER;
    json["sip_pw"] = SipPW;

    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile)
    {
      Serial.println("Failed to open config file for writing");
    }

    serializeJson(json, Serial);
    serializeJson(json, configFile);

    configFile.close();
    //end save
  }

  pinMode(LED_BUILTIN, OUTPUT);    // LED as Output
  digitalWrite(LED_BUILTIN, HIGH); // LED off

  mqttClient.setServer(mqtt_server.c_str(), 1883); // configure MQTT client

  myMWI.Init(SipIP, SipPORT, SipUSER, SipPW); // Initialise the MWI component
}

//////////////////////////////////////////////////////////////////////////////
void loop()
{
  int mwi_state = 0;
  static int old_mwi_state = 0;
  mwi_state = myMWI.Handler();

  // SIP processing
  if (mwi_state == 1) // new messages waiting
  {
    digitalWrite(LED_BUILTIN, LOW); // LED on
  }
  else if (mwi_state == 0) // no new messages
  {
    digitalWrite(LED_BUILTIN, HIGH); // LED off
  }
  else // error
  {
    digitalWrite(LED_BUILTIN, HIGH); // LED off
    Serial.println("Not connected to SIP server.");
    delay(1000);
  }

  // if the MWI state has changed, send an MQTT update
  if (mwi_state != old_mwi_state)
  {
    //(re-)connect MQTT client
    mqttConnect();
    // publish the state
    mqttClient.publish("MWIState", String(mwi_state).c_str());
    Serial.println("Sending MWI state " + String(mwi_state));
  }
  old_mwi_state = mwi_state;
}