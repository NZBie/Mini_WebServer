#include "Reactor.h"

Reactor::Reactor() {

	// root文件夹路径
	char server_path[256];
	getcwd(server_path, sizeof(server_path));
	printf("(%d)", (int)sizeof(server_path));
	char root[6] = "/root";
	_root_path = new char[strlen(server_path) + strlen(root) + 1];
	strcpy(_root_path, server_path);
	strcat(_root_path, root);
}

Reactor::~Reactor() {
	
	close(_epoll_fd);
	close(_listen_fd);
	// delete _conn_pool;
	delete _thread_pool;
}

// 初始化
void Reactor::init(
	int thread_num, int max_task_num,
	string url, short port, string user, string passwd, string dbname) {
	
	init_thread_pool(thread_num, max_task_num);
	init_sql_conn_pool(url, port, user, passwd, dbname);
}

// 初始化 线程池
void Reactor::init_thread_pool(int thread_num, int max_task_num) {
	_thread_pool = new ThreadPool<HttpConn> (thread_num, max_task_num);
}

// 初始化 数据库连接池
void Reactor::init_sql_conn_pool(string url, short port, string user, string passwd, string dbname) {

	_conn_pool = SqlConnPool::get_instance();
	_conn_pool->init(url, port, user, passwd, dbname);
}

// 开启监听socket
void Reactor::event_listen() {
	
	// 创建listen的socket
	_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	assert(_listen_fd >= 0);

	// 设置关闭连接方式
	linger lin = {0, 0};
	setsockopt(_listen_fd, SOL_SOCKET, SO_LINGER, &lin, sizeof(lin));
	
	// 设置socket地址参数
	sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(_port);

	// 防止重启服务器，bind失败
	int flag = 1;
    setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

	// 绑定地址 & 开启监听
	int ret;
	ret = bind(_listen_fd, (sockaddr*)&address, sizeof(address));
	assert(ret >= 0);
	ret = listen(_listen_fd, 30);
	assert(ret >= 0);

	// 创建epoll
	_epoll_fd = epoll_create(5);
	assert(_epoll_fd != -1);

	// 设置HttpConn的_epoll_fd;
}

// 主事件循环体
void Reactor::event_loop() {

	bool server_close = false;
	bool timeout = false;

	while(server_close == false) {

		// epoll等待事件
		int event_num = epoll_wait(_epoll_fd, _events, MAX_EVENT_NUM, -1);
		if(event_num < 0 && errno != EINTR) {
			printf("epoll failure!\n");
			break;
		}

		for(int i = 0; i < event_num; ++i) {

			int sock_fd = _events[i].data.fd;

			// 接收到 新的客户端连接
			if(sock_fd == _listen_fd) {
				if(deal_client_connect() == false) continue;
			}

			// 接收到 关闭服务器的信号
			else if(_events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
				
			}

			// 接收到 定时器信号
			// else if(1) {

			// }

			// 接收到 客户端发来的数据
			else if(_events[i].events & EPOLLIN) {
				deal_client_read(sock_fd);
			}
			
			// 接收到 服务器发来的写事件，写完发送回客户端
			else if(_events[i].events & EPOLLOUT) {
				deal_client_write(sock_fd);
			}
		}
	}
}

// 处理 新的客户端 连接请求
bool Reactor::deal_client_connect() {

	sockaddr_in client_address;
	socklen_t client_address_len = sizeof(client_address);

	// ET边缘触发，一次性循环处理
	while(true) {

		int conn_fd = accept(_listen_fd, (sockaddr *)&client_address, &client_address_len);
		// 连接失败
		if(conn_fd  < 0) {
			printf("accept error");
			break;
		}
		// 服务器连接客户端达到上限
		if(HttpConn::_user_count >= MAX_FD_NUM) {
			printf("Server busy");
			return false;
		}
	}
	return true;
}

// 处理 来自客户端 的读事件
void Reactor::deal_client_read(int client_fd) {

	// 检测到读事件，放入请求队列
	_thread_pool->append(_users + client_fd, 0);
}

// 处理 来自服务器 的写事件
void Reactor::deal_client_write(int client_fd) {

	// 检测到写事件，放入请求队列
	_thread_pool->append(_users + client_fd, 1);
}