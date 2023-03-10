# [USR IOT] 库文件/示例代码/配置文件

## 4G拨号程序示例
utils/usr_dialnet

## Digital_IO示例程序
utils/usr_dialnet

## DTU 库/示例
libs/libusrdtu

## 配置及编译
1. 执行 `./scripts/feeds update -a` 获取软件包
2. 执行`./scripts/feeds install -a` 安装软件包
3. 拷贝`package/USR/configs/USR-G809.config` 为 `.config` 或者 在`make menuconfig` 中选择 `Load` 加载 `package/USR/configs/USR-G809.config` 配置文件
4. `make menuconfig` 配置需要的包
5. `make -j4` 编译固件
