//
// Created by droc101 on 2/11/26.
//

#include "LanguageDefinition.h"
#include <string>
#include <utility>
#include <vector>
#include "Palette.h"
#include "Types.h"

const LanguageDefinition &LanguageDefinition::GLSL()
{
    static bool inited = false;
    static LanguageDefinition langDef;
    if (!inited)
    {
        static const std::vector<std::string> keywords = {
            "const",
            "uniform",
            "buffer",
            "shared",
            "attribute",
            "varying",
            "coherent",
            "volatile",
            "restrict",
            "readonly",
            "writeonly",
            "atomic_uint",
            "layout",
            "centroid",
            "flat",
            "smooth",
            "noperspective",
            "patch",
            "sample",
            "invariant",
            "precise",
            "break",
            "continue",
            "do",
            "for",
            "while",
            "switch",
            "case",
            "default",
            "if",
            "else",
            "subroutine",
            "in",
            "out",
            "inout",
            "int",
            "void",
            "bool",
            "true",
            "false",
            "float",
            "double",
            "discard",
            "return",
            "vec2",
            "vec3",
            "vec4",
            "ivec2",
            "ivec3",
            "ivec4",
            "bvec2",
            "bvec3",
            "bvec4",
            "uint",
            "uvec2",
            "uvec3",
            "uvec4",
            "dvec2",
            "dvec3",
            "dvec4",
            "mat2",
            "mat3",
            "mat4",
            "mat2x2",
            "mat2x3",
            "mat2x4",
            "mat3x2",
            "mat3x3",
            "mat3x4",
            "mat4x2",
            "mat4x3",
            "mat4x4",
            "dmat2",
            "dmat3",
            "dmat4",
            "dmat2x2",
            "dmat2x3",
            "dmat2x4",
            "dmat3x2",
            "dmat3x3",
            "dmat3x4",
            "dmat4x2",
            "dmat4x3",
            "dmat4x4",
            "lowp",
            "mediump",
            "highp",
            "precision",
            "sampler1D",
            "sampler1DShadow",
            "sampler1DArray",
            "sampler1DArrayShadow",
            "isampler1D",
            "isampler1DArray",
            "usampler1D",
            "usampler1DArray",
            "sampler2D",
            "sampler2DShadow",
            "sampler2DArray",
            "sampler2DArrayShadow",
            "isampler2D",
            "isampler2DArray",
            "usampler2D",
            "usampler2DArray",
            "sampler2DRect",
            "sampler2DRectShadow",
            "isampler2DRect",
            "usampler2DRect",
            "sampler2DMS",
            "isampler2DMS",
            "usampler2DMS",
            "sampler2DMSArray",
            "isampler2DMSArray",
            "usampler2DMSArray",
            "sampler3D",
            "isampler3D",
            "usampler3D",
            "samplerCube",
            "samplerCubeShadow",
            "isamplerCube",
            "usamplerCube",
            "samplerCubeArray",
            "samplerCubeArrayShadow",
            "isamplerCubeArray",
            "usamplerCubeArray",
            "samplerBuffer",
            "isamplerBuffer",
            "usamplerBuffer",
            "image1D",
            "iimage1D",
            "uimage1D",
            "image1DArray",
            "iimage1DArray",
            "uimage1DArray",
            "image2D",
            "iimage2D",
            "uimage2D",
            "image2DArray",
            "iimage2DArray",
            "uimage2DArray",
            "image2DRect",
            "iimage2DRect",
            "uimage2DRect",
            "image2DMS",
            "iimage2DMS",
            "uimage2DMS",
            "image2DMSArray",
            "iimage2DMSArray",
            "uimage2DMSArray",
            "image3D",
            "iimage3D",
            "uimage3D",
            "imageCube",
            "iimageCube",
            "uimageCube",
            "imageCubeArray",
            "iimageCubeArray",
            "uimageCubeArray",
            "imageBuffer",
            "iimageBuffer",
            "uimageBuffer",
            "struct",
            "texture1D",
            "texture1DArray",
            "itexture1D",
            "itexture1DArray",
            "utexture1D",
            "utexture1DArray",
            "texture2D",
            "texture2DArray",
            "itexture2D",
            "itexture2DArray",
            "utexture2D",
            "utexture2DArray",
            "texture2DRect",
            "itexture2DRect",
            "utexture2DRect",
            "texture2DMS",
            "itexture2DMS",
            "utexture2DMS",
            "texture2DMSArray",
            "itexture2DMSArray",
            "utexture2DMSArray",
            "texture3D",
            "itexture3D",
            "utexture3D",
            "textureCube",
            "itextureCube",
            "utextureCube",
            "textureCubeArray",
            "itextureCubeArray",
            "utextureCubeArray",
            "textureBuffer",
            "itextureBuffer",
            "utextureBuffer",
            "sampler",
            "samplerShadow",
            "subpassInput",
            "isubpassInput",
            "usubpassInput",
            "subpassInputMS",
            "isubpassInputMS",
            "usubpassInputMS",
            "common",
            "partition",
            "active",
            "asm",
            "class",
            "union",
            "enum",
            "typedef",
            "template",
            "this",
            "resource",
            "goto",
            "inline",
            "noinline",
            "public",
            "static",
            "extern",
            "external",
            "interface",
            "long",
            "short",
            "half",
            "fixed",
            "unsigned",
            "superp",
            "input",
            "output",
            "hvec2",
            "hvec3",
            "hvec4",
            "fvec2",
            "fvec3",
            "fvec4",
            "filter",
            "sizeof",
            "cast",
            "namespace",
            "using",
            "sampler3DRect",
        };
        for (const auto &k: keywords)
        {
            langDef.mKeywords.insert(k);
        }

        static const std::vector<std::string> builtin_functions = {
            "radians",
            "degrees",
            "sin",
            "cos",
            "tan",
            "asin",
            "acos",
            "atan",
            "sinh",
            "cosh",
            "asinh",
            "acosh",
            "atanh",
            "pow",
            "exp",
            "log",
            "exp2",
            "log2",
            "sqrt",
            "inversesqrt",
            "abs",
            "sign",
            "floor",
            "trunc",
            "round",
            "roundEven",
            "ceil",
            "fract",
            "mod",
            "modf",
            "min",
            "max",
            "clamp",
            "mix",
            "step",
            "smoothstep",
            "isnan",
            "isinf",
            "floatBitsToInt",
            "floatBitsToUint",
            "intBitsToFloat",
            "uintBitsToFloat",
            "fma",
            "frexp",
            "ldexp",
            "packUnorm2x16",
            "packSnorm2x16",
            "packUnorm4x8",
            "packSnorm4x8",
            "unpackUnorm2x16",
            "unpackSnorm2x16",
            "unpackUnorm4x8",
            "unpackSnorm4x8",
            "packHalf2x16",
            "unpackHalf2x16",
            "packDouble2x32",
            "unpackDouble2x32",
            "length",
            "distance",
            "dot",
            "cross",
            "normalize",
            "faceforward",
            "reflect",
            "refract",
            "matrixCompMult",
            "outerProduct",
            "transpose",
            "determinant",
            "inverse",
            "textureSize",
            "texture",
            "textureProj",
            "textureLod",
            "texelFetch",
            "noise1",
            "noise2",
            "noise3",
            "noise4",
        };
        for (const auto &k: builtin_functions)
        {
            Identifier id;
            id.mDeclaration = "Built-in function";
            langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
        }

        static const std::vector<std::string> builtin_variables = {
            "gl_VertexID",     "gl_InstanceID",    "gl_VertexIndex",      "gl_InstanceIndex",  "gl_DrawID",
            "gl_BaseVertex",   "gl_BaseInstance",  "gl_Position",         "gl_PointSize",      "gl_ClipDistance",
            "gl_CullDistance", "gl_FragCoord",     "gl_FrontFacing",      "gl_ClipDistance",   "gl_CullDistance",
            "gl_PointCoord",   "gl_PrimitiveID",   "gl_SampleID",         "gl_SamplePosition", "gl_SampleMaskIn",
            "gl_Layer",        "gl_ViewportIndex", "gl_HelperInvocation", "gl_FragDepth",      "gl_SampleMask",
        };
        for (const auto &k: builtin_variables)
        {
            Identifier id;
            id.mDeclaration = "Built-in variable";
            langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+",
                                                                                       PaletteIndex::Preprocessor));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"(L?\"(\\.|[^\"])*\")",
                                                                                       PaletteIndex::String));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"(\'\\?[^\']\')",
                                                                                       PaletteIndex::CharLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-"
                                                                                       "9]+)([eE][+-]?[0-9]+)?[fF]?",
                                                                                       PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?",
                                                                                       PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?",
                                                                                       PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]"
                                                                                       "?",
                                                                                       PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*",
                                                                                       PaletteIndex::Identifier));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\("
                                                                                       "\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/"
                                                                                       "\\;\\,\\.]",
                                                                                       PaletteIndex::Punctuation));

        langDef.mCommentStart = "/*";
        langDef.mCommentEnd = "*/";
        langDef.mSingleLineComment = "//";

        langDef.mCaseSensitive = true;
        langDef.mAutoIndentation = true;

        langDef.mName = "GLSL";

        inited = true;
    }
    return langDef;
}

