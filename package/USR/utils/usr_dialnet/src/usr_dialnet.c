#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h> /* memset */
#include <ctype.h>	/*isdigit*/
#include <termios.h>
#include <signal.h> /* sigaction */
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include "gpio_opt.h"
#include <stdarg.h>

#define SUCCESS (0)
#define FAILURE (-1)
#define NEED_POWER_RESET (-99)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define USE_DTS_LABEL 1

#ifdef USE_DTS_LABEL
#define LED_SIGNAL_SGN0 "/sys/class/leds/green:sgn0/brightness"
#define LED_SIGNAL_SGN1 "/sys/class/leds/green:sgn1/brightness"
#define LED_SIGNAL_SGN2 "/sys/class/leds/green:sgn2/brightness"

#define LED_MODE0 "/sys/class/leds/red:mode0/brightness"
#define LED_MODE1 "/sys/class/leds/green:mode1/brightness"

#define LTE_POWER_CONTROL "/sys/class/gpio/modem-power/value"
#define LTE_RESET "/sys/class/gpio/modem-reset/value"

#define dialnet_setval gpio_opt_setval_withlabel

#define LED_ON 1
#define LED_OFF 0

#define POWER_ON 1
#define POWER_OFF 0

#else
#define LED_SIGNAL_SGN0 31
#define LED_SIGNAL_SGN1 35
#define LED_SIGNAL_SGN2 34

#define LED_MODE0 33
#define LED_MODE1 32

#define LTE_POWER_CONTROL 64
#define LTE_RESET 25

#define dialnet_setval gpio_opt_setval

#define LED_ON 0
#define LED_OFF 1

#define POWER_ON 1
#define POWER_OFF 0

#endif

#define READCOM (0)
#define WRITECOM (1)

#define MAX_BUF_SIZE (1024)
#define CUR_BRATE (115200)

// 90秒，等待一个开机时间
#define TTYNOTFIND_FAILED_MAX (90)
// AT指令失败最大次数
#define AT_FAILED_MAX (5)
// 串口发送失败最大次数
// AT指令等待返回的最大时间s
#define RECV_WAIT_5 (5)
#define RECV_WAIT_30 (30)
#define RECV_WAIT_60 (60)

#define EC25 (1)
#define G405TF (2)

