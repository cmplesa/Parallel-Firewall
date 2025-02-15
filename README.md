# Parallel-Firewall

A firewall is a program that checks network packets against a series of filters which provide a decision regarding dropping or allowing the packets to continue to their intended destination.

In a real setup, the network card will receive real packets (e.g. packets having Ethernet, IP headers plus payload) from the network and will send them to the firewall for processing. The firewall will decide if the packets are to be dropped or not and, if not, passes them further.

In this assignment, instead of real network packets, we'll deal with made up packets consisting of a made up source (a number), a made up destination (also a number), a timestamp (also a number) and some payload. And instead of the network card providing the packets, we'll have a producer thread creating these packets.

The created packets will be inserted into a circular buffer, out of which consumer threads (which implement the firewall logic) will take packets and process them in order to decide whether they advance to the destination.

The result of this processing is a log file in which the firewall will record the decision taken (PASS or DROP) for each packet, along with other information such as timestamp.

The purpose of this assignment is to:

1) implement the circular buffer, along with synchronization mechanisms for it to work in a multithreaded program

2) implement the consumer threads, which consume packets and process them

3) provide the log file containing the result of the packet processing
