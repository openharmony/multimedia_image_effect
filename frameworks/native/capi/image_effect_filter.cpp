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

#include "image_effect_filter.h"

#include "common_utils.h"
#include "filter_delegate.h"
#include "effect_log.h"
#include "efilter.h"
#include "efilter_factory.h"
#include "native_effect_base.h"
#include "native_common_utils.h"
#include "memcpy_helper.h"
#include "event_report.h"
#include <cstring>

using namespace OHOS::Media;
using namespace OHOS::Media::Effect;

namespace {
    constexpr int const MAX_CHAR_LEN = 1024;

    std::mutex filterMutex_;
}

static std::vector<std::shared_ptr<ImageEffect_FilterNames>> sOHFilterNames;

void OH_EffectFilter::SetParameter(const std::string &key, Plugin::Any &param)
{
    params_.erase(key);
    params_.emplace(key, param);
}

ErrorCode OH_EffectFilter::GetParameter(const std::string &key, Plugin::Any &param)
{
    auto it = params_.find(key);
    if (it == params_.end()) {
        return ErrorCode::ERR_NO_VALUE;
    }

    param = std::move(it->second);
    return ErrorCode::SUCCESS;
}

void OH_EffectFilter::RemoveParameter(const std::string &key)
{
    params_.erase(key);
}

