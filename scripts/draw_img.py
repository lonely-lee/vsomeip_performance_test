import matplotlib.pyplot as plt  
  
# 读取文件并解析数据  
with open('data.txt', 'r') as file:  
    lines = file.readlines()  
  
# 初始化列表来存储数据  
payload_sizes = []
average_vmrsses = []
average_throughputs = []
average_latency = []
average_cpu_load = []
  
# 解析数据  
for line in lines[1:]:  # 跳过第一行（标题）  
    values = line.strip().split()  # 去除每行末尾的换行符，并按空格分割  
    payload_sizes.append(int(values[0]))  
    average_vmrsses.append(int(values[1]))  
    average_throughputs.append(int(values[2]))  
    average_latency.append(float(values[3]))  
    average_cpu_load.append(float(values[4]))
  
# 绘制第一幅图：average_vmrss  
plt.figure(figsize=(10, 6))  # 设置图形大小  
plt.plot(payload_sizes, average_vmrsses, marker='o')  # 绘制点图  
plt.title('Average VMRSS')  # 设置标题  
plt.xlabel('Payload Size(Byte)')  # 设置x轴标签  
plt.ylabel('Average VMRSS(KB)')  # 设置y轴标签  
plt.grid(True)  # 显示网格  
plt.savefig('VMRSS.png') 
plt.show()  # 显示图形  
  
# 绘制第二幅图：average_throughput  
plt.figure(figsize=(10, 6))  # 设置图形大小  
plt.plot(payload_sizes, average_throughputs, marker='o')  # 绘制点图  
plt.title('Average Throughput')  # 设置标题  
plt.xlabel('Payload Size(Bytes)')  # 设置x轴标签  
plt.ylabel('Throughput(Bytes/s)')  # 设置y轴标签  
plt.grid(True)  # 显示网格  
plt.savefig('throughput.png') 
plt.show()  # 显示图形

# 绘制第三幅图：average_latency  
plt.figure(figsize=(10, 6))  # 设置图形大小  
plt.plot(payload_sizes, average_throughputs, marker='o')  # 绘制点图  
plt.title('Average Latency')  # 设置标题  
plt.xlabel('Payload Size(Byte)')  # 设置x轴标签  
plt.ylabel('Latency(ms)')  # 设置y轴标签  
plt.grid(True)  # 显示网格  
plt.savefig('Latency.png') 
plt.show()  # 显示图形

# 绘制第四幅图：average_cpu_load  
plt.figure(figsize=(10, 6))  # 设置图形大小  
plt.plot(payload_sizes, average_cpu_load, marker='o')  # 绘制点图  
plt.title('Average CPULoad')  # 设置标题  
plt.xlabel('Payload Size(Byte)')  # 设置x轴标签  
plt.ylabel('CPULoad(%)')  # 设置y轴标签  
plt.grid(True)  # 显示网格  
plt.savefig('CPULoad.png') 
plt.show()  # 显示图形