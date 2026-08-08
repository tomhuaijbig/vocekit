#include "faq_panel.h"

#include "attention_message.h"
#include "ui_style.h"

#include <QtWidgets>

static QString tr8(const char *text)
{
    return QString::fromUtf8(text);
}

static QString faqCardsStyle()
{
    return cardStyle() + QStringLiteral(
        "QFrame#card { background: #ffffff; }"
        "QLabel#faqNumber { background: #111827; color: #ffffff; border-radius: 14px; font-weight: 700; }"
        "QLabel#faqLabel { color: #047857; font-weight: 700; }"
        "QLabel#faqBody { color: #344054; line-height: 1.45; }"
        "QFrame#faqBlock { background: #f8fafc; border: 1px solid #edf0f3; border-radius: 8px; }"
    );
}
QString FaqPanel::faqCategoryForText(const QString &text) const
    {
        if (text.contains(tr8("历史"), Qt::CaseInsensitive)
            || text.contains(tr8("备份"), Qt::CaseInsensitive)
            || text.contains(tr8("导入"), Qt::CaseInsensitive)
            || text.contains(tr8("导出"), Qt::CaseInsensitive)) {
            return QStringLiteral("history");
        }
        if (text.contains(tr8("词库"), Qt::CaseInsensitive)
            || text.contains(tr8("词条"), Qt::CaseInsensitive)) {
            return QStringLiteral("vocabulary");
        }
        if (text.contains(tr8("快捷键"), Qt::CaseInsensitive)
            || text.contains(tr8("选中文字"), Qt::CaseInsensitive)
            || text.contains(tr8("强力选中"), Qt::CaseInsensitive)) {
            return QStringLiteral("hotkey");
        }
        if (text.contains(tr8("结果小框"), Qt::CaseInsensitive)
            || text.contains(tr8("浮动条"), Qt::CaseInsensitive)
            || text.contains(tr8("卡片"), Qt::CaseInsensitive)) {
            return QStringLiteral("display");
        }
        if (text.contains(tr8("设置"), Qt::CaseInsensitive)
            || text.contains(tr8("配置文件"), Qt::CaseInsensitive)
            || text.contains(tr8("开机自启动"), Qt::CaseInsensitive)) {
            return QStringLiteral("settings");
        }
        if (text.contains(tr8("网络"), Qt::CaseInsensitive)
            || text.contains(tr8("代理"), Qt::CaseInsensitive)
            || text.contains(QStringLiteral("TUN"), Qt::CaseInsensitive)
            || text.contains(QStringLiteral("DNS"), Qt::CaseInsensitive)
            || text.contains(QStringLiteral("SSL"), Qt::CaseInsensitive)) {
            return QStringLiteral("network");
        }
        if (text.contains(tr8("接口"), Qt::CaseInsensitive)
            || text.contains(tr8("密钥"), Qt::CaseInsensitive)
            || text.contains(tr8("模型"), Qt::CaseInsensitive)
            || text.contains(tr8("图片识别"), Qt::CaseInsensitive)
            || text.contains(QStringLiteral("OCR"), Qt::CaseInsensitive)
            || text.contains(QStringLiteral("API"), Qt::CaseInsensitive)) {
            return QStringLiteral("interface");
        }
        if (text.contains(tr8("麦克风"), Qt::CaseInsensitive)
            || text.contains(tr8("录音"), Qt::CaseInsensitive)
            || text.contains(tr8("语音"), Qt::CaseInsensitive)) {
            return QStringLiteral("microphone");
        }
        return QStringLiteral("other");
    }

QString FaqPanel::faqDiagnosticKeyword(const QString &category, const QString &title) const
    {
        if (category == QStringLiteral("interface")) return tr8("接口自检");
        if (category == QStringLiteral("network")) return tr8("网络诊断");
        if (category == QStringLiteral("microphone")) return tr8("麦克风测试");
        if (category == QStringLiteral("vocabulary")) return tr8("词库测试");
        if (category == QStringLiteral("display")) {
            return title.contains(tr8("结果小框")) ? tr8("结果小框测试") : tr8("浮动条测试");
        }
        return QString();
    }

void FaqPanel::openDiagnosticForFaq(const QString &category, const QString &title)
{
    const QString keyword = faqDiagnosticKeyword(category, title);
    if (keyword.isEmpty() || !m_openDiagnostic) {
        return;
    }
    m_openDiagnostic(keyword);
}

void FaqPanel::applyFaqSearch()
    {
        if (!m_faqItemsLayout) {
            return;
        }
        const QString keyword = m_faqSearchEdit ? m_faqSearchEdit->text().trimmed() : QString();
        const QString category = m_faqCategoryBox ? m_faqCategoryBox->currentData().toString() : QStringLiteral("all");
        int matchedCount = 0;
        int renderedCount = 0;
        for (int i = 0; i < m_faqItemsLayout->count(); ++i) {
            QLayoutItem *item = m_faqItemsLayout->itemAt(i);
            QWidget *widget = item ? item->widget() : nullptr;
            if (!widget || widget == m_faqEmptyLabel) {
                continue;
            }
            const QString searchText = widget->property("faqSearchText").toString();
            if (searchText.isEmpty()) {
                continue;
            }
            const bool categoryMatched = category.isEmpty()
                || category == QStringLiteral("all")
                || widget->property("faqCategory").toString() == category;
            const bool matched = categoryMatched
                && (keyword.isEmpty() || searchText.contains(keyword, Qt::CaseInsensitive));
            if (matched) {
                ++matchedCount;
            }
            const bool shouldShow = matched && renderedCount < m_faqRenderLimit;
            if (shouldShow) {
                ensureFaqCardMaterialized(widget);
                ++renderedCount;
            }
            if (widget->isHidden() == shouldShow) {
                widget->setVisible(shouldShow);
            }
        }
        if (m_faqEmptyLabel) {
            m_faqEmptyLabel->setVisible(matchedCount == 0);
        }
        if (m_faqLoadMoreButton) {
            const int remainingCount = matchedCount - renderedCount;
            m_faqLoadMoreButton->setVisible(remainingCount > 0);
            m_faqLoadMoreButton->setText(
                tr8("显示更多（还有 %1 条）").arg(remainingCount)
            );
        }
    }

