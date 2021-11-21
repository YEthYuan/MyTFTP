#include <cstdio>
#include <iostream>
#include <winsock2.h>
#include <windows.h>
#include <cstring>
#include <algorithm>

/*   定义包类型   */
#define RRQ 1					//读文件请求包：Read request
#define WRQ 2					//写文件请求包：Write requst
#define DATA 3					//文件数据包：Data
#define ACK 4					//回应包：Acknowledgement
#define ERROR 5					//错误信息包：Error

/*   定义常规变量   */
#define BUFFER_SIZE 1024		//缓冲区大小
#define RECV_LOOP_COUNT 6		//超时最大重发次数
#define TIME_OUT_SEC 5			//超时时间

using namespace std;

/*   初始化一些变量   */
FILE* fp = fopen("log.txt", "a+");
SOCKET sServSock;
sockaddr_in addr;
int addrLen = sizeof(addr);

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
		exit(1);
	}
	if (wsaData.wVersion != 0x0101) {
		printf("Winsocket版本支持错误！\n");
		WSACleanup();
		exit(2);
	}
	cout << "请输入服务器IP：";
	char ip[20] = "10.201.179.86";
	cin >> ip;
	cout << "请输入服务器端口号：";
	int port = 69;
	cin >> port;
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr(ip);
	addr.sin_port = htons(port);
	sServSock = socket(AF_INET, SOCK_DGRAM, 0);
	cout << "下载文件请输入1，上传文件请输入2：";
	int op = 1;
	cin >> op;
	if (op != RRQ && op != WRQ) {
		cout << "输入错误\n";
		exit(3);
	}
	cout << "请输入想要传输的文件类型(文本文件-1，二进制文件-2)：";
	int type = 1;
	cin >> type;
	if (type != 1 && type != 2) {
		cout << "输入错误\n";
		exit(3);
	}
	cout << "请输入文件名(含后缀)：";
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
	cout << "平均传输速率：" << speed << "Kbps\n";
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
		cout << "makeRequest Error!\n";
		printTime(); fprintf(fp, "makeRequest Error!\n");
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
	case 0:cout << "未定义："; fprintf(fp, "未定义："); break;
	case 1:cout << "文件未找到："; fprintf(fp, "文件未找到："); break;
	case 2:cout << "访问被拒绝："; fprintf(fp, "访问被拒绝："); break;
	case 3:cout << "磁盘满或超出可分配空间："; fprintf(fp, "磁盘满或超出可分配空间："); break;
	case 4:cout << "非法的TFTP操作:"; fprintf(fp, "非法的TFTP操作:"); break;
	case 5:cout << "未知的传输:"; fprintf(fp, "未知的传输:"); break;
	case 6:cout << "文件已存在:"; fprintf(fp, "文件已存在:"); break;
	case 7:cout << "没有该用户:"; fprintf(fp, "没有该用户:"); break;
	default:cout << "未知错误号：" << (int)buf[3]; fprintf(fp, "未知错误号：%d", (int)buf[3]); break;
	}
	cout << buf + 4 << '\n'; fprintf(fp, "%s\n", buf + 4);
}

bool getFile(const char filename[], int type) //type=1为文本格式(txt)，2为二进制文件
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
					printf("传输失败：重传Request次数过多!\n");
					printTime(); fprintf(fp, "传输失败：重传Request次数过多!\n");
					return false;
				}
				printf("应答超时，重传Request!\n");
				len = sendRequest(RRQ, filename, type);
			}
			else if (len == -1) {
				printf("传输失败：errno=%d\n", WSAGetLastError());
				printTime(); fprintf(fp, "传输失败：errno=%d\n", WSAGetLastError());
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
					if (errortimes < RECV_LOOP_COUNT) {
						cout << "重传：";
						sendACK(blockNum);
					}
					else {
						printf("文件传输失败：未收到正确数据包！\n");
						printTime(); fprintf(fp, "文件传输失败：未收到正确数据包！\n");
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
					printf("传输失败：重传Request次数过多!\n");
					printTime(); fprintf(fp, "传输失败：重传Request次数过多!\n");
					return false;
				}
				printf("应答超时，重传Request!\n");
				len = sendRequest(RRQ, filename, type);
			}
			else if (len == -1) {
				printf("传输失败errno=%d\n", WSAGetLastError());
				printTime(); fprintf(fp, "传输失败errno=%d\n", WSAGetLastError());
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
					if (errortimes < RECV_LOOP_COUNT) {
						cout << "重传：";
						sendACK(blockNum);
					}
					else {
						printf("文件传输失败：未收到正确数据包！\n");
						printTime(); fprintf(fp, "文件传输失败：未收到正确数据包！\n");
						return false;
					}
				}
				if (len < 516) break;
			}
		}
	}
	printf("文件传输成功！\n");
	printTime(); fprintf(fp, "文件传输成功！\n");
	return true;
}

bool pushFile(const char filename[], int type) //type=1为文本格式(txt)，2为二进制文件
{
	char recvbuf[BUFFER_SIZE], sendbuf[BUFFER_SIZE];
	memset(sendbuf, 0, sizeof(sendbuf));
	sendbuf[1] = DATA;
	int senderror = 0;
	if (type == 2) {
		FILE* readfp = fopen(filename, "rb+");
		if (readfp == NULL) {
			printf("传输失败：未能打开此文件\n");
			printTime(); fprintf(fp, "传输失败：未能打开此文件\n");
			exit(4);
		}
		int len = sendRequest(WRQ, filename, type);
		int blocknum = 0;
		while (true) {
			len = recvfrom_time(sServSock, recvbuf, BUFFER_SIZE, (LPSOCKADDR)&addr, &addrLen, sendbuf, len + 4, blocknum);
			if (len == -2) {
				senderror++;
				if (senderror >= RECV_LOOP_COUNT) {
					printf("传输失败：重传Request次数过多!\n");
					printTime(); fprintf(fp, "传输失败：重传Request次数过多!\n");
					return false;
				}
				printf("应答超时，重传Request!\n");
				len = sendRequest(WRQ, filename, type);
			}
			else if (len == -1) {
				printf("传输失败：errno=%d\n", WSAGetLastError());
				printTime(); fprintf(fp, "传输失败：errno=%d\n", WSAGetLastError());
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
			printf("传输失败：未能打开此文件\n");
			printTime(); fprintf(fp, "传输失败：未能打开此文件\n");
			return false;
		}
		int len = sendRequest(WRQ, filename, type);
		int blocknum = 0;
		while (true) {
			len = recvfrom_time(sServSock, recvbuf, BUFFER_SIZE, (LPSOCKADDR)&addr, &addrLen, sendbuf, len + 4, blocknum);
			if (len == -2) {
				senderror++;
				if (senderror >= RECV_LOOP_COUNT) {
					printf("传输失败：重传Request次数过多!\n");
					printTime(); fprintf(fp, "传输失败：重传Request次数过多!\n");
					return false;
				}
				printf("应答超时，重传Request!\n");
				len = sendRequest(WRQ, filename, type);
			}
			else if (len == -1) {
				printf("传输失败：errno=%d\n", WSAGetLastError());
				printTime(); fprintf(fp, "传输失败：errno=%d\n", WSAGetLastError());
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
	printf("文件传输成功！\n");
	printTime(); fprintf(fp, "文件传输成功！\n");
	return true;
}
