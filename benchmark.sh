#!/bin/bash

THREAD_NUM=$1
CONCURRENT=$2
URL="http://localhost:9006/1m.bin"

echo "=== 线程数: $THREAD_NUM, 并发: $CONCURRENT ==="

# 记录开始时间
start=$(date +%s.%N)

# 并发请求
for i in $(seq 1 $CONCURRENT); do
    curl -o /dev/null -s -w "%{time_total}\n" $URL &
done
wait

# 记录结束时间
end=$(date +%s.%N)
total=$(echo "$end - $start" | bc)

echo "总时间: $total 秒"
echo "平均响应: $(echo "scale=3; $total / $CONCURRENT" | bc) 秒"