
#ifndef GPIO_OPT_H
#define GPIO_OPT_H

typedef enum GPIO_MODE
{
    GPIO_MODE_IN = 0,
    GPIO_MODE_OUT,
} gpio_mode_t;


typedef enum GPIO_LEVEL
{
    GPIO_LEVEL_L,
    GPIO_LEVEL_H
} gpio_level_t;

int gpio_opt_init(int gpio_num, gpio_mode_t mode);
int gpio_opt_getval(int gpio_num);
int gpio_opt_setval(int gpio_num, gpio_level_t level);
void gpio_opt_release(int gpio_num);
int gpio_opt_setval_withlabel(char *label_path, gpio_level_t level);

#endif  //GPIO_OPT_H