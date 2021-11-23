//
//		计算机网络实验：第一部分-TFTP协议的实现
//		网安1902班 袁也 U201911808
//		日期：2021/11/20
// 
//		网络连接信息：
//		Server IP:		10.12.188.22
//		Server Port:	69
//

//		这里为了防止windows.h与socket库的递归重入
#ifndef _WINSOCK2API_
#ifdef _WINSOCKAPI_
#error Header winsock.h is included unexpectedly.
#endif
#if !defined(WIN32_LEAN_AND_MEAN) && (_WIN32_WINNT >= 0x0400) && !defined(USING_WIN_PSDK)
#include <windows.h>
#else
#include <winsock2.h>
#endif
#endif
#include <cstdio>
#include <iostream>
#include <cstring>
#include <ctime>
#include <list>
#include <algorithm>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "winmm.lib")

/*   定义包类型   */
#define RRQ 1					//读文件请求包：Read request
#define WRQ 2					//写文件请求包：Write requst
#define DATA 3					//文件数据包：Data
#define ACK 4					//回应包：Acknowledgement
#define ERROR 5					//错误信息包：Error

/*   定义错误类型   */
#define _CRT_SECURE_NO_WARNINGS
#define START_UP_ERROR 1
#define VERSION_NOT_SUPPORT 2
#define INPUT_WRONG_COMMAND 3

/*   定义tftp错误码   */
#define NOT_DEFINED 0
#define FILE_NOT_FOUND 1
#define ACCESS_VIOLATION 2
#define ALLOCATION_EXCEEDED 3
#define ILLEGAL_OPERATION 4
#define UNKNOWN_ID 5
#define FILE_ALREADY_EXIST 6
#define USER_NOT_FOUND 7

/*   定义常规变量   */
#define BUF_LEN 1024		//缓冲区大小
#define MAX_CONN 6		//超时最大重发次数
#define TIME_OUT_SEC 5			//超时时间

using namespace std;
template <typename T>
void input(T& t)
{
	char c;
	cin.get(c);
	if (c != '\n')
	{
		cin.putback(c);
		cin >> t;
		cin.get();    //清楚输入后留下的回车，或者直接清空缓存区也可
	}
}

/*   初始化一些变量   */
FILE* fp = fopen("log.txt", "a+");
SOCKET sServSock;
sockaddr_in addr;
int addrLen = sizeof(addr);
char ip[20] = "10.12.188.22";	//服务器IP
int port = 69;					//服务器端口号

/*   声明函数   */
void printTime();
void sendACK(int blocknum);
int recvfrom_time(SOCKET fd, char recvbuf[], size_t buf_n, sockaddr* addr, int* len, const char sendbuf[], int sendbufsize, int blocknum);
int recvfrom_time(SOCKET fd, char recvbuf[], size_t buf_n, sockaddr* addr, int* len, int ACKnum);
int sendRequest(int op, const char filename[], int type);
void dealError(const char buf[]);
bool getFile(const char filename[], int type);
bool pushFile(const char filename[], int type);


int main() 
{
	WSADATA wsaData;
	int nRc = WSAStartup(0x0101, &wsaData);
	if (nRc) {
		exit(START_UP_ERROR);
	}
	if (wsaData.wVersion != 0x0101) {
		printf("抱歉，当前Winsocket版本为0x%X, 本代码仅支持版本0x0101! \n", wsaData.wVersion);
		WSACleanup();
		exit(VERSION_NOT_SUPPORT);
	}

	printf("直接回车使用默认服务器IP %s, 否则, 请输入新的服务器IP并回车:", ip);
	input<char [20]>(ip);

	printf("直接回车使用默认服务器Port %d, 否则, 请输入新的服务器Port并回车:", port);
	input<int>(port);

	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr(ip);
	addr.sin_port = htons(port);
	sServSock = socket(AF_INET, SOCK_DGRAM, 0);

	int mode_sel = 0;
	int op, type;
	printf("\n\n***************************Yuan Ye's TFTP Client***************************\n");
	printf("1. 下载纯文本文件； 2. 上传纯文本文件； 3. 下载二进制文件； 4. 上传二进制文件.\n\n");
	printf("请选择服务：");
	scanf("%d", &mode_sel);
	switch (mode_sel)
	{
	case 1:
		op = 1;
		type = 1;
		break;
	case 2:
		op = 2;
		type = 1;
		break;
	case 3:
		op = 1;
		type = 2;
		break;
	case 4:
		op = 2;
		type = 2;
		break;
	default:
		cout << "输入错误\n";
		exit(INPUT_WRONG_COMMAND);
		break;
	}

	cout << "请输入文件名及后缀(如1.txt)：";
	char filename[20] = "1.txt";
	cin >> filename;
	DWORD start, end;
	start = timeGetTime();
	if (op == RRQ) {
		getFile(filename, type);
	}
	else {
		pushFile(filename, type);
	}
	end = timeGetTime();
	double time = (end - start) * 1.0 / 1000;
	FILE* file = fopen(filename, "r+");
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fclose(file);
	double speed = ((size == 0) ? 0 : (1.0 * size / time / 1024 / 1024));
	cout << "平均传输速率：" << speed << "MB/s\n";
	closesocket(sServSock);
	fclose(fp);
	return 0;
}

