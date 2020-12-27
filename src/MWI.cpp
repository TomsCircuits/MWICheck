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

#include <Arduino.h>
#include <MWI.h>

//#define DEBUGLOG // Use this to see (a lot of) status information

WiFiServer sip_server(MY_SIP_PORT); // This is the server through which the UAS sends their NOTIFY messages.

//////////////////////////////////////////////////////////////////////////77
//
// Public functions
//
////////////////////////////////////////////////////////////////////////////

// Class MWI constructor
MWI::MWI()
{
    state_machine = init;
    sip_server.begin();
}

// initialise the MWI class
int16_t MWI::Init(String SipIp, uint16_t SipPort, String SipUser, String SipPasswd)
{
    //Get SIP data
    if (!sip_ip.fromString(SipIp))
    {
#ifdef DEBUGLOG
        Serial.println("ERROR: SipIp not correct!");
#endif
        return 0;
    }
    sip_port = SipPort;
    sip_user = SipUser;
    sip_password = SipPasswd;

    my_ip = GetMyIP();
    my_port = MY_SIP_PORT;

    cseq = 0;

    sip_client.setTimeout(200);

#ifdef DEBUGLOG
    Serial.printf("\r\n*** Time: %.2f\r\n", millis() / 1000.0);
    Serial.println(">>> Connecting to SIP Server...");
#endif

    // Do this in advance to check if TCP connection works
    // The usre might have entered wrong SIP data
    if (!ConnectSIP())
    {
#ifdef DEBUGLOG
    Serial.printf("\r\n*** Time: %.2f\r\n", millis() / 1000.0);
    Serial.println(">>> SIP server not found. Check server IP address and port!");
#endif
        return 0;
    }

    // First, try to subscribe without authentification
    Subscribe("none");
    state_machine = notify_wait;
    return 1; // connected successfully to server
}

// Handle all SIP events and timeouts
int16_t MWI::Handler(void)
{
    String rx_message = "";       // Will contain received message
    static int16_t mwi_state = 0; // contains last known MWI state

    // Handle SUBSCRIBE message replies
    if (sip_client.available())
    {
        rx_message = "";

        // get message
        rx_message = sip_client.readString();

#ifdef DEBUGLOG
        IPAddress remoteIp = sip_ip;
        Serial.printf("\r\n*** Time: %.2f\r\n", millis() / 1000.0);
        Serial.printf("------ received from: %s:%i via client ----\r\n", remoteIp.toString().c_str(), sip_port);
        Serial.print(rx_message);
        Serial.print("----------------------------------------------------\r\n");
#endif

        // does the server ask for authorisation?
        if (rx_message.startsWith("SIP/2.0 401 Unauthorized"))
        {
            if (state_machine == notify_wait_auth)
            {
                state_machine = terminated;
                terminated_time = millis();
#ifdef DEBUGLOG
                Serial.printf("\r\n*** Time: %.2f\r\n", millis() / 1000.0);
                Serial.println("*** Authorisation went wrong. Check SIP credentials!");
#endif
                return -1;
            }
            else
            {
                Subscribe(rx_message);
                state_machine = notify_wait_auth;
            }
        }
        else if (rx_message.startsWith("SIP/2.0 200 OK"))
        {
            // update request timer (needed for various timeout detection)
            request_time = millis();
#ifdef DEBUGLOG
            Serial.printf("\r\n*** Time: %.2f\r\n", millis() / 1000.0);
            Serial.println(">>> Received OK message.");
#endif
        }
    }

    // Check and process NOTIFY messages via our server
    WiFiClient server_client = sip_server.available();
    server_client.setTimeout(200);

    if (server_client)
    {
#ifdef DEBUGLOG
        Serial.printf("\r\n*** Time: %.2f\r\n", millis() / 1000.0);
        Serial.println(">>> Client connected.");
#endif
        // The client gets disconnected from our server when we have read the message
        while (server_client.connected())
        {
            // wait for data to arrive
            if (server_client.available())
            {
                String rx_message = server_client.readString();

#ifdef DEBUGLOG
                IPAddress remoteIp = server_client.remoteIP();
                Serial.printf("\r\n*** Time: %.2f\r\n", millis() / 1000.0);
                Serial.printf("------ received from: %s:%i via server ----\r\n", remoteIp.toString().c_str(), server_client.remotePort());
                Serial.print(rx_message);
                Serial.print("----------------------------------------------------\r\n");
#endif
                // We don't expect any other message but NOTIFY, so ignore anything else
                if (rx_message.startsWith("NOTIFY sip:"))
                {
                    Ok(rx_message, server_client);
#ifdef DEBUGLOG
                    Serial.printf("\r\n*** Time: %.2f\r\n", millis() / 1000.0);
                    Serial.println(">>> Received NOTIFY message.");
                    Serial.println(">>> Sent OK message.");
#endif

                    // Get data from NOTIFY message
                    // get Cseq and refresh time
                    refresh_time = 1000 * GetInteger(rx_message, "expires=");
                    request_time = millis();
                    cseq = GetInteger(rx_message, "CSeq: ");

                    // only if subscription is confirmed
                    if (GetLine(rx_message, "Subscription-State:").indexOf("active") > 0)
                    {
                        state_machine = active;

                        // FINALLY get message waiting state
                        String mwi_result;
                        mwi_result = GetLine(rx_message, "Messages-Waiting:");
                        if (mwi_result.indexOf("yes") >= 0)
                        {
                            mwi_state = 1;
                        }
                        else
                        {
                            mwi_state = 0;
                        }
                    }
                    else // NOTIFY message doesn't give a valid MWI state
                    {
                        mwi_state = -1;
                    }
                    server_client.stop();
                }
            }
        }

        // close the connection:
        server_client.stop();
#ifdef DEBUGLOG
        Serial.printf("\r\n*** Time: %.2f\r\n", millis() / 1000.0);
        Serial.println(">>> Client disconnected.");
#endif
    }

    // Check for Time-Out after SUBSCRIBE initiated
    if ((state_machine == notify_wait)||(state_machine == notify_wait_auth))
    {
        if (millis() - request_time > TIMEOUT_SUBSCRIBE) // This way it survives a timer wrap around! (after abt. 50 days...)
        {
#ifdef DEBUGLOG
            Serial.printf("\r\n*** Time: %.2f\r\n", millis() / 1000.0);
            Serial.println(">>> SUBSCRIBE attempt timed out!");
#endif
            mwi_state = -1; // there is no valid MWI data!

            // Try again after XX seconds
            state_machine = terminated;
            terminated_time = millis();
        }
    }

    // if things went wrong, try again after some time
    if (state_machine == terminated)
    {
        if (millis() - terminated_time > TIMEOUT_TERMINATED) // This way it survives a timer wrap around! (after abt. 50 days...)
        {
#ifdef DEBUGLOG
            Serial.printf("\r\n*** Time: %.2f\r\n", millis() / 1000.0);
            Serial.println(">>> Trying again after termination!");
#endif

            Subscribe("none"); // First, try to subscribe without authentification
            state_machine = notify_wait;
        }
    }

    // Check for RE-SUBSCRIBE time
    if (state_machine == active)
    {
        if ((millis() - request_time) > (refresh_time - 5000)) // This way it survives a timer wrap around! (after abt. 50 days...)
        {
#ifdef DEBUGLOG
            Serial.println("Refreshing...");
            Serial.print("system time: ");
            Serial.println(millis());
            Serial.print("request time: ");
            Serial.println(request_time);
            Serial.print("refresh time: ");
            Serial.println(refresh_time);
#endif
            Subscribe("resubscribe");
            state_machine = notify_wait;
        }
    }

    return mwi_state;
}

