# MWI Check



## Introduction
Iinitially I wrote this library for my own use. Owning a FritzBox 7430 I wanted to use its answering machine function. The problem was, however, that the FritzBox is not easily accessible, so when coming home I am unable to see the boxes INFO LED. This LED is meant to signal new messages on the anwering machine.

My solution comes in the form of an ESP8266. It partially mimics a SIP phone, subscribing to the "Message Waiting Indication Event" (RFC3842, RFC6665) and signals the result with an external LED. The ESP8266 due to its WiFi network connection is very mobile and can be placed anywhere in my home.

Another use case could the integration of the answering machine into a home automation system, e.g. by sending MQTT messages.

Note that this solution is not intended for battery operation. No considerations were given to power saving. So I recommend the use of a mains-fed 5V supply.

## Usage
The library is relatively easy to use. There are only two functions: The **Init** and the **Handler** function. They go in the setup() and loop() part of the Arduino code respectively.

You need to create an instance of the class **MWI**. See details in the "Simple example" that comes with this library.

Of course your SIP server must be configured to have a SIP account for this device. For the FritzBox this is done `Telephony -> Telephony Devices -> Configure New Device`. Then select `Telephone (with or without answering machine)` and next, select `LAN/WLAN (IP telephone)`. Finally, enter SIP user and password and apply.

## Functions
### Init
```
    int16_t Init(String SipIp, uint16_t SipPort, String SipUser, String SipPasswd);
```
The Init function initialises the class and initiates the subscription to the MWI event. It takes a few parameters:
* SipIp, the IP address if the SIP server (e.g. your FritzBox, e.g. 192.168.0.1)
* SipPort, the respective port (usually 5060)
* SipUser, the user name created for the SIP phone device on the SIP server
* SipPasswd, the corresponding password

The function returns 0 if it cannot establish a connection to the SIP server and 1 if it can. Note that after initialisation, the client has not subscribed successfully yet. So at that stage, e.g. wrong SIP credentials will not be detected!

### Handler
```
int16_t Handler(void);
```
The Handler has to be called frequently. It manages all incoming data, keeps the connection alive and returns the current MWI state to the user. Any delays in your loop() function could potentially cause problems and timeouts.

It returns 1 if new messages are waiting and 0 otherwise. If the subscription went wrong, it returns -1.

## Security
This code is meant to be used in a private network. It doesn't use encryption like TLS or similar. Note however, that the SIP password is not transmitted. It is combined with other data and then its MD5 hash is used to authenticate the client with the server.

The server on the other hand is never authenticated.

Worst case, an attacker from within your network could listen in on the exchanged SIP messages. So they could gain the MWI status data. They could also pretend to be the server and send false MWI data.

In a private network this is somewhat unlikely and will probably cause no real harm. But at the end of the day you have to assess the risk for yourself.


## Limitations
My code is not fully compliant with RFC6665. E.g. I did not check for the connection states 'pending" or "terminated. This is because during development my FritzBox never sent those states. So I didn't need to evaluate them and I couldn't test the required code anyway.

I only own a FritzBoc 7430, so I was only able to test the code on this box. I expect however that it will work on basically any FritzBox and indeed on other SIP servers. I would appreciate feedback on this, so I can add a compatibility list here for the benefit of other users.

Please be advised also that I am not a professional programmer. My code might still have bugs. Feel free to test it and send feedback if there are problems. Also, there might be faster, more efficient and more professional ways to write such a library. Don't let me stop you from writing your own. I might or might not be able to integrate your proposals.

## Troubleshooting
Normally the library works silently. But if you cannot make it work, you can #define DEBUGLOG. This enables output on the Serial interface showing status information. All messages, transmitted and received are also shown. This can help to spot were things go wrong.

On my FritzBox, I get the following messages (TX: transmit / RX: receive):
* TX: SUBSCRIBE
* RX: Unauthorized
* TX: SUBSCRIBE (with MD5 hashes)
* RX: OK
* RX: NOTIFY (with connection-State: active, and with MWI state)
* TX: OK

## Examples
**Simple** is a very simple demonstration of the library. It shows the usage of the available functions. All connection details, credentials etc. have to be set in the code.

## References
[RFC3261](https://tools.ietf.org/html/rfc3261) - SIP: Session Initiation Protocol
[RFC6665](https://tools.ietf.org/html/rfc6665) -  SIP-Specific Event Notification
[RFC3842](https://tools.ietf.org/html/rfc3842) -  A Message Summary and Message Waiting Indication Event Package for the Session Initiation Protocol (SIP)
[SIP](https://en.wikipedia.org/wiki/Session_Initiation_Protocol)(Wikipedia)

## Attribution
I took some inspiration from the [ArduinoSIP library](https://github.com/dl9sec/ArduinoSIP), by Juergen Liegner / Thorsten Godau - especially how to calculate the MD5 digest. This saved me from digging through even more RFCs. The implementation however is my own.

## TODO
* Move strings to program memory - save RAM
* Switch debug messages to [SerialDebug](https://github.com/JoaoLopesF/SerialDebug)