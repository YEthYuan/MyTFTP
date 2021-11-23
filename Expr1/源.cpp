//
//		���������ʵ�飺��һ����-TFTPЭ���ʵ��
//		����1902�� ԬҲ U201911808
//		���ڣ�2021/11/20
// 
//		����������Ϣ��
//		Server IP:		10.12.188.22
//		Server Port:	69
//

//		����Ϊ�˷�ֹwindows.h��socket��ĵݹ�����
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

/*   ���������   */
#define RRQ 1					//���ļ��������Read request
#define WRQ 2					//д�ļ��������Write requst
#define DATA 3					//�ļ����ݰ���Data
#define ACK 4					//��Ӧ����Acknowledgement
#define ERROR 5					//������Ϣ����Error

/*   �����������   */
#define _CRT_SECURE_NO_WARNINGS
#define START_UP_ERROR 1
#define VERSION_NOT_SUPPORT 2
#define INPUT_WRONG_COMMAND 3

/*   ����tftp������   */
#define NOT_DEFINED 0
#define FILE_NOT_FOUND 1
#define ACCESS_VIOLATION 2
#define ALLOCATION_EXCEEDED 3
#define ILLEGAL_OPERATION 4
#define UNKNOWN_ID 5
#define FILE_ALREADY_EXIST 6
#define USER_NOT_FOUND 7

/*   ���峣�����   */
#define BUF_LEN 1024		//��������С
#define MAX_CONN 6		//��ʱ����ط�����
#define TIME_OUT_SEC 5			//��ʱʱ��

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
		cin.get();    //�����������µĻس�������ֱ����ջ�����Ҳ��
	}
}

/*   ��ʼ��һЩ����   */
FILE* fp = fopen("log.txt", "a+");
SOCKET sServSock;
sockaddr_in addr;
int addrLen = sizeof(addr);
char ip[20] = "10.12.188.22";	//������IP
int port = 69;					//�������˿ں�

/*   ��������   */
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
		printf("��Ǹ����ǰWinsocket�汾Ϊ0x%X, �������֧�ְ汾0x0101! \n", wsaData.wVersion);
		WSACleanup();
		exit(VERSION_NOT_SUPPORT);
	}

	printf("ֱ�ӻس�ʹ��Ĭ�Ϸ�����IP %s, ����, �������µķ�����IP���س�:", ip);
	input<char [20]>(ip);

	printf("ֱ�ӻس�ʹ��Ĭ�Ϸ�����Port %d, ����, �������µķ�����Port���س�:", port);
	input<int>(port);

	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr(ip);
	addr.sin_port = htons(port);
	sServSock = socket(AF_INET, SOCK_DGRAM, 0);

	int mode_sel = 0;
	int op, type;
	printf("\n\n***************************Yuan Ye's TFTP Client***************************\n");
	printf("1. ���ش��ı��ļ��� 2. �ϴ����ı��ļ��� 3. ���ض������ļ��� 4. �ϴ��������ļ�.\n\n");
	printf("��ѡ�����");
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
		cout << "�������\n";
		exit(INPUT_WRONG_COMMAND);
		break;
	}

	cout << "�������ļ�������׺(��1.txt)��";
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
	cout << "ƽ���������ʣ�" << speed << "MB/s\n";
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
		//�ش�����
		sendto(fd, sendbuf, sendbufsize, 0, addr, *len);
		printf("��ʱ�ش�:sendBlockNum=%d!\n", blocknum);
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
		//�ش�����
		sendACK(ACKnum);
		printf("��ʱ�ش�:");
	}
	return -1;
}

