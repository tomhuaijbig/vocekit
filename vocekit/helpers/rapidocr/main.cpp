#include <chrono>
#include <cstdio>
#include <filesystem>
#include <io.h>
#include <iostream>
#include <string>
#include <vector>

#include <windows.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/base.h>

#include "OcrLite.h"
#include "opencv2/imgcodecs.hpp"

using namespace winrt;
using namespace Windows::Data::Json;

namespace {

std::filesystem::path executableDirectory()
{
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(
        nullptr,
        buffer.data(),
        static_cast<DWORD>(buffer.size())
    );
    if (length == 0 || length >= buffer.size()) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}

std::filesystem::path modelsDirectory(int argc, wchar_t **argv)
{
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::wstring(argv[index]) == L"--models") {
            return std::filesystem::path(argv[index + 1]);
        }
    }
    return executableDirectory() / L"models";
}

std::string utf8Path(const std::filesystem::path &path)
{
    return to_string(path.wstring());
}

long long elapsedMilliseconds(
    const std::chrono::steady_clock::time_point &startedAt)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startedAt
    ).count();
}

void writeResponse(const JsonObject &response)
{
    std::cout << to_string(response.Stringify()) << std::endl;
}

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
    writeResponse(response);
}

bool readBinaryFile(const std::filesystem::path &path, std::vector<unsigned char> *data)
{
    FILE *file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) {
        return false;
    }

    _fseeki64(file, 0, SEEK_END);
    const __int64 size = _ftelli64(file);
    _fseeki64(file, 0, SEEK_SET);
    if (size <= 0 || size > 25LL * 1024LL * 1024LL) {
        fclose(file);
        return false;
    }

    data->resize(static_cast<size_t>(size));
    const size_t readSize = fread(data->data(), 1, data->size(), file);
    fclose(file);
    return readSize == data->size();
}

class RedirectStdoutToStderr
{
public:
    RedirectStdoutToStderr()
    {
        fflush(stdout);
        m_savedDescriptor = _dup(_fileno(stdout));
        if (m_savedDescriptor >= 0) {
            _dup2(_fileno(stderr), _fileno(stdout));
        }
    }

    ~RedirectStdoutToStderr()
    {
        if (m_savedDescriptor < 0) {
            return;
        }
        fflush(stdout);
        _dup2(m_savedDescriptor, _fileno(stdout));
        _close(m_savedDescriptor);
    }

private:
    int m_savedDescriptor = -1;
};

}

