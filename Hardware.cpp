#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

#include <Winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include "Hardware.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <WS2tcpip.h>
#include <string>
#include <sstream>
#include <fcntl.h>

#if defined(__linux__)
#  include <unistd.h>
#elif defined(_WIN32)
#include <Windows.h>

#  define sleep(s) Sleep((s)*1000)
#endif
#pragma comment(lib, "ws2_32.lib")

#pragma pack(push, 1)
using namespace std;


struct HWINFO_SENSORS_READING
{
	unsigned int readingType;
	unsigned int sensorIndex;
	unsigned int readingId;
	char labelOriginal[128];
	char labelUser[128];
	char unit[16];
	double value;
	double valueMin;
	double valueMax;
	double valueAvg;
};

struct HWINFO_SENSORS_SENSOR
{
	unsigned int sensorId;
	unsigned int sensorInst;
	char sensorNameOriginal[128];
	char sensorNameUser[128];
};

struct HWINFO_SENSORS_SHARED_MEM2
{
	unsigned int signature;
	unsigned int version;
	unsigned int revision;
	long long int pollTime;
	unsigned int sensorOffset;
	unsigned int sensorSize;
	unsigned int sensorCount;
	unsigned int readingOffset;
	unsigned int readingSize;
	unsigned int readingCount;
};

struct GPUZ_RECORD
{
	wchar_t key[256];
	wchar_t value[256];
};

struct GPUZ_SENSOR_RECORD
{
	wchar_t name[256];
	wchar_t unit[8];
	unsigned int digits;
	double value;
};

struct GPUZ_SH_MEM
{
	unsigned int version;
	volatile int busy;
	unsigned int lastUpdate;
	GPUZ_RECORD data[128];
	GPUZ_SENSOR_RECORD sensors[128];
};

struct MAHM_SHARED_MEMORY_ENTRY
{
	char name[MAX_PATH];
	char units[MAX_PATH];
	char localName[MAX_PATH];
	char localUnits[MAX_PATH];
	char format[MAX_PATH];
	float data;
	float minLimit;
	float maxLimit;
	unsigned int flags;
};

struct MAHM_SHARED_MEMORY_HEADER
{
	unsigned int signature;
	unsigned int version;
	unsigned int headerSize;
	unsigned int entryCount;
	unsigned int entrySize;
	int time;
};

#pragma pack(pop)

#define	MAHM_SHARED_MEMORY_ENTRY_FLAG_SHOW_IN_OSD  0x00000001
#define	MAHM_SHARED_MEMORY_ENTRY_FLAG_SHOW_IN_LCD  0x00000002
#define	MAHM_SHARED_MEMORY_ENTRY_FLAG_SHOW_IN_TRAY 0x00000004

enum class HwinfoReadingType
{
	None,
	Temp,
	Voltage,
	Fan,
	Current,
	Power,
	Clock,
	Usage,
	Other
};