#define LOG(fmt, ...)                                                                           \
	{                                                                                           \
		char buffer[1024] = {0};                                                                \
		snprintf(buffer, sizeof(buffer), "[%s : %d]: " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
		printf("%s", buffer);                                                                   \
	}

#define EC20TF_NET_SUM 16
static char ec20_net_name[][20] = {"NONE", "CDMA1X", "CDMA1XANDHDR", "CDMA1XANDEHRPD", "HDR", "HDR-EHRPD", "GSM", "GPRS", "EDGE", "WCDMA", "HSDPA", "HSUPA", "HSPA+", "TDSCDMA", "TDDLTE", "FDD LTE"};

typedef struct st_com_para_en
{
	char i_com_name[128];
} st_com_para;

st_com_para istcompara = {0};

typedef enum
{
	EFINDUSBSTA = 0, // 等待系统加载USB 状态
	ERESET,			 // 模块复位
	POWEROFF,		 // 断电重启
	EINITMODEL,		 // 初始化模块
	CHECKIMEI,		 // 查询IMEI
	CHECKCPIN,		 // 查询CPIN
	CHECKICCID,		 // 查询ICCID
	CHECKIMSI,		 // 查询IMSI
	CHECKAPN,		 // 设置APN
	ESTICKNET,		 // 检查是否附着网络
	GOBINET,		 // 拨号
	CHECKNWINFOCSQ,	 // 查询网络制式、信号值
	EGETNETSTA,		 // 检查网络链路
} e_model_sta;

typedef enum
{
	LTE_MODE_NONE = 0,
	LTE_MODE_2G,
	LTE_MODE_3G,
	LTE_MODE_4G,
} lte_mode;

enum
{
	UNKNOW = 0,
	CM,
	CU,
	CT,
	GD, // 广电
};

typedef struct
{
	char name[127];
	char user[127];
	char psw[127];
	char auth[8];
} apn_data_t;

struct modemdev_t
{
	char operator;
	int csq;
	lte_mode ltemode;
	apn_data_t apn;
};

struct modemdev_t modem = {0};

int recv_com_len = 0;
int read_handle = -1;
int write_handle = -1;
char c_buf[MAX_BUF_SIZE] = {0};

void str_replace_chr(char *str, char c, char r)
{
	int i;

	for (i = 0; i < strlen(str); i++)
	{
		if (str[i] == c)
		{
			str[i] = r;
		}
	}
}

/*
获取指令返回数据的第几个参数
*/
int module_getIndex_para(int index, char *p_in_para, char *p_out_para, int len)
{
	int count = 0;
	int char_num = 0, leftlen = len;
	char *p_str = NULL, *ptoken = NULL;
	char token[512] = {0};

	memset(p_out_para, 0, len);

	p_str = strchr(p_in_para, ':'); // 查找':'略过指令

	if (!p_str)
		return FAILURE;

	p_str++; // 略过":"

	while (1)
	{
		if ((*p_str != ',') && (*p_str != '\r') && (*p_str != '\n') && (*p_str != 0))
		{
			token[char_num] = *p_str;
			char_num++;
		}
		else
		{
			count++;
			token[char_num] = '\0';

			if (count == index)
			{

				ptoken = token;
				if (char_num > 2)
				{
					if (token[0] == '\"' && token[char_num - 1] == '\"')
					{
						ptoken++;
						token[char_num - 1] = '\0';
					}
				}
				if (len > strlen(ptoken))
					memcpy(p_out_para, ptoken, strlen(ptoken));
				else
					memcpy(p_out_para, ptoken, len);

				return SUCCESS;
			}
			memset(token, 0, sizeof(token));
			char_num = 0;

			if ((*p_str == '\r') || (*p_str == '\n') || (*p_str == 0))
			{
				break;
			}
		}
		p_str++;
	}
	return FAILURE;
}

/*
控制模块复位重启
*/
void module_reset_restart(void)
{
	dialnet_setval(LTE_RESET, POWER_OFF);
	sleep(10);
	dialnet_setval(LTE_RESET, POWER_ON);
}

/*
控制模块断电重启
*/
void module_power_restart(void)
{
	dialnet_setval(LTE_POWER_CONTROL, POWER_OFF);
	dialnet_setval(LTE_RESET, POWER_OFF);
	sleep(10);
	dialnet_setval(LTE_RESET, POWER_ON);
	dialnet_setval(LTE_POWER_CONTROL, POWER_ON);
}

/*
控制信号灯
*/
void module_signal_led_control(int signal)
{
	if (signal >= 100) // 兼容g405
	{
		if ((signal <= 199) && (signal >= 145))
		{
			dialnet_setval(LED_SIGNAL_SGN0, LED_ON);
			dialnet_setval(LED_SIGNAL_SGN1, LED_ON);
			dialnet_setval(LED_SIGNAL_SGN2, LED_ON);
		}
		else if ((signal < 145) && (signal >= 130))
		{
			dialnet_setval(LED_SIGNAL_SGN0, LED_ON);
			dialnet_setval(LED_SIGNAL_SGN1, LED_ON);
			dialnet_setval(LED_SIGNAL_SGN2, LED_OFF);
		}
		else if ((signal < 130) && (signal >= 100))
		{
			dialnet_setval(LED_SIGNAL_SGN0, LED_ON);
			dialnet_setval(LED_SIGNAL_SGN1, LED_OFF);
			dialnet_setval(LED_SIGNAL_SGN2, LED_OFF);
		}
		// else if((signal < 115) && (signal >0))
		// {
		//     dialnet_setval(LED_SIGNAL_SGN0, LED_ON);
		//     dialnet_setval(LED_SIGNAL_SGN1, LED_OFF);
		//     dialnet_setval(LED_SIGNAL_SGN2, LED_OFF);
		//     dialnet_setval(LED_SIGNAL_SGN3, LED_OFF);
		// }
		else
		{
			dialnet_setval(LED_SIGNAL_SGN0, LED_OFF);
			dialnet_setval(LED_SIGNAL_SGN1, LED_OFF);
			dialnet_setval(LED_SIGNAL_SGN2, LED_OFF);
		}
	}
	else
	{
		if ((signal <= 31) && (signal >= 25))
		{
			dialnet_setval(LED_SIGNAL_SGN0, LED_ON);
			dialnet_setval(LED_SIGNAL_SGN1, LED_ON);
			dialnet_setval(LED_SIGNAL_SGN2, LED_ON);
		}
		else if ((signal < 25) && (signal >= 15))
		{
			dialnet_setval(LED_SIGNAL_SGN0, LED_ON);
			dialnet_setval(LED_SIGNAL_SGN1, LED_ON);
			dialnet_setval(LED_SIGNAL_SGN2, LED_OFF);
		}
		else if ((signal < 15) && (signal > 0))
		{
			dialnet_setval(LED_SIGNAL_SGN0, LED_ON);
			dialnet_setval(LED_SIGNAL_SGN1, LED_OFF);
			dialnet_setval(LED_SIGNAL_SGN2, LED_OFF);
		}
		else
		{
			dialnet_setval(LED_SIGNAL_SGN0, LED_OFF);
			dialnet_setval(LED_SIGNAL_SGN1, LED_OFF);
			dialnet_setval(LED_SIGNAL_SGN2, LED_OFF);
		}
	}
}

/*
控制网络制式灯
*/
void module_net_led_control(int net_value)
{

	switch (net_value)
	{
	case LTE_MODE_2G: // 2G
		dialnet_setval(LED_MODE0, LED_ON);
		dialnet_setval(LED_MODE1, LED_OFF);
		break;

	case LTE_MODE_3G: // 3G
		dialnet_setval(LED_MODE0, LED_OFF);
		dialnet_setval(LED_MODE1, LED_ON);
		break;
	case LTE_MODE_4G: // 4G
		dialnet_setval(LED_MODE0, LED_ON);
		dialnet_setval(LED_MODE1, LED_ON);
		break;

	default: // others
		dialnet_setval(LED_MODE0, LED_OFF);
		dialnet_setval(LED_MODE1, LED_OFF);
		break;
	}
}

/*
转换波特率
*/
unsigned int con_ver_sion_brate(unsigned long i_brate)
{
	switch (i_brate)
	{
	case 2400:
		return B2400;

	case 4800:
		return B4800;

	case 9600:
		return B9600;

	case 19200:
		return B19200;

	case 38400:
		return B38400;
		break;

	case 57600:
		return B57600;

	case 115200:
		return B115200;

	case 1152000:
		return B1152000;

	default:
		return B115200;
	}
}

/*
串口初始化
*/
int com_recv_send_init(char *params, int i_open_stey)
{
	int m_com_seriver_port = -1;
	unsigned int i_brate = 0;

	if (params == NULL)
		return m_com_seriver_port;

	st_com_para i_com_para;
	memcpy(&i_com_para, params, sizeof(st_com_para));

	i_brate = con_ver_sion_brate(CUR_BRATE);

	struct termios options;
	if (i_open_stey == READCOM)
		m_com_seriver_port = open(i_com_para.i_com_name, O_RDONLY | O_NDELAY); // 采用O_NDELAY
	else if (i_open_stey == WRITECOM)
		m_com_seriver_port = open(i_com_para.i_com_name, O_WRONLY);

	if (m_com_seriver_port < 0)
	{
		LOG("open com failed!\n");
		return m_com_seriver_port;
	}

	if (tcgetattr(m_com_seriver_port, &options) != 0)
	{
		perror("getserialattr");
		return m_com_seriver_port;
	}

	options.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK | ISTRIP | INLCR | IGNCR | ICRNL | IUCLC | IXON | IXOFF | IXANY);
	options.c_lflag &= ~(ECHO | ECHONL | ISIG | IEXTEN | ICANON);
	options.c_cflag |= CLOCAL | CREAD;
	options.c_oflag &= ~OPOST;

	cfsetispeed(&options, (speed_t)i_brate);
	cfsetospeed(&options, (speed_t)i_brate);

	if (i_open_stey == READCOM)
	{
		// 使用此方式做非阻塞，无作用
		// options.c_cc[VTIME] = 30;	//3s
		// options.c_cc[VMIN] = 1;

		// 非阻塞设置
		options.c_cc[VTIME] = 0;
		options.c_cc[VMIN] = 1;
	}

	tcflush(m_com_seriver_port, TCIFLUSH);

	if (tcsetattr(m_com_seriver_port, TCSANOW, &options) != 0)
	{
		perror("set com attr");
		return m_com_seriver_port;
	}

	LOG("open com port succ\n");

	return m_com_seriver_port;
}

/*
串口去初始化
*/
void com_recv_send_destroy(int m_com_seriver_port)
{
	int i = 0;

	if (m_com_seriver_port != -1)
	{
		close(m_com_seriver_port);

		for (i = 0; i < 5; i++)
		{
			if (m_com_seriver_port == -1)
				return;
			else
				close(m_com_seriver_port);
		}
	}

	return;
}

/*
串口发送
*/
int com_send(int m_com_seriver_port, char *p_cbuf, int iLen)
{
	int iWriteLen = 0;

	if (NULL == p_cbuf || iLen <= 0)
	{
		LOG("send buf is NULL.\n");
		return FAILURE;
	}
	else
	{
		iWriteLen = write(m_com_seriver_port, p_cbuf, iLen);
		if (iLen != iWriteLen)
		{
			LOG("send com fail.\n");
			return FAILURE;
		}
	}
	sleep(1);

	return SUCCESS;
}

