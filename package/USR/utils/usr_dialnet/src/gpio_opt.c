
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "gpio_opt.h"

int gpio_opt_init(int gpio_num, gpio_mode_t mode)
{
    assert(mode == GPIO_MODE_IN || mode == GPIO_MODE_OUT);

    FILE *fp;
    char path[128];

    sprintf(path, "/sys/class/gpio/gpio%d", gpio_num);

    // 不存在创建
    if (access(path, 0) < 0)
    {
        fp = fopen("/sys/class/gpio/export", "w");
        if (NULL == fp)
        {
            printf("gpio init error\n");
            return -1;
        }
        fprintf(fp, "%d\n", gpio_num);
        fclose(fp);
    }

    sprintf(path, "/sys/class/gpio/gpio%d/direction", gpio_num);
    fp = fopen(path, "w");
    if (NULL == fp)
    {
        printf("gpio init error\n");
        return -1;
    }

    rewind(fp);

    if (mode == GPIO_MODE_IN)
    {
        fprintf(fp, "%s", "in");
    }
    else if (mode == GPIO_MODE_OUT)
    {
        fprintf(fp, "%s", "out");
    }

    fclose(fp);

    return 0;
}

int gpio_opt_setval(int gpio_num, gpio_level_t level)
{
    assert(level == GPIO_LEVEL_L || level == GPIO_LEVEL_H);
    FILE *fp;
    char path[128];

    sprintf(path, "/sys/class/gpio/gpio%d/value", gpio_num);

    // 不存在需要初始化
    if (access(path, 0) < 0)
    {
        gpio_opt_init(gpio_num, GPIO_MODE_OUT);
    }

    fp = fopen(path, "w");
    if (NULL == fp)
    {
        perror("gpio_opt_setval error:\n");
        return -1;
    }

    rewind(fp);

    if (level == GPIO_LEVEL_L)
    {
        fprintf(fp, "%d\n", GPIO_LEVEL_L);
    }
    else if (level == GPIO_LEVEL_H)
    {
        fprintf(fp, "%d\n", GPIO_LEVEL_H);
    }

    fclose(fp);

    return 0;
}

int gpio_opt_getval(int gpio_num)
{
    FILE *fp;
    char path[128];

    sprintf(path, "/sys/class/gpio/gpio%d/value", gpio_num);

    // 不存在需要初始化
    if (access(path, 0) < 0)
    {
        gpio_opt_init(gpio_num, GPIO_MODE_IN);
    }

    fp = fopen(path, "r");
    if (fp)
    {
        memset(path, 0, sizeof(path));
        fgets(path, sizeof(path), fp);
        fclose(fp);

        if (path[0] == '1')
        {
            return GPIO_LEVEL_H;
        }
        else
        {
            return GPIO_LEVEL_L;
        }
    }
    else
    {
        printf("gpio_opt_getval error\n");
        return -1;
    }
}

void gpio_opt_release(int gpio_num)
{
    FILE *fp;

    fp = fopen("/sys/class/gpio/unexport", "w");
    if (NULL == fp)
    {
        printf("gpio init error\n");
        return;
    }

    fprintf(fp, "%d\n", gpio_num);
    fclose(fp);
}

int gpio_opt_setval_withlabel(char *label_path, gpio_level_t level)
{
    assert(level == GPIO_LEVEL_L || level == GPIO_LEVEL_H);
    FILE *fp;

    if ((label_path == NULL) || strlen(label_path) <= 0)
    {
        perror("gpio_opt_setval error:\n");
        return -1;
    }
    fp = fopen(label_path, "w");
    if (NULL == fp)
    {
        perror("gpio_opt_setval error:\n");
        return -1;
    }

    rewind(fp);

    if (level == GPIO_LEVEL_L)
    {
        fprintf(fp, "%d\n", GPIO_LEVEL_L);
    }
    else if (level == GPIO_LEVEL_H)
    {
        fprintf(fp, "%d\n", GPIO_LEVEL_H);
    }

    fclose(fp);

    return 0;
}
