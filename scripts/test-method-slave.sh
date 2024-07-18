#!/bin/sh  
  
small_start_size=100
small_end_size=1300
small_step_size=200

big_start_size=1500
big_end_size=81500
big_step_size=20000

export LD_LIBRARY_PATH=/home/hikerlee02/workspace/x86target/lib:$LD_LIBRARY_PATH
export VSOMEIP_CONFIGURATION=./../etc/vsomeip-udp-method-service.json
export VSOMEIP_APPLICATION_NAME=test_method_server

echo "This test will conduct a one-on-one method notification communication 
        test to calculate the delay and throughput of communication.
        This script will be used to start and configure the server program, 
        and the corresponding script is to start and configure the client. 
        This test requires running the test-method-mastter.sh script simultaneously" 
echo "The preset notification cycle for this test is 50ms, 
        with 10 tests run per round and 100 notification messages sent and received per test" 


echo "Firstly, test using UDP communication and changing the payload size from $small_start_size to $small_end_size with step size of $small_step_size"
for size in $(seq $small_start_size $small_step_size $small_end_size); do  
    echo "1. exec [./../build/test_method_server --protocol UDP]
            Please enter 'a' when ready to start......" 
    read input
    ./../build/test_method_server --protocol UDP&  
    pid=$!  
    
    echo "2. The PID of test_method_server is: $pid.Please enter 'b' when app over......"
    read over
    
    while pgrep -f "test_method_ser" >/dev/null; do  
        echo "./../build/test_method_server --protocol UDP still running,wait 1s..." 
        sleep 1  
    done 

    sleep 1
    echo "3. Finished running with --protocol UDP"  
done

echo "secondly, test using UDP communication and changing the payload size from $big_start_size to $big_end_size with step size of $big_step_size"
for size in $(seq $big_start_size $big_step_size $big_end_size); do  
    echo "1. exec [./../build/test_method_server --protocol UDP]
            Please enter 'a' when ready to start......" 
    read input
    ./../build/test_method_server --protocol UDP&  
    pid=$!  
    
    echo "2. The PID of test_method_server is: $pid.Please enter 'b' when app over......"
    read over
    
    while pgrep -f "test_method_ser" >/dev/null; do  
        echo "./../build/test_method_server --protocol UDP still running,wait 1s..." 
        sleep 1  
    done 

    sleep 1
    echo "3. Finished running with --protocol UDP"  
done

export VSOMEIP_CONFIGURATION=./../etc/vsomeip-tcp-method-service.json
echo "Thirdly, test using UDTCPP communication and changing the payload size from $small_start_size to $small_end_size with step size of $small_step_size"
for size in $(seq $small_start_size $small_step_size $small_end_size); do  
    echo "1. exec [./../build/test_method_server --protocol TCP]
            Please enter 'a' when ready to start......" 
    read input
    ./../build/test_method_server --protocol TCP&  
    pid=$!  
    
    echo "2. The PID of test_method_server --protocol TCP is: $pid.Please enter 'b' when app over......"
    read over
    
    while pgrep -f "test_method_ser" >/dev/null; do  
        echo "./../build/test_method_server --protocol TCP still running,wait 1s..." 
        sleep 1  
    done 

    sleep 1
    echo "3. Finished running with --protocol TCP"  
done

echo "Fourthly, test using TCP communication and changing the payload size from $big_start_size to $big_end_size with step size of $big_step_size"
for size in $(seq $big_start_size $big_step_size $big_end_size); do  
    echo "1. exec [./../build/test_method_server --protocol TCP]
            Please enter 'a' when ready to start......" 
    read input
    ./../build/test_method_server --protocol TCP&  
    pid=$!  
    
    echo "2. The PID of test_method_server is: $pid.Please enter 'b' when app over......"
    read over
    
    while pgrep -f "test_method_ser" >/dev/null; do  
        echo "./../build/test_method_server --protocol TCP still running,wait 1s..." 
        sleep 1  
    done 

    sleep 1
    echo "3. Finished running with --protocol TCP"  
done
  
echo "All tests completed."