const char JsonHeader[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: application/json; charset=utf-8\r\n"
"Content-Size: %d\r\n"
"Connection: close\r\n"
"Access-Control-Allow-Origin: *\r\n"
"\r\n";

const char HtmlIndexHeader[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html; charset=utf-8\r\n"
"Content-Size: %d\r\n"
"Connection: close\r\n"
"Access-Control-Allow-Origin: *\r\n"
"\r\n";

const char HtmlNotFoundHeader[] =
"HTTP/1.1 404 Not Found\r\n"
"Content-Type: text/html; charset=utf-8\r\n"
"Content-Size: %d\r\n"
"Connection: close\r\n"
"Access-Control-Allow-Origin: *\r\n"
"\r\n";

const wchar_t HtmlIndexDataDefault[] =
L"<html>\n\t<head>\n\t\t<title>RemoteHWInfo</title>\n\t</head>\n\t<body>\n\t</body>\n</html>";

const wchar_t HtmlNotFoundDataDefault[] =
L"<html>\n\t<head>\n\t\t<title>RemoteHWInfo</title>\n\t</head>\n\t<body>\n\t\t404 Not Found\n\t</body>\n</html>";

char* HtmlIndexData = 0;
size_t HtmlIndexSize = 0;

char* HtmlNotFoundData = 0;
size_t HtmlNotFoundSize = 0;

const unsigned int EntryTotalCount = 1024;
bool EntryEnabled[EntryTotalCount] = { 0 };

unsigned int Port = 60000;

bool Hwinfo = true, Gpuz = true, Afterburner = true;
bool LogFileEnable = true;
bool stopServer = false;
SOCKET serverSocket;
#define LOG(expression) Log(#expression, strrchr(__FILE__, '\\') + 1, __LINE__, (intptr_t) (expression))

FILE* LogFile = 0;

intptr_t Log(const char* expression, const char* fileName, unsigned int line, intptr_t result)
{
	if (LogFile)
	{
		time_t t = time(0);
		tm* local = localtime(&t);

		fprintf(LogFile, "[%02d.%02d.%04d %02d:%02d:%02d][%8s:%04d] %-102s %-20zd (0x%0*zX)\n",
			local->tm_mday, local->tm_mon + 1, local->tm_year + 1900, local->tm_hour, local->tm_min, local->tm_sec, fileName, line, expression, result, (unsigned int)sizeof(result) * 2, result);

		fflush(LogFile);
	}

	return result;
}

size_t LoadFile(const char* fileName, void** fileData)
{
	size_t readSize = 0;

	FILE* file = 0;

	LOG(file = fopen(fileName, "rb"));

	if (file)
	{
		if (LOG(fseek(file, 0, SEEK_END)) == 0)
		{
			int fileSize = 0;

			LOG(fileSize = ftell(file));

			if (fileSize > 0)
			{
				if (LOG(fseek(file, 0, SEEK_SET)) == 0)
				{
					if (fileData)
					{
						LOG(*fileData = malloc(fileSize));

						if (*fileData)
							LOG(readSize = fread(*fileData, 1, fileSize, file));
					}
				}
			}
		}

		LOG(fclose(file));
	}

	return readSize;
}

size_t UnicodeToUtf8(const wchar_t* unicode, char** utf8)
{
	size_t utf8Size = 0;

	size_t unicodeSize = wcslen(unicode);

	if (unicodeSize > 0)
	{
		LOG(utf8Size = WideCharToMultiByte(CP_UTF8, 0, unicode, (int)unicodeSize, 0, 0, 0, 0));

		if (utf8Size > 0)
		{
			if (utf8)
			{
				LOG(*utf8 = (char*)malloc(utf8Size * sizeof(char)));

				if (*utf8)
				{
					LOG(utf8Size = WideCharToMultiByte(CP_UTF8, 0, unicode, (int)unicodeSize, *utf8, (int)utf8Size, 0, 0));

					if (utf8Size == 0)
						free(*utf8);
				}
			}
		}
	}

	return utf8Size;
}

char* FormatSpecialChar(char* str)
{
	size_t length = strlen(str);

	for (unsigned int i = 0; i < length; ++i)
	{
		if ((str[i] == '\\') || (str[i] == '\"'))
		{
			for (size_t j = length; j > i; --j)
				str[j] = str[j - 1];

			str[i++] = '\\';

			++length;
		}
	}

	str[length] = '\0';

	return str;
}

wchar_t* FormatSpecialCharUnicode(wchar_t* str)
{
	size_t length = wcslen(str);

	for (unsigned int i = 0; i < length; ++i)
	{
		if ((str[i] == L'\\') || (str[i] == L'\"'))
		{
			for (size_t j = length; j > i; --j)
				str[j] = str[j - 1];

			str[i++] = L'\\';

			++length;
		}
	}

	str[length] = L'\0';

	return str;
}

bool GetHwinfo(HWINFO_SENSORS_SHARED_MEM2* hwinfo, void** sensors, void** readings)
{
	bool result = false;

	HANDLE mapFile = 0;

	LOG(mapFile = OpenFileMappingA(FILE_MAP_READ, false, "Global\\HWiNFO_SENS_SM2"));

	if (mapFile)
	{
		void* mapAddress = 0;

		LOG(mapAddress = MapViewOfFile(mapFile, FILE_MAP_READ, 0, 0, 0));

		if (mapAddress)
		{
			if (hwinfo)
				memcpy(hwinfo, mapAddress, sizeof(HWINFO_SENSORS_SHARED_MEM2));

			result = true;

			if (sensors)
			{
				LOG(*sensors = malloc(hwinfo->sensorSize * hwinfo->sensorCount));

				if (*sensors)
					memcpy(*sensors, (unsigned char*)mapAddress + hwinfo->sensorOffset, hwinfo->sensorSize * hwinfo->sensorCount);
				else
					result = false;
			}

			if (result)
			{
				if (readings)
				{
					LOG(*readings = malloc(hwinfo->readingSize * hwinfo->readingCount));

					if (*readings)
					{
						memcpy(*readings, (unsigned char*)mapAddress + hwinfo->readingOffset, hwinfo->readingSize * hwinfo->readingCount);
					}
					else
					{
						if (sensors)
							free(*sensors);

						result = false;
					}
				}
			}

			LOG(UnmapViewOfFile(mapAddress));
		}

		LOG(CloseHandle(mapFile));
	}

	return result;
}

bool GetGpuz(GPUZ_SH_MEM* gpuz)
{
	bool result = false;

	HANDLE mapFile = 0;

	LOG(mapFile = OpenFileMappingA(FILE_MAP_READ, false, "GPUZShMem"));

	if (mapFile)
	{
		void* mapAddress = 0;

		LOG(mapAddress = MapViewOfFile(mapFile, FILE_MAP_READ, 0, 0, 0));

		if (mapAddress)
		{
			if (gpuz)
				memcpy(gpuz, mapAddress, sizeof(GPUZ_SH_MEM));

			result = true;

			LOG(UnmapViewOfFile(mapAddress));
		}

		LOG(CloseHandle(mapFile));
	}

	return result;
}

bool GetAfterburner(MAHM_SHARED_MEMORY_HEADER* afterburner, void** entries)
{
	bool result = false;

	HANDLE mapFile = 0;

	LOG(mapFile = OpenFileMappingA(FILE_MAP_READ, false, "MAHMSharedMemory"));

	if (mapFile)
	{
		void* mapAddress = 0;

		LOG(mapAddress = MapViewOfFile(mapFile, FILE_MAP_READ, 0, 0, 0));

		if (mapAddress)
		{
			if (afterburner)
				memcpy(afterburner, mapAddress, sizeof(MAHM_SHARED_MEMORY_HEADER));

			result = true;

			if (entries)
			{
				LOG(*entries = malloc(afterburner->entrySize * afterburner->entryCount));

				if (*entries)
					memcpy(*entries, (unsigned char*)mapAddress + afterburner->headerSize, afterburner->entrySize * afterburner->entryCount);
				else
					result = false;
			}

			LOG(UnmapViewOfFile(mapAddress));
		}

		LOG(CloseHandle(mapFile));
	}

	return result;
}

void ParseParams(const char* buffer)
{
	bool enable = false, disable = false;

	char* start = 0;

	if ((start = (char*)strstr(buffer, "?enable=")) != 0)
		enable = true;
	else if ((start = (char*)strstr(buffer, "?disable=")) != 0)
		disable = true;

	if (enable)
	{
		for (unsigned int i = 0; i < EntryTotalCount; ++i)
			EntryEnabled[i] = false;
	}
	else
	{
		for (unsigned int i = 0; i < EntryTotalCount; ++i)
			EntryEnabled[i] = true;
	}

	if ((enable) || (disable))
	{
		start = (char*)strstr(start, "=");
		char* end = (char*)strstr(start, " ");

		if ((start != 0) && (end != 0))
		{
			++start;
			*end = 0;

			while (start < end)
			{
				int sensorIndex = atoi(start);

				if (sensorIndex < EntryTotalCount)
					EntryEnabled[sensorIndex] = enable ? true : false;

				char* next = (char*)strstr(start, ",");

				if (next != 0)
					*next = 0;
				else
					next = end;

				start = next + 1;
			}
		}
	}
}

size_t CreateJson(char** jsonData)
{
	size_t utf8Size = 0;

	wchar_t* json = 0;

	LOG(json = (wchar_t*)malloc(1000000 * sizeof(wchar_t)));

	if (json)
	{
		int entryIndex = 0;

		bool first = false;
		bool first_output = true;

		swprintf(json, L"{\n");

		if (Hwinfo)
		{
			HWINFO_SENSORS_SHARED_MEM2 hwinfo = { 0 };

			void* sensors = 0;
			void* readings = 0;

			if (GetHwinfo(&hwinfo, &sensors, &readings))
			{
				first_output = false;

				swprintf(json + wcslen(json),
					L"\t\"hwinfo\":\n"
					L"\t{\n"
					L"\t\t\"signature\": %d,\n"
					L"\t\t\"version\": %d,\n"
					L"\t\t\"revision\": %d,\n"
					L"\t\t\"pollTime\": %lld,\n"
					L"\t\t\"sensorOffset\": %d,\n"
					L"\t\t\"sensorSize\": %d,\n"
					L"\t\t\"sensorCount\": %d,\n"
					L"\t\t\"readingOffset\": %d,\n"
					L"\t\t\"readingSize\": %d,\n"
					L"\t\t\"readingCount\": %d,\n"
					L"\t\t\"sensors\":\n"
					L"\t\t[",
					hwinfo.signature,
					hwinfo.version,
					hwinfo.revision,
					hwinfo.pollTime,
					hwinfo.sensorOffset,
					hwinfo.sensorSize,
					hwinfo.sensorCount,
					hwinfo.readingOffset,
					hwinfo.readingSize,
					hwinfo.readingCount);

				first = true;

				for (unsigned int i = 0; i < hwinfo.sensorCount; ++i)
				{
					HWINFO_SENSORS_SENSOR* sensor = (HWINFO_SENSORS_SENSOR*)((unsigned char*)sensors + hwinfo.sensorSize * i);

					if (EntryEnabled[entryIndex])
					{
						swprintf(json + wcslen(json),
							L"%hs\n"
							L"\t\t\t{\n"
							L"\t\t\t\t\"entryIndex\": %d,\n"
							L"\t\t\t\t\"sensorId\": %u,\n"
							L"\t\t\t\t\"sensorInst\": %d,\n"
							L"\t\t\t\t\"sensorNameOriginal\": \"%hs\",\n"
							L"\t\t\t\t\"sensorNameUser\": \"%hs\"\n"
							L"\t\t\t}",
							first ? "" : ",",
							entryIndex,
							sensor->sensorId,
							sensor->sensorInst,
							FormatSpecialChar(sensor->sensorNameOriginal),
							FormatSpecialChar(sensor->sensorNameUser));

						first = false;
					}

					++entryIndex;
				}

				swprintf(json + wcslen(json),
					L"\n"
					L"\t\t],\n"
					L"\t\t\"readings\":\n"
					L"\t\t[");

				first = true;

				for (unsigned int i = 0; i < hwinfo.readingCount; ++i)
				{
					HWINFO_SENSORS_READING* reading = (HWINFO_SENSORS_READING*)((unsigned char*)readings + hwinfo.readingSize * i);

					if (EntryEnabled[entryIndex])
					{
						swprintf(json + wcslen(json),
							L"%hs\n"
							L"\t\t\t{\n"
							L"\t\t\t\t\"entryIndex\": %d,\n"
							L"\t\t\t\t\"readingType\": %d,\n"
							L"\t\t\t\t\"sensorIndex\": %d,\n"
							L"\t\t\t\t\"readingId\": %u,\n"
							L"\t\t\t\t\"labelOriginal\": \"%hs\",\n"
							L"\t\t\t\t\"labelUser\": \"%hs\",\n"
							L"\t\t\t\t\"unit\": \"%hs\",\n"
							L"\t\t\t\t\"value\": %lf,\n"
							L"\t\t\t\t\"valueMin\": %lf,\n"
							L"\t\t\t\t\"valueMax\": %lf,\n"
							L"\t\t\t\t\"valueAvg\": %lf\n"
							L"\t\t\t}",
							first ? "" : ",",
							entryIndex,
							reading->readingType,
							reading->sensorIndex,
							reading->readingId,
							FormatSpecialChar(reading->labelOriginal),
							FormatSpecialChar(reading->labelUser),
							FormatSpecialChar(reading->unit),
							reading->value,
							reading->valueMin,
							reading->valueMax,
							reading->valueAvg);

						first = false;
					}

					++entryIndex;
				}

				swprintf(json + wcslen(json),
					L"\n"
					L"\t\t]\n"
					L"\t}");

				free(sensors);
				free(readings);
			}
		}

		if (Gpuz)
		{
			GPUZ_SH_MEM gpuz = { 0 };

			if (GetGpuz(&gpuz))
			{
				if (first_output)
					first_output = false;
				else
					swprintf(json + wcslen(json), L",\n");

				swprintf(json + wcslen(json),
					L"\t\"gpuz\":\n"
					L"\t{\n"
					L"\t\t\"version\": %d,\n"
					L"\t\t\"busy\": %d,\n"
					L"\t\t\"lastUpdate\": %d,\n"
					L"\t\t\"data\":\n"
					L"\t\t[",
					gpuz.version,
					gpuz.busy,
					gpuz.lastUpdate);

				first = true;

				for (unsigned int i = 0; i < 128; ++i)
				{
					if (wcslen(gpuz.data[i].key) > 0)
					{
						if (EntryEnabled[entryIndex])
						{
							swprintf(json + wcslen(json),
								L"%hs\n"
								L"\t\t\t{\n"
								L"\t\t\t\t\"entryIndex\": %d,\n"
								L"\t\t\t\t\"key\": \"%s\",\n"
								L"\t\t\t\t\"value\": \"%s\"\n"
								L"\t\t\t}",
								first ? "" : ",",
								entryIndex,
								FormatSpecialCharUnicode(gpuz.data[i].key),
								FormatSpecialCharUnicode(gpuz.data[i].value));

							first = false;
						}

						++entryIndex;
					}
				}

				swprintf(json + wcslen(json),
					L"\n"
					L"\t\t],\n"
					L"\t\t\"sensors\":\n"
					L"\t\t[");

				first = true;

				for (unsigned int i = 0; i < 128; ++i)
				{
					if (wcslen(gpuz.sensors[i].name) > 0)
					{
						if (EntryEnabled[entryIndex])
						{
							swprintf(json + wcslen(json),
								L"%hs\n"
								L"\t\t\t{\n"
								L"\t\t\t\t\"entryIndex\": %d,\n"
								L"\t\t\t\t\"name\": \"%s\",\n"
								L"\t\t\t\t\"unit\": \"%s\",\n"
								L"\t\t\t\t\"digits\": %d,\n"
								L"\t\t\t\t\"value\": %lf\n"
								L"\t\t\t}",
								first ? "" : ",",
								entryIndex,
								FormatSpecialCharUnicode(gpuz.sensors[i].name),
								FormatSpecialCharUnicode(gpuz.sensors[i].unit),
								gpuz.sensors[i].digits,
								gpuz.sensors[i].value);

							first = false;
						}

						++entryIndex;
					}
				}

				swprintf(json + wcslen(json),
					L"\n"
					L"\t\t]\n"
					L"\t}");
			}
		}

		if (Afterburner)
		{
			MAHM_SHARED_MEMORY_HEADER afterburner = { 0 };

			void* entries = 0;

			if (GetAfterburner(&afterburner, &entries))
			{
				if (first_output)
					first_output = false;
				else
					swprintf(json + wcslen(json), L",\n");

				swprintf(json + wcslen(json),
					L"\t\"afterburner\":\n"
					L"\t{\n"
					L"\t\t\"signature\": %d,\n"
					L"\t\t\"version\": %d,\n"
					L"\t\t\"headerSize\": %d,\n"
					L"\t\t\"entryCount\": %d,\n"
					L"\t\t\"entrySize\": %d,\n"
					L"\t\t\"time\": %d,\n"
					L"\t\t\"entries\":\n"
					L"\t\t[",
					afterburner.signature,
					afterburner.version,
					afterburner.headerSize,
					afterburner.entryCount,
					afterburner.entrySize,
					afterburner.time);

				first = true;

				for (unsigned int i = 0; i < afterburner.entryCount; ++i)
				{
					MAHM_SHARED_MEMORY_ENTRY* entry = (MAHM_SHARED_MEMORY_ENTRY*)((unsigned char*)entries + afterburner.entrySize * i);

					if (EntryEnabled[entryIndex])
					{
						entry->format[strlen(entry->format)] = 0;

						unsigned int digits = atoi(entry->format + 2);

						swprintf(json + wcslen(json),
							L"%hs\n"
							L"\t\t\t{\n"
							L"\t\t\t\t\"entryIndex\": %d,\n"
							L"\t\t\t\t\"name\": \"%hs\",\n"
							L"\t\t\t\t\"units\": \"%hs\",\n"
							L"\t\t\t\t\"localName\": \"%hs\",\n"
							L"\t\t\t\t\"localUnits\": \"%hs\",\n"
							L"\t\t\t\t\"digits\": %d,\n"
							L"\t\t\t\t\"data\": %f,\n"
							L"\t\t\t\t\"minLimit\": %f,\n"
							L"\t\t\t\t\"maxLimit\": %f,\n"
							L"\t\t\t\t\"flags\": %d\n"
							L"\t\t\t}",
							first ? "" : ",",
							entryIndex,
							FormatSpecialChar(entry->name),
							FormatSpecialChar(entry->units),
							FormatSpecialChar(entry->localName),
							FormatSpecialChar(entry->localUnits),
							digits,
							entry->data,
							entry->minLimit,
							entry->maxLimit,
							entry->flags);

						first = false;
					}

					++entryIndex;
				}

				swprintf(json + wcslen(json),
					L"\n"
					L"\t\t]\n"
					L"\t}");

				free(entries);
			}
		}

		swprintf(json + wcslen(json), L"\n}");

		utf8Size = UnicodeToUtf8(json, jsonData);

		free(json);
	}

	return utf8Size;
}
using namespace std::chrono_literals;

unsigned long int __stdcall ClientThread(void* parameter)
{
		SOCKET clientSocket = (SOCKET)parameter;
	
		int received = 0, sent = 0;
		int bufferSize = 1000000;

		char* buffer = 0;
		while (true) {
			

			

			LOG(buffer = (char*)malloc(bufferSize * sizeof(char)));


			if (buffer)
			{

				buffer[received] = 0;

				printf(buffer);

				size_t size = 0;
				//LOG(received = recv(clientSocket, buffer, bufferSize, 0));

				ParseParams(buffer);

				char* jsonData = 0;

				size_t jsonSize = CreateJson(&jsonData);

				printf(buffer, jsonSize);

				size = strlen(buffer);

				if (jsonSize > 0)
				{
					memcpy(buffer + size, jsonData, jsonSize);

					free(jsonData);

					size += jsonSize;
				}

				buffer[size] = 0;

				buffer[size] = 0;

				printf(JsonHeader, jsonSize);




				LOG(sent = send(clientSocket, buffer, (int)size, 0));
				//Sleep(1000);

				free(buffer);

				//return sent;
			}
			Sleep(500);
		//return sent;
		}


	//	LOG(shutdown(clientSocket, SD_BOTH));

		//LOG(closesocket(clientSocket));

		return sent;
	
}
void Hardware::StopServer()
{
	shutdown(serverSocket, SD_BOTH);
	closesocket(serverSocket);
	WSACleanup();
}
void Hardware::CreateServer()
{
	/*printf("\n");
	LOG(setlocale(LC_CTYPE, ""));


	WSADATA wsaData = { 0 };

	if (LOG(WSAStartup(MAKEWORD(2, 2), &wsaData)) == 0)
	{
		serverSocket = INVALID_SOCKET;

		serverSocket = socket(AF_INET, SOCK_STREAM, 0);

		if (serverSocket != INVALID_SOCKET)
		{
			struct sockaddr_in serverAddress = { 0 };

			serverAddress.sin_family = AF_INET;
			serverAddress.sin_addr.S_un.S_addr = INADDR_ANY;
			serverAddress.sin_port = htons(Port);
			if (LOG(bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress))) == 0)
			{
				if (LOG(listen(serverSocket, SOMAXCONN)) == 0)
				{
					while (true)
					{
						
						SOCKET clientSocket = INVALID_SOCKET;

						clientSocket = accept(serverSocket, 0, 0);

						if (clientSocket != INVALID_SOCKET)
						{
							HANDLE clientThread = 0;

							if (LOG(clientThread = CreateThread(0, 0, ClientThread, (void*)clientSocket, 0, 0)) != 0)
									LOG(CloseHandle(clientThread));


						}

					}
				}
			}

			LOG(closesocket(serverSocket));
		}

		LOG(WSACleanup());
	}
	if (HtmlIndexData)
		free(HtmlIndexData);

	if (HtmlNotFoundData)
		free(HtmlNotFoundData);*/

	WSADATA wsData;
	WORD ver = MAKEWORD(2, 2);

	int wsOk = WSAStartup(ver, &wsData);
	if (wsOk != 0)
	{
		cerr << "Can't Initialize winsock! Quitting" << endl;
		return;
	}

	// Create a socket
	SOCKET listening = socket(AF_INET, SOCK_STREAM, 0);

	if (listening == INVALID_SOCKET)
	{
		cerr << "Can't create a socket! Quitting" << endl;
		return;
	}

	// Bind the ip address and port to a socket
	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(Port);
	hint.sin_addr.S_un.S_addr = INADDR_ANY; // Could also use inet_pton .... 
	fd_set master;    // master file descriptor list
	fd_set read_fds;  // temp file descriptor list for select() read
	fd_set write_fds; // temp file descriptor list for select() read
	bind(listening, (sockaddr*)&hint, sizeof(hint));

	// Tell Winsock the socket is for listening 
	listen(listening, SOMAXCONN);

	// Create the master file descriptor set and zero it
	//fd_set master;
	FD_ZERO(&master);    // clear the master and temp sets
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);

	//FD_ZERO(&master);


	// Add our first socket that we're interested in interacting with; the listening socket!
	// It's important that this socket is added for our server or else we won't 'hear' incoming
	// connections 
	FD_SET(listening, &master);

	// this will be changed by the \quit command (see below, bonus not in video!)
	bool running = true;
	int bufferSize = 1024 * 1024;
	int received = 0, sent = 0;

	while (running)
	{
		// Make a copy of the master file descriptor set, this is SUPER important because
		// the call to select() is _DESTRUCTIVE_. The copy only contains the sockets that
		// are accepting inbound connection requests OR messages. 

		// E.g. You have a server and it's master file descriptor set contains 5 items;
		// the listening socket and four clients. When you pass this set into select(), 
		// only the sockets that are interacting with the server are returned. Let's say
		// only one client is sending a message at that time. The contents of 'copy' will
		// be one socket. You will have LOST all the other sockets.

		// SO MAKE A COPY OF THE MASTER LIST TO PASS INTO select() !!!

		fd_set copy = master;

		// See who's talking to us
		//struct timeval tv;
		//tv.tv_sec = 5;
		//tv.tv_usec = 0;
		int socketCount = select(0, &copy, &copy, nullptr, nullptr);//select(0, &copy, nullptr, nullptr, nullptr);

	
		// Loop through all the current connections / potential connect
		for (int i = 0; i < socketCount; i++)
		{

			// Makes things easy for us doing this assignment
			SOCKET sock = copy.fd_array[i];

			// Is it an inbound communication?
			if (sock == listening)
			{
				// Accept a new connection
				SOCKET client = accept(listening, nullptr, nullptr);

				// Add the new connection to the list of connected clients
				FD_SET(client, &master);
				// Send a welcome message to the connected client
				/*string welcomeMsg = "Welcome to the Awesome Chat Server!\r\n";
				send(client, welcomeMsg.c_str(), welcomeMsg.size() + 1, 0);*/

			}
			else {
				for (int i = 0; i < master.fd_count; i++)
				{
					SOCKET outSock = master.fd_array[i];



					char* buffer = 0;

					buffer = (char*)malloc(bufferSize * sizeof(char));

					if (buffer)
					{

						buffer[received] = 0;

						printf(buffer);

						size_t size = 0;
						//LOG(received = recv(clientSocket, buffer, bufferSize, 0));

						ParseParams(buffer);

						char* jsonData = 0;

						size_t jsonSize = CreateJson(&jsonData);

						printf(buffer, jsonSize);

						size = strlen(buffer);

						if (jsonSize > 0)
						{
							memcpy(buffer + size, jsonData, jsonSize);

							free(jsonData);

							size += jsonSize;
						}

						buffer[size] = 0;

						send(outSock, buffer, (int)size, 0);
						//Sleep(1000);

						free(buffer);

						//return sent;

						Sleep(1);

					}

				}
			}
		}
	}
	// Remove the listening socket from the master file descriptor set and close it
	// to prevent anyone else trying to connect.
	FD_CLR(listening, &master);
	closesocket(listening);

	// Message to let users know what's happening.
	string msg = "Server is shutting down. Goodbye\r\n";

	while (master.fd_count > 0)
	{
		// Get the socket number
		SOCKET sock = master.fd_array[0];

		// Send the goodbye message
		send(sock, msg.c_str(), msg.size() + 1, 0);

		// Remove it from the master file list and close the socket
		FD_CLR(sock, &master);
		closesocket(sock);
	}

	// Cleanup winsock
	WSACleanup();


	
}

void PrintUsage()
{
	printf(
		"\n"
		"Usage:\n"
		"-port (60000 = default)\n"
		"-hwinfo (0 = disable; 1 = enable = default)\n"
		"-gpuz (0 = disable; 1 = enable = default)\n"
		"-afterburner (0 = disable; 1 = enable = default)\n"
		"-log (0 = disable; 1 = enable = default)\n"
		"-help\n"
		"\n"
		"http://ip:port/json.json (UTF-8)\n"
		"http://ip:port/json.json?enable=0,1,2,3 (0,1,2,3 = entryIndex)\n"
		"http://ip:port/json.json?disable=0,1,2,3 (0,1,2,3 = entryIndex)\n"
		"http://ip:port/index.html (UTF-8)\n"
		"http://ip:port/404.html (UTF-8)\n");
}

void ParseArgs(int argc, char* argv[])
{
	int arg = 1;

	while (arg < argc)
	{
		if (strcmp(strupr(argv[arg]), "-PORT") == 0)
		{
			if (arg + 1 < argc)
				Port = atoi(argv[++arg]);
		}
		else if (strcmp(strupr(argv[arg]), "-HWINFO") == 0)
		{
			if (arg + 1 < argc)
				Hwinfo = (atoi(argv[++arg]) != 0);
		}
		else if (strcmp(strupr(argv[arg]), "-GPUZ") == 0)
		{
			if (arg + 1 < argc)
				Gpuz = (atoi(argv[++arg]) != 0);
		}
		else if (strcmp(strupr(argv[arg]), "-AFTERBURNER") == 0)
		{
			if (arg + 1 < argc)
				Afterburner = (atoi(argv[++arg]) != 0);
		}
		else if (strcmp(strupr(argv[arg]), "-LOG") == 0)
		{
			if (arg + 1 < argc)
				LogFileEnable = (atoi(argv[++arg]) != 0);
		}
		else if (strcmp(strupr(argv[arg]), "-HELP") == 0)
		{
			Hwinfo = Gpuz = Afterburner = false;
			LogFileEnable = false;

			PrintUsage();
		}

		++arg;
	}
}
#pragma once

