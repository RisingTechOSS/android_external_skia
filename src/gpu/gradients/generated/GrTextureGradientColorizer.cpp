/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**************************************************************************************************
 *** This file was autogenerated from GrTextureGradientColorizer.fp; do not modify.
 **************************************************************************************************/
#include "GrTextureGradientColorizer.h"

#include "src/core/SkUtils.h"
#include "src/gpu/GrTexture.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramBuilder.h"
#include "src/sksl/SkSLCPP.h"
#include "src/sksl/SkSLUtil.h"
class GrGLSLTextureGradientColorizer : public GrGLSLFragmentProcessor {
public:
    GrGLSLTextureGradientColorizer() {}
    void emitCode(EmitArgs& args) override {
        GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
        const GrTextureGradientColorizer& _outer = args.fFp.cast<GrTextureGradientColorizer>();
        (void)_outer;
        SkString _sample295 = this->invokeChild(0, args);
        fragBuilder->codeAppendf(
                R"SkSL(%s = %s;
)SkSL",
                args.fOutputColor, _sample295.c_str());
    }

private:
    void onSetData(const GrGLSLProgramDataManager& pdman,
                   const GrFragmentProcessor& _proc) override {}
};
GrGLSLFragmentProcessor* GrTextureGradientColorizer::onCreateGLSLInstance() const {
    return new GrGLSLTextureGradientColorizer();
}
void GrTextureGradientColorizer::onGetGLSLProcessorKey(const GrShaderCaps& caps,
                                                       GrProcessorKeyBuilder* b) const {}
bool GrTextureGradientColorizer::onIsEqual(const GrFragmentProcessor& other) const {
    const GrTextureGradientColorizer& that = other.cast<GrTextureGradientColorizer>();
    (void)that;
    return true;
}
GrTextureGradientColorizer::GrTextureGradientColorizer(const GrTextureGradientColorizer& src)
        : INHERITED(kGrTextureGradientColorizer_ClassID, src.optimizationFlags()) {
    this->cloneAndRegisterAllChildProcessors(src);
}
std::unique_ptr<GrFragmentProcessor> GrTextureGradientColorizer::clone() const {
    return std::unique_ptr<GrFragmentProcessor>(new GrTextureGradientColorizer(*this));
}
