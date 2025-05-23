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

#ifndef IMAGE_EFFECT_COLORSPACE_STRATEGY_H
#define IMAGE_EFFECT_COLORSPACE_STRATEGY_H

#include <unordered_set>

#include "effect_buffer.h"
#include "error_code.h"
#include "image_effect_marco_define.h"

namespace OHOS {
namespace Media {
namespace Effect {

class ColorSpaceStrategy {
public:
    ColorSpaceStrategy() = default;
    ~ColorSpaceStrategy() = default;

    IMAGE_EFFECT_EXPORT static bool IsSupportedColorSpace(EffectColorSpace colorSpace);
    static bool IsNeedConvertColorSpace(EffectColorSpace colorSpace);
    static EffectColorSpace GetTargetColorSpace(EffectColorSpace src);
    static std::unordered_set<EffectColorSpace> GetAllSupportedColorSpaces();

    void Init(const std::shared_ptr<EffectBuffer> &src, const std::shared_ptr<EffectBuffer> &dst);

    IMAGE_EFFECT_EXPORT
    ErrorCode ChooseColorSpace(const std::unordered_set<EffectColorSpace> &filtersSupportedColorSpace,
        const EffectColorSpace &srcRealColorSpace, EffectColorSpace &outputColorSpace);

    IMAGE_EFFECT_EXPORT ErrorCode CheckConverterColorSpace(const EffectColorSpace &targetColorSpace);

    void Deinit();
private:
    std::shared_ptr<EffectBuffer> src_ = nullptr;
    std::shared_ptr<EffectBuffer> dst_ = nullptr;
};
} // namespace Effect
} // namespace Media
} // namespace OHOS
#endif // IMAGE_EFFECT_COLORSPACE_STRATEGY_H
