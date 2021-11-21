#include <cstdio>
#include <iostream>
#include <winsock2.h>
#include <windows.h>
#include <cstring>
#include <algorithm>

/*   ���������   */
#define RRQ 1					//���ļ��������Read request
#define WRQ 2					//д�ļ��������Write requst
#define DATA 3					//�ļ����ݰ���Data
#define ACK 4					//��Ӧ����Acknowledgement
#define ERROR 5					//������Ϣ����Error

/*   ���峣�����   */
#define BUFFER_SIZE 1024		//��������С
#define RECV_LOOP_COUNT 6		//��ʱ����ط�����
#define TIME_OUT_SEC 5			//��ʱʱ��

using namespace std;

/*   ��ʼ��һЩ����   */
FILE* fp = fopen("log.txt", "a+");
SOCKET sServSock;
sockaddr_in addr;
int addrLen = sizeof(addr);

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
		exit(1);
	}
	if (wsaData.wVersion != 0x0101) {
		printf("Winsocket�汾֧�ִ���\n");
		WSACleanup();
		exit(2);
	}
	cout << "�����������IP��";
	char ip[20] = "10.201.179.86";
	cin >> ip;
	cout << "������������˿ںţ�";
	int port = 69;
	cin >> port;
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr(ip);
	addr.sin_port = htons(port);
	sServSock = socket(AF_INET, SOCK_DGRAM, 0);
	cout << "�����ļ�������1���ϴ��ļ�������2��";
	int op = 1;
	cin >> op;
	if (op != RRQ && op != WRQ) {
		cout << "�������\n";
		exit(3);
	}
	cout << "��������Ҫ������ļ�����(�ı��ļ�-1���������ļ�-2)��";
	int type = 1;
	cin >> type;
	if (type != 1 && type != 2) {
		cout << "�������\n";
		exit(3);
	}
	cout << "�������ļ���(����׺)��";
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
	double speed = size == 0 ? 0 : 8 * size / time / 1000;
	cout << "ƽ���������ʣ�" << speed << "Kbps\n";
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
	for (int i = 0; i < RECV_LOOP_COUNT; i++) {
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
	for (int i = 0; i < RECV_LOOP_COUNT; i++) {
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
		cout << "makeRequest Error!\n";
		printTime(); fprintf(fp, "makeRequest Error!\n");
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
	case 0:cout << "δ���壺"; fprintf(fp, "δ���壺"); break;
	case 1:cout << "�ļ�δ�ҵ���"; fprintf(fp, "�ļ�δ�ҵ���"); break;
	case 2:cout << "���ʱ��ܾ���"; fprintf(fp, "���ʱ��ܾ���"); break;
	case 3:cout << "�������򳬳��ɷ���ռ䣺"; fprintf(fp, "�������򳬳��ɷ���ռ䣺"); break;
	case 4:cout << "�Ƿ���TFTP����:"; fprintf(fp, "�Ƿ���TFTP����:"); break;
	case 5:cout << "δ֪�Ĵ���:"; fprintf(fp, "δ֪�Ĵ���:"); break;
	case 6:cout << "�ļ��Ѵ���:"; fprintf(fp, "�ļ��Ѵ���:"); break;
	case 7:cout << "û�и��û�:"; fprintf(fp, "û�и��û�:"); break;
	default:cout << "δ֪����ţ�" << (int)buf[3]; fprintf(fp, "δ֪����ţ�%d", (int)buf[3]); break;
	}
	cout << buf + 4 << '\n'; fprintf(fp, "%s\n", buf + 4);
}