/*
串口非阻塞读取
*/
int com_recv(int m_com_seriver_port, char *p_cbuf, int iLen, const char *expectstr1, const char *expectstr2, int wait_time)
{
	int i_recv_len = 0;
	int i_recv_len_store = 0;
	int i_time_out = 0;
	char *str = NULL;
	char *str1 = NULL;

	i_time_out = wait_time;
	while (i_time_out--)
	{

		i_recv_len = read(m_com_seriver_port, p_cbuf + i_recv_len_store, iLen - 1 - i_recv_len_store);
		sleep(1);
		if (i_recv_len > 0)
		{
			// 记录长度
			i_recv_len_store = i_recv_len_store + i_recv_len;
			p_cbuf[i_recv_len_store] = 0;

			if (expectstr1 != NULL)
			{
				if (NULL != (str = strstr(p_cbuf, expectstr1))) // 指向string2在string1中首次出现的位置,记录当前位置
				{

					if (expectstr2 != NULL)
					{
						if (NULL != (str1 = strstr(str, expectstr2))) // 继续上次的位置搜索
						{
							return i_recv_len_store;
						}
						else
							sleep(1);
					}
					else
					{
						return i_recv_len_store;
					}
				}
				else
					sleep(1);
			}
			else
			{
				return i_recv_len_store;
			}
		}
		else
			sleep(1);
	}
	return FAILURE;
}

int check_usb_exist(char *c_dev_path)
{
	int i_return_code = 0;
	struct stat tmpstat;
	if (NULL == c_dev_path)
	{
		i_return_code = 1;
	}
	else
	{
		if (stat(c_dev_path, &tmpstat) != 0)
		{
			LOG("LTE model USB not exist!\n");
			i_return_code = 1;
		}
	}

	return i_return_code;
}

/*
shell获取单行
*/
int shell_get_for_single(const char *cmd, char *buffer_out, unsigned int len)
{
	FILE *fp;

	fp = popen(cmd, "r");
	if (fp)
	{
		// 从指定的流 stream 读取一行，并把它存储在 str 所指向的字符串内。
		// 当读取 (n-1) 个字符时，或者读取到换行符时，或者到达文件末尾时，它会停止.
		// 如若该行（包括最后一个换行符）的字符数超过n-1，则fgets只返回一个不完整的行，但是，缓冲区总是以NULL字符结尾
		fgets(buffer_out, len, fp);
		strtok(buffer_out, "\r\n"); // 分割字符时,则会将该字符改为\0 字符
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

int get_popen(char *buf, int buf_sz, const char *format, ...)
{
	va_list arg;
	char cmd[1024];

	buf[0] = '\0';
	va_start(arg, format);
	vsprintf(cmd, format, arg);
	va_end(arg);

	FILE *fp = popen(cmd, "r");
	if (!fp)
	{
		// LOG_STD(APP_ERR, MODULE, "popen error");
		return -1;
	}

	int ret = fread(buf, 1, buf_sz - 1, fp);
	if (ret > 0)
	{
		pclose(fp);
		buf[ret] = '\0';
		return 0;
	}

	pclose(fp);
	return -1;
}

/*
 * 获取模块类型
 * return： 1，EC25， 2，G405TF -1，失败
 */
int lte_get_module_type()
{
	char s_usb_list_buf[MAX_BUF_SIZE] = {0};

	shell_get_for_single("lsusb | grep 2c7c:0125", s_usb_list_buf, sizeof(s_usb_list_buf));
	if (strlen(s_usb_list_buf) > 0)
	{

		return EC25;
	}

	shell_get_for_single("lsusb | grep 19d2:0579", s_usb_list_buf, sizeof(s_usb_list_buf));
	if (strlen(s_usb_list_buf) > 0)
	{
		return G405TF;
	}

	return -1;
}

int ec20_check_ip()
{
	// 查询是否有IP
	memset(c_buf, 0, sizeof(c_buf));
	shell_get_for_single("ifconfig  usb0 | grep \"inet addr\" | awk -F \":\" '{print $2}' | awk '{print $1}'", c_buf, sizeof(c_buf));
	if (strlen(c_buf) > 0)
	{
		LOG("IP = [%s] \r\n", c_buf);
		return SUCCESS;
	}
	else
	{
		return FAILURE;
	}
}

int check_dns()
{
	char dns[128] = {0};
	get_popen(dns, sizeof(dns), "cat /etc/resolv.conf");

	// 检查dns
	if (strstr(dns, "8.8.8.8") && strstr(dns, "119.29.29.29"))
	{
		return SUCCESS;
	}

	// 设置dns
	system("echo nameserver 8.8.8.8 >> /etc/resolv.conf");
	system("echo nameserver 119.29.29.29 >> /etc/resolv.conf");

	get_popen(dns, sizeof(dns), "cat /etc/resolv.conf");
	// 检查dns
	if (strstr(dns, "8.8.8.8") && strstr(dns, "119.29.29.29"))
	{
		return SUCCESS;
	}

	return FAILURE;
}

int g405tf_check_ip()
{
	// 查询是否有IP
	memset(c_buf, 0, sizeof(c_buf));
	shell_get_for_single("ifconfig  eth1 | grep \"inet addr\" | awk -F \":\" '{print $2}' | awk '{print $1}'", c_buf, sizeof(c_buf));
	if (strlen(c_buf) > 0)
	{
		LOG("IP = [%s] \r\n", c_buf);
		return SUCCESS;
	}
	else
	{
		return FAILURE;
	}
}

int ec20_find_usb_sta()
{
	char tty_buf[50] = {0};

	// 等待系统识别到USB口
	memset(c_buf, 0, sizeof(c_buf));
	memset(tty_buf, 0, sizeof(tty_buf));
	shell_get_for_single("ls /dev|grep ttyUSB*|awk \'NR==3{print}\'|awk '{printf $0}'", c_buf, sizeof(c_buf));
	if (strlen(c_buf) > 5 && strlen(c_buf) < 10) // ttyusbx
	{
		sprintf(tty_buf, "/dev/%s", c_buf);
		memcpy(istcompara.i_com_name, tty_buf, strlen(tty_buf));
		LOG("ttyUSB = [%s] \r\n", tty_buf);

		if (0 == check_usb_exist(istcompara.i_com_name))
		{
			return SUCCESS;
		}
		else
		{
			return FAILURE;
		}
	}
	else
	{
		return FAILURE;
	}
}

int ec20_module_init()
{
	char *str = NULL;
	char *str1 = NULL;

	// 去初始化AT口收发handle
	com_recv_send_destroy(read_handle);
	com_recv_send_destroy(write_handle);

	// 初始化AT口收发handel
	read_handle = com_recv_send_init((char *)&istcompara, READCOM);
	write_handle = com_recv_send_init((char *)&istcompara, WRITECOM);

	// 关闭回显
	com_send(write_handle, "ATE0\r\n", strlen("ATE0\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "OK", NULL, RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")))
	{
		LOG("ATE0 OK\r\n");
	}
	else
	{
		return FAILURE;
	}

	// 查询模块信息
	// AT+CFUN? 回复+CFUN: 1 确认模块正常工作模式。不是 1 则设置成 1
	com_send(write_handle, "AT+CFUN?\r\n", strlen("AT+CFUN?\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "+CFUN:", "OK", RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != (str = strstr(c_buf, "+CFUN:"))))
	{
		// AT指令的返回结果都是<CR><LF>开头和结尾,例外情况ATV0(返回结果0<CR>)和ATQ1(无返回结果)；
		// 解释
		LOG("CFUN = [%s] \r\n", str);
		str1 = str + strlen("+CFUN: ");
		// strncmp(str1,"1", 1);
		if (str1[0] != '1')
		{
			com_send(write_handle, "AT+CFUN=1\r\n", strlen("AT+CFUN=1\r\n"));
			usleep(5);
			memset(c_buf, 0, sizeof(c_buf));
			// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
			recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "OK", NULL, RECV_WAIT_5);
			if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")))
			{
				LOG("SET CFUN = 1\r\n");
			}
			else
			{
				return FAILURE;
			}
		}
	}
	else
	{
		return FAILURE;
	}

	return SUCCESS;
}

int g405tf_module_init()
{
	char *str = NULL;
	char *str1 = NULL;

	// 去初始化AT口收发handle
	com_recv_send_destroy(read_handle);
	com_recv_send_destroy(write_handle);

	// 初始化AT口收发handel
	read_handle = com_recv_send_init((char *)&istcompara, READCOM);
	write_handle = com_recv_send_init((char *)&istcompara, WRITECOM);

	// 关闭回显
	com_send(write_handle, "ATE0\r\n", strlen("ATE0\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "OK", NULL, RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")))
	{
		LOG("ATE0 OK\r\n");
	}
	else
	{
		return FAILURE;
	}

	// 查询模块信息
	// AT+CFUN? 回复+CFUN: 1 确认模块正常工作模式。不是 1 则设置成 1
	com_send(write_handle, "AT+CFUN?\r\n", strlen("AT+CFUN?\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "+CFUN:", "OK", RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != (str = strstr(c_buf, "+CFUN:"))))
	{
		// AT指令的返回结果都是<CR><LF>开头和结尾,例外情况ATV0(返回结果0<CR>)和ATQ1(无返回结果)；
		// 解释
		LOG("CFUN = [%s] \r\n", str);
		str1 = str + strlen("+CFUN: ");
		// strncmp(str1,"1", 1);
		if (str1[0] != '1')
		{
			com_send(write_handle, "AT+CFUN=1\r\n", strlen("AT+CFUN=1\r\n"));
			usleep(5);
			memset(c_buf, 0, sizeof(c_buf));
			// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
			recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "OK", NULL, RECV_WAIT_5);
			if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")))
			{
				LOG("SET CFUN = 1\r\n");
			}
			else
			{
				return FAILURE;
			}
		}
	}
	else
	{
		return FAILURE;
	}

	return SUCCESS;
}

int ec20_module_get_imei()
{
	// AT+CGSN? 查询 IMEI，每个模块唯一
	com_send(write_handle, "AT+CGSN\r\n", strlen("AT+CGSN\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "OK", NULL, RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")))
	{
		// AT指令的返回结果都是<CR><LF>开头和结尾,例外情况ATV0(返回结果0<CR>)和ATQ1(无返回结果)；
		char CGSN[128] = {0};
		strtok(c_buf, "\r\n"); // 分割字符时,则会将该字符改为\0 字符
		strcpy(CGSN, c_buf);   // strcpy把含有'\0'结束符的字符串复制到另一个地址空间,要检测目的缓冲区和源缓冲区非空和长度

		LOG("IMEI = [%s] \r\n", CGSN);
	}
	else
	{
		return FAILURE;
	}

	return SUCCESS;
}

int g405tf_module_get_imei()
{
	// AT+CGSN? 查询 IMEI，每个模块唯一
	com_send(write_handle, "AT+CGSN\r\n", strlen("AT+CGSN\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "OK", NULL, RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")))
	{
		// AT指令的返回结果都是<CR><LF>开头和结尾,例外情况ATV0(返回结果0<CR>)和ATQ1(无返回结果)；
		char CGSN[128] = {0};
		strtok(c_buf, "\r\n"); // 分割字符时,则会将该字符改为\0 字符
		strcpy(CGSN, c_buf);   // strcpy把含有'\0'结束符的字符串复制到另一个地址空间,要检测目的缓冲区和源缓冲区非空和长度

		LOG("IMEI = [%s] \r\n", CGSN);
	}
	else
	{
		return FAILURE;
	}

	return SUCCESS;
}

