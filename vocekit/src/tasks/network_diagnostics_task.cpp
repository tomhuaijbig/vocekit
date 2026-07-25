#include "network_diagnostics_task.h"

#include "diagnostic_helpers.h"

#include <QByteArray>
#include <QList>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QPair>
#include <QUrl>
#include <QVector>

namespace {

QString ndTr8(const char *text)
{
    return QString::fromUtf8(text);
}

bool isUsableProxy(const QNetworkProxy &proxy)
{
    return proxy.type() != QNetworkProxy::NoProxy
        && proxy.type() != QNetworkProxy::DefaultProxy;
}

QStringList systemProxyDescriptions()
{
    const QList<QNetworkProxy> proxies = QNetworkProxyFactory::systemProxyForQuery(
        QNetworkProxyQuery(QUrl(QStringLiteral("https://api.deepseek.com")))
    );

    QStringList details;
    for (const QNetworkProxy &proxy : proxies) {
        if (isUsableProxy(proxy)) {
            details << proxy.hostName() + QStringLiteral(":") + QString::number(proxy.port());
        }
    }
    return details;
}

QStringList environmentProxyDescriptions()
{
    QStringList details;
    const QList<QByteArray> envNames = QList<QByteArray>()
        << "HTTP_PROXY"
        << "HTTPS_PROXY"
        << "ALL_PROXY";
    for (const QByteArray &name : envNames) {
        const QByteArray value = qgetenv(name.constData());
        if (!value.trimmed().isEmpty()) {
            details << QString::fromLatin1(name) + ndTr8("（已设置，内容已隐藏）");
        }
    }
    return details;
}

QStringList virtualAdapterDescriptions()
{
    QStringList adapters;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &networkInterface : interfaces) {
        if (!(networkInterface.flags() & QNetworkInterface::IsUp)) {
            continue;
        }

        const QString description = networkInterface.humanReadableName()
            + QStringLiteral(" ")
            + networkInterface.name();
        const QString lowered = description.toLower();
        if (lowered.contains(QStringLiteral("tun"))
            || lowered.contains(QStringLiteral("tap"))
            || lowered.contains(QStringLiteral("wintun"))
            || lowered.contains(QStringLiteral("v2ray"))
            || lowered.contains(QStringLiteral("clash"))
            || lowered.contains(QStringLiteral("sing-box"))
            || lowered.contains(QStringLiteral("wireguard"))
            || lowered.contains(QStringLiteral("tailscale"))) {
            adapters << description.trimmed();
        }
    }
    return adapters;
}

bool appendCancellableLine(
    QStringList *lines,
    const QString &line,
    const CancellationToken &cancellation)
{
    if (!lines || cancellation.isCancellationRequested()) {
        return false;
    }
    if (!line.trimmed().isEmpty()) {
        lines->append(line);
    }
    return !cancellation.isCancellationRequested();
}

} // namespace