void FaqPanel::showFaqId(const QString &faqId)
{
    if (m_faqCategoryBox) {
        m_faqCategoryBox->setCurrentIndex(0);
    }
    if (m_faqSearchEdit) {
        m_faqSearchEdit->setText(faqId.trimmed());
    }
    applyFaqSearch();
}

int FaqPanel::matchCount(const QString &keyword) const
{
    if (keyword.trimmed().isEmpty() || !m_faqItemsLayout) {
        return 0;
    }
    const QString needle = keyword.trimmed();
    int matches = 0;
    for (int i = 0; i < m_faqItemsLayout->count(); ++i) {
        QLayoutItem *item = m_faqItemsLayout->itemAt(i);
        QWidget *widget = item ? item->widget() : nullptr;
        if (!widget || widget == m_faqEmptyLabel) {
            continue;
        }
        const QString searchText = widget->property("faqSearchText").toString();
        if (!searchText.isEmpty() && searchText.contains(needle, Qt::CaseInsensitive)) {
            ++matches;
        }
    }
    return matches;
}
FaqPanel::FaqPanel(
    const std::function<void(const QString &)> &openDiagnostic,
    QWidget *parent
)
    : QWidget(parent), m_openDiagnostic(openDiagnostic)
{
        auto *layout = new QVBoxLayout(this);        layout->setContentsMargins(28, 24, 28, 24);
        layout->setSpacing(14);

        auto *title = new QLabel(tr8("常见问题"));
        title->setFont(appFont(24, QFont::DemiBold));
        layout->addWidget(title);

        auto *filterRow = new QHBoxLayout;
        filterRow->setSpacing(10);

        m_faqSearchEdit = new QLineEdit;
        m_faqSearchEdit->setMinimumHeight(44);
        m_faqSearchEdit->setPlaceholderText(tr8("输入编号、弹窗标题、错误原文、原因或处理办法"));
        m_faqSearchEdit->setClearButtonEnabled(true);
        m_faqSearchEdit->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background: #ffffff;"
            "  border: 1px solid #d0d5dd;"
            "  border-radius: 8px;"
            "  padding: 0 14px;"
            "  color: #111827;"
            "  selection-background-color: #2563eb;"
            "}"
        ));
        connect(m_faqSearchEdit, &QLineEdit::textChanged, this, [this]() {
            m_faqRenderLimit = 8;
            applyFaqSearch();
        });
        m_faqCategoryBox = new QComboBox;
        m_faqCategoryBox->setMinimumSize(170, 44);
        m_faqCategoryBox->addItem(tr8("全部分类"), QStringLiteral("all"));
        m_faqCategoryBox->addItem(tr8("接口与模型"), QStringLiteral("interface"));
        m_faqCategoryBox->addItem(tr8("网络与代理"), QStringLiteral("network"));
        m_faqCategoryBox->addItem(tr8("麦克风与录音"), QStringLiteral("microphone"));
        m_faqCategoryBox->addItem(tr8("快捷键与选中"), QStringLiteral("hotkey"));
        m_faqCategoryBox->addItem(tr8("历史记录"), QStringLiteral("history"));
        m_faqCategoryBox->addItem(tr8("词库"), QStringLiteral("vocabulary"));
        m_faqCategoryBox->addItem(tr8("结果与界面"), QStringLiteral("display"));
        m_faqCategoryBox->addItem(tr8("设置与文件"), QStringLiteral("settings"));
        m_faqCategoryBox->addItem(tr8("其它"), QStringLiteral("other"));
        m_faqCategoryBox->setStyleSheet(QStringLiteral(
            "QComboBox { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 8px; padding: 0 10px; }"
        ));
        connect(m_faqCategoryBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this]() {
            m_faqRenderLimit = 8;
            applyFaqSearch();
        });
        filterRow->addWidget(m_faqSearchEdit, 1);
        filterRow->addWidget(m_faqCategoryBox);
        layout->addLayout(filterRow);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setStyleSheet(QStringLiteral(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollArea > QWidget > QWidget { background: transparent; }"
        ));

        auto *holder = new QWidget;
        holder->setStyleSheet(faqCardsStyle());
        auto *items = new QVBoxLayout(holder);
        m_faqItemsLayout = items;
        items->setContentsMargins(0, 0, 10, 0);
        items->setSpacing(12);

        addLatestFeatureFaqItems(items);

        items->addWidget(faqCard(
            tr8("测试工具：接口自检失败"),
            tr8("接口自检会测试已填写的百度、讯飞、自定义语音接口、DeepSeek、OpenAI、Claude 和自定义大模型。未填写的接口会跳过；失败通常是密钥错误、模型名不可用、网络代理分流错误、接口地址格式错误或接口权限未开通。"),
            QStringList()
                << tr8("先打开“设置 -> 接口”，确认对应接口密钥已经填写并保存。")
                << tr8("再打开左侧“功能自定义”，确认功能选择的模型名称和接口服务匹配。")
                << tr8("如果只有某一个服务失败，优先检查该服务控制台的应用权限、密钥状态和接口域名分流规则。")
        ));

        items->addWidget(faqCard(
            tr8("自定义接口调用失败"),
            tr8("弹窗可能写为：自定义语音接口调用失败、自定义大模型调用失败、自定义接口返回的不是 JSON，或没有返回识别文字/内容。通常是接口地址不对、接口协议不匹配、返回字段不符合软件读取规则、认证方式不一致或网络代理影响。"),
            QStringList()
                << tr8("自定义语音接口需要接收 JSON：format、rate、channel、len、speech，返回 text、result、transcript 或 data.text。")
                << tr8("自定义大模型建议兼容 OpenAI chat/completions。地址可以填根地址，也可以填完整 /v1/chat/completions。")
                << tr8("如果接口需要其它认证方式，目前先用你自己的中转服务适配成 Bearer Token 或无认证接口。")
                << tr8("如果开启了 TUN、透明代理或系统代理，先用“测试工具 -> 网络诊断”和“接口自检”确认目标域名能连通。")
        ));

        items->addWidget(faqCard(
            tr8("图片识别失败 / OCR 引擎不可用"),
            tr8("弹窗可能包含：OCR 助手程序不存在、模型缺失、语言包未安装、识别超时、图片无法读取、云端 OCR 鉴权失败或两个本地引擎都失败。自动选择会先尝试 RapidOCR，再回退到 Windows OCR。"),
            QStringList()
                << tr8("先到“测试工具 -> 接口自检”，选择“当前图片识别接口”并开始测试，确认实际使用的引擎和错误码。")
                << tr8("RapidOCR 不可用时，确认程序包里包含 RapidOCR 助手、运行库和模型文件；缺少这些文件时自动模式会改用 Windows OCR。")
                << tr8("Windows OCR 失败时，在 Windows 设置中安装中文（简体）或英文语言包，并确认待识别图片没有损坏。")
                << tr8("使用自定义云 OCR 时，检查接口地址、密钥、模型名称、返回 JSON 字段和网络代理；图片只会在用户明确选择云端接口并确认后上传。")
                << tr8("如果图片超过 25 MB、边长超过 8000 像素或格式不支持，请先缩小并转换为 PNG、JPG、BMP 或 WebP。")
        ));

        items->addWidget(faqCard(
            tr8("测试工具：网络诊断显示域名不可达"),
            tr8("网络诊断会检查 DNS 和核心接口域名连通性。域名不可达通常和 DNS、系统代理、TUN、透明代理、防火墙或安全软件有关。"),
            QStringList()
                << tr8("如果 DNS 失败，先更换 DNS 或检查代理软件的 DNS 设置。")
                << tr8("如果连通失败，分别测试“使用系统代理”开启和关闭两种状态。")
                << tr8("如果开启了 TUN 或透明代理，请把百度、讯飞等国内接口域名按需要设置为直连。")
        ));

        items->addWidget(faqCard(
            tr8("麦克风测试失败 / 没有检测到有效声音"),
            tr8("麦克风测试会录制约 3 秒钟。如果弹出“麦克风测试失败”或结果显示没有有效声音，通常是默认输入设备错误、录音权限被拦截、音量太低或设备被其它程序占用。"),
            QStringList()
                << tr8("在 Windows 声音设置里确认默认输入设备正确，并观察输入音量条是否变化。")
                << tr8("检查系统隐私设置和安全软件，允许本程序访问麦克风。")
                << tr8("关闭正在独占麦克风的软件后重新测试。")
        ));

        items->addWidget(faqCard(
            tr8("浮动条测试失败 / 结果小框测试看不到"),
            tr8("浮动条测试依赖常用设置里的“浮动条”开关；结果小框是普通独立小窗，可能被其它全屏程序、置顶窗口或安全软件遮挡。"),
            QStringList()
                << tr8("浮动条测试提示已关闭时，打开“设置 -> 常用设置”，勾选“启用浮动条”。")
                << tr8("如果结果小框看不到，先切回桌面或主界面，避免被全屏窗口遮住。")
                << tr8("如果安全软件拦截弹窗或置前行为，请把 vocekit.exe 加入信任。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：缺少语音识别密钥"),
            tr8("弹窗可能写为：缺少讯飞语音听写密钥；缺少百度语音识别密钥。一般是接口页没有填写对应语音服务的密钥，或者当前语音识别服务选错了。"),
            QStringList()
                << tr8("打开“设置 -> 接口”，先确认“当前语音识别服务”是你要用的百度或讯飞。")
                << tr8("使用讯飞时填写讯飞 AppID、API Key、API Secret；使用百度时填写百度 API Key、Secret Key。")
                << tr8("填写后点击接口页顶部固定位置的“保存接口配置”，再重新触发功能。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：讯飞连接被远端主机关闭"),
            tr8("弹窗可能写为：讯飞语音听写连接被远端主机关闭；讯飞语音听写网络请求失败：远端主机关闭了这个连接。常见原因是 TUN 模式、透明代理或虚拟网卡在底层接管了 WebSocket/TLS 连接。"),
            QStringList()
                << tr8("如果正在使用 v2rayN、Clash、sing-box 等工具，先检查是否开启了 TUN、透明代理、增强模式或虚拟网卡。")
                << tr8("必须开启这些模式时，把 iat-api.xfyun.cn 和 *.xfyun.cn 加入直连规则。")
                << tr8("仍然失败时，在“设置 -> 常用设置”里关闭“使用系统代理”，并重启软件后再试。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：讯飞语音听写网络请求超时"),
            tr8("讯飞 WebSocket 在限定时间内没有完成连接、鉴权或返回识别结果。网络代理、DNS、TUN 分流和本机防火墙都可能导致超时。"),
            QStringList()
                << tr8("优先关闭系统代理测试一次；如果必须走代理，把讯飞域名设置为直连。")
                << tr8("检查电脑网络、防火墙和安全软件是否拦截 vocekit.exe。")
                << tr8("确认讯飞控制台的应用处于启用状态，并且接口权限已经开通。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：百度令牌获取失败 / 百度识别失败"),
            tr8("百度语音识别需要先用 API Key 和 Secret Key 换取访问令牌，再上传录音。密钥错误、接口未开通、网络失败或录音格式异常都会触发这类弹窗。"),
            QStringList()
                << tr8("打开“设置 -> 接口”，确认百度 API Key 和 Secret Key 没有多余空格。")
                << tr8("确认百度智能云语音识别服务已开通，密钥属于同一个应用。")
                << tr8("如果使用代理或 TUN，把百度语音相关域名设置为直连后重试。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：缺少大模型密钥"),
            tr8("弹窗可能写为：缺少 DeepSeek 密钥；缺少 OpenAI 密钥；缺少 Claude 密钥。说明当前功能选择了对应模型服务，但接口页没有保存该服务的密钥。"),
            QStringList()
                << tr8("打开左侧“功能自定义”，确认当前功能选择的是你已经有密钥的模型服务。")
                << tr8("打开“设置 -> 接口”，在“大模型接口”分区填写对应 API Key。")
                << tr8("如果只是测试普通听写，可以先把听写模型切回 DeepSeek 或关闭听写整理。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：网络请求超时"),
            tr8("大模型或语音接口在超时时间内没有返回。它通常不是软件卡死，而是网络链路、代理分流、接口服务器或模型排队过慢。"),
            QStringList()
                << tr8("先在“设置 -> 常用设置”切换“使用系统代理”，分别测试直连和代理。")
                << tr8("在代理软件里把 DeepSeek、OpenAI、Claude、百度、讯飞域名分别按需要设置为代理或直连。")
                << tr8("把当前功能换成响应更快的模型，或缩短选中文本后再试。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：网络请求失败"),
            tr8("弹窗可能写为：网络请求失败：Connection closed、Host requires authentication 或其它原始错误。说明请求到接口时连接被关闭、代理需要认证、DNS 失败或服务端拒绝连接。"),
            QStringList()
                << tr8("如果看到 Host requires authentication，检查代理软件是否要求用户名密码，或关闭软件的“使用系统代理”。")
                << tr8("如果看到 Connection closed，优先检查 TUN/透明代理分流和接口域名直连规则。")
                << tr8("如果只在某一个接口失败，回到“设置 -> 接口”检查该接口密钥和服务是否可用。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：SSL 运行库缺失或版本不匹配"),
            tr8("弹窗会提示程序目录中缺少 libeay32.dll 和 ssleay32.dll，或出现 Error creating SSL context。这通常是 Qt 5.9 需要的 OpenSSL 运行库没有随程序一起部署。"),
            QStringList()
                << tr8("优先使用打包脚本生成的测试包运行，不要只复制 exe。")
                << tr8("确认程序目录里有 Qt 运行库、OpenSSL 相关 DLL 和平台插件目录。")
                << tr8("如果是开发目录运行，重新执行 deploy 脚本后再打开软件。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：接口认证失败"),
            tr8("弹窗会提示检查百度、讯飞、DeepSeek、OpenAI 或 Claude 密钥。一般是 API Key 填错、复制时多了空格、密钥已失效、模型无权限或模型名称不可用。"),
            QStringList()
                << tr8("重新复制密钥，避免前后空格和换行。")
                << tr8("确认接口页保存的是当前模型服务对应的密钥。")
                << tr8("到对应服务控制台检查接口权限和模型是否可用。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：录音为空 / 没有识别到语音"),
            tr8("弹窗可能写为：录音为空；没有识别到语音；讯飞没有识别到语音。通常是麦克风没有输入、录音时间太短、系统默认麦克风错误或录音权限被拦截。"),
            QStringList()
                << tr8("在 Windows 声音设置里确认默认输入设备正确，并能看到音量条变化。")
                << tr8("录音时说话时间稍长一点，避免按下快捷键后立刻停止。")
                << tr8("检查安全软件和系统隐私设置是否禁止程序访问麦克风。")
        ));

        items->addWidget(faqCard(
            tr8("处理失败：模型没有返回结果"),
            tr8("语音识别已经完成，但大模型返回为空或接口响应没有有效内容。可能是提示词过强、选中文本过长、模型服务异常或模型名称不可用。"),
            QStringList()
                << tr8("换一个模型重新测试，或减少选中的文字长度。")
                << tr8("打开“提示词”页面，检查对应功能的提示词是否要求过于严格。")
                << tr8("查看历史记录里的原始识别文本和错误信息，确认是哪一步为空。")
        ));

        items->addWidget(faqCard(
            tr8("未识别到有选中文字"),
            tr8("弹窗会提示：未识别到有选中文字。默认会先用普通方式读取选中文字；如果开启“强力选中”，普通读取失败后会临时模拟复制来兜底。"),
            QStringList()
                << tr8("用鼠标左键拖动选中网页、文档或文本框里的文字，再按对应快捷键。")
                << tr8("某些网页、PDF 或特殊控件不暴露文本选择内容，可以到“设置 -> 常用设置”开启“强力选中”再试。")
                << tr8("强力选中会临时模拟复制，可能被部分安全软件提示为高危行为；不需要时建议保持关闭。")
                << tr8("为避免浏览器或聊天软件的私有剪贴板格式导致崩溃，强力选中只会恢复文本、HTML、图片和链接等常用内容。")
                << tr8("如果不想读取选中文字，去左侧“功能自定义”关闭该功能的“读取鼠标选中的文字”。")
        ));

        items->addWidget(faqCard(
            tr8("需要输入方式 / 没有启用任何输入方式"),
            tr8("弹窗可能写为：至少需要启用读取鼠标选中的文字或使用语音输入中的一种；这个功能没有启用任何输入方式。说明当前功能没有输入来源。"),
            QStringList()
                << tr8("打开左侧“功能自定义”，给听写、翻译、问答或自定义功能启用至少一种输入。")
                << tr8("自定义功能也要在左侧“功能自定义”里单独设置输入方式。")
                << tr8("翻译如果只想翻译选中文字，就启用“读取鼠标选中的文字”，关闭“使用语音输入”。")
        ));

        items->addWidget(faqCard(
            tr8("提示词已锁定 / 无法保存提示词"),
            tr8("提示词页面和设置里的提示词使用同一份内容。锁定后不能编辑；保存失败通常是 prompts 目录或提示词文件没有写入权限。"),
            QStringList()
                << tr8("如果弹出“提示词已锁定”，先取消“锁定提示词”再编辑。")
                << tr8("如果弹出“无法保存提示词”，确认软件目录不是只读目录，提示词文件没有被其它程序占用。")
                << tr8("保存成功会弹出“已保存”，这是正常提示，不是错误。")
        ));

        items->addWidget(faqCard(
            tr8("保存失败：无法写入配置文件"),
            tr8("弹窗可能写为：无法写入 config/secrets.json；无法写入 config/settings.json；接口密钥已保存，但无法写入语音识别服务选择。"),
            QStringList()
                << tr8("把软件放到桌面、文档或普通磁盘目录，不要放在 Program Files 这类需要管理员权限的位置。")
                << tr8("确认 config 文件夹和里面的 json 文件没有只读属性。")
                << tr8("关闭多个同时运行的软件实例，只保留一个窗口后重新保存。")
        ));

        items->addWidget(faqCard(
            tr8("自定义功能：名称、快捷键或输入方式错误"),
            tr8("弹窗可能写为：名称不能为空；快捷键不能为空；快捷键无效；快捷键冲突；自定义功能至少需要启用选中文字或语音输入中的一种。"),
            QStringList()
                << tr8("每个自定义功能都要填写名称，并设置一个有效快捷键。")
                << tr8("快捷键不能和听写、翻译、问答、设置或其它自定义功能重复。")
                << tr8("每个自定义功能的输入方式、模型、展现方式和显示时间都是独立设置的。")
        ));

        items->addWidget(faqCard(
            tr8("历史记录：无法播放 / 删除失败 / 无法更新收藏状态"),
            tr8("历史记录操作依赖本地 records 目录。录音文件被删除、正在被播放器占用、目录无权限或历史 json 无法写入时，会弹出这些提示。"),
            QStringList()
                << tr8("无法播放时，确认这条记录右侧对应的录音文件仍然存在。")
                << tr8("删除失败时，先停止播放并关闭占用录音文件的程序。")
                << tr8("无法收藏时，确认历史记录保存目录可写，并且没有被同步盘或安全软件锁定。")
        ));

        items->addWidget(faqCard(
            tr8("历史记录：备份、导入或导出失败"),
            tr8("弹窗可能写为：备份失败、导入失败、导出失败、没有选中记录、没有可导出记录或没有录音可导出。通常是没有先选择记录、目录选错、目标目录不可写、录音文件已经被删除，或安全软件/同步盘锁定了文件。"),
            QStringList()
                << tr8("导出前先点“选择记录”勾选单条或多条记录；需要导出当前筛选结果时，点“全选当前”。")
                << tr8("备份会自动保存到当前历史记录保存位置里的“备份文件”，不要手动把备份目录再复制进自己里面。")
                << tr8("导入时选择单独的备份目录；如果选择当前软件目录或 records 的上级目录，软件会拒绝导入，避免把目录复制到自己里面。")
                << tr8("导出录音时只会导出当前筛选结果里仍然存在的录音文件；如果录音被删掉，只能导出文本或详细记录。")
                << tr8("“全部导出”会同时导出文本记录、录音文件和详细 JSON，但仍然只处理你选中的记录。")
                << tr8("如果写入失败，把导出位置换到桌面或文档目录，并暂时避开同步盘、压缩包内部目录和只读目录。")
        ));

        items->addWidget(faqCard(
            tr8("浮动条已关闭"),
            tr8("从托盘或快捷键触发浮动条时，如果常用设置里关闭了浮动条，会弹出“请在设置的常用设置页勾选启用浮动条”。"),
            QStringList()
                << tr8("打开“设置 -> 常用设置”，勾选“启用浮动条”。")
                << tr8("浮动条只在语音输入、识别和处理时临时显示，结束后会按设置的显示时间自动关闭。")
        ));

        items->addWidget(faqCard(
            tr8("删除历史记录 / 删除自定义功能"),
            tr8("这类弹窗是确认提示，不是错误。它会在真正删除本地记录、录音文件或自定义功能前询问一次。"),
            QStringList()
                << tr8("确定删除后，本地记录和对应录音可能无法从软件里恢复。")
                << tr8("不确定时点取消，再先复制或收藏需要保留的内容。")
        ));

        items->addWidget(faqCard(
            tr8("这里没有列出的弹窗"),
            tr8("如果测试人员遇到这里没有出现的弹窗，说明需要继续补充常见问题或修复新的异常路径。"),
            QStringList()
                << tr8("先完整记录弹窗标题和正文。")
                << tr8("再记录当时使用的功能、模型、语音识别服务、是否开启 TUN 或代理。")
                << tr8("把这些信息发给开发者，方便复现和补充说明。")
        ));

        addRecentWorkflowFaqItems(items);

        m_faqLoadMoreButton = new QPushButton;
        m_faqLoadMoreButton->setMinimumHeight(42);
        m_faqLoadMoreButton->setStyleSheet(
            compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827"))
        );
        connect(m_faqLoadMoreButton, &QPushButton::clicked, this, [this]() {
            m_faqRenderLimit += 8;
            applyFaqSearch();
        });
        items->addWidget(m_faqLoadMoreButton);

        m_faqEmptyLabel = new QLabel(tr8("没有找到匹配的常见问题。"));
        m_faqEmptyLabel->setWordWrap(true);
        m_faqEmptyLabel->setAlignment(Qt::AlignCenter);
        m_faqEmptyLabel->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  background: #f2f4f7;"
            "  color: #667085;"
            "  border-radius: 8px;"
            "  padding: 16px;"
            "}"
        ));
        m_faqEmptyLabel->setVisible(false);
        items->addWidget(m_faqEmptyLabel);

        items->addStretch();
        applyFaqSearch();
        scroll->setWidget(holder);
        layout->addWidget(scroll, 1);
}

