#!/bin/sh  

# 创建测试数据保存路径
script_dir="$(dirname "$0")"  
parent_dir="$(dirname "$script_dir")"  
target_dir="./../result"   
mkdir -p "$target_dir"  

small_start_size=100
small_end_size=1300
small_step_size=200

big_start_size=1500
big_end_size=81500
big_step_size=20000

protocol_type=UDP

export LD_LIBRARY_PATH=./../lib:$LD_LIBRARY_PATH
export VSOMEIP_CONFIGURATION=./../etc/vsomeip-method-client-master.json
export VSOMEIP_APPLICATION_NAME=test_method_client

# handle cpu
# nproc=4
firstRun=1
systemTimePrev=0
processTimePrev=0
numCores=$(nproc)

getSystemTime() {
    IFS=' ' read -r cpu user nice system idle rest < /proc/stat
    total=$(($user + $nice + $system + $idle))
    echo $total
}

getProcessTime() {
    total=$(awk "NR==1 {print \$14 + \$15 + \$16 + \$17}" /proc/$pid/stat)
    # total=$(echo "$total" | bc)
    echo $total
}

calculateCPU() {
    if [ $firstRun -eq 1 ]; then
        systemTimePrev=$(getSystemTime)
        processTimePrev=$(getProcessTime $1)
        firstRun=0
        return 0
    fi
    systemTimeCurr=$(getSystemTime)
    processTimeCurr=$(getProcessTime $1)
    deltaSystem=$((systemTimeCurr-systemTimePrev))
    deltaProcess=$((processTimeCurr-processTimePrev))
    systemTimePrev=$systemTimeCurr
    processTimePrev=$processTimeCurr

    cpu_usage=$(($deltaProcess * 100 * $numCores / $deltaSystem))
    echo "cpu_usage=$cpu_usage"
}
# handle memory
calculateMemory() {
    mem_usage=$(cat /proc/$1/status | grep 'VmRSS' | awk '{print $2}')
    echo $mem_usage
}

statistics_resource() {  
    local pid=$1  
    local name=$2  
    local payload=$3  
    local protocal=$4  
    local output_file="${target_dir}/${name}_${protocal}_${payload}_resources.txt" 
    echo "timestamp | systemTime | processTime | memory" >> "$output_file" 
  
    if  !kill -0 $pid 2>/dev/null; then  
        echo "Error: Process $pid does not exist."  
        return 1  
    fi  

    while kill -0 $pid 2>/dev/null; do  
        # Due to the sh script for g9q runs in a busybox environment with limited functionality,data calculation is only processed by the computer
        # echo "static number"
        # cpu=$(calculateCPU $1)
        # memory=$(calculateMemory $1)
        # timestamp=$(date +%s.%N | cut -b1-14) 
        # echo "$timestamp, $cpu, $memory"
        # echo "$timestamp, $cpu, $memory" >> "$output_file"  

        memory=$(calculateMemory $1)
        systemTime=$(getSystemTime)
        processTime=$(getProcessTime $1)
        timestamp=$(date +%s) #Does not support high-precision clocks
        echo "$timestamp | $systemTime | $processTime | $memory" >> "$output_file"   

        sleep 1
    done  
  
    echo "Process $pid has ended."  
}

echo "This test will conduct a one-on-one method R&R communication 
        test to calculate the delay and throughput of communication.
        This script will be used to start and configure the client program, 
        and the corresponding script is to start and configure the server. 
        This test requires running the test-method-slave.sh script simultaneously" 
echo "Each test interval is 1 second, and 100 requests are sent for each test" 

for i in $(seq 1 2);do
    if [ $i -eq 2 ];then
        protocol_type=TCP
    fi

    j=1
    echo "$i. Test using $protocol_type communication and changing the payload size from $small_start_size to $small_end_size with step size of $small_step_size"
    for size in $(seq $small_start_size $small_step_size $small_end_size); do  
        echo "$i.$j. exec [./test_method_client --protocol $protocol_type --payload-size $size --request 100 --test 10]
              Please enter 'a' when ready to start......" 
        read input
        ./test_method_client --protocol $protocol_type --payload-size $size --request 100 --test 10&  
        pid=$! 
        echo "$i.$j. The PID of test_method_client is: $pid."
        # wait $pid
        statistics_resource $pid test_method_client $size $protocol_type

        sleep 1
        echo "$i.$j. Finished running with --protocol $protocol_type --size $size"  
        j=$((j+1))
    done
    
    echo "$i. Test using $protocol_type communication and changing the payload size from $big_start_size to $big_end_size with step size of $big_step_size"
    for size in $(seq $big_start_size $big_step_size $big_end_size); do  
        echo "$i.$j. exec [./test_method_client --protocol $protocol_type --payload-size $size --request 100 --test 10]
                Please enter 'a' when ready to start......" 
        read input
        ./test_method_client --protocol $protocol_type --payload-size $size --request 100 --test 10&  
        pid=$! 
        echo "$i.$j. The PID of test_method_client is: $pid."
        # wait $pid
        statistics_resource $pid test_method_client $size $protocol_type

        sleep 1
        echo "$i.$j. Finished running with --protocol $protocol_type --size $size"  
        j=$((j+1))
    done
done
  
echo "All tests completed."