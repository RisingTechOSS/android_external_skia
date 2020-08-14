/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**************************************************************************************************
 *** This file was autogenerated from GrDeviceSpaceEffect.fp; do not modify.
 **************************************************************************************************/
#ifndef GrDeviceSpaceEffect_DEFINED
#define GrDeviceSpaceEffect_DEFINED

#include "include/core/SkM44.h"
#include "include/core/SkTypes.h"

#include "src/gpu/GrFragmentProcessor.h"

class GrDeviceSpaceEffect : public GrFragmentProcessor {
public:
    SkPMColor4f constantOutputForConstantInput(const SkPMColor4f& inColor) const override {
        return ConstantOutputForConstantInput(this->childProcessor(0), inColor);
    }

    static std::unique_ptr<GrFragmentProcessor> Make(std::unique_ptr<GrFragmentProcessor> fp) {
        return std::unique_ptr<GrFragmentProcessor>(new GrDeviceSpaceEffect(std::move(fp)));
    }
    GrDeviceSpaceEffect(const GrDeviceSpaceEffect& src);
    std::unique_ptr<GrFragmentProcessor> clone() const override;
    const char* name() const override { return "DeviceSpaceEffect"; }

private:
    GrDeviceSpaceEffect(std::unique_ptr<GrFragmentProcessor> fp)
            : INHERITED(kGrDeviceSpaceEffect_ClassID,
                        (OptimizationFlags)ProcessorOptimizationFlags(fp.get())) {
        SkASSERT(fp);
        this->registerChild(std::move(fp), SkSL::SampleUsage::Explicit());
    }
    GrGLSLFragmentProcessor* onCreateGLSLInstance() const override;
    void onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder*) const override;
    bool onIsEqual(const GrFragmentProcessor&) const override;
#if GR_TEST_UTILS
    SkString onDumpInfo() const override;
#endif
    GR_DECLARE_FRAGMENT_PROCESSOR_TEST
    typedef GrFragmentProcessor INHERITED;
};
#endif
