#!/bin/bash  
  
start_size=1000  
end_size=81000
step_size=10000
  
for size in $(seq $start_size $step_size $end_size); do  
    echo "1. exec [./test_method_client --size $size --cycle 50]" 
    ./test_method_client --size $size --cycle 50&  
    pid=$!  
    
    echo "2. The PID of test_method_client is: $pid"

    sleep 150
    kill -2 $pid
    
    while pgrep -f "./test_method_client" >/dev/null; do  
        echo "./test_method_client --size $size --cycle 50 still running,wait 1s..." 
        sleep 1  
    done 

    sleep 1
    echo "3. Finished running with size $size"  
done
  
echo "All tests completed."