void FaqPanel::addLatestFeatureFaqItems(QVBoxLayout *items)
    {
        items->addWidget(faqCard(
            tr8("功能流程：版本或发布内容不兼容"),
            tr8("流程由更高版本创建、发布哈希不一致，或保存文件中的流程结构已经损坏时，软件会停止使用该发布版并保留经典功能作为安全兜底。"),
            QStringList()
                << tr8("先备份 config/settings.json，再升级到创建该流程的软件版本。")
                << tr8("当前版本可读取但发布校验失败时，在流程编辑器确认修复并重新发布；不要直接手改发布哈希。"),
            QStringLiteral("function-flow-schema")
        ));

        items->addWidget(faqCard(
            tr8("功能流程：草稿保存冲突"),
            tr8("同时打开多个编辑窗口或多个程序实例时，较旧草稿不会覆盖较新版本，编辑器会报告版本冲突。"),
            QStringList()
                << tr8("保留需要的内容后重新加载当前功能，再继续编辑。")
                << tr8("关闭重复运行的软件实例；草稿和已发布版本彼此独立，未发布草稿不会改变快捷键运行。"),
            QStringLiteral("function-flow-draft")
        ));

        items->addWidget(faqCard(
            tr8("功能流程：无法发布节点或连线"),
            tr8("环路、悬空连线、端口方向错误、必需输入缺失、触发快捷键冲突或结果动作不完整都会阻止发布。"),
            QStringList()
                << tr8("点击编辑器中的错误定位，逐项检查红色节点和连线。")
                << tr8("确保只有一个输出节点、至少一个结果动作，并让每个模型输入先经过输入节点。"),
            QStringLiteral("function-flow-publish")
        ));

        items->addWidget(faqCard(
            tr8("功能流程：没有取得选区、语音或截图"),
            tr8("当前触发入口无法满足必需输入，或读取选区、录音识别、截图 OCR 被取消或失败时，流程会停止且不会再执行一遍经典功能。"),
            QStringList()
                << tr8("确认使用的是主快捷键、独立截图快捷键或截图悬浮入口中正确的一种。")
                << tr8("检查目标窗口选区、麦克风和 OCR 测试；可选输入允许为空，必需输入必须取得内容。"),
            QStringLiteral("function-flow-input")
        ));

        items->addWidget(faqCard(
            tr8("功能流程：模型、提示词或服务不可用"),
            tr8("发布后删除提示词、修改模型配置、缺少接口密钥，或语音/OCR 服务配置失效时，流程会在运行前报告配置错误，不会经典兜底。"),
            QStringList()
                << tr8("打开“设置 -> 接口”和“提示词库”，确认流程引用的稳定 ID 仍存在且密钥完整。")
                << tr8("修复配置后重新触发；如节点引用已经改变，请重新选择并发布流程。"),
            QStringLiteral("function-flow-model")
        ));

        items->addWidget(faqCard(
            tr8("功能流程：结果无法写回目标窗口"),
            tr8("原目标窗口关闭、切换到软件自身窗口、替换时原选区已经消失，或系统拒绝输入注入时，为避免误写到其它窗口，自动写入会停止。"),
            QStringList()
                << tr8("保留结果窗口并手动复制，不要依赖后来切换到的新前台窗口。")
                << tr8("替换写入前保持原选区；若选区已取消，重新选择文字后再运行。"),
            QStringLiteral("function-flow-output")
        ));

        items->addWidget(faqCard(
            tr8("功能流程：历史保存或编辑回写失败"),
            tr8("历史目录不可写、记录文件被移动，或结果窗口关闭时记录路径已不再属于本次运行冻结的历史目录，会拒绝新增或回写。"),
            QStringList()
                << tr8("检查历史目录权限和剩余空间，避免在流程运行中移动该条详情文件。")
                << tr8("结果窗口的人工编辑只更新同一条流程历史，不会创建第二条记录。"),
            QStringLiteral("function-flow-history")
        ));

        items->addWidget(faqCard(
            tr8("功能流程：运行取消、忙碌或异常终止"),
            tr8("同一切换式流程再次触发会取消当前运行；其它流程在录音、截图、模型或已有流程占用时会被拒绝，以保证一次只运行一个节点和一个流程。"),
            QStringList()
                << tr8("等待当前任务结束，或再次触发同一流程进行取消。")
                << tr8("若持续异常，查看 logs/function-flow.jsonl；日志只含功能、节点、耗时和稳定错误码，不含正文、图片或密钥。"),
            QStringLiteral("function-flow-runtime")
        ));

        items->addWidget(faqCard(
            tr8("结果小框：重新生成、换模型或继续追问失败"),
            tr8("这些按钮会重新调用当前功能对应的大模型。失败通常和模型密钥、模型名称、网络代理、选中文本过长或当前服务不可用有关。"),
            QStringList()
                << tr8("先到“设置 -> 接口”确认对应大模型密钥已经保存，再到左侧“功能自定义”确认当前功能选择的模型可以使用。")
                << tr8("如果只在换模型后失败，说明新模型所属服务没有填密钥、模型名不可用，或当前账号没有该模型权限。")
                << tr8("继续追问时尽量补一句明确要求；如果选中文本很长，先缩短文本再重试。")
        ));

        items->addWidget(faqCard(
            tr8("结果小框：流式显示中断或一直没有文字"),
            tr8("流式显示会保持一条长连接，边接收边显示。TUN、透明代理、系统代理、防火墙、安全软件或服务端排队都可能导致中断、停顿或超时。"),
            QStringList()
                << tr8("先在“测试工具”里运行网络诊断和接口自检，确认当前网络和模型服务可用。")
                << tr8("在“设置 -> 常用设置”切换“使用系统代理”，分别测试直连和代理；开着 TUN 时请给国内语音接口和所用大模型域名设置正确分流。")
                << tr8("如果频繁中断，换一个响应更快的模型，或减少选中文本长度。")
        ));

        items->addWidget(faqCard(
            tr8("主页功能卡片排序没有保存"),
            tr8("拖动主页功能卡片调整顺序后，会写入 config/settings.json。保存失败通常是配置目录只读、文件被安全软件锁定，或同时打开了多个程序实例。"),
            QStringList()
                << tr8("确认软件放在普通可写目录，不要放在需要管理员权限的系统目录。")
                << tr8("关闭重复启动的 vocekit.exe，只保留一个实例后再调整顺序。")
                << tr8("如果调整后仍恢复原顺序，删除损坏的 config/settings.json 后重新打开软件，让它重新生成默认配置。")
        ));

        items->addWidget(faqCard(
            tr8("录音倒计时或提示音没有出现"),
            tr8("倒计时和提示音是可选设置，只在真正需要语音输入的功能触发前出现。只处理选中文字的功能不会进入录音准备阶段。"),
            QStringList()
                << tr8("打开“设置 -> 常用设置”，确认“启用录音倒计时”或“启用录音提示音”已经勾选。")
                << tr8("打开左侧“功能自定义”，确认当前功能启用了“使用语音输入”，并检查该功能里的倒计时秒数和提示音开关。")
                << tr8("如果提示音没有声音，检查 Windows 系统提示音、默认播放设备和静音状态。")
        ));

        items->addWidget(faqCard(
            tr8("浮动条波形没有变化"),
            tr8("波形只在录音过程中显示，用来提示麦克风是否有输入。没有变化通常是默认麦克风错误、录音权限被拦截、输入音量过低或设备被其它软件占用。"),
            QStringList()
                << tr8("先到“测试工具”运行麦克风测试，确认能检测到有效声音。")
                << tr8("在 Windows 声音设置里确认默认输入设备正确，并观察系统输入音量条是否变化。")
                << tr8("关闭可能独占麦克风的软件，然后重新触发听写或需要语音输入的自定义功能。")
        ));

        items->addWidget(faqCard(
            tr8("长录音分段识别失败"),
            tr8("长录音会把声音分成多段并在后台依次识别。某一段网络失败时会自动重试一次；如果全部分段都失败，软件不会继续调用大模型，并会保存分段录音和错误信息。"),
            QStringList()
                << tr8("打开历史记录详情，查看每一段的错误、识别耗时和录音文件，确认是单段失败还是全部失败。")
                << tr8("先运行“测试工具”里的麦克风测试、接口自检和网络诊断，再重新录音。")
                << tr8("代理或 TUN 环境下，确认当前语音服务域名使用了正确的直连或代理规则。")
                << tr8("如果录音本身正常但接口不稳定，可以缩短每段时长后重试。")
        ));

        items->addWidget(faqCard(
            tr8("按住说话没有开始或松开后没有结束"),
            tr8("按住说话依赖 Windows 全局键盘钩子识别快捷键按下和松开。快捷键冲突、安全软件拦截、钩子安装失败或先松开修饰键都可能影响触发。"),
            QStringList()
                << tr8("打开“功能自定义”，确认该功能已选择“按住说话”，并使用包含普通按键的组合快捷键。")
                << tr8("到“设置 -> 快捷键”检查冲突，换一个未被其它软件占用的快捷键。")
                << tr8("如果安全软件拦截全局键盘钩子，确认程序路径后允许 vocekit.exe；不希望允许时改回“切换开始和结束”。")
        ));
    }