int ec20_module_get_cpin()
{
	// 查询是否插了SIM卡
	com_send(write_handle, "AT+CPIN?\r\n", strlen("AT+CPIN?\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "+CPIN:", "OK", RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != strstr(c_buf, "+CPIN:")) && (NULL != strstr(c_buf, "READY")))
	{
		return SUCCESS;
	}
	else
	{
		return FAILURE;
	}
}

int g405tf_module_get_cpin()
{
	// 查询是否插了SIM卡
	com_send(write_handle, "AT+CPIN?\r\n", strlen("AT+CPIN?\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "+CPIN:", "OK", RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != strstr(c_buf, "+CPIN:")) && (NULL != strstr(c_buf, "READY")))
	{
		return SUCCESS;
	}
	else
	{
		return FAILURE;
	}
}

int ec20_module_get_iccid()
{
	char *str = NULL;
	char tmp[50] = {0};

	com_send(write_handle, "AT+CIMI\r\n", strlen("AT+CIMI\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "OK", NULL, RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")))
	{
		str = strstr(c_buf, "+ICCID:");

		if (str != NULL)
		{
			str_replace_chr(str, '\r', '\0');

			module_getIndex_para(1, str, tmp, sizeof(tmp));
			if (strlen(tmp) > 0)
			{
				LOG("iccid %s\n", tmp);
			}
		}
		return SUCCESS;
	}
	return FAILURE;
}

int g405tf_module_get_iccid()
{
	char *str = NULL;
	char tmp[50] = {0};

	com_send(write_handle, "AT+CIMI\r\n", strlen("AT+CIMI\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "OK", NULL, RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")))
	{
		str = strstr(c_buf, "+ICCID:");

		if (str != NULL)
		{
			str_replace_chr(str, '\r', '\0');

			module_getIndex_para(1, str, tmp, sizeof(tmp));
			if (strlen(tmp) > 0)
			{
				LOG("iccid %s\n", tmp);
			}
		}
		return SUCCESS;
	}
	return FAILURE;
}

int ec20_module_get_imsi()
{
	char *str = NULL;

	// AT+CIMI 获取IMSI识别码（获取运营商）
	com_send(write_handle, "AT+CIMI\r\n", strlen("AT+CIMI\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "OK", NULL, RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")))
	{
		// AT指令的返回结果都是<CR><LF>开头和结尾,例外情况ATV0(返回结果0<CR>)和ATQ1(无返回结果)；
		// 解释
		char CIMI[128] = {0};
		str = &c_buf[0];
		while (*str) // 遍历str中的每个元素
		{
			if (isdigit(*str)) // isdigit(*str)的作用是判断str的首个元素内容是不是0-9之间的阿拉伯数字
			{
				strtok(str, "\r\n"); // 分割字符时,则会将该字符改为\0 字符
				strcpy(CIMI, str);	 // strcpy把含有'\0'结束符的字符串复制到另一个地址空间,要检测目的缓冲区和源缓冲区非空和长度

				LOG("CIMI = [%s] \r\n", CIMI);
				break;
			}
			str++; // 将字符串的指针向后移动一个字符
		}

		if (strlen(CIMI))
		{
			// IMSI是15位的十进制数。MCC+MNC+MSIN，南非14位；
			// 国家码 MCC 占3位，中国460
			// 移动网络代码MNC，2位（欧洲标准）或3位数字（北美标准）；
			// 中国移动系统使用00、02、04、07、08，中国联通GSM系统使用01、06、09，中国电信CDMA系统使用03、05、电信4G使用11，中国铁通系统使用20；

			if ((strncmp(&CIMI[0] + 3, "00", 2) == 0) || (strncmp(&CIMI[0] + 3, "02", 2) == 0) || (strncmp(&CIMI[0] + 3, "04", 2) == 0) || (strncmp(&CIMI[0] + 3, "07", 2) == 0) || (strncmp(&CIMI[0] + 3, "08", 2) == 0))
			{
				modem.operator= CM;
			}
			else if ((strncmp(&CIMI[0] + 3, "01", 2) == 0) || (strncmp(&CIMI[0] + 3, "06", 2) == 0) || (strncmp(&CIMI[0] + 3, "09", 2) == 0))
			{
				modem.operator= CU;
			}

			else if ((strncmp(&CIMI[0] + 3, "03", 2) == 0) || (strncmp(&CIMI[0] + 3, "05", 2) == 0) || (strncmp(&CIMI[0] + 3, "11", 2) == 0))
			{
				modem.operator= CT;
			}
			else
			{
				modem.operator= UNKNOW;
			}
		}

		return SUCCESS;
	}
	else
	{
		return FAILURE;
	}
}

int g405tf_module_get_imsi()
{
	char *str = NULL;

	// AT+CIMI 获取IMSI识别码（获取运营商）
	com_send(write_handle, "AT+CIMI\r\n", strlen("AT+CIMI\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "OK", NULL, RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")))
	{
		// AT指令的返回结果都是<CR><LF>开头和结尾,例外情况ATV0(返回结果0<CR>)和ATQ1(无返回结果)；
		// 解释
		char CIMI[128] = {0};
		str = &c_buf[0];
		while (*str) // 遍历str中的每个元素
		{
			if (isdigit(*str)) // isdigit(*str)的作用是判断str的首个元素内容是不是0-9之间的阿拉伯数字
			{
				strtok(str, "\r\n"); // 分割字符时,则会将该字符改为\0 字符
				strcpy(CIMI, str);	 // strcpy把含有'\0'结束符的字符串复制到另一个地址空间,要检测目的缓冲区和源缓冲区非空和长度

				LOG("CIMI = [%s] \r\n", CIMI);
				break;
			}
			str++; // 将字符串的指针向后移动一个字符
		}

		if (strlen(CIMI))
		{
			// IMSI是15位的十进制数。MCC+MNC+MSIN，南非14位；
			// 国家码 MCC 占3位，中国460
			// 移动网络代码MNC，2位（欧洲标准）或3位数字（北美标准）；
			// 中国移动系统使用00、02、04、07、08，中国联通GSM系统使用01、06、09，中国电信CDMA系统使用03、05、电信4G使用11，中国铁通系统使用20；

			if ((strncmp(&CIMI[0] + 3, "00", 2) == 0) || (strncmp(&CIMI[0] + 3, "02", 2) == 0) || (strncmp(&CIMI[0] + 3, "04", 2) == 0) || (strncmp(&CIMI[0] + 3, "07", 2) == 0) || (strncmp(&CIMI[0] + 3, "08", 2) == 0))
			{
				modem.operator= CM;
			}
			else if ((strncmp(&CIMI[0] + 3, "01", 2) == 0) || (strncmp(&CIMI[0] + 3, "06", 2) == 0) || (strncmp(&CIMI[0] + 3, "09", 2) == 0))
			{
				modem.operator= CU;
			}

			else if ((strncmp(&CIMI[0] + 3, "03", 2) == 0) || (strncmp(&CIMI[0] + 3, "05", 2) == 0) || (strncmp(&CIMI[0] + 3, "11", 2) == 0))
			{
				modem.operator= CT;
			}
			else
			{
				modem.operator= UNKNOW;
			}
		}

		return SUCCESS;
	}
	else
	{
		return FAILURE;
	}
}