QStringList runNetworkDiagnosticsTask(const NetworkDiagnosticsRequest &request)
{
    QStringList lines;
    if (request.cancellation.isCancellationRequested()) {
        return lines;
    }

    lines << diagnosticStatusLine(
        ndTr8("软件网络模式"),
        request.useSystemProxy ? ndTr8("使用系统代理") : ndTr8("直连")
    );

    const QStringList proxyDetails = systemProxyDescriptions();
    lines << diagnosticStatusLine(
        ndTr8("Windows 系统代理"),
        proxyDetails.isEmpty() ? ndTr8("未检测到明确代理") : ndTr8("已检测到代理"),
        proxyDetails.join(QStringLiteral("；"))
    );

    const QStringList envProxies = environmentProxyDescriptions();
    lines << diagnosticStatusLine(
        ndTr8("环境变量代理"),
        envProxies.isEmpty() ? ndTr8("未检测到") : ndTr8("已检测到"),
        envProxies.join(QStringLiteral("\n  "))
    );

    const QStringList virtualAdapters = virtualAdapterDescriptions();
    lines << diagnosticStatusLine(
        ndTr8("TUN / 虚拟网卡"),
        virtualAdapters.isEmpty() ? ndTr8("未发现明显特征") : ndTr8("检测到可能接管流量的网卡"),
        virtualAdapters.join(QStringLiteral("\n  "))
    );

    QStringList hosts = QStringList()
        << QStringLiteral("api.deepseek.com")
        << QStringLiteral("aip.baidubce.com")
        << QStringLiteral("vop.baidu.com")
        << QStringLiteral("iat-api.xfyun.cn")
        << QStringLiteral("api.openai.com")
        << QStringLiteral("api.anthropic.com");
    QVector<QPair<QString, QUrl>> customEndpoints;

    if (!request.secrets.customSpeechUrl.trimmed().isEmpty()) {
        const QUrl url(request.secrets.customSpeechUrl.trimmed());
        if (url.isValid() && !url.host().isEmpty()) {
            hosts << url.host();
            customEndpoints.append(qMakePair(ndTr8("自定义语音接口"), url));
        }
    }

    for (const CustomModelProfile &profile : request.secrets.effectiveCustomModels()) {
        const QUrl url(profile.url.trimmed());
        if (!url.isValid() || url.host().isEmpty()) {
            continue;
        }
        hosts << url.host();
        customEndpoints.append(qMakePair(
            profile.name.trimmed().isEmpty() ? ndTr8("自定义大模型") : profile.name.trimmed(),
            url
        ));
    }

    hosts.removeDuplicates();
    for (const QString &host : hosts) {
        if (!appendCancellableLine(
            &lines,
            networkDnsLookupLine(host, request.cancellation),
            request.cancellation
        )) {
            return QStringList();
        }
    }

    if (!appendCancellableLine(&lines, networkProbeLine(
        ndTr8("DeepSeek 域名连接"),
        QUrl(QStringLiteral("https://api.deepseek.com")),
        request.useSystemProxy,
        request.cancellation
    ), request.cancellation)) {
        return QStringList();
    }
    if (!appendCancellableLine(&lines, networkProbeLine(
        ndTr8("百度令牌域名连接"),
        QUrl(QStringLiteral("https://aip.baidubce.com")),
        request.useSystemProxy,
        request.cancellation
    ), request.cancellation)) {
        return QStringList();
    }
    if (!appendCancellableLine(&lines, networkProbeLine(
        ndTr8("讯飞听写域名连接"),
        QUrl(QStringLiteral("https://iat-api.xfyun.cn/v2/iat")),
        request.useSystemProxy,
        request.cancellation
    ), request.cancellation)) {
        return QStringList();
    }
    if (!appendCancellableLine(&lines, networkProbeLine(
        ndTr8("OpenAI 域名连接"),
        QUrl(QStringLiteral("https://api.openai.com")),
        request.useSystemProxy,
        request.cancellation
    ), request.cancellation)) {
        return QStringList();
    }
    if (!appendCancellableLine(&lines, networkProbeLine(
        ndTr8("Claude 域名连接"),
        QUrl(QStringLiteral("https://api.anthropic.com")),
        request.useSystemProxy,
        request.cancellation
    ), request.cancellation)) {
        return QStringList();
    }
    for (const QPair<QString, QUrl> &endpoint : customEndpoints) {
        if (!appendCancellableLine(&lines, networkProbeLine(
            endpoint.first + ndTr8("连接"),
            endpoint.second,
            request.useSystemProxy,
            request.cancellation
        ), request.cancellation)) {
            return QStringList();
        }
    }

    if (request.cancellation.isCancellationRequested()) {
        return QStringList();
    }
    lines << diagnosticStatusLine(
        ndTr8("TUN / 透明代理提示"),
        virtualAdapters.isEmpty() ? ndTr8("仍需人工确认") : ndTr8("建议检查分流规则"),
        ndTr8("虚拟网卡名称只能作为线索，无法百分百判断实际分流。若开启 v2rayN TUN、Clash TUN、sing-box TUN，请把百度、讯飞等国内接口域名按需要设置为直连。")
    );
    return lines;
}
