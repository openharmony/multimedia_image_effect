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

#ifndef IMAGE_EFFECT_COLORSPACE_MANAGER_H
#define IMAGE_EFFECT_COLORSPACE_MANAGER_H

#include <memory>

#include "colorspace_strategy.h"

namespace OHOS {
namespace Media {
namespace Effect {
class ColorSpaceManager {
public:
    ColorSpaceManager();
    ~ColorSpaceManager() = default;

    static bool IsNeedConvertColorSpace(EffectColorSpace colorSpace);
    static bool IsSupportedColorSpace(EffectColorSpace colorSpace);
    static std::unordered_set<EffectColorSpace> GetAllSupportedColorSpaces();

    void Init(std::shared_ptr<EffectBuffer> &src, std::shared_ptr<EffectBuffer> &dst);
    ErrorCode ApplyColorSpace(EffectBuffer *effectBuffer, const EffectColorSpace &colorSpace,
        EffectColorSpace &outputColorSpace);
    ErrorCode ChooseColorSpace(const std::unordered_set<EffectColorSpace> &filtersSupportedColorSpace,
        const EffectColorSpace &srcRealColorSpace, EffectColorSpace &outputColorSpace);
    void Deinit();
private:
    std::shared_ptr<ColorSpaceStrategy> strategy_;
};
} // namespace Effect
} // namespace Media
} // namespace OHOS
#endif // IMAGE_EFFECT_COLORSPACE_MANAGER_H
