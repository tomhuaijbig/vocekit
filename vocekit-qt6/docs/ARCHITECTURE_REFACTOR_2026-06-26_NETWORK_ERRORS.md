# 2026-06-26 网络错误文案模块抽离

## 本次调整

- 新增 `src/providers/network_error_messages.h/.cpp`。
- 从 `voiceassistant.cpp` 移出讯飞语音听写网络错误解释逻辑。
- `ApiClient` 继续在讯飞 WebSocket 报错和断开连接时使用同一套错误文案。

## 为什么拆出

讯飞连接失败、TUN 模式、透明代理和虚拟网卡导致的错误解释属于接口层和网络层逻辑。
把它从主 UI 文件移出后，后续扩展网络诊断、接口单项自检或更多供应商错误文案时，不需要继续增加主文件体积。

## 保留行为

- 远端关闭连接时，仍提示 TUN、透明代理、虚拟网卡、DNS、出口 IP、白名单或 WebSocket 代理兼容性等可能原因。
- 普通网络失败时，仍提示把 `iat-api.xfyun.cn` 和 `*.xfyun.cn` 设置为直连。

## 验证

- 使用 Qt 5.9 MinGW release 构建通过。
