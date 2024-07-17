#!/bin/sh  
  
small_start_size=100
small_end_size=1300
small_step_size=200

big_start_size=1500
big_end_size=81500
big_step_size=20000

export LD_LIBRARY_PATH=./../lib:$LD_LIBRARY_PATH
export VSOMEIP_CONFIGURATION=./../etc/vsomeip-udp-method-client.json
export VSOMEIP_APPLICATION_NAME=test_method_client

echo "This test will conduct a one-on-one method R&R communication 
        test to calculate the delay and throughput of communication.
        This script will be used to start and configure the client program, 
        and the corresponding script is to start and configure the server. 
        This test requires running the test-method-slave.sh script simultaneously" 
echo "Each test interval is 1 second, and 100 requests are sent for each test" 


echo "Firstly, test using UDP communication and changing the payload size from $small_start_size to $small_end_size with step size of $small_step_size"
for size in $(seq $small_start_size $small_step_size $small_end_size); do  
    echo "1. exec [./test_method_client --protocol UDP --size $size --notify 100 --test 10 --cycle 50]
            Please enter 'a' when ready to start......" 
    read input
    ./test_method_client --protocol UDP --size $size --notify 100 --test 10 --cycle 50&  
    pid=$!  
    
    echo "2. The PID of test_method_client is: $pid.Please enter 'b' when app over......"
    read over
    
    while pgrep -f "test_method_clie" >/dev/null; do  
        echo "./test_method_client --size $size --cycle 50 still running,wait 1s..." 
        sleep 1  
    done 

    sleep 1
    echo "3. Finished running with --protocol UDP --size $size"  
done

echo "secondly, test using UDP communication and changing the payload size from $big_start_size to $big_end_size with step size of $big_step_size"
for size in $(seq $big_start_size $big_step_size $big_end_size); do  
    echo "1. exec [./test_method_client --protocol UDP --size $size --notify 100 --test 10 --cycle 50]
            Please enter 'a' when ready to start......" 
    read input
    ./test_method_client --protocol UDP --size $size --notify 100 --test 10 --cycle 50&  
    pid=$!  
    
    echo "2. The PID of test_method_client is: $pid.Please enter 'b' when app over......"
    read over
    
    while pgrep -f "./test_method_client" >/dev/null; do  
        echo "./test_method_client --size $size --cycle 50 still running,wait 1s..." 
        sleep 1  
    done 

    sleep 1
    echo "3. Finished running with --protocol UDP --size $size"  
done

export VSOMEIP_CONFIGURATION=./../etc/vsomeip-tcp-method-client.json
echo "Thirdly, test using UDTCPP communication and changing the payload size from $small_start_size to $small_end_size with step size of $small_step_size"
for size in $(seq $small_start_size $small_step_size $small_end_size); do  
    echo "1. exec [./test_method_client --protocol TCP --size $size --notify 100 --test 10 --cycle 50]
            Please enter 'a' when ready to start......" 
    read input
    ./test_method_client --protocol TCP --size $size --notify 100 --test 10 --cycle 50&  
    pid=$!  
    
    echo "2. The PID of test_method_client is: $pid.Please enter 'b' when app over......"
    read over
    
    while pgrep -f "test_method_clie" >/dev/null; do  
        echo "./test_method_client --size $size --cycle 50 still running,wait 1s..." 
        sleep 1  
    done 

    sleep 1
    echo "3. Finished running with --protocol TCP --size $size"  
done

echo "Fourthly, test using TCP communication and changing the payload size from $big_start_size to $big_end_size with step size of $big_step_size"
for size in $(seq $big_start_size $big_step_size $big_end_size); do  
    echo "1. exec [./test_method_client --protocol TCP --size $size --notify 100 --test 10 --cycle 50]
            Please enter 'a' when ready to start......" 
    read input
    ./test_method_client --protocol TCP --size $size --notify 100 --test 10 --cycle 50&  
    pid=$!  
    
    echo "2. The PID of test_method_client is: $pid.Please enter 'b' when app over......"
    read over
    
    while pgrep -f "test_method_clie" >/dev/null; do  
        echo "./test_method_client --size $size --cycle 50 still running,wait 1s..." 
        sleep 1  
    done 

    sleep 1
    echo "3. Finished running with --protocol TCP --size $size"  
done
  
echo "All tests completed."