void printTime() 
{
	SYSTEMTIME sys;
	GetLocalTime(&sys);
	fprintf(fp, "[%4d/%02d/%02d %02d:%02d:%02d]", sys.wYear, sys.wMonth, sys.wDay, sys.wHour, sys.wMinute, sys.wSecond);
}

void sendACK(int blocknum) 
{
	char buf[4];
	memset(buf, 0, 4);
	buf[1] = ACK;
	memcpy(&buf[3], &blocknum, 1);
	int t = blocknum >> 8;
	memcpy(&buf[2], &t, 1);
	sendto(sServSock, buf, 4, 0, (LPSOCKADDR)&addr, addrLen);
	printf("sendACK=%d\n", blocknum);
}

int recvfrom_time(SOCKET fd, char recvbuf[], size_t buf_n, sockaddr* addr, int* len, const char sendbuf[], int sendbufsize, int blocknum) 
{
	struct timeval tv;
	fd_set readfds;
	int n = 0;
	for (int i = 0; i < MAX_CONN; i++) {
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		tv.tv_sec = TIME_OUT_SEC;
		tv.tv_usec = 0;
		select(fd + 1, &readfds, NULL, NULL, &tv);
		if (FD_ISSET(fd, &readfds)) {
			if ((n = recvfrom(fd, recvbuf, buf_n, 0, addr, len)) >= 0) {
				return n;
			}
		}
		if (blocknum == 0)
			return -2;
		//重传内容
		sendto(fd, sendbuf, sendbufsize, 0, addr, *len);
		printf("超时重传:sendBlockNum=%d!\n", blocknum);
	}
	return -1;
}

int recvfrom_time(SOCKET fd, char recvbuf[], size_t buf_n, sockaddr* addr, int* len, int ACKnum) 
{
	struct timeval tv;
	fd_set readfds;
	int n = 0;
	for (int i = 0; i < MAX_CONN; i++) {
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		tv.tv_sec = TIME_OUT_SEC;
		tv.tv_usec = 0;
		select(fd + 1, &readfds, NULL, NULL, &tv);
		if (FD_ISSET(fd, &readfds)) {
			if ((n = recvfrom(fd, recvbuf, buf_n, 0, addr, len)) >= 0) {
				return n;
			}
		}
		if (ACKnum == 0)
			return -2;
		//重传内容
		sendACK(ACKnum);
		printf("超时重传:");
	}
	return -1;
}

int sendRequest(int op, const char filename[], int type) //type=1为文本格式(txt)，2为二进制文件
{
	char buf[516];
	if (op != RRQ && op != WRQ) {
		cout << "无法创建请求！请重新输入正确的参数！\n";
		printTime(); fprintf(fp, "无法创建请求！请重新输入正确的参数！\n");
		return false;
	}
	printTime();
	if (op == RRQ)
		fprintf(fp, "请求下载数据:%s\n", filename);
	else
		fprintf(fp, "请求上传数据:%s\n", filename);
	memset(buf, 0, sizeof(buf));
	buf[1] = op;
	strcpy(buf + 2, filename);
	int len = strlen(filename) + 3;
	if (type == 2) {
		strcpy(buf + len, "octet");
		return sendto(sServSock, buf, len + 6, 0, (LPSOCKADDR)&addr, addrLen);
	}
	else {
		strcpy(buf + len, "netascii");
		return sendto(sServSock, buf, len + 9, 0, (LPSOCKADDR)&addr, addrLen);
	}
}

