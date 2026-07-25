#include "attention_message.h"

#include <QtWidgets>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

AttentionFaqCallback g_openFaqCallback;

QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

bool textContains(const QString &text, const char *needle)
{
    return text.contains(tr8(needle), Qt::CaseInsensitive);
}

QString faqIdForTitle(const QString &title)
{
    if (textContains(title, "结果小框：重新生成")) return QStringLiteral("1");
    if (textContains(title, "结果小框：流式显示")) return QStringLiteral("2");
    if (textContains(title, "主页功能卡片排序")) return QStringLiteral("3");
    if (textContains(title, "录音倒计时或提示音")) return QStringLiteral("4");
    if (textContains(title, "浮动条波形")) return QStringLiteral("5");
    if (textContains(title, "接口自检失败")) return QStringLiteral("6");
    if (textContains(title, "网络诊断显示域名不可达")) return QStringLiteral("7");
    if (textContains(title, "麦克风测试失败")) return QStringLiteral("8");
    if (textContains(title, "浮动条测试失败")) return QStringLiteral("9");
    if (textContains(title, "缺少语音识别密钥")) return QStringLiteral("10");
    if (textContains(title, "讯飞连接被远端主机关闭")) return QStringLiteral("11");
    if (textContains(title, "讯飞语音听写网络请求超时")) return QStringLiteral("12");
    if (textContains(title, "百度令牌获取失败")) return QStringLiteral("13");
    if (textContains(title, "缺少大模型密钥")) return QStringLiteral("14");
    if (textContains(title, "网络请求超时")) return QStringLiteral("15");
    if (textContains(title, "网络请求失败")) return QStringLiteral("16");
    if (textContains(title, "SSL 运行库缺失")) return QStringLiteral("17");
    if (textContains(title, "接口认证失败")) return QStringLiteral("18");
    if (textContains(title, "录音为空")) return QStringLiteral("19");
    if (textContains(title, "模型没有返回结果")) return QStringLiteral("20");
    if (textContains(title, "未识别到有选中文字") || textContains(title, "没有选中文字")) return QStringLiteral("21");
    if (textContains(title, "需要输入方式")) return QStringLiteral("22");
    if (textContains(title, "提示词已锁定")) return QStringLiteral("23");
    if (textContains(title, "无法写入配置文件")) return QStringLiteral("24");
    if (textContains(title, "名称、快捷键或输入方式错误")) return QStringLiteral("25");
    if (textContains(title, "无法播放 / 删除失败")) return QStringLiteral("26");
    if (textContains(title, "备份、导入或导出失败")) return QStringLiteral("27");
    if (textContains(title, "浮动条已关闭")) return QStringLiteral("28");
    if (textContains(title, "删除历史记录")) return QStringLiteral("29");
    if (textContains(title, "这里没有列出的弹窗")) return QStringLiteral("30");
    if (textContains(title, "词库 AI 生成失败")
        || textContains(title, "保存词条失败")
        || textContains(title, "词条无修正效果")) return QStringLiteral("31");
    if (textContains(title, "百度示例代码解析失败")) return QStringLiteral("32");
    if (textContains(title, "开机自启动没有打开主界面")) return QStringLiteral("33");
    if (textContains(title, "强力选中触发安全软件提醒")) return QStringLiteral("34");
    if (textContains(title, "长录音分段识别失败")) return QStringLiteral("39");
    if (textContains(title, "按住说话没有开始或松开后没有结束")) return QStringLiteral("40");
    if (textContains(title, "程序突然退出")) return QStringLiteral("35");
    if (textContains(title, "接口页只显示当前语音服务")) return QStringLiteral("36");
    if (textContains(title, "自定义接口调用失败")) return QStringLiteral("37");
    if (textContains(title, "图片识别失败") || textContains(title, "OCR 引擎不可用")) return QStringLiteral("38");
    return QString();
}