void FaqPanel::addRecentWorkflowFaqItems(QVBoxLayout *items)
    {
        items->addWidget(faqCard(
            tr8("词库 AI 生成失败 / 保存词条失败"),
            tr8("弹窗可能写为：词库 AI 生成失败、AI 生成失败、保存词条失败、词条无修正效果、缺少 DeepSeek 密钥或模型没有生成可产生修正效果的词条。通常是词库加入方式选择了 AI，但接口密钥、网络、提示词格式、模型返回内容或词条本身不符合要求。"),
            QStringList()
                << tr8("先打开“设置 -> 常用设置”，把“快捷键加入方式”改成“不使用 AI”测试手动新增是否正常。")
                << tr8("如果要使用 AI，打开“设置 -> 接口”填写 DeepSeek API Key，并在“提示词库”检查“词库提示词”是否要求只输出 JSON。")
                << tr8("原词/错词和标准写法完全一样、且别名为空时，词条不会修正任何内容。请把常见错写放到“原词/错词”或“别名”，把正确写法放到“标准写法”。")
                << tr8("如果 AI 失败后弹出手动编辑框，可以先手动保存词条，不影响已有词库和输出修正。")
        ));

        items->addWidget(faqCard(
            tr8("词库导入 / 导出失败"),
            tr8("弹窗可能写为：词库导入失败、没有导入、没有可导出词条、词库导出失败。通常是文件格式不对、词条重复、词条没有修正效果、当前筛选结果为空，或目标目录不可写。"),
            QStringList()
                << tr8("导入支持 JSON、CSV 和 TXT。JSON 可以是 entries 数组；CSV 建议包含 source、target、aliases、scopeId、matchMode、note、enabled 表头；TXT 可以写成“错词 -> 标准写法”。")
                << tr8("原词/错词和标准写法完全相同、且别名为空的词条会被跳过，因为它不会产生修正效果。")
                << tr8("导出只导出当前词库标签和搜索条件下的词条；如果提示没有可导出词条，先切到“全部”并清空搜索框。")
                << tr8("如果写入失败，把导出位置换到桌面或文档目录，并避开只读目录、同步盘冲突和压缩包内部目录。")
        ));

        items->addWidget(faqCard(
            tr8("百度示例代码解析失败"),
            tr8("弹窗可能写为：解析失败，没有在示例代码中找到 client_id 和 client_secret。通常是没有粘贴百度智能云“获取 AccessToken”的完整示例代码，或粘贴内容不包含这两个参数。"),
            QStringList()
                << tr8("在百度智能云示例代码中心选择“获取 AccessToken”，复制包含 oauth/2.0/token 地址的完整代码。")
                << tr8("确认代码里能看到 client_id=... 和 client_secret=...。client_id 会填入百度 API Key，client_secret 会填入百度 Secret Key。")
                << tr8("解析成功后还需要点击“保存接口配置”，否则只是填到界面里，关闭软件后不会生效。")
        ));

        items->addWidget(faqCard(
            tr8("开机自启动没有打开主界面"),
            tr8("这是当前设计，不是错误。开启开机自启动后，Windows 登录时会自动启动 vocekit，但只进入托盘后台，不会弹出主界面打扰用户。"),
            QStringList()
                << tr8("需要打开主界面时，双击托盘图标、右键托盘选择“打开主界面”，或使用打开主界面的快捷键。")
                << tr8("普通双击运行 vocekit.exe 仍会打开主界面；只有带 --autostart 参数的开机启动不会打开。")
                << tr8("如果想确认后台是否运行，查看系统托盘或任务管理器里的 vocekit.exe。")
        ));

        items->addWidget(faqCard(
            tr8("强力选中触发安全软件提醒"),
            tr8("强力选中用于兼容不暴露选中文字的网页、PDF、聊天窗口或特殊控件。它可能临时模拟复制或调用系统文本接口，因此部分安全软件会提示风险。"),
            QStringList()
                << tr8("如果只是普通网页或文本框翻译，建议关闭“强力选中”，使用默认读取方式。")
                << tr8("确实需要兼容特殊软件时，再在“设置 -> 常用设置”开启“强力选中”。")
                << tr8("如果安全软件弹窗，请确认执行程序路径是你自己的 vocekit.exe，再选择允许或把程序加入信任。")
        ));

        items->addWidget(faqCard(
            tr8("程序突然退出 / 后台也没有运行痕迹"),
            tr8("这通常是崩溃而不是正常关闭。旧版本曾经在全局快捷键原生事件里同步执行语音识别和模型处理，容易触发 Qt 事件重入；现在已经改成异步派发并增加处理锁。"),
            QStringList()
                << tr8("先确认当前运行的是最新编译版本。")
                << tr8("如果仍然退出，到 Windows 的 CrashDumps 目录查找 vocekit.exe 的 .dmp 文件。")
                << tr8("把崩溃发生前的操作、当前功能、选中文字内容类型、是否开着代理或强力选中一起记录下来。")
        ));

        items->addWidget(faqCard(
            tr8("接口页只显示当前语音服务的密钥"),
            tr8("这是为了避免接口页太长、填错服务密钥。选择百度语音识别时只显示百度需要的字段；选择讯飞语音听写时只显示讯飞需要的字段。"),
            QStringList()
                << tr8("切换语音服务不会删除另一个服务已经填写过的密钥。")
                << tr8("需要修改另一个服务密钥时，先在“当前语音识别服务”里切换过去，再填写并保存。")
                << tr8("如果切换后仍提示缺少旧服务密钥，先关闭设置页重新打开，确认选择已经保存。")
        ));
    }