int sendRequest(int op, const char filename[], int type) //type=1Ϊ�ı���ʽ(txt)��2Ϊ�������ļ�
{
	char buf[516];
	if (op != RRQ && op != WRQ) {
		cout << "�޷���������������������ȷ�Ĳ�����\n";
		printTime(); fprintf(fp, "�޷���������������������ȷ�Ĳ�����\n");
		return false;
	}
	printTime();
	if (op == RRQ)
		fprintf(fp, "������������:%s\n", filename);
	else
		fprintf(fp, "�����ϴ�����:%s\n", filename);
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
	cout << "����ʧ�ܣ�"; printTime(); fprintf(fp, "����ʧ�ܣ�");
	switch (buf[3]) {
	case NOT_DEFINED:cout << "[ERROR: 0] δ���壺"; fprintf(fp, "[ERROR: 0] δ���壺"); break;
	case FILE_NOT_FOUND:cout << "[ERROR: 1] �ļ�δ�ҵ���"; fprintf(fp, "[ERROR: 1] �ļ�δ�ҵ���"); break;
	case ACCESS_VIOLATION:cout << "[ERROR: 2] ���ʱ��ܾ���"; fprintf(fp, "[ERROR: 2] ���ʱ��ܾ���"); break;
	case ALLOCATION_EXCEEDED:cout << "[ERROR: 3] �������򳬳��ɷ���ռ䣺"; fprintf(fp, "[ERROR: 3] �������򳬳��ɷ���ռ䣺"); break;
	case ILLEGAL_OPERATION:cout << "[ERROR: 4] �Ƿ���TFTP����:"; fprintf(fp, "[ERROR: 4] �Ƿ���TFTP����:"); break;
	case UNKNOWN_ID:cout << "[ERROR: 5] δ֪�Ĵ���:"; fprintf(fp, "[ERROR: 5] δ֪�Ĵ���:"); break;
	case FILE_ALREADY_EXIST:cout << "[ERROR: 6] �ļ��Ѵ���:"; fprintf(fp, "[ERROR: 6] �ļ��Ѵ���:"); break;
	case USER_NOT_FOUND:cout << "[ERROR: 7] û�и��û�:"; fprintf(fp, "[ERROR: 7] û�и��û�:"); break;
	default:cout << "[ERROR: ?] δ֪����ţ�" << (int)buf[3]; fprintf(fp, "[ERROR: ?] δ֪����ţ�%d", (int)buf[3]); break;
	}
	cout << buf + 4 << '\n'; fprintf(fp, "%s\n", buf + 4);
}

bool getFile(const char filename[], int type) //type=1Ϊ�ı���ʽ(txt)��2Ϊ�������ļ�
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
					printf("[ERROR: 0x10] �޷����ӵ�������������IP���˿ں��Ƿ���ȷ���á���Ҳ�п�������Ϊ�����������˷���ǽ���ԡ�\n");
					printTime(); fprintf(fp, "[ERROR: 0x10] �޷����ӵ�������������IP���˿ں��Ƿ���ȷ���á���Ҳ�п�������Ϊ�����������˷���ǽ���ԡ�\n");
					return false;
				}
				printf("[WARNING: 0x90] ���ӳ�ʱ�����ڳ����������ӷ�����\n");
				len = sendRequest(RRQ, filename, type);
			}
			else if (len == -1) {
				printf("[ERROR: %d]����ʧ�ܣ�������ṩ�Ĵ��������������⣡\n", WSAGetLastError());
				printTime(); fprintf(fp, "[ERROR: %d]����ʧ�ܣ�������ṩ�Ĵ��������������⣡\n", WSAGetLastError());
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
				if (blockNum + 1 == recvnum) {//������
					blockNum++;
					errortimes = 0;
					fwrite(recvbuf + 4, 1, len - 4, writefp);
					sendACK(blockNum);
				}
				else {
					errortimes++;
					if (errortimes < MAX_CONN) {
						cout << "�ش���";
						sendACK(blockNum);
					}
					else {
						printf("[ERROR: 0x11] �ļ����մ����������绷������ʱ���³��Խ����ļ���\n");
						printTime(); fprintf(fp, "[ERROR: 0x11] �ļ����մ����������绷������ʱ���³��Խ����ļ���\n");
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
					printf("[ERROR: 0x10] �޷����ӵ�������������IP���˿ں��Ƿ���ȷ���á���Ҳ�п�������Ϊ�����������˷���ǽ���ԡ�\n");
					printTime(); fprintf(fp, "[ERROR: 0x10] �޷����ӵ�������������IP���˿ں��Ƿ���ȷ���á���Ҳ�п�������Ϊ�����������˷���ǽ���ԡ�\n");
					return false;
				}
				printf("[WARNING: 0x90] ���ӳ�ʱ�����ڳ����������ӷ�����\n");
				len = sendRequest(RRQ, filename, type);
			}
			else if (len == -1) {
				printf("[ERROR: %d]����ʧ�ܣ�������ṩ�Ĵ��������������⣡\n", WSAGetLastError());
				printTime(); fprintf(fp, "[ERROR: %d]����ʧ�ܣ�������ṩ�Ĵ��������������⣡\n", WSAGetLastError());
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
				if (blockNum + 1 == recvnum) {//������
					blockNum++;
					recvbuf[len] = 0;
					printTime(); fprintf(writefp, "%s", recvbuf + 4);
					sendACK(blockNum);
				}
				else {
					errortimes++;
					if (errortimes < MAX_CONN) {
						cout << "�ش���";
						sendACK(blockNum);
					}
					else {
						printf("[ERROR: 0x11] �ļ����մ����������绷������ʱ���³��Խ����ļ���\n");
						printTime(); fprintf(fp, "[ERROR: 0x11] �ļ����մ����������绷������ʱ���³��Խ����ļ���\n");
						return false;
					}
				}
				if (len < 516) break;
			}
		}
	}
	printf("[ SUCCESS! ] �ļ�����ɹ���\n");
	printTime(); fprintf(fp, "[ SUCCESS! ] �ļ�����ɹ���\n");
	return true;
}

