///////////////////////////////////////////////////////////////////////////////////
//
//  Library to check the MWI state on a SIP server
//
//  Latest version on https://github.com/TomsCircuits/MWICheck
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

#ifndef MWI_H
#define MWI_H

#include "Arduino.h"

#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <WiFiServer.h>

#define MY_SIP_PORT 5060 // This is where our own WiFiServer listens for NOTIFY messages from the UAS

// timeouts in milliseconds
#define TIMEOUT_SUBSCRIBE 32 * 1000UL      // 32 sec / Timer N in RFC6665
#define TIMEOUT_TERMINATED 5 * 60 * 1000UL // 5 min / If subscription fails, try again after 5 min
// timeout in seconds
#define TIMEOUT_EXPIRE 3600UL // 3600 sec as recommended by RFC3842 / Re-subsciption intervall

class MWI
{
public:
    MWI();
    int16_t Init(String SipIp, uint16_t SipPort, String SipUser, String SipPasswd);
    int16_t Handler(void);

private:
    IPAddress sip_ip;
    uint16_t sip_port;
    String sip_user;
    String sip_password;

    String my_ip;
    uint16_t my_port;

    uint32_t call_id;
    uint32_t tag_id;
    uint32_t branch_id;

    uint16_t cseq;

    enum state
    {
        init,
        notify_wait,
        notify_wait_auth,
        pending,
        active,
        terminated
    }; // state of the state machine as in rfc 6665

    state state_machine;

    uint32_t request_time;    // time of request to determine retry time
    uint32_t terminated_time; // time of subscribe attempt termination
    uint32_t refresh_time;    // time to refresh the subscription

    WiFiClient sip_client;

    void Subscribe(String RxMessage);
    void Ok(String RxMessage, WiFiClient client);
    int16_t GetInteger(String Message, String Parameter);
    String GetParameter(String Message, String Parameter);
    String GetLine(String Message, String Parameter);

    uint32_t Random();
    void ClientSendSIP(String Message);
    void ServerSendSIP(String message, WiFiClient client);
    int16_t ConnectSIP();
    String GetMyIP();
    String CalculateMD5(String Input);
};

#endif // MWI_H