/*
 * Copyright (C) 2023-2024 Huawei Device Co., Ltd.
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

#ifndef IMAGE_EFFECT_GPU_BRIGHTNESS_ALGO_H
#define IMAGE_EFFECT_GPU_BRIGHTNESS_ALGO_H

#include "error_code.h"
#include "effect_buffer.h"
#include "any.h"

namespace OHOS {
namespace Media {
namespace Effect {
class GpuBrightnessAlgo {
public:
    static ErrorCode OnApplyRGBA8888(EffectBuffer *src, EffectBuffer *dst, std::map<std::string, Plugin::Any> &value);

private:
    static ErrorCode InitMesh();
    static ErrorCode PreDraw(uint32_t width, uint32_t height, std::map<std::string, Plugin::Any> &value);
    static ErrorCode Release();
    static void Unbind();
    static float ParseBrightness(std::map<std::string, Plugin::Any> &value);

    static unsigned int vbo_;
    static unsigned int vao_;
    static unsigned int fbo_;
    static unsigned int shaderProgram_;
    static unsigned int texId_;
};
} // namespace Effect
} // namespace Media
} // namespace OHOS

#endif // IMAGE_EFFECT_GPU_BRIGHTNESS_ALGO_H