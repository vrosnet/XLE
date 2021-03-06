// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#if !defined(COMMON_RESOURCES_H)
#define COMMON_RESOURCES_H

Texture2D		DiffuseTexture          : register(t0);
Texture2D		NormalsTexture          : register(t1);
Texture2D       ParametersTexture       : register(t2);

SamplerState	DefaultSampler          : register(s0);
SamplerState	ClampingSampler         : register(s1);
SamplerState    AnisotropicSampler      : register(s2);
SamplerState    PointClampSampler       : register(s3);
SamplerState    WrapUSampler            : register(s6);

Texture2D       NormalsFittingTexture   : register(t14);

#if !defined(PREFER_ANISOTROPIC) || PREFER_ANISOTROPIC==0
    #define MaybeAnisotropicSampler   DefaultSampler
#else
    #define MaybeAnisotropicSampler   AnisotropicSampler
#endif

#endif
