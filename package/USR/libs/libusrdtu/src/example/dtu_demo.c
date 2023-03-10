/*
 * USR DTU demo
 *
 * author yanlufei@usr.cn
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/epoll.h>

#include <sys/timerfd.h>
#include <sys/time.h>

#include "usrdtu.h"

#define MAX_EVENT 10

#define KB 1024
#define BUF_SIZE (10 * KB)

#define DEVNAME "/dev/ttyS0"
#define HELLOMSG "[usr dtu] hello world!\r\n";

int program_flag = 0;

int shell_get(const char *cmd, char *buffer_out, unsigned int len)
{
	FILE *fp;
	memset(buffer_out, 0, len);
	fp = popen(cmd, "r");
	if (fp)
	{
		fgets(buffer_out, len, fp);
		strtok(buffer_out, "\r\n"); // Replace split character with \0
		if (strlen(buffer_out) >= len)
			buffer_out[len - 1] = 0;
		pclose(fp);
	}
	else
	{
		return -1;
	}

	return 0;
}

int param_init(DTU_PARAM *param)
{
	char buf[10];
	int result = 0;
	result = shell_get("uci get usr_dtu.uart2.baud", buf, sizeof(buf));
	param->baund = result ? 115200 : atoi(buf);
	printf("get param->baund: %d\n", param->baund);

	result = shell_get("uci get usr_dtu.uart2.parity", buf, sizeof(buf));
	param->parity_type = result ? 0 : atoi(buf);
	printf("get param->parity_type: %d\n", param->parity_type);

	result = shell_get("uci get usr_dtu.uart2.data", buf, sizeof(buf));
	param->data_bits = result ? 8 : atoi(buf);
	printf("get param->data_bits: %d\n", param->data_bits);

	result = shell_get("uci get usr_dtu.uart2.stop", buf, sizeof(buf));
	param->stop_bits = result ? 1 : atoi(buf);
	printf("get param->stop_bits: %d\n", param->stop_bits);

	result = shell_get("uci get usr_dtu.uart2.flow", buf, sizeof(buf));
	param->flow_type = result ? 0 : atoi(buf);
	printf("get param->flow_type: %d\n", param->flow_type);

	result = shell_get("uci get usr_dtu.uart2.mode", buf, sizeof(buf));
	param->mode_type = result ? 0 : atoi(buf);
	printf("get param->mode_type: %d\n", param->mode_type);

	result = shell_get("uci get usr_dtu.uart2.ft", buf, sizeof(buf));
	param->pack_period = result ? 10 : atoi(buf);
	printf("get param->pack_period: %d\n", param->pack_period);

	result = shell_get("uci get usr_dtu.uart2.fl", buf, sizeof(buf));
	param->pack_length = result ? 1000 : atoi(buf);
	printf("get param->pack_length: %d\n", param->pack_length);
	return 0;
}

int socket_param_init(char *server, int server_len, int *port)
{
	char buff[5] = {0};
	char *p = server;

	memset(buff, 0, sizeof(buff));
	memset(p, 0, sizeof(server_len));

	if (shell_get("uci get usr_dtu.socket.sa_enable", buff, sizeof(buff)) == 0)
	{
		if (strstr(buff, "ON"))
		{
			if (shell_get("uci get usr_dtu.socket.sa_server", p, server_len) < 0)
			{
				return 0;
			}
			if (shell_get("uci get usr_dtu.socket.sa_port", buff, sizeof(buff)) < 0)
			{
				return 0;
			}
			else
			{
				*port = atoi(buff);
				if (*port <= 0)
				{
					return 0;
				}
			}
		}
		else
		{
			return 0;
		}
	}
	printf("get socket_param server : %s\n", server);
	printf("get socket_param port : %d\n", *port);
	return 1;
}

int socket_create(char *server, int port)
{
	int fd = -1, on = 1;
	int flags;
	struct sockaddr_in addr_socket;
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
		return -1;
	}

	// if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
	// {
	// 	printf("setsockopt error: %s(errno: %d)\n", strerror(errno), errno);
	// 	return -1;
	// }

	memset(&addr_socket, 0, sizeof(addr_socket));
	addr_socket.sin_family = AF_INET;
	addr_socket.sin_port = htons(port);
	if (inet_pton(AF_INET, server, &addr_socket.sin_addr) <= 0)
	{
		printf("inet_pton error for %s\n", server);
		return -1;
	}

	if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
	{
		printf("fcntl F_GETFL error: %s(errno: %d)\n", strerror(errno), errno);
		return -1;
	}

	if (connect(fd, (struct sockaddr *)&addr_socket, sizeof(addr_socket)) < 0)
	{
		printf("connect error: %s(errno: %d)\n", strerror(errno), errno);
		return -1;
	}

	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
	{
		printf("fcntl F_SETFL error: %s(errno: %d)\n", strerror(errno), errno);
		return -1;
	}

	return fd;
}

int socket_destroy(int fd)
{
	return close(fd);
}

static void sig_handler(int sigio)
{
	if ((sigio == SIGQUIT) || (sigio == SIGINT) || (sigio == SIGTERM) || (sigio == SIGKILL))
	{
		program_flag = -1;
	}
	else if (sigio == SIGPIPE)
	{
		return;
	}
	return;
}

int main()
{
	DTU_PARAM param;
	char buff[BUF_SIZE] = {0};
	char server[128] = {0};
	int port = 0;
	int socket_enable = -1;
	int dog_flag = 0;
	int fd_serial, fd_socket, fd_epoll, fd_timer;
	int wait_count = 0;

	struct sigaction sigact;
	struct epoll_event event, events[MAX_EVENT];
	struct itimerspec timer_value;

	uint64_t flag;

	int data_len, i;

	// 获取串口参数
	param_init(&param);
	param.devname = DEVNAME;
	param.hello_msg = HELLOMSG;

	// 获取网络参数
	socket_enable = socket_param_init(server, sizeof(server), &port);

	// 串口初始化,获取串口文件描述符
	printf("usrdtu_create\n");
	if ((fd_serial = usrdtu_create(param)) <= 0)
	{
		printf("dtu init failed\n");
		goto __cheanup;
	}

	// 定时器初始化
	printf("timerfd_create\n");
	if ((fd_timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)) <= 0)
	{
		printf("timer init failed\n");
		goto __cheanup;
	}

	timer_value.it_value.tv_sec = 1;
	timer_value.it_value.tv_nsec = 0;
	timer_value.it_interval.tv_sec = 20;
	timer_value.it_interval.tv_nsec = 0;

	if (timerfd_settime(fd_timer, 0, &timer_value, NULL) < 0)
	{
		printf("timerfd_settime failed\n");
		goto __cheanup;
	}

	// 重定向信号处理
	printf("set sigaction\n");
	sigemptyset(&sigact.sa_mask); // configure signal handling
	sigact.sa_flags = 0;
	sigact.sa_handler = sig_handler;
	sigaction(SIGQUIT, &sigact, NULL); // "Ctrl-\"
	sigaction(SIGINT, &sigact, NULL);  // "Ctrl-C"
	sigaction(SIGTERM, &sigact, NULL); // default "kill" command
	sigaction(SIGKILL, &sigact, NULL); // default "kill" command

	// epoll 初始化
	printf("epoll_create\n");
	if ((fd_epoll = epoll_create1(EPOLL_CLOEXEC)) < 0)
	{
		printf("create epoll error: %s(errno: %d)\n", strerror(errno), errno);
		goto __cheanup;
	}

	event.data.fd = fd_serial;
	event.events = EPOLLIN | EPOLLOUT | EPOLLET;
	if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_serial, &event))
	{
		printf("epoll_ctl serial error: %s(errno: %d)\n", strerror(errno), errno);
		goto __cheanup;
	}

	event.data.fd = fd_timer;
	event.events = EPOLLIN;
	if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_timer, &event))
	{
		printf("epoll_ctl timer error: %s(errno: %d)\n", strerror(errno), errno);
		goto __cheanup;
	}

	//网络初始化
	if (socket_enable)
	{
		printf("socket_create\n");
		if ((fd_socket = socket_create(server, port)) <= 0)
		{
			printf("socket init failed\n");
			goto __cheanup;
		}

		event.data.fd = fd_socket;
		event.events = EPOLLIN | EPOLLOUT | EPOLLET;
		if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_socket, &event))
		{
			printf("epoll_ctl socket error: %s(errno: %d)\n", strerror(errno), errno);
			goto __cheanup;
		}
	}

	//循环执行
	printf("while loop\n");
	while (!program_flag)
	{
		wait_count = epoll_wait(fd_epoll, events, MAX_EVENT, 1000);
		usleep(10000);
		memset(buff, 0, BUF_SIZE);
		for (i = 0; i < wait_count; i++)
		{
			if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP || (!events[i].events & EPOLLIN))
			{
				printf("Epoll has error\n");
				close(events[i].data.fd);
				continue;
			}
			else if (events[i].events & EPOLLIN)
			{
				if (events[i].data.fd == fd_serial)
				{
					//printf("fd_serial EPOLLIN\n");
					while ((data_len = usrdtu_rceive_data(fd_serial, buff, BUF_SIZE)) > 0)
					{
						if (socket_enable)
						{
							//printf("socket_enable, data_len=%d, send buff: %s\n", data_len, buff);
							if (send(fd_socket, buff, data_len < BUF_SIZE ? data_len : BUF_SIZE, 0) < 0)
							{
								if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
								{
									printf("fd_socket write error\n");
									goto __cheanup;
								}
							}
						}
						usleep(1000);
					}
					// else
					// {
					// 	printf("fd_serial read error\n");
					// 	goto __cheanup;
					// }
				}
				else if ((socket_enable) && (events[i].data.fd == fd_socket))
				{
					//printf("fd_socket EPOLLIN\n");
					while ((data_len = recv(fd_socket, buff, BUF_SIZE, 0)) > 0)
					{
						if (usrdtu_send_data(fd_serial, buff, data_len < BUF_SIZE ? data_len : BUF_SIZE) < 0)
						{
							printf("fd_serial write error\n");
							goto __cheanup;
						}
						// 喂狗标记置位
						dog_flag = 1;
					}
					// else
					// {
					// 	printf("fd_socket read error\n");
					// 	goto __cheanup;
					// }
				}
				else if (events[i].data.fd == fd_timer)
				{
					//如果在时间内没有给串口发送数据,需要给看门狗喂狗
					if (read(fd_timer, &flag, sizeof(uint64_t)) == sizeof(uint64_t))
					{
						if (!dog_flag)
						{
							printf("feed serial dog\n");
							usrdtu_dog(fd_serial);
						}
						dog_flag = 0;
					}
				}
			}
			else if (events[i].events & EPOLLOUT)
			{
				if (events[i].data.fd == fd_serial)
				{
					// printf("fd_serial EPOLLOUT\n");
				}
				else if (events[i].data.fd == fd_socket)
				{
					// printf("fd_socket EPOLLOUT\n");
				}
			}
			else if (events[i].events & EPOLLET)
			{
				if (events[i].data.fd == fd_serial)
				{
					// printf("fd_serial EPOLLET\n");
				}
				else if (events[i].data.fd == fd_socket)
				{
					// printf("fd_socket EPOLLET\n");
				}
			}
		}
	}

__cheanup:
	// epoll 销毁
	if (fd_epoll > 0)
		close(fd_epoll);

	// 定时器销毁
	if (fd_timer > 0)
		close(fd_timer);

	//套接字销毁
	if (fd_socket > 0)
		socket_destroy(fd_socket);

	//串口销毁
	if (fd_serial > 0)
		usrdtu_destroy(fd_serial);

	return 0;
}
