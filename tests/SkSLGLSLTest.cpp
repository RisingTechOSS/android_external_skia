/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/SkSLCompiler.h"

#include "tests/Test.h"

// Note that the optimizer will aggressively kill dead code and substitute constants in place of
// variables, so we have to jump through a few hoops to ensure that the code in these tests has the
// necessary side-effects to remain live. In some cases we rely on the optimizer not (yet) being
// smart enough to optimize around certain constructs; as the optimizer gets smarter it will
// undoubtedly end up breaking some of these tests. That is a good thing, as long as the new code is
// equivalent!

static void test(skiatest::Reporter* r, const char* src, const SkSL::Program::Settings& settings,
                 const char* expected, SkSL::Program::Inputs* inputs,
                 SkSL::Program::Kind kind = SkSL::Program::kFragment_Kind) {
    SkSL::Compiler compiler;
    SkSL::String output;
    std::unique_ptr<SkSL::Program> program = compiler.convertProgram(kind, SkSL::String(src),
                                                                     settings);
    if (!program) {
        SkDebugf("Unexpected error compiling %s\n%s", src, compiler.errorText().c_str());
    }
    REPORTER_ASSERT(r, program);
    if (program) {
        *inputs = program->fInputs;
        REPORTER_ASSERT(r, compiler.toGLSL(*program, &output));
        if (program) {
            SkSL::String skExpected(expected);
            if (output != skExpected) {
                SkDebugf("GLSL MISMATCH:\nsource:\n%s\n\nexpected:\n'%s'\n\nreceived:\n'%s'", src,
                         expected, output.c_str());
            }
            REPORTER_ASSERT(r, output == skExpected);
        }
    }
}

static void test(skiatest::Reporter* r, const char* src, const GrShaderCaps& caps,
                 const char* expected, SkSL::Program::Kind kind = SkSL::Program::kFragment_Kind) {
    SkSL::Program::Settings settings;
    settings.fCaps = &caps;
    SkSL::Program::Inputs inputs;
    test(r, src, settings, expected, &inputs, kind);
}

DEF_TEST(SkSLVersion, r) {
    test(r,
         "in float test; void main() { sk_FragColor.r = half(test); }",
         *SkSL::ShaderCapsFactory::Version450Core(),
         "#version 450 core\n"
         "out vec4 sk_FragColor;\n"
         "in float test;\n"
         "void main() {\n"
         "    sk_FragColor.x = test;\n"
         "}\n");
    test(r,
         "in float test; void main() { sk_FragColor.r = half(test); }",
         *SkSL::ShaderCapsFactory::Version110(),
         "#version 110\n"
         "varying float test;\n"
         "void main() {\n"
         "    gl_FragColor.x = test;\n"
         "}\n");
}

DEF_TEST(SkSLUsesPrecisionModifiers, r) {
    test(r,
         "void main() { half x = 0.75; float y = 1; x++; y++;"
         "sk_FragColor.rg = half2(x, half(y)); }",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    float x = 0.75;\n"
         "    float y = 1.0;\n"
         "    x++;\n"
         "    y++;\n"
         "    sk_FragColor.xy = vec2(x, y);\n"
         "}\n");
    test(r,
         "void main() { half x = 0.75; float y = 1; x++; y++;"
         "sk_FragColor.rg = half2(x, half(y)); }",
         *SkSL::ShaderCapsFactory::UsesPrecisionModifiers(),
         "#version 400\n"
         "precision mediump float;\n"
         "precision mediump sampler2D;\n"
         "out mediump vec4 sk_FragColor;\n"
         "void main() {\n"
         "    mediump float x = 0.75;\n"
         "    highp float y = 1.0;\n"
         "    x++;\n"
         "    y++;\n"
         "    sk_FragColor.xy = vec2(x, y);\n"
         "}\n");
}