int ec20_module_set_apn()
{
	// 设置APN
	if (strlen(modem.apn.name) == 0)
	{
		if (modem.operator== CM)
		{
			com_send(write_handle, "AT+CGDCONT=1,\"IPV4V6\",\"CMNET\"\r\n", strlen("AT+CGDCONT=1,\"IPV4V6\",\"CMNET\"\r\n"));
		}
		else if (modem.operator== CU)
		{
			com_send(write_handle, "AT+CGDCONT=1,\"IPV4V6\",\"3GNET\"\r\n", strlen("AT+CGDCONT=1,\"IPV4V6\",\"3GNET\"\r\n"));
		}
		else if (modem.operator== CT)
		{
			com_send(write_handle, "AT+CGDCONT=1,\"IPV4V6\",\"CTNET\"\r\n", strlen("AT+CGDCONT=1,\"IPV4V6\",\"CTNET\"\r\n"));
		}
	}
	else
	{
		char sys_cmd[512] = {0};

		if (strlen(modem.apn.user) == 0 || strlen(modem.apn.user) == 0 || strlen(modem.apn.auth) == 0)
		{
			sprintf(sys_cmd, "AT+CGDCONT=1,\"IPV4V6\",\"%s\"\r\n", modem.apn.name);
			com_send(write_handle, sys_cmd, strlen(sys_cmd));
		}
		else
		{
			sprintf(sys_cmd, "AT+ZGPCOAUTH=1,\"%s\",\"%s\",%s\r\n", modem.apn.user, modem.apn.psw, modem.apn.auth);
			com_send(write_handle, sys_cmd, strlen(sys_cmd));
		}
	}

	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	com_recv(read_handle, c_buf, sizeof(c_buf), "OK", NULL, RECV_WAIT_5);

	return SUCCESS;
}

int g405tf_module_set_apn()
{
	// 设置APN
	if (strlen(modem.apn.name) == 0)
	{
		// 设置APN
		if (modem.operator== CM)
		{
			com_send(write_handle, "AT+CGDCONT=1,\"IPV4V6\",\"CMNET\"\r\n", strlen("AT+CGDCONT=1,\"IPV4V6\",\"CMNET\"\r\n"));
		}
		else if (modem.operator== CU)
		{
			com_send(write_handle, "AT+CGDCONT=1,\"IPV4V6\",\"3GNET\"\r\n", strlen("AT+CGDCONT=1,\"IPV4V6\",\"3GNET\"\r\n"));
		}
		else if (modem.operator== CT)
		{
			com_send(write_handle, "AT+CGDCONT=1,\"IPV4V6\",\"CTNET\"\r\n", strlen("AT+CGDCONT=1,\"IPV4V6\",\"CTNET\"\r\n"));
		}
	}
	else
	{
		char sys_cmd[512] = {0};

		if (strlen(modem.apn.user) == 0 || strlen(modem.apn.user) == 0 || strlen(modem.apn.auth) == 0)
		{
			sprintf(sys_cmd, "AT+CGDCONT=1,\"IPV4V6\",\"%s\"\r\n", modem.apn.name);
			com_send(write_handle, sys_cmd, strlen(sys_cmd));
		}
		else
		{
			sprintf(sys_cmd, "AT+ZGPCOAUTH=1,\"%s\",\"%s\",%s\r\n", modem.apn.user, modem.apn.psw, modem.apn.auth);
			com_send(write_handle, sys_cmd, strlen(sys_cmd));
		}
	}

	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	com_recv(read_handle, c_buf, sizeof(c_buf), "OK", NULL, RECV_WAIT_5);

	return SUCCESS;
}