//////////////////////////////////////////////////////////////////////////77
//
// Private functions
//
////////////////////////////////////////////////////////////////////////////

// Send OK message via client
void MWI::Ok(String rx_message, WiFiClient client)
{
    String message; // Tx message buffer

    message = "SIP/2.0 200 OK\r\n";
    message += GetLine(rx_message, "Via: ");
    message += GetLine(rx_message, "From: ");
    message += GetLine(rx_message, "To: ");
    message += GetLine(rx_message, "Call-ID: ");
    message += GetLine(rx_message, "CSeq: ");
    message += "Contact: <sip:" + sip_user + "@" + my_ip + ":" + my_port + ";transport=tcp>\r\n";
    //    message += "Allow: INVITE, ACK, BYE, CANCEL, INFO, MESSAGE, NOTIFY, OPTIONS, REFER, UPDATE, PRACK\r\n";
    message += "Allow: NOTIFY\r\n";
    message += "Content-Length: 0\r\n\r\n";

    ServerSendSIP(message, client);
}

// Call Subscribe with or without authentification, or start a refresh
// If a rx_message is handed over, auth is done
void MWI::Subscribe(String rx_message)
{
    String realm = ""; // Received from UAS
    String nonce = ""; // Received from UAS

    String hash1 = "";    // Hash over Sip User Credentials
    String hash2 = "";    // Hash over Sip request header
    String response = ""; // Response hash

    String open_string = ""; // TempString to generate hashes

    String message = ""; // Tx message buffer

    cseq++;

    if (rx_message == "none") // if this is the initial subscription, create IDs
    {
        call_id = Random();
        tag_id = Random();
        branch_id = Random();
    }

    // Set the intervall time when to re-subscribe to the service
    // Do not change at subsciption with authorisation
    if ((rx_message == "none") || (rx_message == "resubscribe"))
    {
        refresh_time = TIMEOUT_EXPIRE * 1000;
    }

    // start request timer (needed for various timeout detection)
    request_time = millis();

    // create SUBSCRIBE message
    // request header
    message = "SUBSCRIBE sip:" + sip_user + "@" + sip_ip.toString() + " SIP/2.0\r\n";

    // Via
    message += "Via: SIP/2.0/TCP " + my_ip + ":" + my_port + ";branch=" + branch_id + ";rport\r\n";

    // From
    message += "From: <sip:" + sip_user + "@" + sip_ip.toString() + ">;tag=" + tag_id + "\r\n";

    // To
    message += "To: <sip:" + sip_user + "@" + sip_ip.toString() + ">\r\n";

    // Call-ID
    message += "Call-ID: ";
    message += call_id;
    message += "@" + my_ip + "\r\n";

    // CSeq
    message += "CSeq: ";
    message += cseq;
    message += " SUBSCRIBE\r\n";

    // Contact
    message += "Contact: <sip:" + sip_user + "@" + my_ip + ":" + my_port + ";transport=tcp>\r\n";

    // Authentification
    if ((rx_message != "none") && (rx_message != "resubscribe"))
    {
        realm = GetParameter(rx_message, "realm=\"");
        nonce = GetParameter(rx_message, "nonce=\"");

        open_string = sip_user + ":" + realm + ":" + sip_password;
        hash1 = CalculateMD5(open_string);

        open_string = "SUBSCRIBE:sip:" + sip_user + "@" + sip_ip.toString();
        hash2 = CalculateMD5(open_string);

        open_string = hash1 + ":" + nonce + ":" + hash2;
        response = CalculateMD5(open_string);

        message += "Authorization: Digest username=\"" + sip_user + "\", realm=\"" + realm + "\", nonce=\"" + nonce + "\", ";
        message += "uri=\"sip:" + sip_user + "@" + sip_ip.toString() + "\", response=\"" + response + "\"\r\n";
    }

    // Fixed headers
    message += "Max-Forwards: 70\r\nExpires: ";
    message += refresh_time / 1000;
    message += "\r\nEvent: message-summary\r\n";
    message += "Accept: application/simple-message-summary\r\nContent-Length: 0\r\n\r\n";

    ClientSendSIP(message);

    return;
}

