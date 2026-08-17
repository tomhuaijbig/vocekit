#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>

using namespace winrt;
using namespace Windows::Data::Json;
using namespace Windows::Globalization;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Media::Ocr;
using namespace Windows::Storage;

namespace {

void writeError(
    const hstring &requestId,
    const wchar_t *code,
    const hstring &message,
    long long elapsedMs)
{
    JsonObject response;
    response.Insert(L"requestId", JsonValue::CreateStringValue(requestId));
    response.Insert(L"ok", JsonValue::CreateBooleanValue(false));
    response.Insert(L"errorCode", JsonValue::CreateStringValue(code));
    response.Insert(L"errorMessage", JsonValue::CreateStringValue(message));
    response.Insert(L"elapsedMs", JsonValue::CreateNumberValue(double(elapsedMs)));
    std::cout << to_string(response.Stringify()) << std::endl;
}

OcrEngine createEngine(const JsonObject &request)
{
    if (request.HasKey(L"languages")) {
        const JsonArray languages = request.GetNamedArray(L"languages");
        for (uint32_t index = 0; index < languages.Size(); ++index) {
            const hstring tag = languages.GetStringAt(index);
            try {
                const Language language(tag);
                if (OcrEngine::IsLanguageSupported(language)) {
                    const OcrEngine engine = OcrEngine::TryCreateFromLanguage(language);
                    if (engine) {
                        return engine;
                    }
                }
            } catch (...) {
                // 无效语言标签只影响当前候选，继续尝试下一项。
            }
        }
        return nullptr;
    }

    for (const wchar_t *tag : { L"zh-Hans", L"en" }) {
        const Language language(tag);
        if (OcrEngine::IsLanguageSupported(language)) {
            const OcrEngine engine = OcrEngine::TryCreateFromLanguage(language);
            if (engine) {
                return engine;
            }
        }
    }
    return nullptr;
}

}

int main()
{
    const auto startedAt = std::chrono::steady_clock::now();
    hstring requestId;

    try {
        init_apartment(apartment_type::multi_threaded);

        std::string input;
        if (!std::getline(std::cin, input) || input.empty()) {
            writeError(requestId, L"INVALID_REQUEST", L"没有收到 OCR 请求。", 0);
            return 2;
        }

        const JsonObject request = JsonObject::Parse(to_hstring(input));
        requestId = request.GetNamedString(L"requestId", L"");
        const hstring imagePath = request.GetNamedString(L"imagePath", L"");
        if (imagePath.empty()) {
            writeError(requestId, L"INVALID_REQUEST", L"OCR 请求缺少图片路径。", 0);
            return 2;
        }

        const OcrEngine engine = createEngine(request);
        if (!engine) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - startedAt
            ).count();
            writeError(
                requestId,
                L"LANGUAGE_NOT_INSTALLED",
                L"Windows 未安装所选 OCR 语言包。",
                elapsed
            );
            return 3;
        }

        const StorageFile file = StorageFile::GetFileFromPathAsync(imagePath).get();
        const auto stream = file.OpenAsync(FileAccessMode::Read).get();
        const BitmapDecoder decoder = BitmapDecoder::CreateAsync(stream).get();
        const SoftwareBitmap bitmap = decoder.GetSoftwareBitmapAsync(
            BitmapPixelFormat::Bgra8,
            BitmapAlphaMode::Premultiplied
        ).get();
        const OcrResult recognized = engine.RecognizeAsync(bitmap).get();

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt
        ).count();

        JsonObject response;
        response.Insert(L"requestId", JsonValue::CreateStringValue(requestId));
        response.Insert(L"ok", JsonValue::CreateBooleanValue(true));
        response.Insert(L"text", JsonValue::CreateStringValue(recognized.Text()));
        response.Insert(
            L"imageWidth",
            JsonValue::CreateNumberValue(double(bitmap.PixelWidth()))
        );
        response.Insert(
            L"imageHeight",
            JsonValue::CreateNumberValue(double(bitmap.PixelHeight()))
        );
        JsonArray blocks;
        for (const OcrLine &line : recognized.Lines()) {
            bool hasBounds = false;
            float left = 0.0f;
            float top = 0.0f;
            float right = 0.0f;
            float bottom = 0.0f;
            for (const OcrWord &word : line.Words()) {
                const auto bounds = word.BoundingRect();
                const float wordRight = bounds.X + bounds.Width;
                const float wordBottom = bounds.Y + bounds.Height;
                if (!hasBounds) {
                    left = bounds.X;
                    top = bounds.Y;
                    right = wordRight;
                    bottom = wordBottom;
                    hasBounds = true;
                } else {
                    left = std::min(left, bounds.X);
                    top = std::min(top, bounds.Y);
                    right = std::max(right, wordRight);
                    bottom = std::max(bottom, wordBottom);
                }
            }
            if (!hasBounds || line.Text().empty()) {
                continue;
            }
            JsonObject block;
            block.Insert(L"text", JsonValue::CreateStringValue(line.Text()));
            block.Insert(L"confidence", JsonValue::CreateNumberValue(-1.0));
            JsonArray points;
            for (const auto &point : {
                    std::pair<float, float>(left, top),
                    std::pair<float, float>(right, top),
                    std::pair<float, float>(right, bottom),
                    std::pair<float, float>(left, bottom) }) {
                JsonArray coordinates;
                coordinates.Append(JsonValue::CreateNumberValue(double(point.first)));
                coordinates.Append(JsonValue::CreateNumberValue(double(point.second)));
                points.Append(coordinates);
            }
            block.Insert(L"points", points);
            blocks.Append(block);
        }
        response.Insert(L"blocks", blocks);
        response.Insert(L"elapsedMs", JsonValue::CreateNumberValue(double(elapsed)));
        std::cout << to_string(response.Stringify()) << std::endl;
        return 0;
    } catch (const hresult_error &error) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt
        ).count();
        writeError(requestId, L"WINDOWS_OCR_FAILED", error.message(), elapsed);
        return 4;
    } catch (const std::exception &error) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt
        ).count();
        writeError(requestId, L"WINDOWS_OCR_FAILED", to_hstring(error.what()), elapsed);
        return 5;
    } catch (...) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startedAt
        ).count();
        writeError(requestId, L"WINDOWS_OCR_FAILED", L"Windows OCR 发生未知错误。", elapsed);
        return 6;
    }
}