OH_EffectFilterInfo::~OH_EffectFilterInfo()
{
    if (effectBufferType != nullptr) {
        delete[] effectBufferType;
        effectBufferType = nullptr;
    }
    if (effectFormat != nullptr) {
        delete[] effectFormat;
        effectFormat = nullptr;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

EFFECT_EXPORT
OH_EffectFilterInfo *OH_EffectFilterInfo_Create()
{
    std::unique_ptr<OH_EffectFilterInfo> info = std::make_unique<OH_EffectFilterInfo>();
    return info.release();
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilterInfo_SetFilterName(OH_EffectFilterInfo *info, const char *name)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoSetFilterName: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(name != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoSetFilterName: input parameter name is null!");
    CHECK_AND_RETURN_RET_LOG(strlen(name) < MAX_CHAR_LEN, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoSetFilterName: the length of input parameter name is too long! len = %{public}zu", strlen(name));
    EFFECT_LOGD("Set filter name. name=%{public}s", name);
    info->filterName = name;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilterInfo_GetFilterName(OH_EffectFilterInfo *info, char **name)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoGetFilterName: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(name != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoGetFilterName: input parameter name is null!");

    *name = const_cast<char *>(info->filterName.c_str());
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilterInfo_SetSupportedBufferTypes(OH_EffectFilterInfo *info, uint32_t size,
    ImageEffect_BufferType *bufferTypeArray)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoSetSupportedBufferTypes: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(size > 0, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoSetSupportedBufferTypes: input parameter size is invalid! size=%{public}d", size);
    CHECK_AND_RETURN_RET_LOG(bufferTypeArray != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoSetSupportedBufferTypes: input parameter bufferTypeArray is null!");
    EFFECT_LOGD("Set supported buffer types. size=%{public}d", size);
    info->supportedBufferTypes.clear();
    for (uint32_t index = 0; index < size; index++) {
        info->supportedBufferTypes.emplace(bufferTypeArray[index]);
    }
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilterInfo_GetSupportedBufferTypes(OH_EffectFilterInfo *info, uint32_t *size,
    ImageEffect_BufferType **bufferTypeArray)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoGetSupportedBufferTypes: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(size != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoGetSupportedBufferTypes: input parameter size is invalid!");
    CHECK_AND_RETURN_RET_LOG(bufferTypeArray != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoGetSupportedBufferTypes: input parameter bufferTypeArray is null!");

    if (info->supportedBufferTypes.empty()) {
        *size = 0;
        *bufferTypeArray = nullptr;
        return ImageEffect_ErrorCode::EFFECT_SUCCESS;
    }

    auto bufferTypeRealSize = static_cast<uint32_t>(info->supportedBufferTypes.size());
    if (bufferTypeRealSize > info->bufferTypeArraySize && info->effectBufferType != nullptr) {
        delete[] info->effectBufferType;
        info->effectBufferType = nullptr;
        info->bufferTypeArraySize = 0;
    }

    if (info->effectBufferType == nullptr) {
        std::unique_ptr<ImageEffect_BufferType[]> bufferType =
            std::make_unique<ImageEffect_BufferType[]>(bufferTypeRealSize);
        info->effectBufferType = bufferType.release();
        info->bufferTypeArraySize = bufferTypeRealSize;
    }

    uint32_t index = 0;
    for (const auto &bufferType : info->supportedBufferTypes) {
        if (index >= info->bufferTypeArraySize) {
            EFFECT_LOGW("supportedBufferTypes size over bufferTypeArraySize! supportedBufferTypesSize=%{public}zu, "
                "bufferTypeArraySize=%{public}d", info->supportedBufferTypes.size(), info->bufferTypeArraySize);
            break;
        }
        info->effectBufferType[index] = bufferType;
        index++;
    }
    *size = index;
    *bufferTypeArray = info->effectBufferType;

    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilterInfo_SetSupportedFormats(OH_EffectFilterInfo *info, uint32_t size,
    ImageEffect_Format *formatArray)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoSetSupportedFormats: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(size > 0, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoSetSupportedFormats: input parameter size is invalid! size=%{public}d", size);
    CHECK_AND_RETURN_RET_LOG(formatArray != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoSetSupportedFormats: input parameter formatArray is null!");
    EFFECT_LOGD("Set supported formats. size=%{public}d", size);
    info->supportedFormats.clear();
    for (uint32_t index = 0; index < size; index++) {
        info->supportedFormats.emplace(formatArray[index]);
    }
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilterInfo_GetSupportedFormats(OH_EffectFilterInfo *info, uint32_t *size,
    ImageEffect_Format **formatArray)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoGetSupportedFormats: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(size != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoGetSupportedFormats: input parameter size is invalid!");
    CHECK_AND_RETURN_RET_LOG(formatArray != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "InfoGetSupportedFormats: input parameter formatArray is null!");

    if (info->supportedFormats.empty()) {
        *size = 0;
        *formatArray = nullptr;
        return ImageEffect_ErrorCode::EFFECT_SUCCESS;
    }

    auto formatsRealSize = static_cast<uint32_t>(info->supportedFormats.size());
    if (formatsRealSize > info->formatArraySize && info->effectFormat != nullptr) {
        delete[] info->effectFormat;
        info->effectFormat = nullptr;
        info->formatArraySize = 0;
    }

    if (info->effectFormat == nullptr) {
        std::unique_ptr<ImageEffect_Format []> bufferType = std::make_unique<ImageEffect_Format[]>(formatsRealSize);
        info->effectFormat = bufferType.release();
        info->formatArraySize = formatsRealSize;
    }

    uint32_t index = 0;
    for (const auto &format : info->supportedFormats) {
        if (index >= info->formatArraySize) {
            EFFECT_LOGW("supportedFormats size over formatArraySize! supportedFormatsSize=%{public}zu, "
                "formatArraySize=%{public}d", info->supportedFormats.size(), info->formatArraySize);
            break;
        }
        info->effectFormat[index] = format;
        index++;
    }
    *size = index;
    *formatArray = info->effectFormat;

    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilterInfo_Release(OH_EffectFilterInfo *info)
{
    EFFECT_LOGD("Filter release info.");
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "ReleaseInfo: input parameter info is null!");
    delete (info);
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
OH_EffectBufferInfo *OH_EffectBufferInfo_Create()
{
    std::unique_ptr<OH_EffectBufferInfo> info = std::make_unique<OH_EffectBufferInfo>();
    return info.release();
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_SetAddr(OH_EffectBufferInfo *info, void *addr)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoSetAddr: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(addr != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoSetAddr: input parameter addr is null!");
    EFFECT_LOGD("Set buffer info addr.");
    info->addr = addr;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_GetAddr(OH_EffectBufferInfo *info, void **addr)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetAddr: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(addr != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetAddr: input parameter addr is null!");

    *addr = info->addr;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_SetWidth(OH_EffectBufferInfo *info, int32_t width)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoSetWidth: input parameter info is null!");
    EFFECT_LOGD("BufferInfoSetWidth: width=%{public}d", width);
    info->width = width;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_GetWidth(OH_EffectBufferInfo *info, int32_t *width)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetWidth: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(width != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetWidth: input parameter width is null!");

    *width = info->width;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_SetHeight(OH_EffectBufferInfo *info, int32_t height)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoSetHeight: input parameter info is null!");
    EFFECT_LOGD("BufferInfoSetHeight: height=%{public}d", height);
    info->height = height;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_GetHeight(OH_EffectBufferInfo *info, int32_t *height)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetHeight: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(height != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetHeight: input parameter height is null!");

    *height = info->height;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_SetRowSize(OH_EffectBufferInfo *info, int32_t rowSize)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoSetRowSize: input parameter info is null!");
    EFFECT_LOGD("BufferInfoSetRowSize: rowSize=%{public}d", rowSize);
    info->rowSize = rowSize;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_GetRowSize(OH_EffectBufferInfo *info, int32_t *rowSize)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetRowSize: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(rowSize != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetRowSize: input parameter rowSize is null!");

    *rowSize = info->rowSize;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_SetEffectFormat(OH_EffectBufferInfo *info, ImageEffect_Format format)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoSetEffectFormat: input parameter info is null!");
    EFFECT_LOGD("BufferInfoSetEffectFormat: format=%{public}d", format);
    info->format = format;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_GetEffectFormat(OH_EffectBufferInfo *info, ImageEffect_Format *format)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetEffectFormat: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(format != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetEffectFormat: input parameter format is null!");

    *format = info->format;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_SetTimestamp(OH_EffectBufferInfo *info, int64_t timestamp)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoSetTimestamp: input parameter info is null!");
    EFFECT_LOGD("BufferInfoSetTimestamp: timestamp=%{public}lld", static_cast<long long>(timestamp));
    info->timestamp = timestamp;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_GetTimestamp(OH_EffectBufferInfo *info, int64_t *timestamp)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetTimestamp: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(timestamp != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetTimestamp: input parameter timestamp is null!");

    *timestamp = info->timestamp;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_SetTextureId(OH_EffectBufferInfo *info, int32_t textureId)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoSetTextureId: input parameter info is null!");
    EFFECT_LOGD("BufferInfoSetTextureId: TextureId=%{public}d", textureId);
    CHECK_AND_RETURN_RET_LOG(textureId > 0, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetTextureId: input parameter textureId is invalid!");
    info->textureId = textureId;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_GetTextureId(OH_EffectBufferInfo *info, int32_t *textureId)
{
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetTextureId: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(textureId != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "BufferInfoGetTextureId: input parameter timestamp is null!");
    *textureId = info->textureId;
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectBufferInfo_Release(OH_EffectBufferInfo *info)
{
    EFFECT_LOGD("Filter release buffer info.");
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "EffectFilter ReleaseBufferInfo: input parameter info is null!");
    delete (info);
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
OH_EffectFilter *OH_EffectFilter_Create(const char *name)
{
    std::unique_lock<std::mutex> lock(filterMutex_);
    CHECK_AND_RETURN_RET_LOG(name != nullptr, nullptr, "FilterCreate: input parameter name is null!");
    CHECK_AND_RETURN_RET_LOG(strlen(name) < MAX_CHAR_LEN, nullptr,
        "FilterCreate: the length of input parameter name is too long! len = %{public}zu", strlen(name));
    EFFECT_LOGI("Filter create. name=%{public}s", name);
    std::unique_ptr<OH_EffectFilter> nativeEFilter = std::make_unique<OH_EffectFilter>();
    std::shared_ptr<EFilter> filter = EFilterFactory::Instance()->Create(name, nativeEFilter.get());
    if (filter == nullptr) {
        EFFECT_LOGW("FilterCreate: create filter fail. name=%{public}s not exist!", name);
        return nullptr;
    }
    nativeEFilter->filter_ = filter;
    return nativeEFilter.release();
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilter_SetValue(OH_EffectFilter *filter, const char *key, const ImageEffect_Any *value)
{
    CHECK_AND_RETURN_RET_LOG(filter != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterSetValue: input parameter filter is null!");
    CHECK_AND_RETURN_RET_LOG(key != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterSetValue: input parameter key is null!");
    CHECK_AND_RETURN_RET_LOG(strlen(key) < MAX_CHAR_LEN, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterSetValue: the length of input parameter key is too long! len = %{public}zu", strlen(key));
    CHECK_AND_RETURN_RET_LOG(value != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterSetValue: input parameter value is null!");
    EFFECT_LOGD("Effect filter set value. key=%{public}s", key);

    Plugin::Any any;
    ErrorCode result = NativeCommonUtils::ParseOHAny(value, any);
    if (result != ErrorCode::SUCCESS) {
        EFFECT_LOGE("FilterSetValue: parse any fail! result=%{public}d, dataType=%{public}d", result, value->dataType);
        return ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID;
    }

    filter->filter_->ProcessConfig(key);
    result = filter->filter_->SetValue(key, any);
    if (result != ErrorCode::SUCCESS) {
        EFFECT_LOGE("FilterSetValue: set value fail! result=%{public}d, key=%{public}s, dataType=%{public}d", result,
            key, value->dataType);
        return ImageEffect_ErrorCode::EFFECT_KEY_ERROR;
    }

    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilter_GetValue(OH_EffectFilter *nativeEFilter, const char *key, ImageEffect_Any *value)
{
    CHECK_AND_RETURN_RET_LOG(nativeEFilter != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterGetValue: input parameter nativeEFilter is null!");
    CHECK_AND_RETURN_RET_LOG(key != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterGetValue: input parameter key is null!");
    CHECK_AND_RETURN_RET_LOG(strlen(key) < MAX_CHAR_LEN, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterGetValue: the length of input parameter key is too long! len = %{public}zu", strlen(key));
    CHECK_AND_RETURN_RET_LOG(value != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterGetValue: input parameter value is null!");
    EFFECT_LOGD("Effect filter get value. key=%{public}s", key);

    if (strcmp("FILTER_NAME", key) == 0) {
        value->dataType = ImageEffect_DataType::EFFECT_DATA_TYPE_PTR;
        value->dataValue.ptrValue = static_cast<void *>(const_cast<char *>(nativeEFilter->filter_->GetName().c_str()));
        return ImageEffect_ErrorCode::EFFECT_SUCCESS;
    }

    Plugin::Any any;
    ErrorCode result = nativeEFilter->filter_->GetValue(key, any);
    if (result != ErrorCode::SUCCESS) {
        EFFECT_LOGE("FilterGetValue: get value fail! result=%{public}d, key=%{public}s", result, key);
        return ImageEffect_ErrorCode::EFFECT_KEY_ERROR;
    }

    result = NativeCommonUtils::SwitchToOHAny(any, value);
    if (result != ErrorCode::SUCCESS) {
        EFFECT_LOGE("FilterGetValue: get value fail! result=%{public}d, key=%{public}s", result, key);
        return ImageEffect_ErrorCode::EFFECT_UNKNOWN;
    }

    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilter_Register(const OH_EffectFilterInfo *info,
    const ImageEffect_FilterDelegate *delegate)
{
    std::unique_lock<std::mutex> lock(filterMutex_);
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "RegisterFilter: input parameter info is null!");
    CHECK_AND_RETURN_RET_LOG(delegate != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "RegisterFilter: input parameter delegate is null!");
    EFFECT_LOGI("Filter register. filterName=%{public}s", info->filterName.c_str());
    std::shared_ptr<FilterDelegate> effectDelegate = std::make_shared<FilterDelegate>(info, delegate);

    std::shared_ptr<EffectInfo> effectInfo = std::make_shared<EffectInfo>();
    NativeCommonUtils::SwitchToEffectInfo(info, effectInfo);
    EFilterFactory::Instance()->RegisterDelegate(info->filterName, effectDelegate, effectInfo);

    EventInfo eventInfo = {
        .filterName = info->filterName,
        .supportedFormats = NativeCommonUtils::GetSupportedFormats(info),
    };
    EventReport::ReportHiSysEvent(REGISTER_CUSTOM_FILTER_STATISTIC, eventInfo);
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_FilterNames *OH_EffectFilter_LookupFilters(const char *key)
{
    std::unique_lock<std::mutex> lock(filterMutex_);
    CHECK_AND_RETURN_RET_LOG(key != nullptr, nullptr, "LookupFilters: input parameter key is null!");
    CHECK_AND_RETURN_RET_LOG(strlen(key) < MAX_CHAR_LEN, nullptr,
        "LookupFilters: the length of input parameter key is too long! len = %{public}zu", strlen(key));
    EFFECT_LOGD("Lookup filters. key=%{public}s", key);

    std::string lookupKey = key;
    std::vector<const char *> matchEFilter;
    NativeCommonUtils::ParseLookupKey(lookupKey, matchEFilter);

    std::shared_ptr<ImageEffect_FilterNames> filterNames = std::make_shared<ImageEffect_FilterNames>();
    filterNames->size = matchEFilter.size();
    if (filterNames->size != 0) {
        const char **buffer = (const char **)malloc(matchEFilter.size() * sizeof(const char *));
        if (buffer != nullptr) {
            for (size_t i = 0; i < matchEFilter.size(); i++) {
                buffer[i] = matchEFilter[i];
            }
            filterNames->nameList = buffer;
        }
    }
    sOHFilterNames.emplace_back(filterNames);

    return filterNames.get();
}

EFFECT_EXPORT
void OH_EffectFilter_ReleaseFilterNames()
{
    std::unique_lock<std::mutex> lock(filterMutex_);
    EFFECT_LOGI("Release filter names.");
    for (const auto &filterNames : sOHFilterNames) {
        if (filterNames == nullptr) {
            continue;
        }
        free(filterNames->nameList);
    }
    sOHFilterNames.clear();
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilter_LookupFilterInfo(const char *name, OH_EffectFilterInfo *info)
{
    CHECK_AND_RETURN_RET_LOG(name != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "LookupFilterInfo: input parameter name is null!");
    CHECK_AND_RETURN_RET_LOG(strlen(name) < MAX_CHAR_LEN, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "LookupFilterInfo: the length of input parameter name is too long! len = %{public}zu", strlen(name));
    CHECK_AND_RETURN_RET_LOG(info != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "LookupFilterInfo: input parameter info is null!");
    EFFECT_LOGD("Lookup filter info. name=%{public}s", name);

    std::shared_ptr<IFilterDelegate> filterDelegate = EFilterFactory::Instance()->GetDelegate(name);
    if (filterDelegate != nullptr) {
        auto effectInfo = static_cast<OH_EffectFilterInfo *>(filterDelegate->GetEffectInfo());
        CHECK_AND_RETURN_RET_LOG(effectInfo != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
            "LookupFilterInfo: filter delegate get effect info is null! name=%{public}s", name);
        *info = *effectInfo;
        return ImageEffect_ErrorCode::EFFECT_SUCCESS;
    }

    std::shared_ptr<EffectInfo> effectInfo = EFilterFactory::Instance()->GetEffectInfo(name);
    CHECK_AND_RETURN_RET_LOG(effectInfo != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "LookupFilterInfo: lookup fail! name=%{public}s", name);

    info->filterName = name;
    NativeCommonUtils::SwitchToOHEffectInfo(effectInfo.get(), info);
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilter_Render(OH_EffectFilter *filter, OH_PixelmapNative *inputPixelmap,
    OH_PixelmapNative *outputPixelmap)
{
    EFFECT_LOGI("Filter render.");
    CHECK_AND_RETURN_RET_LOG(filter != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterRender: input parameter filter is null!");
    if (filter->filter_->IsTextureInput()) {
        std::shared_ptr<EffectBuffer> tempEffectBuffer = nullptr;
        ErrorCode res = filter->filter_->Render(tempEffectBuffer, tempEffectBuffer);
        CHECK_AND_RETURN_RET_LOG(res == ErrorCode::SUCCESS, NativeCommonUtils::ConvertRenderResult(res),
            "FilterRender: filter render fail! errorCode:%{public}d", res);
        return ImageEffect_ErrorCode::EFFECT_SUCCESS;
    }
    CHECK_AND_RETURN_RET_LOG(inputPixelmap != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterRender: input parameter inputPixelmap is null!");
    CHECK_AND_RETURN_RET_LOG(outputPixelmap != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterRender: input parameter outputPixelmap is null!");

    PixelMap *input = NativeCommonUtils::GetPixelMapFromOHPixelmap(inputPixelmap);
    PixelMap *output = NativeCommonUtils::GetPixelMapFromOHPixelmap(outputPixelmap);

    std::shared_ptr<EffectBuffer> inEffectBuffer = nullptr;
    ErrorCode result = CommonUtils::LockPixelMap(input, inEffectBuffer);
    if (result != ErrorCode::SUCCESS || inEffectBuffer == nullptr) {
        EFFECT_LOGE("FilterRender: lock input native pixelMap error! errorCode:%{public}d", result);
        CommonUtils::UnlockPixelMap(input);
        return ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID;
    }

    std::shared_ptr<EffectBuffer> outEffectBuffer = nullptr;
    result = CommonUtils::LockPixelMap(output, outEffectBuffer);
    if (result != ErrorCode::SUCCESS || outEffectBuffer == nullptr) {
        EFFECT_LOGE("FilterRender: lock output native pixelMap error! errorCode:%{public}d", result);
        CommonUtils::UnlockPixelMap(output);
        return ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID;
    }

    filter->filter_->PreRender(inEffectBuffer->bufferInfo_->formatType_);
    result = filter->filter_->Render(inEffectBuffer, outEffectBuffer);
    CommonUtils::UnlockPixelMap(input);
    CommonUtils::UnlockPixelMap(output);
    CHECK_AND_RETURN_RET_LOG(result == ErrorCode::SUCCESS, NativeCommonUtils::ConvertRenderResult(result),
        "FilterRender: filter render fail! errorCode:%{public}d", result);

    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilter_RenderWithTextureId(OH_EffectFilter *filter, int32_t inputTextureId,
    int32_t outputTextureId, int32_t colorSpace)
{
    EFFECT_LOGI("Filter render with textureId.");
    CHECK_AND_RETURN_RET_LOG(filter != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterRender: input parameter filter is null!");
    CHECK_AND_RETURN_RET_LOG(inputTextureId > 0, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterRender: input parameter inputTextureId is invalid!");
    CHECK_AND_RETURN_RET_LOG(outputTextureId > 0, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterRender: input parameter outputTextureId is invalid!");
    CHECK_AND_RETURN_RET_LOG(colorSpace > 0, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterRender: input parameter outputTextureId is invalid!");
    std::shared_ptr<EffectBuffer> inEffectBuffer = nullptr;
    ErrorCode result = CommonUtils::ParseTex(inputTextureId, colorSpace, inEffectBuffer);
    if (result != ErrorCode::SUCCESS || inEffectBuffer == nullptr) {
        EFFECT_LOGE("FilterRender: parse input tex error! errorCode:%{public}d", result);
        return ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID;
    }
    std::shared_ptr<EffectBuffer> outEffectBuffer = nullptr;
    result = CommonUtils::ParseTex(outputTextureId, colorSpace, outEffectBuffer);
    if (result != ErrorCode::SUCCESS || outEffectBuffer == nullptr) {
        EFFECT_LOGE("FilterRender: parse output tex error! errorCode:%{public}d", result);
        return ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID;
    }
    result = filter->filter_->Render(inEffectBuffer, outEffectBuffer);
    CHECK_AND_RETURN_RET_LOG(result == ErrorCode::SUCCESS, NativeCommonUtils::ConvertRenderResult(result),
        "FilterRender: filter render fail! errorCode:%{public}d", result);
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}

EFFECT_EXPORT
ImageEffect_ErrorCode OH_EffectFilter_Release(OH_EffectFilter *filter)
{
    EFFECT_LOGI("Effect filter release.");
    CHECK_AND_RETURN_RET_LOG(filter != nullptr, ImageEffect_ErrorCode::EFFECT_ERROR_PARAM_INVALID,
        "FilterRelease: input parameter imageEffect is null!");
    delete (filter);
    return ImageEffect_ErrorCode::EFFECT_SUCCESS;
}
#ifdef __cplusplus
}
#endif