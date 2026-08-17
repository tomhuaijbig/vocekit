#include "built_in_provider_factory.h"

#include "../config/app_settings_defaults.h"
#include "../config/secret_config.h"
#include "../providers/baidu_speech_provider.h"
#include "../providers/claude_model_provider.h"
#include "../providers/custom_speech_provider.h"
#include "../providers/deepseek_model_provider.h"
#include "../providers/openai_compatible_model_provider.h"
#include "../providers/windows_speech_provider.h"
#include "../providers/xfyun_speech_provider.h"

QSharedPointer<ISpeechProvider> createBuiltInSpeechProvider(
    const QString &providerId,
    bool useSystemProxy)
{
    const QString normalized = normalizeSpeechProvider(providerId);
    if (normalized == speechProviderWindowsLocal()) {
        return createWindowsSpeechProvider();
    }
    if (normalized == speechProviderBaidu()) {
        return createBaiduSpeechProvider(useSystemProxy);
    }
    if (normalized == speechProviderCustom()) {
        return createCustomSpeechProvider(useSystemProxy);
    }
    return createXfyunSpeechProvider(useSystemProxy);
}

QSharedPointer<IModelProvider> createBuiltInModelProvider(
    const QString &providerId,
    bool useSystemProxy)
{
    const QString normalized = providerId.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("deepseek")) {
        return createDeepSeekModelProvider(useSystemProxy);
    }
    if (normalized == QStringLiteral("openai")
        || normalized == QStringLiteral("custom")
        || normalized.startsWith(QStringLiteral("custom:"))) {
        return createOpenAiCompatibleModelProvider(
            providerId,
            useSystemProxy
        );
    }
    if (normalized == QStringLiteral("claude")) {
        return createClaudeModelProvider(useSystemProxy);
    }
    return QSharedPointer<IModelProvider>();
}

void registerBuiltInProviders(
    ProviderRegistry *registry,
    bool useSystemProxy)
{
    if (!registry) {
        return;
    }
    registry->addSpeechProvider(
        createBuiltInSpeechProvider(
            speechProviderBaidu(),
            useSystemProxy
        )
    );
    registry->addSpeechProvider(
        createBuiltInSpeechProvider(
            speechProviderXfyun(),
            useSystemProxy
        )
    );
    registry->addSpeechProvider(
        createBuiltInSpeechProvider(
            speechProviderCustom(),
            useSystemProxy
        )
    );
    registry->addSpeechProvider(
        createBuiltInSpeechProvider(
            speechProviderWindowsLocal(),
            useSystemProxy
        )
    );
    registry->addModelProvider(
        createBuiltInModelProvider(
            QStringLiteral("deepseek"),
            useSystemProxy
        )
    );
    registry->addModelProvider(
        createBuiltInModelProvider(
            QStringLiteral("openai"),
            useSystemProxy
        )
    );
    registry->addModelProvider(
        createBuiltInModelProvider(
            QStringLiteral("claude"),
            useSystemProxy
        )
    );
    registry->addModelProvider(
        createBuiltInModelProvider(
            QStringLiteral("custom"),
            useSystemProxy
        )
    );
    const QVector<CustomModelProfile> profiles = loadSecrets().effectiveCustomModels();
    for (const CustomModelProfile &profile : profiles) {
        const QString id = normalizeCustomModelProfileId(profile.id);
        if (!id.isEmpty() && profile.hasEndpoint()) {
            registry->addModelProvider(
                createBuiltInModelProvider(
                    QStringLiteral("custom:") + id,
                    useSystemProxy
                )
            );
        }
    }
}
