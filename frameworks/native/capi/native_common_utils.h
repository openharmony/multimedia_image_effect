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

#ifndef IMAGE_EFFECT_NATIVE_COMMON_UTILS_H
#define IMAGE_EFFECT_NATIVE_COMMON_UTILS_H

#include <vector>

#include "any.h"
#include "effect_info.h"
#include "error_code.h"
#include "image_effect_filter.h"
#include "image_type.h"
#include "pixel_map_napi.h"
#include "image_effect_marco_define.h"

#define EFFECT_EXPORT __attribute__((visibility("default")))

namespace OHOS {
namespace Media {
namespace Effect {
class NativeCommonUtils {
public:
    IMAGE_EFFECT_EXPORT static ErrorCode ParseOHAny(const ImageEffect_Any *value, Plugin::Any &any);

    IMAGE_EFFECT_EXPORT static ErrorCode SwitchToOHAny(const Plugin::Any &any, ImageEffect_Any *value);

    static void SwitchToOHFormatType(const IEffectFormat &formatType, ImageEffect_Format &ohFormatType);

    static void SwitchToFormatType(const ImageEffect_Format &ohFormatType, IEffectFormat &formatType);

    static void SwitchToOHEffectInfo(const EffectInfo *effectInfo, OH_EffectFilterInfo *ohFilterInfo);

    IMAGE_EFFECT_EXPORT static PixelMap *GetPixelMapFromOHPixelmap(OH_PixelmapNative *pixelmapNative);

    static void ParseLookupKey(std::string &key, std::vector<const char *> &matchEFilter);

    static void SwitchToEffectInfo(const OH_EffectFilterInfo *info, const std::shared_ptr<EffectInfo> &effectInfo);

    static uint32_t GetSupportedFormats(const OH_EffectFilterInfo *ohFilterInfo);

    static void ReportEventStartFailed(ImageEffect_ErrorCode errorCode, const char *errorMsg);

    static ImageEffect_ErrorCode ConvertStartResult(ErrorCode errorCode);
};
} // namespace Effect
} // namespace Media
} // namespace OHOS
#endif // IMAGE_EFFECT_NATIVE_COMMON_UTILS_H