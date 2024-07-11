# vsomeip_performance_test
## test_app x86build
```bash
mkdir build
cd build
cmake -DCMAKE_PREFIX_PATH=/home/hikerlee02/workspace/x86target ..
cmake .. && make
```

## test_app armbuild
```shell
mkdir armbuild
cd armbuild
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-toolchain.cmake -DCROSS_TOOLS_PATH="/home/hikerlee02/workspace/armcompilerchains/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu" ..
cmake --build .
```

## detaild test_app
### test_method_client



# changes
## 2024-07-09
- 新增测试用例event
- 新增混合服务端，包含event和method
- 代办
- - 去除统计cpu和内存的代码