// Get parameter from incoming message
int16_t MWI::GetInteger(String message, String parameter)
{
    int16_t result = 0;
    String result_string = "";
    uint16_t index = 0;
    char next_character = '0';

    index = message.indexOf(parameter) + parameter.length();
    next_character = message.charAt(index++);

    while ((next_character >= '0') && (next_character <= '9') && (index <= message.length()))
    {
        result_string += next_character;
        next_character = message.charAt(index++);
    }
    result = result_string.toInt();
    return result;
}

// Get parameter from incoming message
String MWI::GetParameter(String message, String parameter)
{
    String result = "";
    uint16_t index = 0;
    char next_character = 'x';

    index = message.indexOf(parameter) + parameter.length();

    while ((next_character != ';') && (next_character != '\"') && (index <= message.length()))
    {
        next_character = message.charAt(index++);
        result += next_character;
    }
    return result.substring(0, result.length() - 1);
}

// Get line from incoming message
String MWI::GetLine(String message, String parameter)
{
    String result = "";
    uint16_t index = 0;
    char next_character = 'x';

    index = message.indexOf(parameter);

    while ((next_character != '\n') && (index <= message.length()))
    {
        next_character = message.charAt(index++);
        result += next_character;
    }

    // Serial.print("\r\nLine ");
    // Serial.print(parameter);
    // Serial.print(" ");
    // Serial.print(Result);

    return result;
}

// Generate a 30 bit random number
uint32_t MWI::Random()
{
    return secureRandom(0x3fffffff);
}

// Calculate MD5
String MWI::CalculateMD5(String Input)
{
    MD5Builder md5;

    md5.begin();
    md5.add(Input);
    md5.calculate();
    return md5.toString();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WiFi functions
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

// Connect to SIP server from which to get the MWI state
// (hopefully a SIP server...)
int16_t MWI::ConnectSIP()
{
    if (!sip_client.connected())
    {
        return sip_client.connect(sip_ip, sip_port);
    }
    // Successfully connected
    return 1;
}

// Get local network data
String MWI::GetMyIP()
{
    return WiFi.localIP().toString();
}

// Send message via client
void MWI::ClientSendSIP(String message)
{
    ConnectSIP();
    sip_client.print(message);
    sip_client.flush();

#ifdef DEBUGLOG
    IPAddress IP = sip_client.remoteIP();
    int port = sip_client.remotePort();
    Serial.printf("\r\n*** Time: %.2f\r\n", millis() / 1000.0);
    Serial.printf("------ sending to: %s:%i via client ----\r\n", IP.toString().c_str(), port);
    Serial.print(message);
    Serial.print("------------------------------------------------\r\n");
#endif
}

// send message vie server
void MWI::ServerSendSIP(String message, WiFiClient client)
{

    client.print(message);

#ifdef DEBUGLOG
    IPAddress IP = client.remoteIP();
    int port = client.remotePort();
    Serial.printf("\r\n*** Time: %.2f\r\n", millis() / 1000.0);
    Serial.printf("------ sending to: %s:%i via server ----\r\n", IP.toString().c_str(), port);
    Serial.print(message);
    Serial.print("------------------------------------------------\r\n");
#endif
}