bool getFile(const char filename[], int type) //type=1Ϊ�ı���ʽ(txt)��2Ϊ�������ļ�
{
	char recvbuf[BUFFER_SIZE];
	if (type == 2) {
		FILE* writefp = fopen(filename, "wb+");
		int len = sendRequest(RRQ, filename, type);
		int blockNum = 0, errortimes = 0, senderror = 0;
		while (true) {
			len = recvfrom_time(sServSock, recvbuf, BUFFER_SIZE, (LPSOCKADDR)&addr, &addrLen, blockNum);
			if (len == -2) {
				senderror++;
				if (senderror >= RECV_LOOP_COUNT) {
					printf("����ʧ�ܣ��ش�Request��������!\n");
					printTime(); fprintf(fp, "����ʧ�ܣ��ش�Request��������!\n");
					return false;
				}
				printf("Ӧ��ʱ���ش�Request!\n");
				len = sendRequest(RRQ, filename, type);
			}
			else if (len == -1) {
				printf("����ʧ�ܣ�errno=%d\n", WSAGetLastError());
				printTime(); fprintf(fp, "����ʧ�ܣ�errno=%d\n", WSAGetLastError());
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
					if (errortimes < RECV_LOOP_COUNT) {
						cout << "�ش���";
						sendACK(blockNum);
					}
					else {
						printf("�ļ�����ʧ�ܣ�δ�յ���ȷ���ݰ���\n");
						printTime(); fprintf(fp, "�ļ�����ʧ�ܣ�δ�յ���ȷ���ݰ���\n");
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
			len = recvfrom_time(sServSock, recvbuf, BUFFER_SIZE, (LPSOCKADDR)&addr, &addrLen, blockNum);
			if (len == -2) {
				senderror++;
				if (senderror >= RECV_LOOP_COUNT) {
					printf("����ʧ�ܣ��ش�Request��������!\n");
					printTime(); fprintf(fp, "����ʧ�ܣ��ش�Request��������!\n");
					return false;
				}
				printf("Ӧ��ʱ���ش�Request!\n");
				len = sendRequest(RRQ, filename, type);
			}
			else if (len == -1) {
				printf("����ʧ��errno=%d\n", WSAGetLastError());
				printTime(); fprintf(fp, "����ʧ��errno=%d\n", WSAGetLastError());
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
					if (errortimes < RECV_LOOP_COUNT) {
						cout << "�ش���";
						sendACK(blockNum);
					}
					else {
						printf("�ļ�����ʧ�ܣ�δ�յ���ȷ���ݰ���\n");
						printTime(); fprintf(fp, "�ļ�����ʧ�ܣ�δ�յ���ȷ���ݰ���\n");
						return false;
					}
				}
				if (len < 516) break;
			}
		}
	}
	printf("�ļ�����ɹ���\n");
	printTime(); fprintf(fp, "�ļ�����ɹ���\n");
	return true;
}

bool pushFile(const char filename[], int type) //type=1Ϊ�ı���ʽ(txt)��2Ϊ�������ļ�
{
	char recvbuf[BUFFER_SIZE], sendbuf[BUFFER_SIZE];
	memset(sendbuf, 0, sizeof(sendbuf));
	sendbuf[1] = DATA;
	int senderror = 0;
	if (type == 2) {
		FILE* readfp = fopen(filename, "rb+");
		if (readfp == NULL) {
			printf("����ʧ�ܣ�δ�ܴ򿪴��ļ�\n");
			printTime(); fprintf(fp, "����ʧ�ܣ�δ�ܴ򿪴��ļ�\n");
			exit(4);
		}
		int len = sendRequest(WRQ, filename, type);
		int blocknum = 0;
		while (true) {
			len = recvfrom_time(sServSock, recvbuf, BUFFER_SIZE, (LPSOCKADDR)&addr, &addrLen, sendbuf, len + 4, blocknum);
			if (len == -2) {
				senderror++;
				if (senderror >= RECV_LOOP_COUNT) {
					printf("����ʧ�ܣ��ش�Request��������!\n");
					printTime(); fprintf(fp, "����ʧ�ܣ��ش�Request��������!\n");
					return false;
				}
				printf("Ӧ��ʱ���ش�Request!\n");
				len = sendRequest(WRQ, filename, type);
			}
			else if (len == -1) {
				printf("����ʧ�ܣ�errno=%d\n", WSAGetLastError());
				printTime(); fprintf(fp, "����ʧ�ܣ�errno=%d\n", WSAGetLastError());
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
					len = fread(sendbuf + 4, 1, 512, readfp);
					if (len == 0) break;
					memcpy(&sendbuf[3], &blocknum, 1);
					int t = blocknum >> 8;
					memcpy(&sendbuf[2], &t, 1);
				}
				sendto(sServSock, sendbuf, len + 4, 0, (LPSOCKADDR)&addr, addrLen);
				printf("sendBlockNum=%d\n", blocknum);
			}
		}
		fclose(readfp);
	}
	else {
		FILE* readfp = fopen(filename, "r+");
		if (readfp == NULL) {
			printf("����ʧ�ܣ�δ�ܴ򿪴��ļ�\n");
			printTime(); fprintf(fp, "����ʧ�ܣ�δ�ܴ򿪴��ļ�\n");
			return false;
		}
		int len = sendRequest(WRQ, filename, type);
		int blocknum = 0;
		while (true) {
			len = recvfrom_time(sServSock, recvbuf, BUFFER_SIZE, (LPSOCKADDR)&addr, &addrLen, sendbuf, len + 4, blocknum);
			if (len == -2) {
				senderror++;
				if (senderror >= RECV_LOOP_COUNT) {
					printf("����ʧ�ܣ��ش�Request��������!\n");
					printTime(); fprintf(fp, "����ʧ�ܣ��ش�Request��������!\n");
					return false;
				}
				printf("Ӧ��ʱ���ش�Request!\n");
				len = sendRequest(WRQ, filename, type);
			}
			else if (len == -1) {
				printf("����ʧ�ܣ�errno=%d\n", WSAGetLastError());
				printTime(); fprintf(fp, "����ʧ�ܣ�errno=%d\n", WSAGetLastError());
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
					len = fread(sendbuf + 4, 1, 512, readfp);
					if (len == 0) break;
					memcpy(&sendbuf[3], &blocknum, 1);
					int t = blocknum >> 8;
					memcpy(&sendbuf[2], &t, 1);
				}
				sendto(sServSock, sendbuf, len + 4, 0, (LPSOCKADDR)&addr, addrLen);
				printf("sendBlockNum=%d\n", blocknum);
			}
		}
		fclose(readfp);
	}
	printf("�ļ�����ɹ���\n");
	printTime(); fprintf(fp, "�ļ�����ɹ���\n");
	return true;
}
