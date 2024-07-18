#!/bin/bash  
  
small_start_size=100
small_end_size=1300
small_step_size=200

big_start_size=1500
big_end_size=81500
big_step_size=20000

protocol_type=UDP

statistics_resoure() {  
    local pid=$1  
    local name=$2  
    local payload=$3  
    local protocal=$4  
    local output_file="${name}_${payload}_${protocal}.txt" 
    echo "Start statistical resource usage"
  
    # 检查 PID 是否存在  
    if  !kill -0 $pid 2>/dev/null; then  
        echo "Error: Process $pid does not exist."  
        return 1  
    fi  
  
    # 写入标题到输出文件  
    echo "Timestamp, CPU (%), Memory (RSS KB)" > "$output_file"  
  
    # 无限循环，直到进程不再存在  
    while kill -0 $pid 2>/dev/null; do  
        # 使用 ps 获取 CPU 和 RSS 内存占用  
        # 注意：这里的 -o rss= 和 -o %cpu= 需要根据你的 ps 版本进行调整  
        # 某些系统可能需要使用 -o rss=, -o %cpu= 或者其他格式  
        info=$(ps -p $pid -o %cpu=,rss= --no-headers)  
        cpu=$(echo "$info" | awk '{print $1}')  
        memory=$(echo "$info" | awk '{print $2}')  
  
        # 获取当前时间戳  
        timestamp=$(date +%s.%N | cut -b1-14)  # 截取前14位作为时间戳  
  
        # 写入日志  
        echo "$timestamp, $cpu, $memory" >> "$output_file"  
  
        # 等待0.01秒  
        sleep 0.01  
    done  
  
    echo "Process $pid has ended."  
} 
export LD_LIBRARY_PATH=./../lib:$LD_LIBRARY_PATH
export VSOMEIP_CONFIGURATION=./../etc/vsomeip-udp-event-service.json
export VSOMEIP_APPLICATION_NAME=test_event_server

echo "This test will conduct a one-on-one event notification communication 
        test to calculate the delay and throughput of communication.
        This script will be used to start and configure the server program, 
        and the corresponding script is to start and configure the client. 
        This test requires running the test-event-mastter.sh script simultaneously" 
echo "The preset notification cycle for this test is 50ms, 
        with 10 tests run per round and 100 notification messages sent and received per test" 


for i in {1,2};do
    if [ $i -eq 2 ];then
        protocol_type=TCP
        export VSOMEIP_CONFIGURATION=./../etc/vsomeip-tcp-event-service.json
    fi
    j=1
    echo "$i. Test using $protocol_type communication and changing the payload size from $small_start_size to $small_end_size with step size of $small_step_size"
    for size in $(seq $small_start_size $small_step_size $small_end_size); do  
        echo "$i.$j. exec [./../build/test_event_server --protocol $protocol_type --size $size --notify 100 --test 10 --cycle 50]
                Please enter 'a' when ready to start......" 
        read input
        ./test_event_server --protocol $protocol_type --size $size --notify 100 --test 10 --cycle 50&  
        pid=$! 
        # wait $pid
        statistics_resoure $pid test_event_server $size $protocol_type
        
        echo "$i.$j. The PID of test_event_server is: $pid."
        
        while pgrep -f "test_event_ser" >/dev/null; do  
            echo "./test_event_server --size $size --cycle 50 still running,wait 1s..." 
            sleep 1  
        done 

        sleep 1
        echo "$i.$j. Finished running with --protocol $protocol_type --size $size"  
        j=$((j+1))
    done
done
  
echo "All tests completed."