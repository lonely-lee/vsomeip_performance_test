import pandas as pd  
import matplotlib.pyplot as plt 
import os
import glob 

handled_dir = './../result/'
# 处理cpu负载和内存占用率数据
files = glob.glob('./../result/test*.txt')

# 创建两个空的DataFrame用于存储所有文件的平均内存和CPU负载
average_cpu_mem_df = pd.DataFrame(columns=['event/method', 'ser/cli', 'protocol', 'payloadsize', 'average_cpuload','average_memory'])

for file in files:

    # 解析文本名称，例如test-event-server-udp-100.txt,解析出来event、server、udp、100四个字段
    filename_parts = os.path.basename(file).split('_')[1:]
    event, server, number,protocol = filename_parts[0], filename_parts[1], filename_parts[2], filename_parts[3].split('.')[0]
    new_filename = handled_dir+'_'.join(filename_parts)
    img_name=handled_dir+event+'_'+server+'_'+protocol+'_'+number+'.png'

    df = pd.read_csv(file, sep=' | ', engine='python')

    # print(df.columns)

    # 计算时间戳差值，以第一个时间戳为基准 
    df['timestamp'] = df['timestamp'].astype(float) 
    first_timestamp = df['timestamp'].iloc[0]  
    df['timestamp'] = df['timestamp'] - first_timestamp +1


    # 计算CPU占用率  
    df['cpu_usage_rate'] = (df['processTime'].diff() / df['systemTime'].diff()) * 100
    # NaN转化为0
    df.fillna(0, inplace=True)
    # print(df['cpu_usage_rate'])


    # 将处理后的第一列数据和第四列数据以及第五列数据重新保存在新的文件内
    df[['timestamp', 'memory', 'cpu_usage_rate']].to_csv(new_filename, index=False)

    # 计算平均内存和CPU占用率  
    average_memory = df['memory'].mean()
    average_cpuload = df['cpu_usage_rate'].mean()
    # print(type(average_mem_df))
    average_cpu_mem_df = average_cpu_mem_df._append({'event/method': event, 'ser/cli': server, 'protocol': protocol, 'payloadsize': number, 'average_cpuload': average_cpuload,'average_memory': average_memory}, ignore_index=True)

    # 绘制CPU占用率随时间变化的图像  
    plt.figure(figsize=(12, 8))  
    
    plt.subplot(2, 1, 1)  
    plt.plot(df['timestamp'], df['cpu_usage_rate'], label='CPU Usage Rate', marker='o')  
    plt.title('CPU Usage Rate Over Time')  
    plt.xlabel('Timestamp')  
    plt.ylabel('CPU Usage Rate')  
    plt.grid(True)  
    plt.legend()
    
    # 绘制内存占用随时间变化的图像（注意：由于内存值在你的数据中都是相同的，所以这条线将是水平的）  
    plt.subplot(2, 1, 2)
    plt.plot(df['timestamp'], df['memory'], label='Memory Usage', marker='o')
    plt.title('Memory Usage Over Time')
    plt.xlabel('Timestamp')
    plt.ylabel('Memory Usage (bytes)')
    plt.grid(True)
    plt.legend()
    
    # 保存图像  
    plt.tight_layout()  # 自动调整子图参数, 使之填充整个图像区域  
    # print(img_name)
    plt.savefig( img_name)
    plt.close() 

# 保存平均内存和CPU负载到文件
average_cpu_mem_df.to_csv(handled_dir+'total_average_mem.txt', index=False)
average_cpu_mem_df['payloadsize'] = average_cpu_mem_df['payloadsize'].astype(int) 
average_cpu_mem_df.sort_values(by='payloadsize', ascending=True,inplace=True)