int ec20_module_get_cgatt()
{
	char *str = NULL;
	char *str1 = NULL;
	// 查询是否附着上网络,AT+CGATT?
	com_send(write_handle, "AT+CGATT?\r\n", strlen("AT+CGATT?\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "+CGATT:", "OK", RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != (str = strstr(c_buf, "+CGATT:"))))
	{
		LOG("CGATT = [%s] \r\n", str);
		str1 = str + strlen("+CGATT: ");
		// strncmp(str1,"1", 1);
		if (str1[0] == '1')
		{
			return SUCCESS;
		}
		else
		{
			return FAILURE;
		}
	}
	else
	{
		return FAILURE;
	}
}

int g405tf_module_get_cgatt()
{
	char *str = NULL;
	char *str1 = NULL;
	// 查询是否附着上网络,AT+CGATT?
	com_send(write_handle, "AT+CGATT?\r\n", strlen("AT+CGATT?\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "+CGATT:", "OK", RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != (str = strstr(c_buf, "+CGATT:"))))
	{
		LOG("CGATT = [%s] \r\n", str);
		str1 = str + strlen("+CGATT: ");
		// strncmp(str1,"1", 1);
		if (str1[0] == '1')
		{
			return SUCCESS;
		}
		else
		{
			return FAILURE;
		}
	}
	else
	{
		return FAILURE;
	}
}

int ec20_module_gobinet()
{
	system("killall -9 quectel-CM; sleep 0.1");
	system("quectel-CM &");

	return SUCCESS;
}

int g405tf_module_gobinet()
{
	char temp_buf[64] = {0};
	com_send(write_handle, "AT+CGATT?\r\n", strlen("AT+CGATT?\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), NULL, NULL, RECV_WAIT_30);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != strstr(c_buf, "+CGATT:")))
	{

		if (strstr(c_buf, "+CGATT:0") != NULL || strstr(c_buf, "+CGATT: 0") != NULL)
		{
			com_send(write_handle, "AT+CGATT=1\r\n", strlen("AT+CGATT=1\r\n"));
			usleep(5);
			memset(c_buf, 0, sizeof(c_buf));
			// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
			recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), NULL, NULL, RECV_WAIT_60);
			if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")))
			{
				LOG("AT+CGATT=1 OK\n");
			}
			else
			{
				return NEED_POWER_RESET;
			}
		}
	}
	else
	{
		return FAILURE;
	}

	com_send(write_handle, "AT+CGACT=1,1\r\n", strlen("AT+CGACT=1,1\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), NULL, NULL, RECV_WAIT_30);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != strstr(c_buf, "+ZGIPDNS:")))
	{
		LOG("AT+CGACT=1,1 OK\n");
	}
	else
	{
		return NEED_POWER_RESET;
	}

	com_send(write_handle, "AT+ZGACT?\r\n", strlen("AT+ZGACT?\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), NULL, NULL, RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != strstr(c_buf, "+ZGACT:")))
	{
		if (strstr(c_buf, "+ZGACT:1,1") == NULL && strstr(c_buf, "+ZGACT: 1,1") == NULL)
		{
			com_send(write_handle, "AT+ZGACT=1,1\r\n", strlen("AT+ZGACT=1,1\r\n"));
			usleep(5);
			memset(c_buf, 0, sizeof(c_buf));
			// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
			recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), NULL, NULL, RECV_WAIT_5);
			if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != strstr(c_buf, "+ZCONSTAT:")))
			{
				LOG("AT+ZGACT=1,1 OK\n");
			}
			else
			{
				return NEED_POWER_RESET;
			}
		}
		else
		{
			LOG("AT+ZGACT=1,1 OK\n");
		}
	}
	else
	{
		return FAILURE;
	}

	shell_get_for_single("ifconfig | grep eth1 &>/dev/null &&  echo up || echo no", temp_buf, sizeof(temp_buf));
	if (strstr(temp_buf, "no"))
		system("ifconfig eth1 up");

	shell_get_for_single("udhcpc -i eth1", temp_buf, sizeof(temp_buf));

	return SUCCESS;
}

int ec20_module_get_csq()
{
	// 查询信号强度
	char *str = NULL;
	char *str1 = NULL;
	com_send(write_handle, "AT+CSQ\r\n", strlen("AT+CSQ\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "+CSQ:", "OK", RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != (str1 = strstr(c_buf, "+CSQ:"))))
	{
		// 解释出信号强度
		char cSignal[32] = {0};
		if (NULL != (str = strstr(str1 + 1, ",")))
		{
			memcpy(cSignal, str1 + strlen("+CSQ:"), str - str1 - strlen("+CSQ:"));
			modem.csq = atoi(cSignal);
			return SUCCESS;
		}
	}
	return FAILURE;
}

int g405tf_module_get_csq()
{
	// 查询信号强度
	char *str = NULL;
	char *str1 = NULL;
	com_send(write_handle, "AT+CSQ\r\n", strlen("AT+CSQ\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), "+CSQ:", "OK", RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != (str1 = strstr(c_buf, "+CSQ:"))))
	{
		// 解释出信号强度
		char cSignal[32] = {0};
		if (NULL != (str = strstr(str1 + 1, ",")))
		{
			memcpy(cSignal, str1 + strlen("+CSQ:"), str - str1 - strlen("+CSQ:"));
			modem.csq = atoi(cSignal);
			return SUCCESS;
		}
	}
	return FAILURE;
}

int ec20_module_get_nwinfo()
{
	// 查询运营商网络制式
	com_send(write_handle, "AT+QNWINFO\r\n", strlen("AT+QNWINFO\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), NULL, NULL, RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != strstr(c_buf, "+QNWINFO")))
	{
		int i = 0, mode = 0;
		char *str = NULL;
		char tmp[50] = {0};
		lte_mode ltemode = LTE_MODE_NONE;
		str = strstr(c_buf, "+QNWINFO:");

		if (str != NULL)
		{
			str_replace_chr(str, '\r', '\0');

			mode = 0;
			if (module_getIndex_para(1, str, tmp, sizeof(tmp)) == 0)
			{
				for (i = 0; i < EC20TF_NET_SUM; i++)
				{
					if (strstr(tmp, ec20_net_name[i]))
					{
						mode = i;
						break;
					}
				}

				switch (mode)
				{
				case 1:
				case 6:
				case 7:
				case 8:
				{
					ltemode = LTE_MODE_2G;
					break;
				}

				case 2:
				case 3:
				case 4:
				case 5:
				case 9:
				case 10:
				case 11:
				case 12:
				case 13:
				{
					ltemode = LTE_MODE_3G;
					break;
				}

				case 14:
				case 15:
				{
					ltemode = LTE_MODE_4G;
					break;
				}

				default:
				{
					ltemode = LTE_MODE_NONE;
					break;
				}
				}
				modem.ltemode = ltemode;
				return SUCCESS;
			}
		}
	}
	return FAILURE;
}

int g405tf_module_get_nwinfo()
{
	// 查询运营商网络制式
	com_send(write_handle, "AT^SYSINFO\r\n", strlen("AT^SYSINFO\r\n"));
	usleep(5);
	memset(c_buf, 0, sizeof(c_buf));
	// recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf));
	recv_com_len = com_recv(read_handle, c_buf, sizeof(c_buf), NULL, NULL, RECV_WAIT_5);
	if (recv_com_len > 0 && (NULL != strstr(c_buf, "OK")) && (NULL != strstr(c_buf, "^SYSINFO:")))
	{
		int mode = 0;
		char *str = NULL;
		char tmp[50] = {0};
		lte_mode ltemode = LTE_MODE_NONE;
		str = strstr(c_buf, "^SYSINFO:");
		if (str != NULL)
		{
			str_replace_chr(str, '\r', '\0');

			if (module_getIndex_para(4, str, tmp, sizeof(tmp)) == 0)
			{
				switch (atoi(tmp))
				{
				case 3:
				{
					ltemode = LTE_MODE_2G;
					break;
				}
				case 5:
				case 15:
				{
					ltemode = LTE_MODE_3G;
					break;
				}
				case 17:
				{
					ltemode = LTE_MODE_4G;
					break;
				}
				default:
				{
					ltemode = LTE_MODE_NONE;
					break;
				}
				}

				modem.ltemode = ltemode;
				return SUCCESS;
			}
		}
	}
	return FAILURE;
}

int system_para_init()
{
	char tmp_buf[128];
	memset(tmp_buf, 0, strlen(tmp_buf));
	shell_get_for_single("uci get cbi_file.APN.name", tmp_buf, sizeof(tmp_buf));
	if (0 == strlen(tmp_buf))
	{
		return 0;
	}
	else
	{
		memcpy(modem.apn.name, tmp_buf, strlen(tmp_buf));
		memset(tmp_buf, 0, strlen(tmp_buf));
		shell_get_for_single("uci get cbi_file.APN.usr", tmp_buf, sizeof(tmp_buf));
		if (0 == strlen(tmp_buf))
		{
			return 0;
		}
		memcpy(modem.apn.user, tmp_buf, strlen(tmp_buf));
		memset(tmp_buf, 0, strlen(tmp_buf));
		shell_get_for_single("uci get cbi_file.APN.paw", tmp_buf, sizeof(tmp_buf));
		if (0 == strlen(tmp_buf))
		{
			return 0;
		}
		memcpy(modem.apn.psw, tmp_buf, strlen(tmp_buf));
		memset(tmp_buf, 0, strlen(tmp_buf));
		// shell_get_for_single("uci get cbi_file.APN.auth", tmp_buf, sizeof(tmp_buf));
		// if (0 == strlen(tmp_buf))
		// {
		// 	return 0;
		// }
		memcpy(modem.apn.auth, "1", strlen("1"));
	}
}

int g405tf_find_usb_sta()
{
	char tty_buf[50] = {0};

	// 等待系统识别到USB口
	memset(c_buf, 0, sizeof(c_buf));
	memset(tty_buf, 0, sizeof(tty_buf));
	shell_get_for_single("ls /dev|grep ttyUSB*|awk \'NR==1{print}\'|awk '{printf $0}'", c_buf, sizeof(c_buf));
	if (strlen(c_buf) > 5 && strlen(c_buf) < 10) // ttyusbx
	{
		sprintf(tty_buf, "/dev/%s", c_buf);
		memcpy(istcompara.i_com_name, tty_buf, strlen(tty_buf));
		LOG("ttyUSB = [%s] \r\n", tty_buf);

		if (0 == check_usb_exist(istcompara.i_com_name))
		{
			return SUCCESS;
		}
		else
		{
			return FAILURE;
		}
	}
	else
	{
		return FAILURE;
	}
}

int main()
{
	int ret;

	int fail_cnt = 0; // 失败次数累计，状态转换时必须清零
	e_model_sta i_model_sta = EFINDUSBSTA;

	system_para_init();
	module_net_led_control(LTE_MODE_NONE);
	module_signal_led_control(0); // Update signal light

	int module_type;
	for (int i = 0; i++ < 60; i++)
	{
		ret = lte_get_module_type();
		if (EC25 == ret)
		{
			module_type = EC25;
			break;
		}

		if (G405TF == ret)
		{
			module_type = G405TF;
			break;
		}

		sleep(1);
	}

	if (EC25 == module_type)
	{
		while (1)
		{
			LOG("cellular state:%d\n", i_model_sta);
			switch (i_model_sta)
			{
			case EFINDUSBSTA:
			{
				ret = ec20_find_usb_sta();
				if (ret < 0)
				{
					if (++fail_cnt > TTYNOTFIND_FAILED_MAX)
					{
						fail_cnt = 0;
						i_model_sta = POWEROFF;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = EINITMODEL;
				}
				break;
			}
			case ERESET:
			{
				module_net_led_control(LTE_MODE_NONE); // 网络制式灯复位
				module_signal_led_control(0);		   // 信号灯复位
				module_reset_restart();				   // 蜂窝模块复位
				// 去初始化AT口收发handle
				com_recv_send_destroy(read_handle);
				com_recv_send_destroy(write_handle);
				usleep(10 * 1000 * 1000); // 10s

				// 状态转换时必须清零
				fail_cnt = 0;
				i_model_sta = EFINDUSBSTA;
				break;
			}
			case POWEROFF:
			{
				module_net_led_control(LTE_MODE_NONE); // 网络制式灯复位
				module_signal_led_control(0);		   // 信号灯复位
				module_power_restart();				   // 蜂窝模块复位
				// 去初始化AT口收发handle
				com_recv_send_destroy(read_handle);
				com_recv_send_destroy(write_handle);
				usleep(10 * 1000 * 1000); // 10s

				// 状态转换时必须清零
				fail_cnt = 0;
				i_model_sta = EFINDUSBSTA;
				break;
			}
			case EINITMODEL:
			{
				ret = ec20_module_init();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = POWEROFF;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = CHECKIMEI;
				}
				break;
			}
			case CHECKIMEI:
			{
				ret = ec20_module_get_imei();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = CHECKCPIN;
				}
				break;
			}
			case CHECKCPIN:
			{
				ret = ec20_module_get_cpin();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = CHECKICCID;
				}
				break;
			}
			case CHECKICCID:
			{
				ret = ec20_module_get_iccid();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = CHECKIMSI;
				}
				break;
			}
			case CHECKIMSI:
			{
				ret = ec20_module_get_imsi();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = CHECKAPN;
				}
				break;
			}
			case CHECKAPN:
			{
				ret = ec20_module_set_apn();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = ESTICKNET;
				}
				break;
			}
			case ESTICKNET:
			{
				ret = ec20_module_get_cgatt();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = GOBINET;
				}
				break;
			}
			case GOBINET:
			{
				ec20_module_gobinet();
				for (; fail_cnt++ < 60;)
				{
					if (ec20_check_ip() < 0)
					{
						if (fail_cnt == 60)
						{
							fail_cnt = 0;
							i_model_sta = ERESET;
							break;
						}
					}
					else
					{
						fail_cnt = 0;
						i_model_sta = CHECKNWINFOCSQ;
						break;
					}
					sleep(1);
				}
				break;
			}
			case CHECKNWINFOCSQ:
			{
				// 查询运营商网络制式
				ret = ec20_module_get_nwinfo();
				if (ret < 0)
				{
					LOG("get nwinfo error");
				}
				else
				{
					module_net_led_control(modem.ltemode); // 网络制式灯
				}

				// 查询信号值
				ret = ec20_module_get_csq();
				if (ret < 0)
				{
					LOG("get csq error");
				}
				else
				{
					module_signal_led_control(modem.csq); // 信号灯
				}

				i_model_sta = EGETNETSTA;
				break;
			}
			case EGETNETSTA:
			{
				int comReturn = FAILURE;
				char *str = NULL;
				char *str1 = NULL;

				i_model_sta = EGETNETSTA;

				// 检查是否附着网络
				ret = ec20_module_get_cgatt();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
						break;
					}
				}
				else
				{
					fail_cnt = 0;
				}

				// 检查ip
				ret = ec20_check_ip();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
						break;
					}
				}
				else
				{
					fail_cnt = 0;
				}

				// 检查dns
				ret = check_dns();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
						break;
					}
				}
				else
				{
					fail_cnt = 0;
				}

				// 查询运营商网络制式
				ret = ec20_module_get_nwinfo();
				if (ret < 0)
				{
					LOG("get nwinfo error");
				}
				else
				{
					module_net_led_control(modem.ltemode); // 网络制式灯
				}

				// 查询信号值
				ret = ec20_module_get_csq();
				if (ret < 0)
				{
					LOG("get csq error");
				}
				else
				{
					module_signal_led_control(modem.csq); // 信号灯
				}

				break;
			}

			default:
				break;
			}

			if (EGETNETSTA == i_model_sta)
			{
				usleep(10 * 1000 * 1000); // 10s
			}
			else
			{
				usleep(1000 * 1000); // 1s
			}
		}
	}
	else if (G405TF == module_type)
	{
		while (1)
		{
			LOG("cellular state:%d\n", i_model_sta);
			switch (i_model_sta)
			{
			case EFINDUSBSTA:
			{
				ret = g405tf_find_usb_sta();
				if (ret < 0)
				{
					if (++fail_cnt > TTYNOTFIND_FAILED_MAX)
					{
						fail_cnt = 0;
						i_model_sta = POWEROFF;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = EINITMODEL;
				}
				break;
			}
			case ERESET:
			{
				module_net_led_control(LTE_MODE_NONE); // 网络制式灯复位
				module_signal_led_control(0);		   // 信号灯复位
				module_reset_restart();				   // 蜂窝模块复位
				// 去初始化AT口收发handle
				com_recv_send_destroy(read_handle);
				com_recv_send_destroy(write_handle);
				usleep(10 * 1000 * 1000); // 10s

				// 状态转换时必须清零
				fail_cnt = 0;
				i_model_sta = EFINDUSBSTA;
				break;
			}
			case POWEROFF:
			{
				module_net_led_control(LTE_MODE_NONE); // 网络制式灯复位
				module_signal_led_control(0);		   // 信号灯复位
				module_power_restart();				   // 蜂窝模块复位
				// 去初始化AT口收发handle
				com_recv_send_destroy(read_handle);
				com_recv_send_destroy(write_handle);
				usleep(10 * 1000 * 1000); // 10s

				// 状态转换时必须清零
				fail_cnt = 0;
				i_model_sta = EFINDUSBSTA;
				break;
			}
			case EINITMODEL:
			{
				ret = g405tf_module_init();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = POWEROFF;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = CHECKIMEI;
				}
				break;
			}
			case CHECKIMEI:
			{
				ret = g405tf_module_get_imei();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = CHECKCPIN;
				}
				break;
			}
			case CHECKCPIN:
			{
				ret = g405tf_module_get_cpin();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = CHECKICCID;
				}
				break;
			}
			case CHECKICCID:
			{
				ret = g405tf_module_get_iccid();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = CHECKIMSI;
				}
				break;
			}
			case CHECKIMSI:
			{
				ret = g405tf_module_get_imsi();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = CHECKAPN;
				}
				break;
			}
			case CHECKAPN:
			{
				ret = g405tf_module_set_apn();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = ESTICKNET;
				}
				break;
			}
			case ESTICKNET:
			{
				ret = g405tf_module_get_cgatt();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = GOBINET;
				}
				break;
			}
			case GOBINET:
			{
				ret = g405tf_module_gobinet();
				if (ret < 0)
				{
					if (ret == NEED_POWER_RESET)
					{
						i_model_sta = POWEROFF;
						fail_cnt = 0;
						break;
					}

					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
						break;
					}
				}
				else
				{
					fail_cnt = 0;
					i_model_sta = CHECKNWINFOCSQ;
				}

				for (; fail_cnt++ < 60;)
				{
					if (g405tf_check_ip() < 0)
					{
						if (fail_cnt > 60)
						{
							fail_cnt = 0;
							i_model_sta = ERESET;
							break;
						}
					}
					else
					{
						fail_cnt = 0;
						i_model_sta = CHECKNWINFOCSQ;
						break;
					}
				}
				break;
			}
			case CHECKNWINFOCSQ:
			{
				// 查询运营商网络制式
				ret = g405tf_module_get_nwinfo();
				if (ret < 0)
				{
					LOG("get nwinfo error");
				}
				else
				{
					module_net_led_control(modem.ltemode); // 网络制式灯
				}

				// 查询信号值
				ret = g405tf_module_get_csq();
				if (ret < 0)
				{
					LOG("get csq error");
				}
				else
				{
					module_signal_led_control(modem.csq); // 信号灯
				}

				i_model_sta = EGETNETSTA;
				break;
			}
			case EGETNETSTA:
			{
				int comReturn = FAILURE;
				char *str = NULL;
				char *str1 = NULL;

				i_model_sta = EGETNETSTA;

				// 检查是否附着网络
				ret = g405tf_module_get_cgatt();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
						break;
					}
				}
				else
				{
					fail_cnt = 0;
				}

				// 检查ip
				ret = g405tf_check_ip();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
						break;
					}
				}
				else
				{
					fail_cnt = 0;
				}

				// 检查dns
				ret = check_dns();
				if (ret < 0)
				{
					if (++fail_cnt > AT_FAILED_MAX)
					{
						i_model_sta = ERESET;
						fail_cnt = 0;
						break;
					}
				}
				else
				{
					fail_cnt = 0;
				}

				// 查询运营商网络制式
				ret = g405tf_module_get_nwinfo();
				if (ret < 0)
				{
					LOG("get nwinfo error");
				}
				else
				{
					module_net_led_control(modem.ltemode); // 网络制式灯
				}

				// 查询信号值
				ret = g405tf_module_get_csq();
				if (ret < 0)
				{
					LOG("get csq error");
				}
				else
				{
					module_signal_led_control(modem.csq); // 信号灯
				}

				break;
			}

			default:
				break;
			}

			if (EGETNETSTA == i_model_sta)
			{
				usleep(10 * 1000 * 1000); // 10s
			}
			else
			{
				usleep(1000 * 1000); // 1s
			}
		}
	}

	com_recv_send_destroy(read_handle);
	com_recv_send_destroy(write_handle);

	return 0;
}
