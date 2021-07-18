
# This is a clone of [RemoteHWInfo](https://github.com/Demion/remotehwinfo)

### Thanks Demion to share this project with us <3 [Access Demion github profile](https://github.com/Demion)
## This project repository is for be used with [arduino-python-computer-monitor](https://github.com/brutalzinn/arduino-python-computer-monitor)

### Only be used for Windows system resource managment

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