QWidget *FaqPanel::faqCard(
    const QString &title,
    const QString &cause,
    const QStringList &solutions,
    const QString &explicitFaqId)
    {
        auto *frame = new QFrame;
        frame->setObjectName(QStringLiteral("card"));
        const QString faqId = explicitFaqId.trimmed().isEmpty()
            ? attentionFaqIdForTitle(title)
            : explicitFaqId.trimmed();
        const QString searchText = (QStringList() << faqId << (tr8("问题") + faqId) << title << cause << solutions).join(QStringLiteral("\n"));
        const QString category = faqCategoryForText(title);
        frame->setProperty("faqSearchText", searchText);
        frame->setProperty("faqCategory", category);
        frame->setProperty("faqId", faqId);
        frame->setProperty("faqTitle", title);
        frame->setProperty("faqCause", cause);
        frame->setProperty("faqSolutions", solutions);
        frame->setProperty("faqMaterialized", false);
        return frame;
    }

void FaqPanel::ensureFaqCardMaterialized(QWidget *card)
    {
        if (!card || card->property("faqMaterialized").toBool()) {
            return;
        }

        const QString faqId = card->property("faqId").toString();
        const QString title = card->property("faqTitle").toString();
        const QString cause = card->property("faqCause").toString();
        const QStringList solutions = card->property("faqSolutions").toStringList();
        const QString category = card->property("faqCategory").toString();
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(12);

        auto *top = new QHBoxLayout;
        top->setContentsMargins(0, 0, 0, 0);
        top->setSpacing(12);

        auto *numberLabel = new QLabel(faqId.isEmpty() ? QStringLiteral("-") : faqId);
        numberLabel->setObjectName(QStringLiteral("faqNumber"));
        numberLabel->setAlignment(Qt::AlignCenter);
        numberLabel->setMinimumSize(42, 28);
        numberLabel->setMaximumHeight(28);
        top->addWidget(numberLabel, 0, Qt::AlignTop);

        auto *titleLabel = new QLabel(title);
        titleLabel->setFont(appFont(12, QFont::DemiBold));
        titleLabel->setWordWrap(true);
        titleLabel->setStyleSheet(QStringLiteral("color: #111827;"));
        top->addWidget(titleLabel, 1);
        const QString diagnosticKeyword = faqDiagnosticKeyword(category, title);
        if (!diagnosticKeyword.isEmpty()) {
            auto *testButton = new QPushButton(tr8("去测试"));
            testButton->setMinimumSize(82, 32);
            testButton->setStyleSheet(compactButtonStyle(QStringLiteral("#ffffff"), QStringLiteral("#111827")));
            connect(testButton, &QPushButton::clicked, this, [this, category, title]() {
                openDiagnosticForFaq(category, title);
            });
            top->addWidget(testButton, 0, Qt::AlignTop);
        }
        layout->addLayout(top);

        auto *causeLabel = new QLabel(cause);
        causeLabel->setObjectName(QStringLiteral("faqBody"));
        causeLabel->setWordWrap(true);
        causeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto *causeBlock = new QFrame;
        causeBlock->setObjectName(QStringLiteral("faqBlock"));
        auto *causeLayout = new QVBoxLayout(causeBlock);
        causeLayout->setContentsMargins(12, 10, 12, 10);
        causeLayout->setSpacing(6);
        auto *causeTitle = new QLabel(tr8("原因"));
        causeTitle->setObjectName(QStringLiteral("faqLabel"));
        causeLayout->addWidget(causeTitle);
        causeLayout->addWidget(causeLabel);

        QString solutionText;
        for (int i = 0; i < solutions.size(); ++i) {
            if (!solutionText.isEmpty()) {
                solutionText += QStringLiteral("\n");
            }
            solutionText += QStringLiteral("%1. %2").arg(i + 1).arg(solutions.at(i));
        }

        auto *solutionLabel = new QLabel(solutionText);
        solutionLabel->setObjectName(QStringLiteral("faqBody"));
        solutionLabel->setWordWrap(true);
        solutionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto *solutionBlock = new QFrame;
        solutionBlock->setObjectName(QStringLiteral("faqBlock"));
        auto *solutionLayout = new QVBoxLayout(solutionBlock);
        solutionLayout->setContentsMargins(12, 10, 12, 10);
        solutionLayout->setSpacing(6);
        auto *solutionTitle = new QLabel(tr8("处理办法"));
        solutionTitle->setObjectName(QStringLiteral("faqLabel"));
        solutionLayout->addWidget(solutionTitle);
        solutionLayout->addWidget(solutionLabel);

        layout->addWidget(causeBlock);
        layout->addWidget(solutionBlock);
        card->setProperty("faqMaterialized", true);
    }