void dealError(const char buf[]) 
{
	if (buf[1] != ERROR) return;
	cout << "传输失败："; printTime(); fprintf(fp, "传输失败：");
	switch (buf[3]) {
	case NOT_DEFINED:cout << "[ERROR: 0] 未定义："; fprintf(fp, "[ERROR: 0] 未定义："); break;
	case FILE_NOT_FOUND:cout << "[ERROR: 1] 文件未找到："; fprintf(fp, "[ERROR: 1] 文件未找到："); break;
	case ACCESS_VIOLATION:cout << "[ERROR: 2] 访问被拒绝："; fprintf(fp, "[ERROR: 2] 访问被拒绝："); break;
	case ALLOCATION_EXCEEDED:cout << "[ERROR: 3] 磁盘满或超出可分配空间："; fprintf(fp, "[ERROR: 3] 磁盘满或超出可分配空间："); break;
	case ILLEGAL_OPERATION:cout << "[ERROR: 4] 非法的TFTP操作:"; fprintf(fp, "[ERROR: 4] 非法的TFTP操作:"); break;
	case UNKNOWN_ID:cout << "[ERROR: 5] 未知的传输:"; fprintf(fp, "[ERROR: 5] 未知的传输:"); break;
	case FILE_ALREADY_EXIST:cout << "[ERROR: 6] 文件已存在:"; fprintf(fp, "[ERROR: 6] 文件已存在:"); break;
	case USER_NOT_FOUND:cout << "[ERROR: 7] 没有该用户:"; fprintf(fp, "[ERROR: 7] 没有该用户:"); break;
	default:cout << "[ERROR: ?] 未知错误号：" << (int)buf[3]; fprintf(fp, "[ERROR: ?] 未知错误号：%d", (int)buf[3]); break;
	}
	cout << buf + 4 << '\n'; fprintf(fp, "%s\n", buf + 4);
}