QString faqIdForPopup(const QString &title, const QString &text)
{
    const QString byTitle = faqIdForTitle(title);
    if (!byTitle.isEmpty()) {
        return byTitle;
    }

    const QString combined = title + QStringLiteral("\n") + text;
    if (textContains(combined, "图片识别失败")
        || textContains(combined, "OCR 助手")
        || textContains(combined, "RapidOCR")
        || textContains(combined, "Windows OCR")
        || textContains(combined, "云端 OCR")
        || textContains(combined, "截图")) return QStringLiteral("38");
    if (textContains(combined, "重新生成") || textContains(combined, "换模型") || textContains(combined, "继续追问")) return QStringLiteral("1");
    if (textContains(combined, "流式显示") || textContains(combined, "一直没有文字")) return QStringLiteral("2");
    if (textContains(combined, "功能卡片") && textContains(combined, "排序")) return QStringLiteral("3");
    if (textContains(combined, "倒计时") || textContains(combined, "提示音")) return QStringLiteral("4");
    if (textContains(combined, "波形")) return QStringLiteral("5");
    if (textContains(combined, "接口自检")) return QStringLiteral("6");
    if (textContains(combined, "网络诊断") || textContains(combined, "域名不可达")) return QStringLiteral("7");
    if (textContains(combined, "麦克风测试") || textContains(combined, "有效声音")) return QStringLiteral("8");
    if (textContains(combined, "浮动条测试") || textContains(combined, "结果小框测试")) return QStringLiteral("9");
    if (textContains(combined, "缺少讯飞") || textContains(combined, "缺少百度") || textContains(combined, "语音识别密钥")) return QStringLiteral("10");
    if (textContains(combined, "讯飞语音听写连接被远端主机关闭")
        || textContains(combined, "iat-api.xfyun.cn")
        || textContains(combined, "远端主机关闭了这个连接")) return QStringLiteral("11");
    if (textContains(combined, "讯飞语音听写网络请求超时")) return QStringLiteral("12");
    if (textContains(combined, "百度令牌获取失败") || textContains(combined, "百度识别失败")) return QStringLiteral("13");
    if (textContains(combined, "词库 AI 生成失败")
        || textContains(combined, "AI 生成失败")
        || textContains(combined, "保存词条失败")
        || textContains(combined, "词条无修正效果")
        || textContains(combined, "没有生成可产生修正效果的词条")
        || textContains(combined, "模型返回内容无法解析为词条")) return QStringLiteral("31");
    if (textContains(combined, "缺少 DeepSeek")
        || textContains(combined, "缺少 OpenAI")
        || textContains(combined, "缺少 Claude")
        || textContains(combined, "缺少大模型")) return QStringLiteral("14");
    if (textContains(combined, "网络请求超时")) return QStringLiteral("15");
    if (textContains(combined, "Error creating SSL context")
        || textContains(combined, "libeay32.dll")
        || textContains(combined, "ssleay32.dll")) return QStringLiteral("17");
    if (textContains(combined, "Host requires authentication")
        || textContains(combined, "Connection closed")
        || textContains(combined, "网络请求失败")) return QStringLiteral("16");
    if (textContains(combined, "认证失败")
        || textContains(combined, "鉴权失败")
        || textContains(combined, "401")
        || textContains(combined, "403")) return QStringLiteral("18");
    if (textContains(combined, "录音为空") || textContains(combined, "没有识别到语音")) return QStringLiteral("19");
    if (textContains(combined, "模型没有返回结果")) return QStringLiteral("20");
    if (textContains(combined, "未识别到有选中文字") || textContains(combined, "没有选中文字")) return QStringLiteral("21");
    if (textContains(combined, "需要输入方式")
        || textContains(combined, "没有启用任何输入方式")
        || textContains(combined, "至少需要启用")) return QStringLiteral("22");
    if (textContains(combined, "提示词已锁定") || textContains(combined, "无法保存提示词")) return QStringLiteral("23");
    if (textContains(combined, "名称不能为空")
        || textContains(combined, "快捷键不能为空")
        || textContains(combined, "快捷键冲突")
        || textContains(combined, "快捷键无效")
        || textContains(combined, "快捷键注册失败")) return QStringLiteral("25");
    if (textContains(combined, "无法播放")
        || textContains(combined, "无法更新收藏")
        || textContains(combined, "部分文件无法删除")) return QStringLiteral("26");
    if (textContains(combined, "备份失败")
        || textContains(combined, "导入失败")
        || textContains(combined, "导出失败")
        || textContains(combined, "没有可导出记录")
        || textContains(combined, "没有录音可导出")
        || textContains(combined, "没有选中记录")) return QStringLiteral("27");
    if (textContains(combined, "config/settings.json")
        || textContains(combined, "config/secrets.json")
        || textContains(combined, "无法写入")) return QStringLiteral("24");
    if (textContains(combined, "浮动条已关闭")) return QStringLiteral("28");
    if (textContains(combined, "所有录音分段都识别失败")
        || textContains(combined, "录音分段")
        || textContains(combined, "分段识别失败")) return QStringLiteral("39");
    if (textContains(combined, "解析失败")
        && (textContains(combined, "client_id")
            || textContains(combined, "client_secret"))) return QStringLiteral("32");
    if (textContains(combined, "自定义语音接口") || textContains(combined, "自定义大模型")) return QStringLiteral("37");
    return QString();
}