DEF_TEST(SkSLMinAbs, r) {
    test(r,
         "void main() {"
         "half x = -5;"
         "sk_FragColor.r = min(abs(x), 6);"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor.x = min(abs(-5.0), 6.0);\n"
         "}\n");

    test(r,
         "void main() {"
         "half x = -5.0;"
         "sk_FragColor.r = min(abs(x), 6.0);"
         "}",
         *SkSL::ShaderCapsFactory::CannotUseMinAndAbsTogether(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    float minAbsHackVar0;\n"
         "    float minAbsHackVar1;\n"
         "    sk_FragColor.x = ((minAbsHackVar0 = abs(-5.0)) < (minAbsHackVar1 = 6.0) ? "
                                                               "minAbsHackVar0 : minAbsHackVar1);\n"
         "}\n");
}

DEF_TEST(SkSLFractNegative, r) {
    static constexpr char input[] =
        "void main() {"
        "float x = -42.0;"
        "sk_FragColor.r = half(fract(x));"
        "}";
    static constexpr char output_default[] =
        "#version 400\n"
        "out vec4 sk_FragColor;\n"
        "void main() {\n"
        "    sk_FragColor.x = fract(-42.0);\n"
        "}\n";
    static constexpr char output_workaround[] =
        "#version 400\n"
        "out vec4 sk_FragColor;\n"
        "void main() {\n"
        "    sk_FragColor.x = (0.5 - sign(-42.0) * (0.5 - fract(abs(-42.0))));\n"
        "}\n";

    test(r, input, *SkSL::ShaderCapsFactory::Default(), output_default);
    test(r, input, *SkSL::ShaderCapsFactory::CannotUseFractForNegativeValues(), output_workaround);
}

DEF_TEST(SkSLNegatedAtan, r) {
    test(r,
         "void main() { float2 x = float2(sqrt(2)); sk_FragColor.r = half(atan(x.x, -x.y)); }",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    vec2 x = vec2(sqrt(2.0));\n"
         "    sk_FragColor.x = atan(x.x, -x.y);\n"
         "}\n");
    test(r,
         "void main() { float2 x = float2(sqrt(2)); sk_FragColor.r = half(atan(x.x, -x.y)); }",
         *SkSL::ShaderCapsFactory::MustForceNegatedAtanParamToFloat(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    vec2 x = vec2(sqrt(2.0));\n"
         "    sk_FragColor.x = atan(x.x, -1.0 * x.y);\n"
         "}\n");
}

DEF_TEST(SkSLModifiersDeclaration, r) {
    test(r,
         "layout(blend_support_all_equations) out;"
         "layout(blend_support_all_equations) out;"
         "layout(blend_support_multiply) out;"
         "layout(blend_support_screen) out;"
         "layout(blend_support_overlay) out;"
         "layout(blend_support_darken) out;"
         "layout(blend_support_lighten) out;"
         "layout(blend_support_colordodge) out;"
         "layout(blend_support_colorburn) out;"
         "layout(blend_support_hardlight) out;"
         "layout(blend_support_softlight) out;"
         "layout(blend_support_difference) out;"
         "layout(blend_support_exclusion) out;"
         "layout(blend_support_hsl_hue) out;"
         "layout(blend_support_hsl_saturation) out;"
         "layout(blend_support_hsl_color) out;"
         "layout(blend_support_hsl_luminosity) out;"
         "void main() { }",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "layout (blend_support_all_equations) out ;\n"
         "layout (blend_support_all_equations) out ;\n"
         "layout (blend_support_multiply) out ;\n"
         "layout (blend_support_screen) out ;\n"
         "layout (blend_support_overlay) out ;\n"
         "layout (blend_support_darken) out ;\n"
         "layout (blend_support_lighten) out ;\n"
         "layout (blend_support_colordodge) out ;\n"
         "layout (blend_support_colorburn) out ;\n"
         "layout (blend_support_hardlight) out ;\n"
         "layout (blend_support_softlight) out ;\n"
         "layout (blend_support_difference) out ;\n"
         "layout (blend_support_exclusion) out ;\n"
         "layout (blend_support_hsl_hue) out ;\n"
         "layout (blend_support_hsl_saturation) out ;\n"
         "layout (blend_support_hsl_color) out ;\n"
         "layout (blend_support_hsl_luminosity) out ;\n"
         "void main() {\n"
         "}\n");
}

DEF_TEST(SkSLDerivatives, r) {
    test(r,
         "void main() { sk_FragColor.r = half(dFdx(1)); }",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor.x = dFdx(1.0);\n"
         "}\n");
    test(r,
         "void main() { sk_FragColor.r = 1; }",
         *SkSL::ShaderCapsFactory::ShaderDerivativeExtensionString(),
         "#version 400\n"
         "precision mediump float;\n"
         "precision mediump sampler2D;\n"
         "out mediump vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor.x = 1.0;\n"
         "}\n");
    test(r,
         "void main() { sk_FragColor.r = half(dFdx(1)); }",
         *SkSL::ShaderCapsFactory::ShaderDerivativeExtensionString(),
         "#version 400\n"
         "#extension GL_OES_standard_derivatives : require\n"
         "precision mediump float;\n"
         "precision mediump sampler2D;\n"
         "out mediump vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor.x = dFdx(1.0);\n"
         "}\n");

    SkSL::Program::Settings settings;
    settings.fFlipY = false;
    auto caps = SkSL::ShaderCapsFactory::Default();
    settings.fCaps = caps.get();
    SkSL::Program::Inputs inputs;
    test(r,
         "void main() { sk_FragColor.r = half(dFdx(1)), sk_FragColor.g = half(dFdy(1)); }",
         settings,
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    (sk_FragColor.x = dFdx(1.0) , sk_FragColor.y = dFdy(1.0));\n"
         "}\n",
         &inputs);
    settings.fFlipY = true;
    test(r,
         "void main() { sk_FragColor.r = half(dFdx(1)), sk_FragColor.g = half(dFdy(1)); }",
         settings,
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    (sk_FragColor.x = dFdx(1.0) , sk_FragColor.y = -dFdy(1.0));\n"
         "}\n",
         &inputs);
}

DEF_TEST(SkSLCaps, r) {
    test(r,
         "void main() {"
         "int x = 0;"
         "int y = 0;"
         "int z = 0;"
         "if (sk_Caps.externalTextureSupport) x = 1;"
         "if (sk_Caps.fbFetchSupport) y = 1;"
         "if (sk_Caps.canUseAnyFunctionInShader) z = 1;"
         "sk_FragColor = half4(x, y, z, 0.0);"
         "}",
         *SkSL::ShaderCapsFactory::VariousCaps(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor = vec4(1.0, 0.0, 0.0, 0.0);\n"
         "}\n");
}

DEF_TEST(SkSLTexture, r) {
    test(r,
         "uniform sampler1D one;"
         "uniform sampler2D two;"
         "void main() {"
         "float4 a = sample(one, 0);"
         "float4 b = sample(two, float2(0));"
         "float4 c = sample(one, float2(0));"
         "float4 d = sample(two, float3(0));"
         "sk_FragColor = half4(half(a.x), half(b.x), half(c.x), half(d.x));"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "uniform sampler1D one;\n"
         "uniform sampler2D two;\n"
         "void main() {\n"
         "    vec4 a = texture(one, 0.0);\n"
         "    vec4 b = texture(two, vec2(0.0));\n"
         "    vec4 c = textureProj(one, vec2(0.0));\n"
         "    vec4 d = textureProj(two, vec3(0.0));\n"
         "    sk_FragColor = vec4(a.x, b.x, c.x, d.x);\n"
         "}\n");
    test(r,
         "uniform sampler1D one;"
         "uniform sampler2D two;"
         "void main() {"
         "float4 a = sample(one, 0);"
         "float4 b = sample(two, float2(0));"
         "float4 c = sample(one, float2(0));"
         "float4 d = sample(two, float3(0));"
         "sk_FragColor = half4(half(a.x), half(b.x), half(c.x), half(d.x));"
         "}",
         *SkSL::ShaderCapsFactory::Version110(),
         "#version 110\n"
         "uniform sampler1D one;\n"
         "uniform sampler2D two;\n"
         "void main() {\n"
         "    vec4 a = texture1D(one, 0.0);\n"
         "    vec4 b = texture2D(two, vec2(0.0));\n"
         "    vec4 c = texture1DProj(one, vec2(0.0));\n"
         "    vec4 d = texture2DProj(two, vec3(0.0));\n"
         "    gl_FragColor = vec4(a.x, b.x, c.x, d.x);\n"
         "}\n");
}

DEF_TEST(SkSLSharpen, r) {
    SkSL::Program::Settings settings;
    settings.fSharpenTextures = true;
    sk_sp<GrShaderCaps> caps = SkSL::ShaderCapsFactory::Default();
    settings.fCaps = caps.get();
    SkSL::Program::Inputs inputs;
    test(r,
         "uniform sampler1D one;"
         "uniform sampler2D two;"
         "void main() {"
         "float4 a = sample(one, 0);"
         "float4 b = sample(two, float2(0));"
         "float4 c = sample(one, float2(0));"
         "float4 d = sample(two, float3(0));"
         "sk_FragColor = half4(half(a.x), half(b.x), half(c.x), half(d.x));"
         "}",
         settings,
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "uniform sampler1D one;\n"
         "uniform sampler2D two;\n"
         "void main() {\n"
         "    vec4 a = texture(one, 0.0, -0.5);\n"
         "    vec4 b = texture(two, vec2(0.0), -0.5);\n"
         "    vec4 c = textureProj(one, vec2(0.0), -0.5);\n"
         "    vec4 d = textureProj(two, vec3(0.0), -0.5);\n"
         "    sk_FragColor = vec4(a.x, b.x, c.x, d.x);\n"
         "}\n",
         &inputs);

    caps = SkSL::ShaderCapsFactory::Version110();
    settings.fCaps = caps.get();
    test(r,
         "uniform sampler1D one;"
         "uniform sampler2D two;"
         "void main() {"
         "float4 a = sample(one, 0);"
         "float4 b = sample(two, float2(0));"
         "float4 c = sample(one, float2(0));"
         "float4 d = sample(two, float3(0));"
         "sk_FragColor = half4(half(a.x), half(b.x), half(c.x), half(d.x));"
         "}",
         settings,
         "#version 110\n"
         "uniform sampler1D one;\n"
         "uniform sampler2D two;\n"
         "void main() {\n"
         "    vec4 a = texture1D(one, 0.0, -0.5);\n"
         "    vec4 b = texture2D(two, vec2(0.0), -0.5);\n"
         "    vec4 c = texture1DProj(one, vec2(0.0), -0.5);\n"
         "    vec4 d = texture2DProj(two, vec3(0.0), -0.5);\n"
         "    gl_FragColor = vec4(a.x, b.x, c.x, d.x);\n"
         "}\n",
         &inputs);
}

DEF_TEST(SkSLFragCoord, r) {
    SkSL::Program::Settings settings;
    settings.fFlipY = true;
    sk_sp<GrShaderCaps> caps = SkSL::ShaderCapsFactory::FragCoordsOld();
    settings.fCaps = caps.get();
    SkSL::Program::Inputs inputs;
    test(r,
         "void main() { sk_FragColor.xy = half2(sk_FragCoord.xy); }",
         settings,
         "#version 110\n"
         "#extension GL_ARB_fragment_coord_conventions : require\n"
         "layout(origin_upper_left) in vec4 gl_FragCoord;\n"
         "void main() {\n"
         "    gl_FragColor.xy = gl_FragCoord.xy;\n"
         "}\n",
         &inputs);
    REPORTER_ASSERT(r, !inputs.fRTHeight);

    caps = SkSL::ShaderCapsFactory::FragCoordsNew();
    settings.fCaps = caps.get();
    test(r,
         "void main() { sk_FragColor.xy = half2(sk_FragCoord.xy); }",
         settings,
         "#version 400\n"
         "layout(origin_upper_left) in vec4 gl_FragCoord;\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor.xy = gl_FragCoord.xy;\n"
         "}\n",
         &inputs);
    REPORTER_ASSERT(r, !inputs.fRTHeight);

    caps = SkSL::ShaderCapsFactory::Default();
    settings.fCaps = caps.get();
    test(r,
         "void main() { sk_FragColor.xy = half2(sk_FragCoord.xy); }",
         settings,
         "#version 400\n"
         "uniform float u_skRTHeight;\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    vec4 sk_FragCoord = vec4(gl_FragCoord.x, u_skRTHeight - gl_FragCoord.y, "
                 "gl_FragCoord.z, gl_FragCoord.w);\n"
         "    sk_FragColor.xy = sk_FragCoord.xy;\n"
         "}\n",
         &inputs);
    REPORTER_ASSERT(r, inputs.fRTHeight);

    settings.fFlipY = false;
    test(r,
         "void main() { sk_FragColor.xy = half2(sk_FragCoord.xy); }",
         settings,
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor.xy = gl_FragCoord.xy;\n"
         "}\n",
         &inputs);
    REPORTER_ASSERT(r, !inputs.fRTHeight);

    test(r,
         "in float4 pos; void main() { sk_Position = pos; }",
         *SkSL::ShaderCapsFactory::CannotUseFragCoord(),
         "#version 400\n"
         "out vec4 sk_FragCoord_Workaround;\n"
         "in vec4 pos;\n"
         "void main() {\n"
         "    sk_FragCoord_Workaround = (gl_Position = pos);\n"
         "}\n",
         SkSL::Program::kVertex_Kind);

    test(r,
         "uniform float4 sk_RTAdjust; in float4 pos; void main() { sk_Position = pos; }",
         *SkSL::ShaderCapsFactory::CannotUseFragCoord(),
         "#version 400\n"
         "out vec4 sk_FragCoord_Workaround;\n"
         "uniform vec4 sk_RTAdjust;\n"
         "in vec4 pos;\n"
         "void main() {\n"
         "    sk_FragCoord_Workaround = (gl_Position = pos);\n"
         "    gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * sk_RTAdjust.yw,"
                                " 0.0, gl_Position.w);\n"
         "}\n",
         SkSL::Program::kVertex_Kind);

    test(r,
         "void main() { sk_FragColor.xy = half2(sk_FragCoord.xy); }",
         *SkSL::ShaderCapsFactory::CannotUseFragCoord(),
         "#version 400\n"
         "in vec4 sk_FragCoord_Workaround;\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    float sk_FragCoord_InvW = 1. / sk_FragCoord_Workaround.w;\n"
         "    vec4 sk_FragCoord_Resolved = vec4(sk_FragCoord_Workaround.xyz * "
              "sk_FragCoord_InvW, sk_FragCoord_InvW);\n"
         "    sk_FragCoord_Resolved.xy = floor(sk_FragCoord_Resolved.xy) + vec2(.5);\n"
         "    sk_FragColor.xy = sk_FragCoord_Resolved.xy;\n"
         "}\n");
}

DEF_TEST(SkSLWidthAndHeight, r) {
    SkSL::Program::Settings settings;
    sk_sp<GrShaderCaps> caps = SkSL::ShaderCapsFactory::Default();
    settings.fCaps = caps.get();
    SkSL::Program::Inputs inputs;
    test(r,
         "void main() { sk_FragColor.r = half(sk_FragCoord.x / sk_Width); }",
         settings,
         "#version 400\n"
         "uniform float u_skRTWidth;\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor.x = gl_FragCoord.x / u_skRTWidth;\n"
         "}\n",
         &inputs);
    REPORTER_ASSERT(r, inputs.fRTWidth);
    REPORTER_ASSERT(r, !inputs.fRTHeight);

    test(r,
         "void main() { sk_FragColor.r = half(sk_FragCoord.y / sk_Height); }",
         settings,
         "#version 400\n"
         "uniform float u_skRTHeight;\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor.x = gl_FragCoord.y / u_skRTHeight;\n"
         "}\n",
         &inputs);
    REPORTER_ASSERT(r, !inputs.fRTWidth);
    REPORTER_ASSERT(r, inputs.fRTHeight);
}

DEF_TEST(SkSLGeometry, r) {
    test(r,
         "layout(points) in;"
         "layout(invocations = 2) in;"
         "layout(line_strip, max_vertices = 2) out;"
         "void main() {"
         "sk_Position = sk_in[0].sk_Position + float4(-0.5, 0, 0, sk_InvocationID);"
         "EmitVertex();"
         "sk_Position = sk_in[0].sk_Position + float4(0.5, 0, 0, sk_InvocationID);"
         "EmitVertex();"
         "EndPrimitive();"
         "}",
         *SkSL::ShaderCapsFactory::GeometryShaderSupport(),
         "#version 400\n"
         "layout (points) in ;\n"
         "layout (invocations = 2) in ;\n"
         "layout (line_strip, max_vertices = 2) out ;\n"
         "void main() {\n"
         "    gl_Position = gl_in[0].gl_Position + vec4(-0.5, 0.0, 0.0, float(gl_InvocationID));\n"
         "    EmitVertex();\n"
         "    gl_Position = gl_in[0].gl_Position + vec4(0.5, 0.0, 0.0, float(gl_InvocationID));\n"
         "    EmitVertex();\n"
         "    EndPrimitive();\n"
         "}\n",
         SkSL::Program::kGeometry_Kind);
}

DEF_TEST(SkSLRectangleTexture, r) {
    test(r,
         "uniform sampler2D test;"
         "void main() {"
         "    sk_FragColor = sample(test, float2(0.5));"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "uniform sampler2D test;\n"
         "void main() {\n"
         "    sk_FragColor = texture(test, vec2(0.5));\n"
         "}\n");
    test(r,
         "uniform sampler2DRect test;"
         "void main() {"
         "    sk_FragColor = sample(test, float2(0.5));"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "uniform sampler2DRect test;\n"
         "void main() {\n"
         "    sk_FragColor = texture(test, vec2(0.5));\n"
         "}\n");
    test(r,
         "uniform sampler2DRect test;"
         "void main() {"
         "    sk_FragColor = sample(test, float3(0.5));"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "uniform sampler2DRect test;\n"
         "void main() {\n"
         "    sk_FragColor = texture(test, vec3(0.5));\n"
         "}\n");
}

DEF_TEST(SkSLGeometryShaders, r) {
    test(r,
         "layout(points) in;"
         "layout(invocations = 2) in;"
         "layout(line_strip, max_vertices = 2) out;"
         "void test() {"
         "sk_Position = sk_in[0].sk_Position + float4(0.5, 0, 0, sk_InvocationID);"
         "EmitVertex();"
         "}"
         "void main() {"
         "test();"
         "sk_Position = sk_in[0].sk_Position + float4(-0.5, 0, 0, sk_InvocationID);"
         "EmitVertex();"
         "}",
         *SkSL::ShaderCapsFactory::NoGSInvocationsSupport(),
         R"__GLSL__(#version 400
int sk_InvocationID;
layout (points) in ;
layout (line_strip, max_vertices = 4) out ;
void _invoke() {
    {
        gl_Position = gl_in[0].gl_Position + vec4(0.5, 0.0, 0.0, float(sk_InvocationID));
        EmitVertex();
    }


    gl_Position = gl_in[0].gl_Position + vec4(-0.5, 0.0, 0.0, float(sk_InvocationID));
    EmitVertex();
}
void main() {
    for (sk_InvocationID = 0;sk_InvocationID < 2; sk_InvocationID++) {
        _invoke();
        EndPrimitive();
    }
}
)__GLSL__",
         SkSL::Program::kGeometry_Kind);
    test(r,
         "layout(points, invocations = 2) in;"
         "layout(invocations = 3) in;"
         "layout(line_strip, max_vertices = 2) out;"
         "void main() {"
         "sk_Position = sk_in[0].sk_Position + float4(-0.5, 0, 0, sk_InvocationID);"
         "EmitVertex();"
         "EndPrimitive();"
         "}",
         *SkSL::ShaderCapsFactory::GSInvocationsExtensionString(),
         "#version 400\n"
         "#extension GL_ARB_gpu_shader5 : require\n"
         "layout (points, invocations = 2) in ;\n"
         "layout (invocations = 3) in ;\n"
         "layout (line_strip, max_vertices = 2) out ;\n"
         "void main() {\n"
         "    gl_Position = gl_in[0].gl_Position + vec4(-0.5, 0.0, 0.0, float(gl_InvocationID));\n"
         "    EmitVertex();\n"
         "    EndPrimitive();\n"
         "}\n",
         SkSL::Program::kGeometry_Kind);
    test(r,
         "layout(points, invocations = 2) in;"
         "layout(invocations = 3) in;"
         "layout(line_strip, max_vertices = 2) out;"
         "void main() {"
         "sk_Position = sk_in[0].sk_Position + float4(-0.5, 0, 0, sk_InvocationID);"
         "EmitVertex();"
         "EndPrimitive();"
         "}",
         *SkSL::ShaderCapsFactory::GeometryShaderExtensionString(),
         "#version 310es\n"
         "#extension GL_EXT_geometry_shader : require\n"
         "layout (points, invocations = 2) in ;\n"
         "layout (invocations = 3) in ;\n"
         "layout (line_strip, max_vertices = 2) out ;\n"
         "void main() {\n"
         "    gl_Position = gl_in[0].gl_Position + vec4(-0.5, 0.0, 0.0, float(gl_InvocationID));\n"
         "    EmitVertex();\n"
         "    EndPrimitive();\n"
         "}\n",
         SkSL::Program::kGeometry_Kind);
}

DEF_TEST(SkSLTypePrecision, r) {
    test(r,
         "float f = 1;"
         "half h = 2;"
         "float d = 3;"
         "float2 f2 = float2(1, 2);"
         "half3 h3 = half3(1, 2, 3);"
         "float4 d4 = float4(1, 2, 3, 4);"
         "float2x2 f22 = float2x2(1, 2, 3, 4);"
         "half2x4 h24 = half2x4(1, 2, 3, 4, 5, 6, 7, 8);"
         "float4x2 d42 = float4x2(1, 2, 3, 4, 5, 6, 7, 8);"
         "void main() {"
         "sk_FragColor.r = half(f + h + d + f2.x + h3.x + d4.x + f22[0][0] + h24[0][0] + "
                               "d42[0][0]);"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "float f = 1.0;\n"
         "float h = 2.0;\n"
         "float d = 3.0;\n"
         "vec2 f2 = vec2(1.0, 2.0);\n"
         "vec3 h3 = vec3(1.0, 2.0, 3.0);\n"
         "vec4 d4 = vec4(1.0, 2.0, 3.0, 4.0);\n"
         "mat2 f22 = mat2(1.0, 2.0, 3.0, 4.0);\n"
         "mat2x4 h24 = mat2x4(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);\n"
         "mat4x2 d42 = mat4x2(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);\n"
         "void main() {\n"
         "    sk_FragColor.x = (((((((f + h) + d) + f2.x) + h3.x) + d4.x) + "
                               "f22[0][0]) + h24[0][0]) + d42[0][0];\n"
         "}\n");
    test(r,
         "float f = 1;"
         "half h = 2;"
         "float2 f2 = float2(1, 2);"
         "half3 h3 = half3(1, 2, 3);"
         "float2x2 f22 = float2x2(1, 2, 3, 4);"
         "half2x4 h24 = half2x4(1, 2, 3, 4, 5, 6, 7, 8);"
         "void main() {"
         "sk_FragColor.r = half(f + h + f2.x + h3.x + f22[0][0] + h24[0][0]);"
         "}",
         *SkSL::ShaderCapsFactory::UsesPrecisionModifiers(),
         "#version 400\n"
         "precision mediump float;\n"
         "precision mediump sampler2D;\n"
         "out mediump vec4 sk_FragColor;\n"
         "highp float f = 1.0;\n"
         "mediump float h = 2.0;\n"
         "highp vec2 f2 = vec2(1.0, 2.0);\n"
         "mediump vec3 h3 = vec3(1.0, 2.0, 3.0);\n"
         "highp mat2 f22 = mat2(1.0, 2.0, 3.0, 4.0);\n"
         "mediump mat2x4 h24 = mat2x4(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);\n"
         "void main() {\n"
         "    sk_FragColor.x = ((((f + h) + f2.x) + h3.x) + f22[0][0]) + h24[0][0];\n"
         "}\n");
}

DEF_TEST(SkSLNumberConversions, r) {
    test(r,
         "short s = short(sqrt(1));"
         "int i = int(sqrt(1));"
         "ushort us = ushort(sqrt(1));"
         "uint ui = uint(sqrt(1));"
         "half h = half(sqrt(1));"
         "float f = sqrt(1);"
         "short s2s = s;"
         "short i2s = short(i);"
         "short us2s = short(us);"
         "short ui2s = short(ui);"
         "short h2s = short(h);"
         "short f2s = short(f);"
         "int s2i = s;"
         "int i2i = i;"
         "int us2i = int(us);"
         "int ui2i = int(ui);"
         "int h2i = int(h);"
         "int f2i = int(f);"
         "ushort s2us = ushort(s);"
         "ushort i2us = ushort(i);"
         "ushort us2us = us;"
         "ushort ui2us = ushort(ui);"
         "ushort h2us = ushort(h);"
         "ushort f2us = ushort(f);"
         "uint s2ui = uint(s);"
         "uint i2ui = uint(i);"
         "uint us2ui = us;"
         "uint ui2ui = ui;"
         "uint h2ui = uint(h);"
         "uint f2ui = uint(f);"
         "float s2f = s;"
         "float i2f = i;"
         "float us2f = us;"
         "float ui2f = ui;"
         "float h2f = h;"
         "float f2f = f;"
         "void main() {"
         "sk_FragColor.r = half(s + i + us + half(ui) + h + f + s2s + i2s + us2s + ui2s + h2s + "
                               "f2s + s2i + i2i + us2i + ui2i + h2i + f2i + s2us + i2us + us2us);"
         "sk_FragColor.r += half(ui2us + h2us + f2us + half(s2ui) + half(i2ui) + half(us2ui) + "
                                "half(ui2ui) + half(h2ui) + half(f2ui) + s2f + i2f + us2f + ui2f + "
                                "h2f + f2f);"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "int s = int(sqrt(1.0));\n"
         "int i = int(sqrt(1.0));\n"
         "uint us = uint(sqrt(1.0));\n"
         "uint ui = uint(sqrt(1.0));\n"
         "float h = sqrt(1.0);\n"
         "float f = sqrt(1.0);\n"
         "int s2s = s;\n"
         "int i2s = i;\n"
         "int us2s = int(us);\n"
         "int ui2s = int(ui);\n"
         "int h2s = int(h);\n"
         "int f2s = int(f);\n"
         "int s2i = s;\n"
         "int i2i = i;\n"
         "int us2i = int(us);\n"
         "int ui2i = int(ui);\n"
         "int h2i = int(h);\n"
         "int f2i = int(f);\n"
         "uint s2us = uint(s);\n"
         "uint i2us = uint(i);\n"
         "uint us2us = us;\n"
         "uint ui2us = ui;\n"
         "uint h2us = uint(h);\n"
         "uint f2us = uint(f);\n"
         "uint s2ui = uint(s);\n"
         "uint i2ui = uint(i);\n"
         "uint us2ui = us;\n"
         "uint ui2ui = ui;\n"
         "uint h2ui = uint(h);\n"
         "uint f2ui = uint(f);\n"
         "float s2f = float(s);\n"
         "float i2f = float(i);\n"
         "float us2f = float(us);\n"
         "float ui2f = float(ui);\n"
         "float h2f = h;\n"
         "float f2f = f;\n"
         "void main() {\n"
         "    sk_FragColor.x = (((((((((((((((((float((s + i) + int(us)) + float(ui)) + h) + f) + "
         "float(s2s)) + float(i2s)) + float(us2s)) + float(ui2s)) + float(h2s)) + float(f2s)) + "
         "float(s2i)) + float(i2i)) + float(us2i)) + float(ui2i)) + float(h2i)) + float(f2i)) + "
         "float(s2us)) + float(i2us)) + float(us2us);\n"
         "    sk_FragColor.x += (((((((((((float((ui2us + h2us) + f2us) + float(s2ui)) + "
         "float(i2ui)) + float(us2ui)) + float(ui2ui)) + float(h2ui)) + float(f2ui)) + s2f) + "
         "i2f) + us2f) + ui2f) + h2f) + f2f;\n"
         "}\n");
}

DEF_TEST(SkSLForceHighPrecision, r) {
    test(r,
         "void main() {\n half x = half(sqrt(1));\n half4 y = half4(x);\n sk_FragColor = y;\n }",
         *SkSL::ShaderCapsFactory::UsesPrecisionModifiers(),
         "#version 400\n"
         "precision mediump float;\n"
         "precision mediump sampler2D;\n"
         "out mediump vec4 sk_FragColor;\n"
         "void main() {\n"
         "    mediump float x = sqrt(1.0);\n"
         "    mediump vec4 y = vec4(x);\n"
         "    sk_FragColor = y;\n"
         "}\n");
    SkSL::Program::Settings settings;
    settings.fForceHighPrecision = true;
    sk_sp<GrShaderCaps> caps = SkSL::ShaderCapsFactory::UsesPrecisionModifiers();
    settings.fCaps = caps.get();
    SkSL::Program::Inputs inputs;
    test(r,
         "void main() { half x = half(sqrt(1)); half4 y = half4(x); sk_FragColor = y; }",
         settings,
         "#version 400\n"
         "precision mediump float;\n"
         "precision mediump sampler2D;\n"
         "out mediump vec4 sk_FragColor;\n"
         "void main() {\n"
         "    highp float x = sqrt(1.0);\n"
         "    highp vec4 y = vec4(x);\n"
         "    sk_FragColor = y;\n"
         "}\n",
         &inputs);
}

DEF_TEST(SkSLNormalization, r) {
    test(r,
         "uniform float4 sk_RTAdjust; void main() { sk_Position = half4(1); }",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "uniform vec4 sk_RTAdjust;\n"
         "void main() {\n"
         "    gl_Position = vec4(1.0);\n"
         "    gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * "
                                "sk_RTAdjust.yw, 0.0, gl_Position.w);\n"
         "}\n",
         SkSL::Program::kVertex_Kind);
    test(r,
         "uniform float4 sk_RTAdjust;"
         "layout(points) in;"
         "layout(invocations = 2) in;"
         "layout(line_strip, max_vertices = 2) out;"
         "void main() {"
         "sk_Position = sk_in[0].sk_Position + float4(-0.5, 0, 0, sk_InvocationID);"
         "EmitVertex();"
         "sk_Position = sk_in[0].sk_Position + float4(0.5, 0, 0, sk_InvocationID);"
         "EmitVertex();"
         "EndPrimitive();"
         "}",
         *SkSL::ShaderCapsFactory::GeometryShaderSupport(),
         "#version 400\n"
         "uniform vec4 sk_RTAdjust;\n"
         "layout (points) in ;\n"
         "layout (invocations = 2) in ;\n"
         "layout (line_strip, max_vertices = 2) out ;\n"
         "void main() {\n"
         "    gl_Position = gl_in[0].gl_Position + vec4(-0.5, 0.0, 0.0, float(gl_InvocationID));\n"
         "    {\n"
         "        gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * "
                                    "sk_RTAdjust.yw, 0.0, gl_Position.w);\n"
         "        EmitVertex();\n"
         "    }\n"
         "    gl_Position = gl_in[0].gl_Position + vec4(0.5, 0.0, 0.0, float(gl_InvocationID));\n"
         "    {\n"
         "        gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * "
                                    "sk_RTAdjust.yw, 0.0, gl_Position.w);\n"
         "        EmitVertex();\n"
         "    }\n"
         "    EndPrimitive();\n"
         "}\n",
         SkSL::Program::kGeometry_Kind);
}

DEF_TEST(SkSLTernaryLValue, r) {
    test(r,
         "void main() { int r, g; (true ? r : g) = 1; (false ? r : g) = 0; "
         "sk_FragColor = half4(r, g, 1, 1); }",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor = vec4(1.0, 0.0, 1.0, 1.0);\n"
         "}\n");
    test(r,
         "void main() { half r, g; (true ? r : g) = half(sqrt(1)); (false ? r : g) = half(sqrt(0));"
         "sk_FragColor = half4(r, g, 1, 1); }",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    float r, g;\n"
         "    r = sqrt(1.0);\n"
         "    g = sqrt(0.0);\n"
         "    sk_FragColor = vec4(r, g, 1.0, 1.0);\n"
         "}\n");
    test(r,
         "void main() {"
         "half r, g;"
         "(sqrt(1) > 0 ? r : g) = half(sqrt(1));"
         "(sqrt(0) > 0 ? r : g) = half(sqrt(0));"
         "sk_FragColor = half4(r, g, 1, 1);"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    float r, g;\n"
         "    sqrt(1.0) > 0.0 ? r : g = sqrt(1.0);\n"
         "    sqrt(0.0) > 0.0 ? r : g = sqrt(0.0);\n"
         "    sk_FragColor = vec4(r, g, 1.0, 1.0);\n"
         "}\n");
}

DEF_TEST(SkSLIncompleteShortIntPrecision, r) {
    test(r,
         "uniform sampler2D tex;"
         "in float2 texcoord;"
         "in short2 offset;"
         "void main() {"
         "    short scalar = offset.y;"
         "    sk_FragColor = sample(tex, texcoord + float2(offset * scalar));"
         "}",
         *SkSL::ShaderCapsFactory::UsesPrecisionModifiers(),
         "#version 400\n"
         "precision mediump float;\n"
         "precision mediump sampler2D;\n"
         "out mediump vec4 sk_FragColor;\n"
         "uniform sampler2D tex;\n"
         "in highp vec2 texcoord;\n"
         "in mediump ivec2 offset;\n"
         "void main() {\n"
         "    mediump int scalar = offset.y;\n"
         "    sk_FragColor = texture(tex, texcoord + vec2(offset * scalar));\n"
         "}\n",
         SkSL::Program::kFragment_Kind);
    test(r,
         "uniform sampler2D tex;"
         "in float2 texcoord;"
         "in short2 offset;"
         "void main() {"
         "    short scalar = offset.y;"
         "    sk_FragColor = sample(tex, texcoord + float2(offset * scalar));"
         "}",
         *SkSL::ShaderCapsFactory::IncompleteShortIntPrecision(),
         "#version 310es\n"
         "precision mediump float;\n"
         "precision mediump sampler2D;\n"
         "out mediump vec4 sk_FragColor;\n"
         "uniform sampler2D tex;\n"
         "in highp vec2 texcoord;\n"
         "in highp ivec2 offset;\n"
         "void main() {\n"
         "    highp int scalar = offset.y;\n"
         "    sk_FragColor = texture(tex, texcoord + vec2(offset * scalar));\n"
         "}\n",
         SkSL::Program::kFragment_Kind);
}

DEF_TEST(SkSLFrExp, r) {
    test(r,
         "void main() {"
         "    int exp;"
         "    float foo = frexp(0.5, exp);"
         "    sk_FragColor = half4(exp);"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    int exp;\n"
         "    float foo = frexp(0.5, exp);\n"
         "    sk_FragColor = vec4(float(exp));\n"
         "}\n");
}

DEF_TEST(SkSLWorkaroundAddAndTrueToLoopCondition, r) {
    test(r,
         "void main() {"
         "    int c = 0;"
         "    for (int i = 0; i < 4 || c < 10; ++i) {"
         "        c += 1;"
         "    }"
         "}",
         *SkSL::ShaderCapsFactory::AddAndTrueToLoopCondition(),
         "#version 400\n"
         "void main() {\n"
         "    int c = 0;\n"
         "    for (int i = 0;(i < 4 || c < 10) && true; ++i) {\n"
         "        c += 1;\n"
         "    }\n"
         "}\n",
         SkSL::Program::kFragment_Kind
         );
}

DEF_TEST(SkSLWorkaroundUnfoldShortCircuitAsTernary, r) {
    test(r,
         "uniform bool x;"
         "uniform bool y;"
         "uniform int i;"
         "uniform int j;"
         "void main() {"
         "    bool andXY = x && y;"
         "    bool orXY = x || y;"
         "    bool combo = (x && y) || (x || y);"
         "    bool prec = (i + j == 3) && y;"
         "    while (andXY && orXY && combo && prec) {"
         "        sk_FragColor = half4(0);"
         "        break;"
         "    }"
         "}",
         *SkSL::ShaderCapsFactory::UnfoldShortCircuitAsTernary(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "uniform bool x;\n"
         "uniform bool y;\n"
         "uniform int i;\n"
         "uniform int j;\n"
         "void main() {\n"
         "    bool andXY = x ? y : false;\n"
         "    bool orXY = x ? true : y;\n"
         "    bool combo = (x ? y : false) ? true : (x ? true : y);\n"
         "    bool prec = i + j == 3 ? y : false;\n"
         "    while (((andXY ? orXY : false) ? combo : false) ? prec : false) {\n"
         "        sk_FragColor = vec4(0.0);\n"
         "        break;\n"
         "    }\n"
         "}\n",
         SkSL::Program::kFragment_Kind
         );
}

DEF_TEST(SkSLWorkaroundEmulateAbsIntFunction, r) {
    test(r,
         "uniform int i;"
         "uniform float f;"
         "void main() {"
         "    float output = abs(f) + abs(i);"
         "    sk_FragColor = half4(half(output));"
         "}",
         *SkSL::ShaderCapsFactory::EmulateAbsIntFunction(),
         "#version 400\n"
         "int _absemulation(int x) {\n"
         "    return x * sign(x);\n"
         "}\n"
         "out vec4 sk_FragColor;\n"
         "uniform int i;\n"
         "uniform float f;\n"
         "void main() {\n"
         "    float output = abs(f) + float(_absemulation(i));\n"
         "    sk_FragColor = vec4(output);\n"
         "}\n",
         SkSL::Program::kFragment_Kind
         );
}

DEF_TEST(SkSLWorkaroundRewriteDoWhileLoops, r) {
    test(r,
         "void main() {"
         "    int i = 0;"
         "    do {"
         "      ++i;"
         "      do {"
         "        i++;"
         "      } while (true);"
         "    } while (i < 10);"
         "    sk_FragColor = half4(i);"
         "}",
         *SkSL::ShaderCapsFactory::RewriteDoWhileLoops(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    int i = 0;\n"
         "    bool _tmpLoopSeenOnce0 = false;\n"
         "    while (true) {\n"
         "        if (_tmpLoopSeenOnce0) {\n"
         "            if (!(i < 10)) {\n"
         "                break;\n"
         "            }\n"
         "        }\n"
         "        _tmpLoopSeenOnce0 = true;\n"
         "        {\n"
         "            ++i;\n"
         "            bool _tmpLoopSeenOnce1 = false;\n"
         "            while (true) {\n"
         "                if (_tmpLoopSeenOnce1) {\n"
         "                    if (!true) {\n"
         "                        break;\n"
         "                    }\n"
         "                }\n"
         "                _tmpLoopSeenOnce1 = true;\n"
         "                {\n"
         "                    i++;\n"
         "                }\n"
         "            }\n"
         "        }\n"
         "    }\n"
         "    sk_FragColor = vec4(float(i));\n"
         "}\n",
         SkSL::Program::kFragment_Kind
         );
}

DEF_TEST(SkSLWorkaroundRemovePowWithConstantExponent, r) {
    test(r,
         "uniform float x;"
         "uniform float y;"
         "void main() {"
         "    float z = pow(x + 1.0, y + 2.0);"
         "    sk_FragColor = half4(half(z));"
         "}",
         *SkSL::ShaderCapsFactory::RemovePowWithConstantExponent(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "uniform float x;\n"
         "uniform float y;\n"
         "void main() {\n"
         "    float z = exp2((y + 2.0) * log2(x + 1.0));\n"
         "    sk_FragColor = vec4(z);\n"
         "}\n",
         SkSL::Program::kFragment_Kind
         );
}

DEF_TEST(SkSLSwizzleLTRB, r) {
    test(r,
         "void main() {"
         "    sk_FragColor = sk_FragColor.BRTL;"
         "}",
         *SkSL::ShaderCapsFactory::RemovePowWithConstantExponent(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor = sk_FragColor.wzyx;\n"
         "}\n",
         SkSL::Program::kFragment_Kind
         );
}

DEF_TEST(SkSLSwizzleConstants, r) {
    test(r,
         "void main() {"
         "    half4 v = half4(half(sqrt(1)));"
         "    sk_FragColor = half4(v.x, 1, 1, 1);"
         "    sk_FragColor = half4(v.xy, 1, 1);"
         "    sk_FragColor = half4(v.x1, 1, 1);"
         "    sk_FragColor = half4(v.0y, 1, 1);"
         "    sk_FragColor = half4(v.xyz, 1);"
         "    sk_FragColor = half4(v.xy1, 1);"
         "    sk_FragColor = half4(v.x0z, 1);"
         "    sk_FragColor = half4(v.x10, 1);"
         "    sk_FragColor = half4(v.1yz, 1);"
         "    sk_FragColor = half4(v.0y1, 1);"
         "    sk_FragColor = half4(v.11z, 1);"
         "    sk_FragColor = v.xyzw;"
         "    sk_FragColor = v.xyz1;"
         "    sk_FragColor = v.xy0w;"
         "    sk_FragColor = v.xy10;"
         "    sk_FragColor = v.x1zw;"
         "    sk_FragColor = v.x0z1;"
         "    sk_FragColor = v.x11w;"
         "    sk_FragColor = v.x101;"
         "    sk_FragColor = v.1yzw;"
         "    sk_FragColor = v.0yz1;"
         "    sk_FragColor = v.0y1w;"
         "    sk_FragColor = v.1y11;"
         "    sk_FragColor = v.00zw;"
         "    sk_FragColor = v.00z1;"
         "    sk_FragColor = v.011w;"
         "}",
         *SkSL::ShaderCapsFactory::RemovePowWithConstantExponent(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    vec4 v = vec4(sqrt(1.0));\n"
         "    sk_FragColor = vec4(v.x, 1.0, 1.0, 1.0);\n"
         "    sk_FragColor = vec4(v.xy, 1.0, 1.0);\n"
         "    sk_FragColor = vec4(vec2(v.x, 1.0), 1.0, 1.0);\n"
         "    sk_FragColor = vec4(vec2(v.y, 0.0).yx, 1.0, 1.0);\n"
         "    sk_FragColor = vec4(v.xyz, 1.0);\n"
         "    sk_FragColor = vec4(vec3(v.xy, 1.0), 1.0);\n"
         "    sk_FragColor = vec4(vec3(v.xz, 0.0).xzy, 1.0);\n"
         "    sk_FragColor = vec4(vec3(v.x, 1.0, 0.0), 1.0);\n"
         "    sk_FragColor = vec4(vec3(v.yz, 1.0).zxy, 1.0);\n"
         "    sk_FragColor = vec4(vec3(v.y, 0.0, 1.0).yxz, 1.0);\n"
         "    sk_FragColor = vec4(vec3(v.z, 1.0, 1.0).yzx, 1.0);\n"
         "    sk_FragColor = v;\n"
         "    sk_FragColor = vec4(v.xyz, 1.0);\n"
         "    sk_FragColor = vec4(v.xyw, 0.0).xywz;\n"
         "    sk_FragColor = vec4(v.xy, 1.0, 0.0);\n"
         "    sk_FragColor = vec4(v.xzw, 1.0).xwyz;\n"
         "    sk_FragColor = vec4(v.xz, 0.0, 1.0).xzyw;\n"
         "    sk_FragColor = vec4(v.xw, 1.0, 1.0).xzwy;\n"
         "    sk_FragColor = vec4(v.x, 1.0, 0.0, 1.0);\n"
         "    sk_FragColor = vec4(v.yzw, 1.0).wxyz;\n"
         "    sk_FragColor = vec4(v.yz, 0.0, 1.0).zxyw;\n"
         "    sk_FragColor = vec4(v.yw, 0.0, 1.0).zxwy;\n"
         "    sk_FragColor = vec4(v.y, 1.0, 1.0, 1.0).yxzw;\n"
         "    sk_FragColor = vec4(v.zw, 0.0, 0.0).zwxy;\n"
         "    sk_FragColor = vec4(v.z, 0.0, 0.0, 1.0).yzxw;\n"
         "    sk_FragColor = vec4(v.w, 0.0, 1.0, 1.0).yzwx;\n"
         "}\n",
         SkSL::Program::kFragment_Kind
         );
}

DEF_TEST(SkSLSwizzleOpt, r) {
    test(r,
         "void main() {"
         "    half v = half(sqrt(1));"
         "    sk_FragColor = half4(v).rgba;"
         "    sk_FragColor = half4(v).rgb0.abgr;"
         "    sk_FragColor = half4(v).rgba.00ra;"
         "    sk_FragColor = half4(v).rgba.rrra.00ra.11ab;"
         "    sk_FragColor = half4(v).abga.gb11;"
         "    sk_FragColor = half4(v).abgr.abgr;"
         "    sk_FragColor = half4(half4(v).rrrr.bb, 1, 1);"
         "    sk_FragColor = half4(half4(v).ba.grgr);"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    float v = sqrt(1.0);\n"
         "    sk_FragColor = vec4(v);\n"
         "    sk_FragColor = vec4(vec4(v).xyz, 0.0).wzyx;\n"
         "    sk_FragColor = vec4(vec4(v).xw, 0.0, 0.0).zwxy;\n"
         "    sk_FragColor = vec4(vec4(vec4(v).xw, 0.0, 0.0).yx, 1.0, 1.0).zwxy;\n"
         "    sk_FragColor = vec4(vec4(v).zy, 1.0, 1.0);\n"
         "    sk_FragColor = vec4(v);\n"
         "    sk_FragColor = vec4(vec4(v).xx, 1.0, 1.0);\n"
         "    sk_FragColor = vec4(v).wzwz;\n"
         "}\n",
         SkSL::Program::kFragment_Kind
         );
}

DEF_TEST(SkSLSwizzleScalar, r) {
    test(r,
         "void main() {"
         "    half x = half(sqrt(4));"
         "    sk_FragColor = x.xx01;"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    float x = sqrt(4.0);\n"
         "    sk_FragColor = vec4(vec2(x), 0.0, 1.0);\n"
         "}\n");
    test(r,
         "void main() {"
         "    sk_FragColor = half(sqrt(4)).xx01;"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor = vec4(vec2(sqrt(4.0)), 0.0, 1.0);\n"
         "}\n");
    test(r,
         "void main() {"
         "    sk_FragColor = half(sqrt(4)).0x01;"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor = vec4(sqrt(4.0), 0.0, 0.0, 1.0).yxzw;\n"
         "}\n");
    test(r,
         "void main() {"
         "    sk_FragColor = half(sqrt(4)).0x0x;"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor = vec4(vec2(sqrt(4.0)), 0.0, 0.0).zxwy;\n"
         "}\n");
}

DEF_TEST(SkSLStackingVectorCasts, r) {
    test(r,
         "void main() {"
         "    if (half4(0, 0, 1, 1) == half4(int4(0, 0, 1, 1)))"
         "        sk_FragColor = half4(0, 1, 0, 1);"
         "    else"
         "        sk_FragColor = half4(1, 0, 0, 1);"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
         "}\n");
    test(r,
         "void main() {"
         "    if (half4(int4(0, 0, 1, 1)) == half4(int4(half4(0, 0, 1, 1))))"
         "        sk_FragColor = half4(0, 1, 0, 1);"
         "    else"
         "        sk_FragColor = half4(1, 0, 0, 1);"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
         "}\n");
}

DEF_TEST(SkSLCastsRoundTowardZero, r) {
    test(r,
         "void main() {"
         "    if (half4(int4(0, 0, 1, 2)) == half4(int4(half4(0.01, 0.99, 1.49, 2.75))))"
         "        sk_FragColor = half4(0, 1, 0, 1);"
         "    else"
         "        sk_FragColor = half4(1, 0, 0, 1);"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
         "}\n");
    test(r,
         "void main() {"
         "    if (half4(int4(0, 0, -1, -2)) == half4(int4(half4(-0.01, -0.99, -1.49, -2.75))))"
         "        sk_FragColor = half4(0, 1, 0, 1);"
         "    else"
         "        sk_FragColor = half4(1, 0, 0, 1);"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
         "}\n");
}

DEF_TEST(SkSLNegatedVectorLiteral, r) {
    test(r,
         "void main() {"
         "    if (half4(1) == half4(-half2(-1), half2(1)))"
         "        sk_FragColor = half4(0, 1, 0, 1);"
         "    else"
         "        sk_FragColor = half4(1, 0, 0, 1);"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         "#version 400\n"
         "out vec4 sk_FragColor;\n"
         "void main() {\n"
         "    sk_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
         "}\n");
}

DEF_TEST(SkSLDiscard, r) {
    test(r,
         "void main() {"
         "half x;"
         "    @switch (1) {"
         "       case 0: x = 0; break;"
         "       default: x = 1; discard;"
         "    }"
         "    sk_FragColor = half4(x);"
         "}",
         *SkSL::ShaderCapsFactory::Default(),
         R"__GLSL__(#version 400
void main() {
    {
        discard;
    }
}
)__GLSL__");
}