bool getFile(const char filename[], int type) //type=1为文本格式(txt)，2为二进制文件
{
	char recvbuf[BUF_LEN];
	if (type == 2) {
		FILE* writefp = fopen(filename, "wb+");
		int len = sendRequest(RRQ, filename, type);
		int blockNum = 0, errortimes = 0, senderror = 0;
		while (true) {
			len = recvfrom_time(sServSock, recvbuf, BUF_LEN, (LPSOCKADDR)&addr, &addrLen, blockNum);
			if (len == -2) {
				senderror++;
				if (senderror >= MAX_CONN) {
					printf("[ERROR: 0x10] 无法连接到服务器，请检查IP及端口号是否正确设置。这也有可能是因为你错误地设置了防火墙策略。\n");
					printTime(); fprintf(fp, "[ERROR: 0x10] 无法连接到服务器，请检查IP及端口号是否正确设置。这也有可能是因为你错误地设置了防火墙策略。\n");
					return false;
				}
				printf("[WARNING: 0x90] 连接超时，正在尝试重新连接服务器\n");
				len = sendRequest(RRQ, filename, type);
			}
			else if (len == -1) {
				printf("[ERROR: %d]传输失败，请根据提供的错误码检查连接问题！\n", WSAGetLastError());
				printTime(); fprintf(fp, "[ERROR: %d]传输失败，请根据提供的错误码检查连接问题！\n", WSAGetLastError());
				return false;
			}
			else if (recvbuf[1] == ERROR) {
				dealError(recvbuf);
				return false;
			}
			else if (recvbuf[1] == DATA) {
				int recvnum = 0, t = 0;
				memcpy(&recvnum, &recvbuf[2], 1);
				memcpy(&t, &recvbuf[3], 1);
				recvnum = (recvnum << 8) + t;
				if (blockNum + 1 == recvnum) {//正常块
					blockNum++;
					errortimes = 0;
					fwrite(recvbuf + 4, 1, len - 4, writefp);
					sendACK(blockNum);
				}
				else {
					errortimes++;
					if (errortimes < MAX_CONN) {
						cout << "重传：";
						sendACK(blockNum);
					}
					else {
						printf("[ERROR: 0x11] 文件接收错误，请在网络环境允许时重新尝试接收文件！\n");
						printTime(); fprintf(fp, "[ERROR: 0x11] 文件接收错误，请在网络环境允许时重新尝试接收文件！\n");
						return false;
					}
				}
				if (len < 516) break;
			}
		}
	}
	else {
		FILE* writefp = fopen(filename, "w+");
		int len = sendRequest(RRQ, filename, type);
		int blockNum = 0, errortimes = 0, senderror = 0;
		while (true) {
			len = recvfrom_time(sServSock, recvbuf, BUF_LEN, (LPSOCKADDR)&addr, &addrLen, blockNum);
			if (len == -2) {
				senderror++;
				if (senderror >= MAX_CONN) {
					printf("[ERROR: 0x10] 无法连接到服务器，请检查IP及端口号是否正确设置。这也有可能是因为你错误地设置了防火墙策略。\n");
					printTime(); fprintf(fp, "[ERROR: 0x10] 无法连接到服务器，请检查IP及端口号是否正确设置。这也有可能是因为你错误地设置了防火墙策略。\n");
					return false;
				}
				printf("[WARNING: 0x90] 连接超时，正在尝试重新连接服务器\n");
				len = sendRequest(RRQ, filename, type);
			}
			else if (len == -1) {
				printf("[ERROR: %d]传输失败，请根据提供的错误码检查连接问题！\n", WSAGetLastError());
				printTime(); fprintf(fp, "[ERROR: %d]传输失败，请根据提供的错误码检查连接问题！\n", WSAGetLastError());
				return false;
			}
			else if (recvbuf[1] == ERROR) {
				dealError(recvbuf);
				return false;
			}
			else if (recvbuf[1] == DATA) {
				int recvnum = 0, t = 0;
				memcpy(&recvnum, &recvbuf[2], 1);
				memcpy(&t, &recvbuf[3], 1);
				recvnum = (recvnum << 8) + t;
				if (blockNum + 1 == recvnum) {//正常块
					blockNum++;
					recvbuf[len] = 0;
					printTime(); fprintf(writefp, "%s", recvbuf + 4);
					sendACK(blockNum);
				}
				else {
					errortimes++;
					if (errortimes < MAX_CONN) {
						cout << "重传：";
						sendACK(blockNum);
					}
					else {
						printf("[ERROR: 0x11] 文件接收错误，请在网络环境允许时重新尝试接收文件！\n");
						printTime(); fprintf(fp, "[ERROR: 0x11] 文件接收错误，请在网络环境允许时重新尝试接收文件！\n");
						return false;
					}
				}
				if (len < 516) break;
			}
		}
	}
	printf("[ SUCCESS! ] 文件传输成功！\n");
	printTime(); fprintf(fp, "[ SUCCESS! ] 文件传输成功！\n");
	return true;
}