bool pushFile(const char filename[], int type) //type=1Ϊ�ı���ʽ(txt)��2Ϊ�������ļ�
{
	char recvbuf[BUF_LEN], sendbuf[BUF_LEN];
	memset(sendbuf, 0, sizeof(sendbuf));
	sendbuf[1] = DATA;
	int senderror = 0;
	if (type == 2) {
		FILE* readfp = fopen(filename, "rb+");
		if (readfp == NULL) {
			printf("[ERROR: 0x12] �޷����ļ���%s����ȷ�ϸ��ļ��Ƿ�����ȷ��·���´��ڡ�\n", filename);
			printTime(); fprintf(fp, "[ERROR: 0x12] �޷����ļ���%s����ȷ�ϸ��ļ��Ƿ�����ȷ��·���´��ڡ�\n", filename);
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
					printf("[ERROR: 0x11] �޷����ӵ�������������IP���˿ں��Ƿ���ȷ���á���Ҳ�п�������Ϊ�����������˷���ǽ���ԡ�\n");
					printTime(); fprintf(fp, "[ERROR: 0x11] �޷����ӵ�������������IP���˿ں��Ƿ���ȷ���á���Ҳ�п�������Ϊ�����������˷���ǽ���ԡ�\n");
					return false;
				}
				printf("[WARNING: 0x90] ���ӳ�ʱ�����ڳ����������ӷ�����\n");
				len = sendRequest(WRQ, filename, type);
			}
			else if (len == -1) {
				printf("[ERROR: %d]����ʧ�ܣ�������ṩ�Ĵ��������������⣡\n", WSAGetLastError());
				printTime(); fprintf(fp, "[ERROR: %d]����ʧ�ܣ�������ṩ�Ĵ��������������⣡\n", WSAGetLastError());
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
				if (blocknum == recvnum) {//������
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
				printf(" ... ���ڴ���Block %d\t... ������� [%.2f %%] .......................... \r", blocknum, 100.0 * current_size / total_size);
			}
		}
		fclose(readfp);
	}
	else {
		FILE* readfp = fopen(filename, "r+");
		if (readfp == NULL) {
			printf("[ERROR: 0x12] �޷����ļ���%s����ȷ�ϸ��ļ��Ƿ�����ȷ��·���´��ڡ�\n", filename);
			printTime(); fprintf(fp, "[ERROR: 0x12] �޷����ļ���%s����ȷ�ϸ��ļ��Ƿ�����ȷ��·���´��ڡ�\n", filename);
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
					printf("[ERROR: 0x11] �޷����ӵ�������������IP���˿ں��Ƿ���ȷ���á���Ҳ�п�������Ϊ�����������˷���ǽ���ԡ�\n");
					printTime(); fprintf(fp, "[ERROR: 0x11] �޷����ӵ�������������IP���˿ں��Ƿ���ȷ���á���Ҳ�п�������Ϊ�����������˷���ǽ���ԡ�\n");
					return false;
				}
				printf("[WARNING: 0x90] ���ӳ�ʱ�����ڳ����������ӷ�����\n");
				len = sendRequest(WRQ, filename, type);
			}
			else if (len == -1) {
				printf("[ERROR: %d]����ʧ�ܣ�������ṩ�Ĵ��������������⣡\n", WSAGetLastError());
				printTime(); fprintf(fp, "[ERROR: %d]����ʧ�ܣ�������ṩ�Ĵ��������������⣡\n", WSAGetLastError());
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
				if (blocknum == recvnum) {//������
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
				printf(" ... ���ڴ���Block %d\t... ������� [%.2f %%] .......................... \r", blocknum, 100.0 * current_size / total_size);
			}
		}
		fclose(readfp);
	}
	printf("[ SUCCESS! ] �ļ�����ɹ���\n");
	printTime(); fprintf(fp, "[ SUCCESS! ] �ļ�����ɹ���\n");
	return true;
}
