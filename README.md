# CSE237B_RPi
## Functionalities
* Simulated network time protocol(NTP). Used UDP to get the clock offset between Raspberry Pi and laptop. Raspberry Pi is acting as the client and laptop is acting as the NTP server whose clock is assumed to be accurate.
* Used TCP and so called "TCP retransmission timeout estimator" algorithm to estimate the wireless network latency.