int wmain(int argc, wchar_t **argv)
{
    const auto startedAt = std::chrono::steady_clock::now();
    hstring requestId;

    try {
        init_apartment(apartment_type::multi_threaded);

        std::string input;
        if (!std::getline(std::cin, input) || input.empty()) {
            writeError(
                requestId,
                L"INVALID_REQUEST",
                L"没有收到 OCR 请求。",
                elapsedMilliseconds(startedAt)
            );
            return 2;
        }

        const JsonObject request = JsonObject::Parse(to_hstring(input));
        requestId = request.GetNamedString(L"requestId", L"");
        const hstring imagePathValue = request.GetNamedString(L"imagePath", L"");
        if (imagePathValue.empty()) {
            writeError(
                requestId,
                L"INVALID_REQUEST",
                L"OCR 请求缺少图片路径。",
                elapsedMilliseconds(startedAt)
            );
            return 2;
        }

        const std::filesystem::path modelsPath = modelsDirectory(argc, argv);
        const std::filesystem::path detector =
            modelsPath / L"ch_PP-OCRv3_det_infer.onnx";
        const std::filesystem::path classifier =
            modelsPath / L"ch_ppocr_mobile_v2.0_cls_infer.onnx";
        const std::filesystem::path recognizer =
            modelsPath / L"ch_PP-OCRv3_rec_infer.onnx";
        const std::filesystem::path keys =
            modelsPath / L"ppocr_keys_v1.txt";

        for (const auto &model : { detector, classifier, recognizer, keys }) {
            if (!std::filesystem::is_regular_file(model)) {
                writeError(
                    requestId,
                    L"MODEL_MISSING",
                    L"RapidOCR 模型文件不完整，请重新部署 OCR 模型。",
                    elapsedMilliseconds(startedAt)
                );
                return 3;
            }
        }

        std::vector<unsigned char> imageBytes;
        const std::filesystem::path imagePath(imagePathValue.c_str());
        if (!readBinaryFile(imagePath, &imageBytes)) {
            writeError(
                requestId,
                L"IMAGE_DECODE_FAILED",
                L"无法读取图片，或图片超过 25 MB。",
                elapsedMilliseconds(startedAt)
            );
            return 4;
        }

        const cv::Mat image = cv::imdecode(imageBytes, cv::IMREAD_COLOR);
        if (image.empty()) {
            writeError(
                requestId,
                L"IMAGE_DECODE_FAILED",
                L"图片格式无法解码。",
                elapsedMilliseconds(startedAt)
            );
            return 4;
        }

        OcrLite ocr;
        ocr.setNumThread(4);
        ocr.initLogger(false, false, false);
        ocr.setGpuIndex(-1);

        try {
            const RedirectStdoutToStderr redirect;
            ocr.initModels(
                    utf8Path(detector),
                    utf8Path(classifier),
                    utf8Path(recognizer),
                    utf8Path(keys)
                );
        } catch (const std::exception &error) {
            writeError(
                requestId,
                L"MODEL_LOAD_FAILED",
                to_hstring(error.what()),
                elapsedMilliseconds(startedAt)
            );
            return 5;
        }

        OcrResult result;
        try {
            result = ocr.detect(
                image,
                50,
                1600,
                0.5f,
                0.3f,
                1.6f,
                true,
                true
            );
        } catch (const std::exception &error) {
            writeError(
                requestId,
                L"RECOGNITION_FAILED",
                to_hstring(error.what()),
                elapsedMilliseconds(startedAt)
            );
            return 6;
        }

        const hstring text = to_hstring(result.strRes);
        if (text.empty()) {
            writeError(
                requestId,
                L"EMPTY_TEXT",
                L"RapidOCR 没有识别到文字。",
                elapsedMilliseconds(startedAt)
            );
            return 7;
        }

        JsonObject response;
        response.Insert(L"requestId", JsonValue::CreateStringValue(requestId));
        response.Insert(L"ok", JsonValue::CreateBooleanValue(true));
        response.Insert(L"text", JsonValue::CreateStringValue(text));
        response.Insert(
            L"imageWidth",
            JsonValue::CreateNumberValue(double(image.cols))
        );
        response.Insert(
            L"imageHeight",
            JsonValue::CreateNumberValue(double(image.rows))
        );
        JsonArray blocks;
        for (const TextBlock &textBlock : result.textBlocks) {
            JsonObject block;
            block.Insert(
                L"text",
                JsonValue::CreateStringValue(to_hstring(textBlock.text))
            );
            block.Insert(
                L"confidence",
                JsonValue::CreateNumberValue(double(textBlock.boxScore))
            );
            JsonArray points;
            for (const cv::Point &point : textBlock.boxPoint) {
                JsonArray coordinates;
                coordinates.Append(JsonValue::CreateNumberValue(double(point.x)));
                coordinates.Append(JsonValue::CreateNumberValue(double(point.y)));
                points.Append(coordinates);
            }
            block.Insert(L"points", points);
            blocks.Append(block);
        }
        response.Insert(L"blocks", blocks);
        response.Insert(
            L"elapsedMs",
            JsonValue::CreateNumberValue(double(elapsedMilliseconds(startedAt)))
        );
        writeResponse(response);
        return 0;
    } catch (const hresult_error &error) {
        writeError(
            requestId,
            L"INVALID_REQUEST",
            error.message(),
            elapsedMilliseconds(startedAt)
        );
        return 8;
    } catch (const std::exception &error) {
        writeError(
            requestId,
            L"RECOGNITION_FAILED",
            to_hstring(error.what()),
            elapsedMilliseconds(startedAt)
        );
        return 9;
    } catch (...) {
        writeError(
            requestId,
            L"RECOGNITION_FAILED",
            L"RapidOCR 发生未知错误。",
            elapsedMilliseconds(startedAt)
        );
        return 10;
    }
}
