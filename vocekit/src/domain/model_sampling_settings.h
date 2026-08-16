#ifndef VOCEKIT_MODEL_SAMPLING_SETTINGS_H
#define VOCEKIT_MODEL_SAMPLING_SETTINGS_H

#include <QtMath>

struct ModelSamplingSettings
{
    bool temperatureEnabled = false;
    double temperature = 0.2;
    bool topPEnabled = false;
    double topP = 1.0;
};

inline bool isValidModelTemperature(double value)
{
    return qIsFinite(value) && value >= 0.0 && value <= 2.0;
}

inline bool isValidModelTopP(double value)
{
    return qIsFinite(value) && value >= 0.0 && value <= 1.0;
}

inline ModelSamplingSettings normalizeModelSamplingSettings(
    const ModelSamplingSettings &settings)
{
    ModelSamplingSettings normalized = settings;
    if (!isValidModelTemperature(normalized.temperature)) {
        normalized.temperatureEnabled = false;
        normalized.temperature = 0.2;
    }
    if (!isValidModelTopP(normalized.topP)) {
        normalized.topPEnabled = false;
        normalized.topP = 1.0;
    }
    return normalized;
}

#endif // VOCEKIT_MODEL_SAMPLING_SETTINGS_H