const LanguageDefinition &LanguageDefinition::AngelScript()
{
    static bool inited = false;
    static LanguageDefinition langDef;
    if (!inited)
    {
        static const std::vector<std::string> keywords = {"and",       "abstract", "auto",      "bool",      "break",
                                                          "case",      "cast",     "class",     "const",     "continue",
                                                          "default",   "do",       "double",    "else",      "enum",
                                                          "false",     "final",    "float",     "for",       "from",
                                                          "funcdef",   "function", "get",       "if",        "import",
                                                          "in",        "inout",    "int",       "interface", "int8",
                                                          "int16",     "int32",    "int64",     "is",        "mixin",
                                                          "namespace", "not",      "null",      "or",        "out",
                                                          "override",  "private",  "protected", "return",    "set",
                                                          "shared",    "super",    "switch",    "this ",     "true",
                                                          "typedef",   "uint",     "uint8",     "uint16",    "uint32",
                                                          "uint64",    "void",     "while",     "xor"};

        for (const auto &k: keywords)
        {
            langDef.mKeywords.insert(k);
        }

        static const std::vector<std::string> identifiers = {"cos",         "sin",         "tab",         "acos",
                                                             "asin",        "atan",        "atan2",       "cosh",
                                                             "sinh",        "tanh",        "log",         "log10",
                                                             "pow",         "sqrt",        "abs",         "ceil",
                                                             "floor",       "fraction",    "closeTo",     "fpFromIEEE",
                                                             "fpToIEEE",    "complex",     "opEquals",    "opAddAssign",
                                                             "opSubAssign", "opMulAssign", "opDivAssign", "opAdd",
                                                             "opSub",       "opMul",       "opDiv"};
        for (const auto &k: identifiers)
        {
            Identifier id;
            id.mDeclaration = "Built-in function";
            langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"(L?\"(\\.|[^\"])*\")",
                                                                                       PaletteIndex::String));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>(R"(\'\\?[^\']\')",
                                                                                       PaletteIndex::String));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-"
                                                                                       "9]+)([eE][+-]?[0-9]+)?[fF]?",
                                                                                       PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?",
                                                                                       PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?",
                                                                                       PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]"
                                                                                       "?",
                                                                                       PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*",
                                                                                       PaletteIndex::Identifier));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\("
                                                                                       "\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/"
                                                                                       "\\;\\,\\.]",
                                                                                       PaletteIndex::Punctuation));

        langDef.mCommentStart = "/*";
        langDef.mCommentEnd = "*/";
        langDef.mSingleLineComment = "//";

        langDef.mCaseSensitive = true;
        langDef.mAutoIndentation = true;

        langDef.mName = "AngelScript";

        inited = true;
    }
    return langDef;
}
