/*
 * USR DTU Library
 *
 * author yanlufei@usr.cn
 *
 */

#ifndef __USRDTU_H
#define __USRDTU_H

typedef struct _uci_param
{
    unsigned int baud;
    unsigned char parity_type;
    unsigned char data_bits;
    unsigned char stop_bits;
    unsigned char flow_type;
    unsigned char mode_type;
    unsigned int pack_period;
    unsigned int pack_length;
    const char *devname;
    const char *hello_msg;
} DTU_PARAM;

/***************************************************************************************************
 * @brief: 串口句柄初始化,非阻塞
 * @param: dtu_config: DTU配置
 * @return: 文件句柄, 失败返回 -1;
 * @modification: none
 ***************************************************************************************************/
int usrdtu_create(DTU_PARAM dtu_config);

/***************************************************************************************************
 * @brief: 串口销毁
 * @param: fd_serial: 串口文件描述符
 * @return: 结果,失败返回 -1;
 * @modification: none
 ***************************************************************************************************/
int usrdtu_destroy(int fd_serial);

/***************************************************************************************************
 * @brief: 读取,非阻塞
 * @param: fd: 串口文件描述符
 * @param: data: 数据存储buffer
 * @param: len: 数据存储buffer长度
 * @return: 数据读取长度, 失败返回 -1
 * @modification: none
 ***************************************************************************************************/
int usrdtu_rceive_data(int fd, char *data, int len);

/***************************************************************************************************
 * @brief: 发送
 * @param: fd: 串口文件描述符
 * @param: data: 数据存储buffer
 * @param: len: 数据存储buffer长度
 * @return: 数据发送长度, 失败返回 -1
 * @modification: none
 ***************************************************************************************************/
int usrdtu_send_data(int fd, char *buf, short len);

/***************************************************************************************************
 * @brief: 串口看门狗喂狗
 * @param: fd: 串口文件描述符
 * @return: None
 * @modification: none
 ***************************************************************************************************/
void usrdtu_dog(int fd);

#endif //__USRDTU_H
