#!/bin/bash  
  
# 设置起始和结束范围，以及步长  
start_size=1000  
end_size=21000
step_size=2000  
  
for size in $(seq $start_size $step_size $end_size); do  
    # 使用&将进程放到后台运行，并立即获取其PID 
    echo "1. exec [./test_method_client --size $size]" 
    ./test_method_client --size $size &  
    pid=$!  
    
    echo "2. The PID of test_method_client is: $pid"

    sleep 100
    kill -2 $pid
    # 定义一个函数来检查进程是否还在运行  
    
    # 循环检查进程是否还在运行  
    while pgrep -f "./test_method_client" >/dev/null; do  
        echo "./test_method_client --size $size 仍在运行,等待1秒后再次检查..." 
        sleep 1  
    done 

    sleep 1
    echo "3. Finished running with size $size"  
done
  
echo "All tests completed."
