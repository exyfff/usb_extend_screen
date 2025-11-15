# USB扩展屏音频问题修复测试指南

## 修复内容总结

### 1. **任务优先级调整**
- 音频扬声器任务优先级：5 → 9 (最高)
- USB音频任务优先级：5 → 8
- 麦克风任务优先级：5 → 7  
- 视频处理任务优先级：10 → 6 (降低)

### 2. **视频处理优化**
- 增加视频帧缓冲区：6 → 10个
- 添加任务调度延时：1ms
- 减少视频处理对CPU的占用

### 3. **音频调试增强**
- 添加音频数据接收监控
- 增强音频输出回调调试信息
- 添加USB音频接口状态监控

## 测试步骤

### 1. 编译项目
```bash
cd d:\esp-idf\project\jc\usb_extend_screen
idf.py build
```

### 2. 烧录固件
```bash
idf.py flash
```

### 3. 监控日志
```bash
idf.py monitor
```

## 预期日志输出

### 正常情况下应该看到：

1. **音频初始化成功**
```
I (1235) app_uac: Codec initialized successfully
I (1235) app_uac: Codec format set: 48000Hz, 16bit, 1 channels
I (1235) app_uac: UAC device initialized successfully
```

2. **USB音频接口激活**
```
I (xxxx) usbd_uac: Set interface X alt Y
I (xxxx) usbd_uac: Speaker interface X-Y opened, bytes_per_ms=XXX, resolution=16
```

3. **音频数据接收**
```
I (xxxx) usbd_uac: First audio RX! bytes_received=XXX, spk_active=1
I (xxxx) usbd_uac: Audio RX: count=100, bytes_received=XXX, available=XXX
```

4. **音频输出处理**
```
I (xxxx) app_uac: First audio frame received! len=XXX
I (xxxx) app_uac: Audio output: frame 50, len=XXX, interval=XXX us
```

5. **视频丢帧减少**
- 丢帧数量应该显著减少
- 丢帧警告频率降低

## 故障排除

### 如果仍然没有声音：

1. **检查USB音频接口是否激活**
   - 查找 "Speaker interface opened" 日志
   - 确认 `spk_active=1`

2. **检查音频数据流**
   - 查找 "First audio RX" 日志
   - 确认 `bytes_received > 0`

3. **检查I2S输出**
   - 查找 "First audio frame received" 日志
   - 确认没有 "i2s write failed" 错误

4. **检查硬件连接**
   - 确认音频编解码器连接正常
   - 检查扬声器/耳机连接

### 如果视频性能下降：

1. **调整缓冲区数量**
   - 可以尝试增加到12-15个缓冲区

2. **调整任务延时**
   - 可以减少延时到0.5ms或移除延时

3. **调整任务优先级**
   - 可以适当提高视频任务优先级到7

## 性能监控

### 关键指标：
- 音频丢帧：应该为0或极少
- 视频丢帧：应该显著减少
- 音频延迟：应该在可接受范围内
- CPU使用率：应该平衡

### 监控命令：
```bash
# 实时监控日志
idf.py monitor | grep -E "(Audio|dropped_frames|fps)"

# 统计丢帧数量
idf.py monitor | grep "dropped_frames" | tail -20
```

## 进一步优化建议

1. **如果问题持续存在**：
   - 考虑将音频和视频任务分配到不同CPU核心
   - 优化内存分配和DMA配置
   - 调整USB传输参数

2. **性能调优**：
   - 根据实际使用场景调整缓冲区大小
   - 优化任务调度策略
   - 考虑使用硬件加速

## 回滚方案

如果修改导致其他问题，可以恢复原始配置：

```bash
git checkout HEAD -- sdkconfig main/app_vendor.c main/app_uac.c main/uac/usb_device_uac.c
```