bool pushFile(const char filename[], int type) //type=1为文本格式(txt)，2为二进制文件
{
	char recvbuf[BUF_LEN], sendbuf[BUF_LEN];
	memset(sendbuf, 0, sizeof(sendbuf));
	sendbuf[1] = DATA;
	int senderror = 0;
	if (type == 2) {
		FILE* readfp = fopen(filename, "rb+");
		if (readfp == NULL) {
			printf("[ERROR: 0x12] 无法打开文件：%s，请确认该文件是否在正确的路径下存在。\n", filename);
			printTime(); fprintf(fp, "[ERROR: 0x12] 无法打开文件：%s，请确认该文件是否在正确的路径下存在。\n", filename);
			exit(4);
		}
		int len = sendRequest(WRQ, filename, type);
		int blocknum = 0;
		FILE* file = fopen(filename, "rb+");
		fseek(file, 0, SEEK_END);
		long total_size = ftell(file);
		fclose(file);
		long current_size = 0;
		while (true) {
			len = recvfrom_time(sServSock, recvbuf, BUF_LEN, (LPSOCKADDR)&addr, &addrLen, sendbuf, len + 4, blocknum);
			if (len == -2) {
				senderror++;
				if (senderror >= MAX_CONN) {
					printf("[ERROR: 0x11] 无法连接到服务器，请检查IP及端口号是否正确设置。这也有可能是因为你错误地设置了防火墙策略。\n");
					printTime(); fprintf(fp, "[ERROR: 0x11] 无法连接到服务器，请检查IP及端口号是否正确设置。这也有可能是因为你错误地设置了防火墙策略。\n");
					return false;
				}
				printf("[WARNING: 0x90] 连接超时，正在尝试重新连接服务器\n");
				len = sendRequest(WRQ, filename, type);
			}
			else if (len == -1) {
				printf("[ERROR: %d]传输失败，请根据提供的错误码检查连接问题！\n", WSAGetLastError());
				printTime(); fprintf(fp, "[ERROR: %d]传输失败，请根据提供的错误码检查连接问题！\n", WSAGetLastError());
				return false;
			}
			else if (recvbuf[1] == ERROR) {
				dealError(recvbuf);
				return false;
			}
			else if (recvbuf[1] == ACK) {
				int recvnum = 0, t = 0;
				memcpy(&recvnum, &recvbuf[2], 1);
				memcpy(&t, &recvbuf[3], 1);
				recvnum = (recvnum << 8) + t;
				if (blocknum == recvnum) {//正常块
					blocknum++;
					if (blocknum == 65536) blocknum = 0;
					current_size += 512;
					len = fread(sendbuf + 4, 1, 512, readfp);
					if (len == 0) break;
					memcpy(&sendbuf[3], &blocknum, 1);
					int t = blocknum >> 8;
					memcpy(&sendbuf[2], &t, 1);
				}
				sendto(sServSock, sendbuf, len + 4, 0, (LPSOCKADDR)&addr, addrLen);
				printf(" ... 正在传输Block %d\t... 传输进度 [%.2f %%] .......................... \r", blocknum, 100.0 * current_size / total_size);
			}
		}
		fclose(readfp);
	}
	else {
		FILE* readfp = fopen(filename, "r+");
		if (readfp == NULL) {
			printf("[ERROR: 0x12] 无法打开文件：%s，请确认该文件是否在正确的路径下存在。\n", filename);
			printTime(); fprintf(fp, "[ERROR: 0x12] 无法打开文件：%s，请确认该文件是否在正确的路径下存在。\n", filename);
			exit(4);
		}
		int len = sendRequest(WRQ, filename, type);
		int blocknum = 0;
		FILE* file = fopen(filename, "r+");
		fseek(file, 0, SEEK_END);
		long total_size = ftell(file);
		fclose(file);
		long current_size = 0;
		while (true) {
			len = recvfrom_time(sServSock, recvbuf, BUF_LEN, (LPSOCKADDR)&addr, &addrLen, sendbuf, len + 4, blocknum);
			if (len == -2) {
				senderror++;
				if (senderror >= MAX_CONN) {
					printf("[ERROR: 0x11] 无法连接到服务器，请检查IP及端口号是否正确设置。这也有可能是因为你错误地设置了防火墙策略。\n");
					printTime(); fprintf(fp, "[ERROR: 0x11] 无法连接到服务器，请检查IP及端口号是否正确设置。这也有可能是因为你错误地设置了防火墙策略。\n");
					return false;
				}
				printf("[WARNING: 0x90] 连接超时，正在尝试重新连接服务器\n");
				len = sendRequest(WRQ, filename, type);
			}
			else if (len == -1) {
				printf("[ERROR: %d]传输失败，请根据提供的错误码检查连接问题！\n", WSAGetLastError());
				printTime(); fprintf(fp, "[ERROR: %d]传输失败，请根据提供的错误码检查连接问题！\n", WSAGetLastError());
				return false;
			}
			else if (recvbuf[1] == ERROR) {
				dealError(recvbuf);
				return false;
			}
			else if (recvbuf[1] == ACK) {
				int recvnum = 0, t = 0;
				memcpy(&recvnum, &recvbuf[2], 1);
				memcpy(&t, &recvbuf[3], 1);
				recvnum = (recvnum << 8) + t;
				if (blocknum == recvnum) {//正常块
					blocknum++;
					if (blocknum == 65536) blocknum = 0;
					current_size += 512;
					len = fread(sendbuf + 4, 1, 512, readfp);
					if (len == 0) break;
					memcpy(&sendbuf[3], &blocknum, 1);
					int t = blocknum >> 8;
					memcpy(&sendbuf[2], &t, 1);
				}
				sendto(sServSock, sendbuf, len + 4, 0, (LPSOCKADDR)&addr, addrLen);
				printf(" ... 正在传输Block %d\t... 传输进度 [%.2f %%] .......................... \r", blocknum, 100.0 * current_size / total_size);
			}
		}
		fclose(readfp);
	}
	printf("[ SUCCESS! ] 文件传输成功！\n");
	printTime(); fprintf(fp, "[ SUCCESS! ] 文件传输成功！\n");
	return true;
}
