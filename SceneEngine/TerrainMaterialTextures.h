// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../RenderCore/Metal/ShaderResource.h"
#include "../../RenderCore/Metal/Buffer.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../Assets/Assets.h"

namespace SceneEngine
{
    class TerrainMaterialConfig;

    class TerrainMaterialTextures
    {
    public:
        enum Resources { Diffuse, Normal, Roughness, ResourceCount };
        using ResLocator = intrusive_ptr<BufferUploads::ResourceLocator>;
        ResLocator _textureArray[ResourceCount];
        RenderCore::Metal::ShaderResourceView _srv[ResourceCount];
        RenderCore::Metal::ConstantBuffer _texturingConstants;
        RenderCore::Metal::ConstantBuffer _procTexContsBuffer;
        unsigned _strataCount;

        TerrainMaterialTextures();
        TerrainMaterialTextures(
            RenderCore::Metal::DeviceContext& devContext,
            const TerrainMaterialConfig& scaffold, 
            bool useGradFlagMaterials = true);
        ~TerrainMaterialTextures();

        const std::shared_ptr<::Assets::DependencyValidation>& GetDependencyValidation() const   { return _validationCallback; }
    private:
        std::shared_ptr<::Assets::DependencyValidation>  _validationCallback;
    };
}