event_grouped = average_cpu_mem_df.groupby('event/method')
for name, event_g in event_grouped:
    server_event_grouped = event_g.groupby('ser/cli')
    for name, ser_eve_g in server_event_grouped:
        pro_ser_eve_grouped = ser_eve_g.groupby('protocol')
        # 对于每一组（即每一种协议），绘制一条线
        plt.figure(figsize=(12, 8))
        for name, group in pro_ser_eve_grouped:
            small_payloads = group[group['payloadsize'] < 1400]
            large_payloads  = group[group['payloadsize'] >= 1400]
            plt.subplot(2, 2, 1) 
            plt.plot(small_payloads['payloadsize'], small_payloads['average_memory'], label=name, marker='*')
            # 添加图例和标签
            plt.legend()
            plt.xlabel('Payload Size')
            plt.ylabel('Average Memory')
            plt.title('Average Memory vs small Payload Size for Different Protocols')
            plt.grid(True)
            plt.subplot(2, 2, 2) 
            plt.plot(large_payloads['payloadsize'], large_payloads['average_memory'], label=name,marker='*')
            # 添加图例和标签
            plt.legend()
            plt.xlabel('Payload Size')
            plt.ylabel('Average Memory')
            plt.title('Average Memory vs large Payload Size for Different Protocols')
            plt.grid(True)
            plt.subplot(2, 2, 3) 
            plt.plot(small_payloads['payloadsize'], small_payloads['average_cpuload'], label=name,marker='*')
            # 添加图例和标签
            plt.legend()
            plt.xlabel('Payload Size')
            plt.ylabel('Average CPUload')
            plt.title('Average CPUload vs small Payload Size for Different Protocols')
            plt.grid(True)
            plt.subplot(2, 2, 4) 
            plt.plot(large_payloads['payloadsize'], large_payloads['average_cpuload'], label=name,marker='*')
            # 添加图例和标签
            plt.legend()
            plt.xlabel('Payload Size')
            plt.ylabel('Average CPUload')
            plt.title('Average CPUload vs large Payload Size for Different Protocols')
            plt.grid(True)
        plt.tight_layout()
        img_name=handled_dir+group['event/method'].iloc[0] +'_'+group['ser/cli'].iloc[0] +'_Mem_cpu.png'
        # print(img_name)
        plt.savefig(img_name)
        plt.close() 


# 处理通信延迟和吞吐量数据
files = glob.glob('./../result/*_data.txt')
protocol_labels = {1: 'UDP', 0: 'TCP'} 
for file in files:
    # 解析文本名称，例如event_server_data.txt,解析出来event、server、udp、100四个字段
    filename_parts = os.path.basename(file).split('_')
    event, server = filename_parts[0], filename_parts[1].split('.')[0]
    new_filename = '_'.join(filename_parts)

    df = pd.read_csv(file, sep=' | ', engine='python')
    df.sort_values(by='payload_size(Bytes)', ascending=True,inplace=True)
    grouped = df.groupby('protocol(1:udp|0:tcp)')

    plt.figure(figsize=(12, 8))  
    
    for name, group in grouped:
        small_payloads = group[group['payload_size(Bytes)'] < 1400]
        large_payloads  = group[group['payload_size(Bytes)'] >= 1400]
        plt.subplot(2, 2, 1) 
        protocol = protocol_labels.get(name, 'Unknown Protocol')  # 如果name不在字典中，则使用'Unknown Protocol'
        plt.plot(small_payloads['payload_size(Bytes)'], small_payloads['average_throughput(Byte/s)'], label=protocol,marker='*')
        plt.legend()
        plt.xlabel('Payload Size')
        plt.ylabel('Average Throughput')
        plt.title('Throughput vs small Payload Size for Different Protocols')
        plt.grid(True)
        plt.subplot(2, 2, 2) 
        large_payloads  = group[group['payload_size(Bytes)'] >= 1400]
        plt.plot(large_payloads['payload_size(Bytes)'], large_payloads['average_throughput(Byte/s)'], label=protocol,marker='*')
        plt.legend()
        plt.xlabel('Payload Size')
        plt.ylabel('Average Throughput')
        plt.title('Throughput vs large Payload Size for Different Protocols')
        plt.grid(True)
        plt.subplot(2, 2, 3) 
        plt.plot(small_payloads['payload_size(Bytes)'], small_payloads['average_latency(us)'], label=protocol,marker='*')
        plt.legend()
        plt.xlabel('Payload Size')
        plt.ylabel('Average Latency')
        plt.title('Latency vs small Payload Size for Different Protocols')
        plt.grid(True)
        plt.subplot(2, 2, 4) 
        plt.plot(large_payloads['payload_size(Bytes)'], large_payloads['average_latency(us)'], label=protocol,marker='*')
        plt.legend()
        plt.xlabel('Payload Size')
        plt.ylabel('Average Latency')
        plt.title('Latency vs large Payload Size for Different Protocols')
        plt.grid(True)

    plt.tight_layout()
    plt.savefig( handled_dir+event+'_'+server+'_Latency_Throughput.png')
    plt.close() 