void showAttentionMessageBox(
    QMessageBox::Icon icon,
    QWidget *parent,
    const QString &title,
    const QString &text
)
{
    QWidget *visibleParent = parent && parent->isVisible() ? parent : nullptr;
    const QString faqId = faqIdForPopup(title, text);
    const QString displayTitle = faqId.isEmpty()
        ? title
        : tr8("问题 ") + faqId + tr8("：") + title;
    QString displayText = text;
    if (!faqId.isEmpty() && !displayText.startsWith(tr8("问题编号："))) {
        displayText = tr8("问题编号：") + faqId
            + tr8("\n点击“查看解决办法”，或在左侧“常见问题”里搜索这个编号。\n\n")
            + text;
    }
    QMessageBox box(icon, displayTitle, displayText, QMessageBox::Ok, visibleParent);
    box.setWindowFlags(box.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    box.setWindowModality(Qt::ApplicationModal);
    QPushButton *faqButton = nullptr;
    if (!faqId.isEmpty()) {
        faqButton = box.addButton(tr8("查看解决办法"), QMessageBox::ActionRole);
    }
    if (QAbstractButton *okButton = box.button(QMessageBox::Ok)) {
        okButton->setText(tr8("确定"));
    }

    QTimer::singleShot(0, &box, [&box]() {
        const int screenNumber = QApplication::desktop()->screenNumber(QCursor::pos());
        const QRect screen = QApplication::desktop()->availableGeometry(screenNumber);
        box.adjustSize();
        box.move(screen.center() - box.rect().center());
        box.show();
        box.raise();
        box.activateWindow();
        QApplication::alert(&box, 3000);
#ifdef Q_OS_WIN
        const HWND handle = reinterpret_cast<HWND>(box.winId());
        SetWindowPos(handle, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        ShowWindow(handle, SW_RESTORE);
        SetForegroundWindow(handle);
#endif
    });
    box.exec();
    if (faqButton && box.clickedButton() == faqButton && g_openFaqCallback) {
        g_openFaqCallback(faqId);
    }
}

} // namespace

void setAttentionFaqCallback(const AttentionFaqCallback &callback)
{
    g_openFaqCallback = callback;
}

QString attentionFaqIdForTitle(const QString &title)
{
    return faqIdForTitle(title);
}

void showAttentionWarning(QWidget *parent, const QString &title, const QString &text)
{
    showAttentionMessageBox(QMessageBox::Warning, parent, title, text);
}

void showAttentionInformation(QWidget *parent, const QString &title, const QString &text)
{
    showAttentionMessageBox(QMessageBox::Information, parent, title, text);
}
