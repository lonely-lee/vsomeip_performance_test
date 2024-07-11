#!/bin/bash
if [ "$#" -ne 1 ]; then  
    echo "Usage: $0 <1|2|3|4|5>"  
    exit 1  
fi
export LD_LIBRARY_PATH=/home/hikerlee02/workspace/x86target/lib:$LD_LIBRARY_PATH

case $1 in  
    1)  
        echo "Executing test_method_client"  
        export VSOMEIP_CONFIGURATION=./../etc/vsomeip-udp-method-client.json
        export VSOMEIP_APPLICATION_NAME=test_method_client
        ./../build/test_method_client --size 4000
        ;;  
    2)  
        echo "Executing test_method_server"    
        export VSOMEIP_CONFIGURATION=./../etc/vsomeip-udp-method-service.json
        export VSOMEIP_APPLICATION_NAME=test_method_server
        ./../build/test_method_server
        ;;  
    3)  
        echo "Executing test_event_client"    
        export VSOMEIP_CONFIGURATION=./../etc/vsomeip-udp-event-client.json
        export VSOMEIP_APPLICATION_NAME=test_event_client
        ./../build/test_event_client
        ;;  
    4)  
        echo "Executing test_event_server"    
        export VSOMEIP_CONFIGURATION=./../etc/vsomeip-udp-event-service.json
        export VSOMEIP_APPLICATION_NAME=test_event_server
        ./../build/test_event_server
        ;;
    5)  
        echo "Executing test_mix_server"    
        export VSOMEIP_CONFIGURATION=./../etc/vsomeip-udp-mix-service.json
        export VSOMEIP_APPLICATION_NAME=test_mix_server
        ./../build/test_mix_server
        ;; 
    *)  
        echo "Invalid argument. Please use 1 or 2 or 3 or 4 or 5."  
        exit 1
        ;;  
esac