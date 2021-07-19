
# This is a clone of [RemoteHWInfo](https://github.com/Demion/remotehwinfo)

# Description 

This project is my implementation above [RemoteHWInfo](https://github.com/Demion/remotehwinfo) project. This is a background application to be used with ONLY windows system. We can access msi afterburner shared memory api and send data to a python wrapper and the python wrapper sends data to a micro controller. At this moment, i dont pretend use this project to sends data direct to serial port. But, you will always can clone this project and do your properly implementation.


### This project repository is for be used with [arduino-python-computer-monitor](https://github.com/brutalzinn/arduino-python-computer-monitor)	



# Requeriments:

1. Windows 7 or above
2. Msi afterburner software
3. Microsoft C++ SDK developer tools

### About:

RemoteHWInfo HWiNFO / GPU-Z / MSI Afterburner Remote Monitor HTTP JSON Web Server

### Usage:

- **-port** *(60000 = default)*
- **-hwinfo** *(0 = disable; 1 = enable = default)*
- **-gpuz** *(0 = disable; 1 = enable = default)*
- **-afterburner** *(0 = disable; 1 = enable = default)*
- **-log** *(0 = disable; 1 = enable = default)*
- **-help**
+ http<nolink>://ip:port/**json.json** *(UTF-8)*
	+ http<nolink>://ip:port/json.json?**enable=0,1,2,3** *(0,1,2,3 = entryIndex)*
	+ http<nolink>://ip:port/json.json?**disable=0,1,2,3** *(0,1,2,3 = entryIndex)*
+ http<nolink>://ip:port/**index.html** *(UTF-8)*
+ http<nolink>://ip:port/**404.html** *(UTF-8)*

### Credits:
- Demion for share this awesome project to github community https://github.com/Demion
- HWiNFO - Professional System Information and Diagnostics https://www.hwinfo.com/
- Remote Sensor Monitor - A RESTful Web Server (Ganesh_AT) https://www.hwinfo.com/forum/Thread-Introducing-Remote-Sensor-Monitor-A-RESTful-Web-Server
- GPU-Z - Graphics Card GPU Information Utility https://www.techpowerup.com/gpuz/
- MSI Afterburner https://www.msi.com/page/afterburner

### Thanks to github user Demion to share his project with us. 
[Access Demion github profile](https://github.com/Demion)