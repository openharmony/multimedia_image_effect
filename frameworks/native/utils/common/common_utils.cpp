/*
 * Copyright (C) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common_utils.h"

#include "effect_log.h"
#include "effect_buffer.h"
#include "image_source.h"
#include "uri.h"
#include "string_helper.h"
#include "memcpy_helper.h"
#include "colorspace_helper.h"

namespace OHOS {
namespace Media {
namespace Effect {
namespace {
    const std::string IMAGE_LENGTH = "ImageLength";
    const std::string IMAGE_WIDTH = "ImageWidth";
    const std::string DATE_TIME = "DateTime";
    const std::string PIXEL_X_DIMENSION = "PixelXDimension";
    const std::string PIXEL_Y_DIMENSION = "PixelYDimension";
    const int32_t TIME_MAX = 64;
}

using namespace OHOS::ColorManager;
using namespace OHOS::HDI::Display::Graphic::Common::V1_0;

const std::unordered_map<PixelFormat, IEffectFormat> CommonUtils::pixelFmtToEffectFmt_ = {
    { PixelFormat::RGBA_8888, IEffectFormat::RGBA8888 },
    { PixelFormat::NV21, IEffectFormat::YUVNV21 },
    { PixelFormat::NV12, IEffectFormat::YUVNV12 },
    { PixelFormat::RGBA_1010102, IEffectFormat::RGBA_1010102 },
};

const std::unordered_map<GraphicPixelFormat, IEffectFormat> CommonUtils::surfaceBufferFmtToEffectFmt_ = {
    { GraphicPixelFormat::GRAPHIC_PIXEL_FMT_RGBA_8888, IEffectFormat::RGBA8888},
    { GraphicPixelFormat::GRAPHIC_PIXEL_FMT_YCBCR_420_SP, IEffectFormat::YUVNV12 },
    { GraphicPixelFormat::GRAPHIC_PIXEL_FMT_YCRCB_420_SP, IEffectFormat::YUVNV21 },
    { GraphicPixelFormat::GRAPHIC_PIXEL_FMT_RGBA_1010102, IEffectFormat::RGBA_1010102 },
    { GraphicPixelFormat::GRAPHIC_PIXEL_FMT_YCBCR_P010, IEffectFormat::YCBCR_P010 },
    { GraphicPixelFormat::GRAPHIC_PIXEL_FMT_YCRCB_P010, IEffectFormat::YCRCB_P010 },
};

const std::unordered_map<AllocatorType, BufferType> CommonUtils::allocatorTypeToEffectBuffType_ = {
    { AllocatorType::HEAP_ALLOC, BufferType::HEAP_MEMORY },
    { AllocatorType::DMA_ALLOC, BufferType::DMA_BUFFER },
    { AllocatorType::SHARE_MEM_ALLOC, BufferType::SHARED_MEMORY },
};

template <class ValueType>
ErrorCode ParseJson(Plugin::Any any, nlohmann::json &value)
{
    auto result = Plugin::AnyCast<ValueType>(&any);
    if (result == nullptr) {
        return ErrorCode::ERR_ANY_CAST_TYPE_NOT_MATCH;
    }

    value = *result;
    return ErrorCode::SUCCESS;
}

ErrorCode CommonUtils::LockPixelMap(PixelMap *pixelMap, std::shared_ptr<EffectBuffer> &effectBuffer)
{
    CHECK_AND_RETURN_RET_LOG(pixelMap != nullptr, ErrorCode::ERR_INPUT_NULL, "pixelMap is null!");
    EFFECT_LOGD("pixelMapInfos: width=%{public}d, height=%{public}d, formatType=%{public}d " \
        "rowStride=%{public}d, byteCount=%{public}d, addr=%{private}p, colorSpaceName=%{public}d",
        pixelMap->GetWidth(), pixelMap->GetHeight(), pixelMap->GetPixelFormat(), pixelMap->GetRowStride(),
        pixelMap->GetByteCount(), pixelMap->GetPixels(), (pixelMap->InnerGetGrColorSpacePtr() == nullptr ?
        ColorSpaceName::NONE : pixelMap->InnerGetGrColorSpacePtr()->GetColorSpaceName()));

    IEffectFormat formatType = SwitchToEffectFormat(pixelMap->GetPixelFormat());
    if (formatType == IEffectFormat::DEFAULT) {
        EFFECT_LOGE("pixelFormat not support! pixelFormat=%{public}d", pixelMap->GetPixelFormat());
        return ErrorCode::ERR_UNSUPPORTED_PIXEL_FORMAT;
    }

    std::shared_ptr<BufferInfo> bufferInfo = std::make_unique<BufferInfo>();
    bufferInfo->width_ = static_cast<uint32_t>(pixelMap->GetWidth());
    bufferInfo->height_ = static_cast<uint32_t>(pixelMap->GetHeight());
    bufferInfo->rowStride_ = static_cast<uint32_t>(pixelMap->GetRowStride());
    bufferInfo->len_ = bufferInfo->height_ * bufferInfo->rowStride_;
    bufferInfo->formatType_ = formatType;
    if (pixelMap->InnerGetGrColorSpacePtr() != nullptr) {
        bufferInfo->colorSpace_ =
            ColorSpaceHelper::ConvertToEffectColorSpace(pixelMap->InnerGetGrColorSpacePtr()->GetColorSpaceName());
    }

    uint8_t *pixels = const_cast<uint8_t *>(pixelMap->GetPixels());
    void *srcData = static_cast<void *>(pixels);
    CHECK_AND_RETURN_RET_LOG(srcData != nullptr, ErrorCode::ERR_PIXELMAP_ACCESSPIXELS_FAIL, "fail exec GetPixels!");

    EFFECT_LOGI("pixelMap bufferInfos: width=%{public}d, height=%{public}d, formatType=%{public}d, " \
        "rowStride_=%{public}d, len=%{public}d, colorspace=%{public}d, addr=%{private}p",
        bufferInfo->width_, bufferInfo->height_, bufferInfo->formatType_, bufferInfo->rowStride_,
        bufferInfo->len_, bufferInfo->colorSpace_, pixels);

    std::shared_ptr<ExtraInfo> extraInfo = std::make_unique<ExtraInfo>();
    extraInfo->dataType = DataType::PIXEL_MAP;
    extraInfo->bufferType = SwitchToEffectBuffType(pixelMap->GetAllocatorType());
    extraInfo->pixelMap = pixelMap;
    extraInfo->surfaceBuffer = nullptr;
    if (extraInfo->bufferType == BufferType::DMA_BUFFER && pixelMap->GetFd() != nullptr) {
        extraInfo->surfaceBuffer = reinterpret_cast<SurfaceBuffer*> (pixelMap->GetFd());
    }
    EFFECT_LOGI("pixelMap extraInfos: dataType=%{public}d, bufferType=%{public}d, pixelMap=%{private}p,"
        " surfaceBuffer=%{private}p", extraInfo->dataType, extraInfo->bufferType, extraInfo->pixelMap,
        extraInfo->surfaceBuffer);

    if (extraInfo->surfaceBuffer != nullptr) {
        SurfaceBuffer *sb = extraInfo->surfaceBuffer;
        EFFECT_LOGD("pixelMap surfaceBufferInfo: width=%{public}d, height=%{public}d, format=%{public}d, "
            "rowStride=%{public}d, size=%{public}d, addr=%{private}p", sb->GetWidth(),
            sb->GetHeight(), sb->GetFormat(), sb->GetStride(), sb->GetSize(), sb->GetVirAddr());
    }

    effectBuffer = std::make_unique<EffectBuffer>(bufferInfo, srcData, extraInfo);
    return ErrorCode::SUCCESS;
}

ErrorCode CommonUtils::ParseNativeWindowData(std::shared_ptr<EffectBuffer> &effectBuffer, const DataType &dataType)
{
    std::shared_ptr<BufferInfo> bufferInfo = std::make_unique<BufferInfo>();
    bufferInfo->width_ = 0;
    bufferInfo->height_ = 0;
    bufferInfo->rowStride_ = 0;
    bufferInfo->len_ = 0;
    bufferInfo->formatType_ = IEffectFormat::DEFAULT;
    std::shared_ptr<ExtraInfo> extraInfo = std::make_unique<ExtraInfo>();
    extraInfo->dataType = dataType;
    extraInfo->bufferType = BufferType::DMA_BUFFER;
    extraInfo->pixelMap = nullptr;
    extraInfo->surfaceBuffer = nullptr;
    effectBuffer = std::make_unique<EffectBuffer>(bufferInfo, nullptr, extraInfo);
    return ErrorCode::SUCCESS;
}

ErrorCode CommonUtils::ParseSurfaceData(OHOS::SurfaceBuffer *surfaceBuffer,
    std::shared_ptr<EffectBuffer> &effectBuffer, const DataType &dataType)
{
    CHECK_AND_RETURN_RET_LOG(surfaceBuffer != nullptr, ErrorCode::ERR_INPUT_NULL, "surfaceBuffer is null!");

    std::shared_ptr<BufferInfo> bufferInfo = std::make_unique<BufferInfo>();
    bufferInfo->width_ = static_cast<uint32_t>(surfaceBuffer->GetWidth());
    bufferInfo->height_ = static_cast<uint32_t>(surfaceBuffer->GetHeight());
    bufferInfo->rowStride_ = static_cast<uint32_t>(surfaceBuffer->GetStride());
    bufferInfo->len_ = surfaceBuffer->GetSize();
    bufferInfo->formatType_ = SwitchToEffectFormat((GraphicPixelFormat)surfaceBuffer->GetFormat());
    CM_ColorSpaceType colorSpaceType = CM_ColorSpaceType::CM_COLORSPACE_NONE;
    ColorSpaceHelper::GetSurfaceBufferColorSpaceType(surfaceBuffer, colorSpaceType);
    bufferInfo->colorSpace_ = ColorSpaceHelper::ConvertToEffectColorSpace(colorSpaceType);

    std::shared_ptr<ExtraInfo> extraInfo = std::make_unique<ExtraInfo>();
    extraInfo->dataType = dataType;
    extraInfo->bufferType = BufferType::DMA_BUFFER;
    extraInfo->pixelMap = nullptr;
    extraInfo->surfaceBuffer = surfaceBuffer;

    effectBuffer = std::make_unique<EffectBuffer>(bufferInfo, surfaceBuffer->GetVirAddr(), extraInfo);
    EFFECT_LOGI("surfaceBuffer width=%{public}d, height=%{public}d, stride=%{public}d, format=%{public}d, "
        "size=%{public}d, usage = %{public}llu, addr=%{private}p, colorSpace=%{public}d",
        surfaceBuffer->GetWidth(), surfaceBuffer->GetHeight(), surfaceBuffer->GetStride(), surfaceBuffer->GetFormat(),
        surfaceBuffer->GetSize(), static_cast<unsigned long long>(surfaceBuffer->GetUsage()),
        surfaceBuffer->GetVirAddr(), colorSpaceType);
    return ErrorCode::SUCCESS;
}

std::string CommonUtils::UrlToPath(const std::string &url)
{
    OHOS::Uri uri = OHOS::Uri(url);
    return uri.GetPath();
}

ErrorCode CommonUtils::ParseUri(std::string &uri, std::shared_ptr<EffectBuffer> &effectBuffer, bool isOutputData)
{
    if (isOutputData) {
        std::shared_ptr<BufferInfo> bufferInfo = std::make_unique<BufferInfo>();
        std::shared_ptr<ExtraInfo> extraInfo = std::make_unique<ExtraInfo>();
        extraInfo->dataType = DataType::URI;
        extraInfo->uri = std::move(uri);
        effectBuffer = std::make_unique<EffectBuffer>(bufferInfo, nullptr, extraInfo);
        return ErrorCode::SUCCESS;
    }

    auto path = UrlToPath(uri);
    ErrorCode res = ParsePath(path, effectBuffer, isOutputData);
    CHECK_AND_RETURN_RET_LOG(res == ErrorCode::SUCCESS, res,
        "ParseUri: path name fail! uri=%{public}s, res=%{public}d", uri.c_str(), res);

    CHECK_AND_RETURN_RET_LOG(effectBuffer->extraInfo_ != nullptr, ErrorCode::ERR_EXTRA_INFO_NULL,
        "ParseUri: extra info is null! uri=%{public}s", uri.c_str());
    effectBuffer->extraInfo_->dataType = DataType::URI;
    effectBuffer->extraInfo_->uri = std::move(uri);

    return ErrorCode::SUCCESS;
}

ErrorCode CommonUtils::ParsePath(std::string &path, std::shared_ptr<EffectBuffer> &effectBuffer,
    bool isOutputData)
{
    if (isOutputData) {
        std::shared_ptr<BufferInfo> bufferInfo = std::make_unique<BufferInfo>();
        std::shared_ptr<ExtraInfo> extraInfo = std::make_unique<ExtraInfo>();
        extraInfo->dataType = DataType::PATH;
        extraInfo->path = std::move(path);
        effectBuffer = std::make_unique<EffectBuffer>(bufferInfo, nullptr, extraInfo);
        return ErrorCode::SUCCESS;
    }

    SourceOptions opts;
    uint32_t errorCode = 0;
    std::unique_ptr<ImageSource> imageSource = ImageSource::CreateImageSource(path, opts, errorCode);
    CHECK_AND_RETURN_RET_LOG(imageSource != nullptr, ErrorCode::ERR_CREATE_IMAGESOURCE_FAIL,
        "ImageSource::CreateImageSource fail! path=%{public}s, errorCode=%{public}d", path.c_str(), errorCode);

    DecodeOptions options;
    options.desiredDynamicRange = DecodeDynamicRange::AUTO;
    std::unique_ptr<PixelMap> pixelMap = imageSource->CreatePixelMap(options, errorCode);
    CHECK_AND_RETURN_RET_LOG(pixelMap != nullptr, ErrorCode::ERR_CREATE_PIXELMAP_FAIL,
        "CreatePixelMap fail! path=%{public}s, errorCode=%{public}d", path.c_str(), errorCode);

    ErrorCode res = LockPixelMap(pixelMap.get(), effectBuffer);
    CHECK_AND_RETURN_RET_LOG(res == ErrorCode::SUCCESS, res,
        "ParsePath: lock pixel map fail! path=%{public}s, res=%{public}d", path.c_str(), res);

    CHECK_AND_RETURN_RET_LOG(effectBuffer->extraInfo_ != nullptr, ErrorCode::ERR_EXTRA_INFO_NULL,
        "ParsePath: extra info is null! uri=%{public}s", path.c_str());
    effectBuffer->extraInfo_->dataType = DataType::PATH;
    effectBuffer->extraInfo_->path = std::move(path);
    effectBuffer->extraInfo_->pixelMap = nullptr;
    effectBuffer->extraInfo_->innerPixelMap = std::move(pixelMap);

    return ErrorCode::SUCCESS;
}

IEffectFormat CommonUtils::SwitchToEffectFormat(GraphicPixelFormat pixelFormat)
{
    IEffectFormat formatType = IEffectFormat::DEFAULT;

    auto itr = surfaceBufferFmtToEffectFmt_.find(pixelFormat);
    if (itr != surfaceBufferFmtToEffectFmt_.end()) {
        formatType = itr->second;
    }

    return formatType;
}

IEffectFormat CommonUtils::SwitchToEffectFormat(PixelFormat pixelFormat)
{
    IEffectFormat formatType = IEffectFormat::DEFAULT;

    auto itr = pixelFmtToEffectFmt_.find(pixelFormat);
    if (itr != pixelFmtToEffectFmt_.end()) {
        formatType = itr->second;
    }

    return formatType;
}

GraphicPixelFormat CommonUtils::SwitchToGraphicPixelFormat(IEffectFormat formatType)
{
    GraphicPixelFormat pixelFormat = GraphicPixelFormat::GRAPHIC_PIXEL_FMT_BUTT;

    for (const auto &itr : surfaceBufferFmtToEffectFmt_) {
        if (itr.second == formatType) {
            pixelFormat = itr.first;
            break;
        }
    }

    return pixelFormat;
}

PixelFormat CommonUtils::SwitchToPixelFormat(IEffectFormat formatType)
{
    PixelFormat pixelFormat = PixelFormat::UNKNOWN;

    for (const auto &itr : pixelFmtToEffectFmt_) {
        if (itr.second == formatType) {
            pixelFormat = itr.first;
            break;
        }
    }

    return pixelFormat;
}

BufferType CommonUtils::SwitchToEffectBuffType(AllocatorType allocatorType)
{
    BufferType bufferType = BufferType::DEFAULT;

    auto itr = allocatorTypeToEffectBuffType_.find(allocatorType);
    if (itr != allocatorTypeToEffectBuffType_.end()) {
        bufferType = itr->second;
    }

    return bufferType;
}

void CommonUtils::UnlockPixelMap(const PixelMap *pixelMap)
{
    EFFECT_LOGI("UnlockPixelMap! pixelMap=%{public}p", pixelMap);
}

ErrorCode CommonUtils::ParseAnyToJson(Plugin::Any &any, nlohmann::json &result)
{
    CHECK_AND_RETURN_RET(ParseJson<float>(any, result) != ErrorCode::SUCCESS, ErrorCode::SUCCESS);
    CHECK_AND_RETURN_RET(ParseJson<int32_t>(any, result) != ErrorCode::SUCCESS, ErrorCode::SUCCESS);
    CHECK_AND_RETURN_RET(ParseJson<uint32_t>(any, result) != ErrorCode::SUCCESS, ErrorCode::SUCCESS);
#ifndef HST_ANY_WITH_NO_RTTI
    EFFECT_LOGE("inner any type not support switch to json! type:%{public}s", any.Type().name());
#else
    EFFECT_LOGE("inner any type not support switch to json! type:%{public}s", std::string(any.TypeName()).c_str());
#endif
    return ErrorCode::ERR_ANY_CAST_TYPE_NOT_MATCH;
}

bool CommonUtils::EndsWithJPG(const std::string &input)
{
    return StringHelp::EndsWithIgnoreCase(input, "jpg") || StringHelp::EndsWithIgnoreCase(input, "jpeg");
}

ErrorCode GetPixelsContext(std::shared_ptr<MemoryData> &memoryData, BufferType bufferType, void **context)
{
    switch (bufferType) {
        case BufferType::HEAP_MEMORY:
            *context = nullptr;
            break;
        case BufferType::DMA_BUFFER: {
            void *extra = memoryData->memoryInfo.extra;
            auto surfaceBuffer = reinterpret_cast<SurfaceBuffer *>(extra);
            CHECK_AND_RETURN_RET_LOG(surfaceBuffer != nullptr, ErrorCode::ERR_INVALID_SURFACE_BUFFER,
                "DMA_BUFFER: extra info error!");
            *context = surfaceBuffer;
            break;
        }
        case BufferType::SHARED_MEMORY: {
            void *extra = memoryData->memoryInfo.extra;
            auto fd = static_cast<int *>(extra);
            CHECK_AND_RETURN_RET_LOG(fd != nullptr, ErrorCode::ERR_INVALID_FD, "SHARED_MEMORY: extra info error!");
            *context = fd;
            break;
        }
        default:
            EFFECT_LOGE("bufferType not support! bufferType=%{public}d", bufferType);
            return ErrorCode::ERR_UNSUPPORTED_BUFFER_TYPE;
    }
    return ErrorCode::SUCCESS;
}

void PrintImageExifInfo(PixelMap *pixelMap, const std::string &tag)
{
    if (pixelMap == nullptr) {
        return;
    }

    int32_t width = 0;
    pixelMap->GetImagePropertyInt(IMAGE_WIDTH, width);
    int32_t length = 0;
    pixelMap->GetImagePropertyInt(IMAGE_LENGTH, length);
    std::string dateTime;
    pixelMap->GetImagePropertyString(DATE_TIME, dateTime);
    int32_t xDimension = 0;
    pixelMap->GetImagePropertyInt(PIXEL_X_DIMENSION, xDimension);
    int32_t yDimension = 0;
    pixelMap->GetImagePropertyInt(PIXEL_Y_DIMENSION, yDimension);

    EFFECT_LOGD("%{public}s: width=%{public}d, length=%{public}d, dateTime=%{public}s, xDimension=%{public}d, "
        "yDimension=%{public}d", tag.c_str(), width, length, dateTime.c_str(), xDimension, yDimension);
}

void CommonUtils::UpdateImageExifDateTime(PixelMap *pixelMap)
{
    if (pixelMap == nullptr) {
        return;
    }

    std::string dateTime;
    if (pixelMap->GetImagePropertyString(DATE_TIME, dateTime) != 0 || dateTime.empty()) {
        return;
    }

    time_t now = time(nullptr);
    CHECK_AND_RETURN_LOG(now > 0, "UpdateExifDateTime: time fail!");

    struct tm *locTime = localtime(&now);
    CHECK_AND_RETURN_LOG(locTime != nullptr, "UpdateExifDateTime: localtime fail!");

    char tempTime[TIME_MAX];
    auto size = strftime(tempTime, sizeof(tempTime), "%Y:%m:%d %H:%M:%S", locTime);
    CHECK_AND_RETURN_LOG(size > 0, "UpdateExifDateTime: strftime fail!");

    std::string currentTime = std::string(tempTime, size);
    pixelMap->ModifyImageProperty(DATE_TIME, currentTime);
}

void CommonUtils::UpdateImageExifInfo(PixelMap *pixelMap)
{
    if (pixelMap == nullptr) {
        return;
    }
    PrintImageExifInfo(pixelMap, "UpdateImageExifInfo::update before");

    // Set Width
    int32_t width = 0;
    if (pixelMap->GetImagePropertyInt(IMAGE_WIDTH, width) == 0 && pixelMap->GetWidth() != width) {
        pixelMap->ModifyImageProperty(IMAGE_WIDTH, std::to_string(pixelMap->GetWidth()));
    }

    // Set Length
    int32_t length = 0;
    if (pixelMap->GetImagePropertyInt(IMAGE_LENGTH, width) == 0 && pixelMap->GetHeight() != length) {
        pixelMap->ModifyImageProperty(IMAGE_LENGTH, std::to_string(pixelMap->GetHeight()));
    }

    // Set DateTime
    CommonUtils::UpdateImageExifDateTime(pixelMap);

    // Set PixelXDimension
    int32_t xDimension = 0;
    if (pixelMap->GetImagePropertyInt(PIXEL_X_DIMENSION, xDimension) == 0 && pixelMap->GetWidth() != xDimension) {
        pixelMap->ModifyImageProperty(PIXEL_X_DIMENSION, std::to_string(pixelMap->GetWidth()));
    }

    // Set PixelYDimension
    int32_t yDimension = 0;
    if (pixelMap->GetImagePropertyInt(PIXEL_Y_DIMENSION, xDimension) == 0 && pixelMap->GetHeight() != yDimension) {
        pixelMap->ModifyImageProperty(PIXEL_Y_DIMENSION, std::to_string(pixelMap->GetHeight()));
    }

    PrintImageExifInfo(pixelMap, "UpdateImageExifInfo::update after");
}

ErrorCode ModifyPixelMapPropertyInner(std::shared_ptr<MemoryData> &memoryData, PixelMap *pixelMap,
    AllocatorType &allocatorType)
{
    void *context = nullptr;
    const MemoryInfo &memoryInfo = memoryData->memoryInfo;
    ErrorCode res = GetPixelsContext(memoryData, memoryInfo.bufferType, &context);
    CHECK_AND_RETURN_RET_LOG(res == ErrorCode::SUCCESS, res, "get pixels context fail! res=%{public}d", res);

    // not need to release the origin buffer in pixelMap, SetPixelsAddr will release it.
    pixelMap->SetPixelsAddr(memoryData->data, context, memoryInfo.bufferInfo.len_, allocatorType, nullptr);

    ImageInfo imageInfo;
    pixelMap->GetImageInfo(imageInfo);
    imageInfo.size.width = static_cast<int32_t>(memoryInfo.bufferInfo.width_);
    imageInfo.size.height = static_cast<int32_t>(memoryInfo.bufferInfo.height_);
    imageInfo.pixelFormat = CommonUtils::SwitchToPixelFormat(memoryInfo.bufferInfo.formatType_);
    uint32_t result = pixelMap->SetImageInfo(imageInfo, true);
    EFFECT_LOGI("ModifyPixelMapPropertyInner: SetImageInfo width=%{public}d, height=%{public}d, result: %{public}d",
        imageInfo.size.width, imageInfo.size.height, result);
    CHECK_AND_RETURN_RET_LOG(result == 0, ErrorCode::ERR_SET_IMAGE_INFO_FAIL,
        "ModifyPixelMapPropertyInner: exec SetImageInfo fail! result=%{public}d", result);

    // update rowStride
    pixelMap->SetRowStride(memoryInfo.bufferInfo.rowStride_);

    // update exif
    CommonUtils::UpdateImageExifInfo(pixelMap);

    EffectColorSpace colorSpace = memoryInfo.bufferInfo.colorSpace_;
    if (colorSpace != EffectColorSpace::DEFAULT) {
        OHOS::ColorManager::ColorSpace grColorSpace(ColorSpaceHelper::ConvertToColorSpaceName(colorSpace));
        pixelMap->InnerSetColorSpace(grColorSpace);
    }

    // update metadata if need
    if (memoryInfo.bufferType == BufferType::DMA_BUFFER) {
        res = ColorSpaceHelper::UpdateMetadata(static_cast<SurfaceBuffer *>(memoryInfo.extra),
            memoryInfo.bufferInfo.colorSpace_);
        CHECK_AND_RETURN_RET_LOG(res == ErrorCode::SUCCESS, res,
            "ModifyPixelMapPropertyInner: UpdateMetadata fail! res=%{public}d", res);
    }

    return ErrorCode::SUCCESS;
}

ErrorCode CommonUtils::ModifyPixelMapProperty(PixelMap *pixelMap, const std::shared_ptr<EffectBuffer> &buffer,
    const std::shared_ptr<EffectMemoryManager> &memoryManager)
{
    EFFECT_LOGI("ModifyPixelMapProperty enter!");
    CHECK_AND_RETURN_RET_LOG(pixelMap != nullptr, ErrorCode::ERR_INPUT_NULL, "pixel map is null");
    AllocatorType allocatorType = pixelMap->GetAllocatorType();
    BufferType bufferType = SwitchToEffectBuffType(allocatorType);
    EFFECT_LOGD("ModifyPixelMapProperty: allocatorType=%{public}d, bufferType=%{public}d", allocatorType, bufferType);
    std::shared_ptr<Memory> allocMemory = memoryManager->GetAllocMemoryByAddr(buffer->buffer_);
    std::shared_ptr<MemoryData> memoryData;
    if (allocMemory != nullptr && allocMemory->memoryData_->memoryInfo.bufferType == bufferType) {
        EFFECT_LOGD("ModifyPixelMapProperty reuse allocated memory. addr=%{public}p", buffer->buffer_);
        allocMemory->memoryData_->memoryInfo.isAutoRelease = false;
        memoryData = allocMemory->memoryData_;
    } else {
        EFFECT_LOGD("ModifyPixelMapProperty alloc memory.");
        std::unique_ptr<AbsMemory> memory = EffectMemory::CreateMemory(bufferType);
        CHECK_AND_RETURN_RET_LOG(memory != nullptr, ErrorCode::ERR_CREATE_MEMORY_FAIL,
            "memory create fail! allocatorType=%{public}d", allocatorType);

        MemoryInfo memoryInfo = {
            .isAutoRelease = false,
            .bufferInfo = *buffer->bufferInfo_,
            .extra = pixelMap->GetFd()
        };
        memoryData = memory->Alloc(memoryInfo);
        CHECK_AND_RETURN_RET_LOG(memoryData != nullptr, ErrorCode::ERR_ALLOC_MEMORY_FAIL, "Alloc fail!");
        MemcpyHelper::CopyData(buffer.get(), memoryData.get());
    }

    return ModifyPixelMapPropertyInner(memoryData, pixelMap, allocatorType);
}

ErrorCode CommonUtils::ModifyPixelMapPropertyForTexture(PixelMap *pixelMap, const std::shared_ptr<EffectBuffer> &buffer,
    std::shared_ptr<EffectContext> &context)
{
    EFFECT_LOGI("ModifyPixelMapPropertyForTexture enter!");
    CHECK_AND_RETURN_RET_LOG(pixelMap != nullptr, ErrorCode::ERR_INPUT_NULL, "pixel map is null");
    AllocatorType allocatorType = pixelMap->GetAllocatorType();
    BufferType bufferType = SwitchToEffectBuffType(allocatorType);
    EFFECT_LOGD("ModifyPixelMapProperty: allocatorType=%{public}d, bufferType=%{public}d", allocatorType, bufferType);
    std::unique_ptr<AbsMemory> memory = EffectMemory::CreateMemory(bufferType);
    CHECK_AND_RETURN_RET_LOG(memory != nullptr, ErrorCode::ERR_CREATE_MEMORY_FAIL,
        "memory create fail! allocatorType=%{public}d", allocatorType);

    MemoryInfo memoryInfo = {
        .isAutoRelease = false,
        .bufferInfo = *buffer->bufferInfo_,
        .extra = pixelMap->GetFd()
    };
    std::shared_ptr<MemoryData> memoryData = memory->Alloc(memoryInfo);
    CHECK_AND_RETURN_RET_LOG(memoryData != nullptr, ErrorCode::ERR_ALLOC_MEMORY_FAIL, "Alloc fail!");
    context->renderEnvironment_->ReadPixelsFromTex(buffer->tex, memoryData->data, buffer->bufferInfo_->width_,
        buffer->bufferInfo_->height_, memoryData->memoryInfo.bufferInfo.rowStride_ / RGBA_BYTES_PER_PIXEL);

    return ModifyPixelMapPropertyInner(memoryData, pixelMap, allocatorType);
}
} // namespace Effect
} // namespace Media
} // namespace OHOS