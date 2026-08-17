#include "network_error_messages.h"

namespace {

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

bool looksLikeRemoteClosedError(const QString &errorText)
{
    return errorText.contains(QStringLiteral("remote host closed"), Qt::CaseInsensitive)
        || errorText.contains(QStringLiteral("host closed"), Qt::CaseInsensitive)
        || errorText.contains(QStringLiteral("connection closed"), Qt::CaseInsensitive)
        || errorText.contains(tr8("远端主机关闭"))
        || errorText.contains(tr8("关闭了这个连接"));
}

} // namespace

QString xfyunNetworkErrorMessage(const QString &rawError)
{
    const QString original = rawError.trimmed().isEmpty()
        ? tr8("连接被关闭")
        : rawError.trimmed();
    if (looksLikeRemoteClosedError(original)) {
        return tr8(
            "讯飞语音听写连接被远端主机关闭。\n\n"
            "专业原因：当前网络可能启用了 TUN 模式、透明代理或虚拟网卡接管。"
            "这类模式会在网卡层重定向流量，软件即使设置为直连，WebSocket 握手也可能被代理链路转发、改写或更换出口 IP。"
            "讯飞 iat-api.xfyun.cn 可能因为出口 IP、DNS 解析、TLS/WebSocket 代理兼容性、控制台 IP 白名单或接口权限不匹配而主动断开连接。\n\n"
            "处理建议：在 v2rayN、Clash 或类似工具的 TUN 规则中，将 iat-api.xfyun.cn 和 *.xfyun.cn 设置为 DIRECT/直连；"
            "然后重新测试讯飞语音听写。\n\n"
            "原始错误："
        ) + original;
    }

    return tr8("讯飞语音听写网络请求失败：") + original
        + tr8("\n\n如果正在使用 TUN 模式、透明代理或虚拟网卡，请优先将 iat-api.xfyun.cn 和 *.xfyun.cn 设置为直连。");
}
