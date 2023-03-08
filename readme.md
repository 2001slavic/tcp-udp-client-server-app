# TCP/UDP Client-server application
This is an application in which TCP clients (*subscribers*) connect to a
server in order to *subscribe* to *topics* and receive *topic updates*. Server
receives topics updates from UDP clients and forward them to proper (subscribed)
clients.

## Server
The server can be described as a broker beetween UDP clients and TCP clients.

### Launch
```
./server <PORT>
```

### Logging
In order to monitor server activity, connection events are shown in ``stdin``.

```
New client <CLIENT_ID> connected from IP:PORT.
Client <CLIENT_ID> disconnected.

Client <CLIENT_ID> already connected.
```
### Accepted commands
* ``exit`` Disconnects all TCP clients (*subscribers*) and shuts down the server.

### Communicating with UDP clients
The server can receive the following data from UDP clients:
|                    | **topic**                               | **datatype**                                                     | **content** |
|--------------------|-----------------------------------------|------------------------------------------------------------------|-------------|
| **Size** *(bytes)* | 50                                      | 1                                                                | 1500 max.   |
| **Format**         | String, null terminated if length < 50. | ``unsigned int``, used to specify data type of received content. | Variable    |

## TCP Clients
These clients can subscribe and unsubscribe from certain topics.
### Launch
```
./subscriber <CLIENT_ID> <SERVER_IP> <SERVER_PORT>
```

### Accepted commands
* ``subscribe <TOPIC> <SF>`` request server to subscribe to a topic
    * ``TOPIC`` String, the topic to subscribe to.
    * ``SF`` 0 or 1, store-and-forward, explained further.
* ``unsubscribe <TOPIC>`` tell server that client is unsubscribing from a topic.
* ``exit`` disconnect from server and close client.

### Logging
When (un-)subscribing to/from topic:
```
Subscribed to topic.
Unsubscribed from topic.
```
When receiving a message from server:
```
<UDP_CLIENT_IP>:<UDP_CLIENT_PORT> - <TOPIC> - <DATATYPE> - <CONTENT>
```

**Example:** If and UDP client with IP *address:port* ``1.2.3.4:4573`` publishes
``23.5``, with a **float with 2 decimal places** as **datatype** in
``UPB/precis/1/temperature`` topic, the shown message will be:
```
1.2.3.4:4573 - UPB/precis/1/temperature - SHORT_REAL - 23.5
```
Data type description and specific *contents*:

| **Payload type**            | **Type ID** (used by clients) | ***datatype*** **value** | **Payload format**                                                |
|-----------------------------|-------------------------------|--------------------------|-------------------------------------------------------------------|
| Unsigned integer            | ``INT``                       | 0                        | *Sign byte*, an ``uint32_t``                                      |
| Float with 2 decimal places | ``SHORT_REAL``                | 1                        | ``uint16_t`` which represents the float number multiplied by 100. |
| Float                       | ``FLOAT``                     | 2                        | *Sign byte*, ``uint32_t`` -- *number*, ``uint8_t`` -- *power*.        |
| String                      | ``STRING``                    | 3                        | A null terminated string.                                         |

*Notes:*
* To properly display the datatype **1**, divide the received ``uint16_t``
by 100 (ex. received ``uint16_t`` -- **2350** will be printed as **23.50**).
* To properly display the datatype **2**, print the **-** if *sign byte* is **1**. Print the number as follows: ``(double)number / pow(10, power))``, where
*number* and *power* are received as **contents** as in the table above.

## Store-and-forward
If the ``SF`` flag is set to **1** in the TCP-client command:
```
subscribe <TOPIC> <SF>
```

means that the server should store the topic updates for the client if it is
disconnected. Server should forward the missed updates when client reconnects.
If ``SF`` is set to **0**: when client disconnects, further messages will be lost,
but client will still be subscribed.

## Credits
The project assignment, UDP clients and testing tools are given by the *Protocoale de comunicatii* 2021-2022 course team of *Faculty of Automatic Control and Computers* of *University POLITEHNICA of Bucharest*.

The usage of UDP clients is described in ``pcom_hw2_udp_client/README.md``.
