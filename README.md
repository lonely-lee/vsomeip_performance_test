# vsomeip_performance_test
## test_app x86build
```bash
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=/home/hikerlee02/workspace/x86target \
    -DCMAKE_INSTALL_PREFIX=./../_x86output \
    -DTEST_IP_MASTER=192.254.8.1 \
    -DTEST_IP_SLAVE=192.254.8.102 \
    ..
make -j4 && make install
```

## test_app armbuild
```shell
mkdir armbuild
cd armbuild
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-toolchain.cmake \
    -DCROSS_TOOLS_PATH="/home/hikerlee02/workspace/armcompilerchains/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu" \
    -DCMAKE_INSTALL_PREFIX=./../_armoutput \
    -DTEST_IP_MASTER=192.254.8.1 \
    -DTEST_IP_SLAVE=192.254.8.102 \
    ..
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-toolchain.cmake -DCROSS_TOOLS_PATH="/home/lee_home/armtoolchain/compile-tool/gcc-linaro-5.5.0-2017.10-x86_64_arm-linux-gnueabihf" ..
make -j4 && make install
```

## detaild test_app
### test_method_client



# changes
## 2024-07-09
- 新增测试用例event
- 新增混合服务端，包含event和method
- 代办
- - 去除统计cpu和内存的代码

## 2024-07-26
- vsomeip测试暂告一段落
- 目前仍然存在问题
- - 部署在王哥板子上收发会有问题，丢包或直接抓到包，但someip程序无法解析