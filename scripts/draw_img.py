import matplotlib.pyplot as plt  
  
# 读取文件并解析数据  
with open('data.txt', 'r') as file:  
    lines = file.readlines()  
  
# 初始化列表来存储数据  
payload_sizes = []
average_throughputs = []
average_latency = []
average_cpu_load = []
average_load_no_zero = []
  
# # 解析数据  
# for line in lines[1:]:  # 跳过第一行（标题）  
#     values = line.strip().split()  # 去除每行末尾的换行符，并按空格分割  
#     payload_sizes.append(int(values[0]))  
#     average_throughputs.append(int(values[1]))  
#     average_latency.append(float(values[2]))  
#     average_cpu_load.append(float(values[3]))
#     average_load_no_zero.append(float(values[4]))

    # 解析数据  
for line in lines[1:]:  # 跳过第一行（标题）  
    values = line.strip().split()  # 去除每行末尾的换行符，并按空格分割  
    payload_sizes.append(int(values[0]))  
    average_throughputs.append(int(values[1]))  
    average_latency.append(float(values[2]))  
    average_cpu_load.append(float(values[3]))

average_throughputs = [throughput / 1000000 for throughput in average_throughputs]
  
# # 绘制第一幅图：average_load_no_zero  
# plt.figure(figsize=(10, 6))  # 设置图形大小  
# plt.plot(payload_sizes, average_load_no_zero, marker='o')  # 绘制点图  
# plt.title('Average CPULoad')  # 设置标题  
# plt.xlabel('Payload Size(Byte)')  # 设置x轴标签  
# plt.ylabel('Average CPULoad(%)')  # 设置y轴标签  
# plt.grid(True)  # 显示网格  
# plt.savefig('CPULoad_no_zero.png') 
  
# 绘制第二幅图：average_throughput  
plt.figure(figsize=(10, 6))  # 设置图形大小  
plt.plot(payload_sizes, average_throughputs, marker='o')  # 绘制点图  
plt.title('Average Throughput')  # 设置标题  
plt.xlabel('Payload Size(Bytes)')  # 设置x轴标签  
plt.ylabel('Throughput(MBytes/s)')  # 设置y轴标签  
plt.grid(True)  # 显示网格  
plt.savefig('throughput.png') 

# 绘制第三幅图：average_latency  
plt.figure(figsize=(10, 6))  # 设置图形大小  
plt.plot(payload_sizes, average_latency, marker='o')  # 绘制点图  
plt.title('Average Latency')  # 设置标题  
plt.xlabel('Payload Size(Byte)')  # 设置x轴标签  
plt.ylabel('Latency(us)')  # 设置y轴标签  
plt.grid(True)  # 显示网格  
plt.savefig('Latency.png') 

# 绘制第四幅图：average_cpu_load  
plt.figure(figsize=(10, 6))  # 设置图形大小  
plt.plot(payload_sizes, average_cpu_load, marker='o')  # 绘制点图  
plt.title('Average CPULoad')  # 设置标题  
plt.xlabel('Payload Size(Byte)')  # 设置x轴标签  
plt.ylabel('CPULoad(%)')  # 设置y轴标签  
plt.grid(True)  # 显示网格  
plt.savefig('CPULoad.png') 