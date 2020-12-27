///////////////////////////////////////////////////////////////////////////////////
//
//  Simple example to demonstrate the Usage of the MWICheck library
//
//  All connection details are hard coded into the code for the sake of clarity.
//  The built-in LED is set, if new messages are waiting.
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

#include <ESP8266WiFi.h>
#include <MWI.h>

// WiFi network credentials
const char *ssid = "........";
const char *password = "........";

// Sip parameters
String SipIP = "192.168.0.1";   // IP of the FRITZ!Box - depends on your network settings
uint16_t SipPORT = 5060;        // SIP port of the FRITZ!Box - almost always 5060
String SipUSER = "..........."; // SIP username at the FRITZ!Box - as set in Telephony Devices / your SIP account
String SipPW = "............";  // SIP password at the FRITZ!Box - as set in Telephony Devices / your SIP account

MWI myMWI; // Instantiation of the MWI class

void setup()
{
  // Start serial connection
  Serial.begin(115200);
  delay(10);

  // Connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\r\nWiFi connected.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  pinMode(LED_BUILTIN, OUTPUT);    // LED as Output
  digitalWrite(LED_BUILTIN, HIGH); // LED off

  // Initialising MWI
  myMWI.Init(SipIP, SipPORT, SipUSER, SipPW);
}

void loop()
{
  int mwi_state = 0; // Variable to hold the result of the MWI processing

  // MWI message processing
  mwi_state = myMWI.Handler();

  // Do something, depending on the MWI state
  if (mwi_state == 1) // Message is waiting
  {
    digitalWrite(LED_BUILTIN, LOW); // LED on
  }
  else if (mwi_state == 0) // No new message
  {
    digitalWrite(LED_BUILTIN, HIGH); // LED off
  }
  else if (mwi_state == -1) // Connection error
  {
    Serial.println("Connection error!");
    digitalWrite(LED_BUILTIN, HIGH); // LED off
    delay(500);
  }
}