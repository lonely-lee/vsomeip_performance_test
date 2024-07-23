import pandas as pd  
import matplotlib.pyplot as plt  
  
# 读取数据  
df = pd.read_csv('./../_x86output/bin/test_event_server_100_TCP.txt', sep='|', header=None, names=['timestamp', 'system_cpu_ticks', 'process_cpu_ticks', 'memory'])  
  
# 计算CPU占用率  
df['cpu_usage_rate'] = df['process_cpu_ticks'] / df['system_cpu_ticks']  
  
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
  
# 显示图像  
plt.tight_layout()  # 自动调整子图参数, 使之填充整个图像区域  
plt.show()
plt.savefig('CPULoad.png') 