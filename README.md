# GlobalTime_MasterProject
Kernel drivers, programs and configuration files created for Global Time in Embedded Systems master's thesis.
These files were intended to work on Beaglebone Black platform.


# Testing programs
Located in /test directory.

run makefile to compile all at once


http://derekmolloy.ie/kernel-gpio-programming-buttons-and-leds/

# NTPd
Located in /ntpd directory is the configuration file we used to run this protocol.

To install ntpd, run following commands in your terminal:
   sudo apt update
   sudo apt install ntpd
   
NOTE: Installing ntpd will replace chrony on your device. It's not possible to have both installed at once. Installation will not affect configuration files though.

Then replace the configuration file in /etc with the one from this repository.
By default, ntpd will be configured to run as a background process. To start it up, run the following with sudo priviledges:
   service ntp start
To stop the daemon, run:
   service ntp stop
To check status of the daemon:
    service ntp status

Use ntpq program to check the status of protocol itself. Following command for example prints out the status of synchronization to servers the daemon is configured to connect to:
    ntpq -p 
    
For more information on the protocol, see: https://docs.ntpsec.org/latest/ntpd.html

# Chrony
Located in /chrony directory is the configuration file we used to run this protocol.

To install chrony, run following commands in your terminal:
   sudo apt update
   sudo apt install chrony
   
NOTE: Installing chrony will replace ntpd on your device. It's not possible to have both installed at once. Installation will not affect configuration files though.
   
Then replace the configuration file in /etc with the one from this repository.
By default, chrony will be configured to run as a background process. To start it up, run the following with sudo priviledges:
   service chrony start
To stop the daemon, run:
   service chrony stop
To check status of the daemon:
    service chrony status

Use chronyc program to check the status of protocol itself. Following command for example prints out the status of synchronization to the selected primary server:
    chronyc tracking 
The following command will display all peer and master servers Chrony is connected to:
    chronyc sources

For more information on the protocol, see: https://chrony.tuxfamily.org/index.html

# PTP
Configuration for LinuxPTP is located in /ptp directory. 
To run the software, you first need Linux Kernel version 3.0 or higher. Check your version with following command:
    ethtool -T eth0
This command will also let you know if your device and kernel support hardware timestamping, which is necessary for high-precision synchronization.
Also make sure ports 319 and 320 on system firewall are enabled.

To install the software, run:
    sudo apt update
    sudo pat install linuxptp
Then replace the configuration file in /etc with the one from this repository.

Linuxptp usues two utilities to synchronize time. ptp4l creates a precision clock for network time data exchange, while phc2sys adjusts system time according to that clock. It is also possible to feed precision clock from system time. Before running the protocol, make sure no other time synchronizations daemons or programs are running on the device, as they could disrupt ptp.

To run on a master device (using local system clock as master ptp clock for the network):
phc2sys -a -r -r
ptp4l -f /etc/ptp4l.conf -i -eth0

To run on slave device (sync to a master ptp clock in the network and adjust local system time to it):
phc2sys -s eth0 -c CLOCK_REALTIME -w
ptp4l -f /etc/ptp4l.conf -i eth0

ptp4l by default prints out current status to terminal. For quicker sync, we recommend clocks on all devices to be close to one another before running the protocol.
For more information: http://linuxptp.sourceforge.net/

# GPS
Configuration files are located in /gps directory. 
To synchronize a linux system clock to GPS time, we first need to attach a gps receiver. We used Adafruit ultimate gps: https://www.adafruit.com/product/746

In order to run and operate an external device, we must connect it to GPIO ports on Beagleboard:
TODO
Then we need to enable appropriate device overlays. In this configuration, gps uses serial port ttyO4 for satellite tracking messages, and gpio 9-12 for PPS signal. 
The full guide on installing proper overlays can be found on this address: https://groups.google.com/g/beagleboard/c/Qu78BPEdTtQ/m/IZuOAJADAwAJ
We will include shorter instructions here:
TODO


Secondly, we need to have gpsd and chrony daemons installed on our system. Gpsd captures data from gps receiver, and converts it to a readable form for other programs. Chrony uses captured time data to adjust time of local system clock.
For installation of chrony, check the section above. to install gpsd, run:
   sudo apt update
   sudo apt install gpsd
Then replace configurations of chrony and gpsd with files from this repository. To receive time data from gpsd through shared memory interface, chrony needs to be configured appropriately.

Both gpsd and chrony can be run in userspace, or as background processes. Running gpsd as a service with our configuration file should take care of neccessary settings.


Device overlays tutorial
https://groups.google.com/g/beagleboard/c/Qu78BPEdTtQ/m/IZuOAJADAwAJ

