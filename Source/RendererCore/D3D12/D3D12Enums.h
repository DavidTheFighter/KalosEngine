#ifndef RENDERING_D3D12_D3D12ENUMS_H_
#define RENDERING_D3D12_D3D12ENUMS_H_
#if BUILD_D3D12_BACKEND

inline D3D12_RESOURCE_STATES BufferLayoutToD3D12ResourceStates(BufferLayout layout)
{
	switch (layout)
	{
		case BUFFER_LAYOUT_VERTEX_BUFFER:
			return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
			break;
		case BUFFER_LAYOUT_INDEX_BUFFER:
			return D3D12_RESOURCE_STATE_INDEX_BUFFER;
			break;
		case BUFFER_LAYOUT_CONSTANT_BUFFER:
			return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
			break;
		case BUFFER_LAYOUT_VERTEX_CONSTANT_BUFFER:
			return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
			break;
		case BUFFER_LAYOUT_VERTEX_INDEX_BUFFER:
			return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER;
			break;
		case BUFFER_LAYOUT_VERTEX_INDEX_CONSTANT_BUFFER:
			return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER;
			break;
		case BUFFER_LAYOUT_GENERAL:
			return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			break;
		case BUFFER_LAYOUT_INDIRECT_BUFFER:
			return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
			break;
		case BUFFER_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return D3D12_RESOURCE_STATE_COPY_SOURCE;
			break;
		case BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL:
			return D3D12_RESOURCE_STATE_COPY_DEST;
			break;
		default:
			return D3D12_RESOURCE_STATE_COMMON;
	}
}

inline D3D12_RESOURCE_STATES TextureLayoutToD3D12ResourceStates(TextureLayout layout)
{
	switch (layout)
	{
		case TEXTURE_LAYOUT_INITIAL_STATE:
			return D3D12_RESOURCE_STATE_COMMON;
		case TEXTURE_LAYOUT_GENERAL:
			return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		case TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return D3D12_RESOURCE_STATE_RENDER_TARGET;
		case TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			return D3D12_RESOURCE_STATE_DEPTH_WRITE;
		case TEXTURE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			return D3D12_RESOURCE_STATE_DEPTH_READ;
		case TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:
			return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		case TEXTURE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return D3D12_RESOURCE_STATE_COPY_SOURCE;
		case TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return D3D12_RESOURCE_STATE_COPY_DEST;
		case TEXTURE_LAYOUT_RESOLVE_SRC_OPTIMAL:
			return D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
		case TEXTURE_LAYOUT_RESOLVE_DST_OPTIMAL:
			return D3D12_RESOURCE_STATE_RESOLVE_DEST;
		default:
			return D3D12_RESOURCE_STATE_COMMON;
	}
}

inline D3D12_INPUT_CLASSIFICATION vertexInputRateToD3D12InputClassification(VertexInputRate rate)
{
	switch (rate)
	{
		case VERTEX_INPUT_RATE_INSTANCE:
			return D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
		case VERTEX_INPUT_RATE_VERTEX:
		default:
			return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	}
}

inline D3D12_SRV_DIMENSION textureViewTypeToD3D12SRVDimension(TextureViewType type)
{
	switch (type)
	{
		case TEXTURE_VIEW_TYPE_1D:
			return D3D12_SRV_DIMENSION_TEXTURE1D;
		case TEXTURE_VIEW_TYPE_2D:
			return D3D12_SRV_DIMENSION_TEXTURE2D;
		case TEXTURE_VIEW_TYPE_3D:
			return D3D12_SRV_DIMENSION_TEXTURE3D;
		case TEXTURE_VIEW_TYPE_CUBE:
			return D3D12_SRV_DIMENSION_TEXTURECUBE;
		case TEXTURE_VIEW_TYPE_1D_ARRAY:
			return D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
		case TEXTURE_VIEW_TYPE_2D_ARRAY:
			return D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		case TEXTURE_VIEW_TYPE_CUBE_ARRAY:
			return D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
		default:
			return D3D12_SRV_DIMENSION_TEXTURE2D;
	}
}

inline D3D12_UAV_DIMENSION textureViewTypeToD3D12UAVDimension(TextureViewType type)
{
	switch (type)
	{
		case TEXTURE_VIEW_TYPE_1D:
			return D3D12_UAV_DIMENSION_TEXTURE1D;
		case TEXTURE_VIEW_TYPE_1D_ARRAY:
			return D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
		case TEXTURE_VIEW_TYPE_2D:
			return D3D12_UAV_DIMENSION_TEXTURE2D;
		case TEXTURE_VIEW_TYPE_2D_ARRAY:
		case TEXTURE_VIEW_TYPE_CUBE:
		case TEXTURE_VIEW_TYPE_CUBE_ARRAY:
			return D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		case TEXTURE_VIEW_TYPE_3D:
			return D3D12_UAV_DIMENSION_TEXTURE3D;
		default:
			return D3D12_UAV_DIMENSION_TEXTURE2D;
	}
}

inline D3D_PRIMITIVE_TOPOLOGY primitiveTopologyToD3DPrimitiveTopology(PrimitiveTopology topo, uint32_t patchControlPoints = 4)
{
	switch (topo)
	{
		case PRIMITIVE_TOPOLOGY_POINT_LIST:
			return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
		case PRIMITIVE_TOPOLOGY_LINE_LIST:
			return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		case PRIMITIVE_TOPOLOGY_LINE_STRIP:
			return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
		case PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
			return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		case PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
			return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
			return D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
		case PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
		case PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
			return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
		case PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
		case PRIMITIVE_TOPOLOGY_PATCH_LIST:
			return D3D_PRIMITIVE_TOPOLOGY(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (patchControlPoints - 1));
		default:
			return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
}

inline D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyToD3D12PrimitiveTopologyType(PrimitiveTopology topo)
{
	switch (topo)
	{
		case PRIMITIVE_TOPOLOGY_POINT_LIST:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		case PRIMITIVE_TOPOLOGY_LINE_LIST:
		case PRIMITIVE_TOPOLOGY_LINE_STRIP:
		case PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
		case PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
		case PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
		case PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
		case PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		case PRIMITIVE_TOPOLOGY_PATCH_LIST:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
		default:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	}
}

inline D3D12_COMPARISON_FUNC compareOpToD3D12ComparisonFunc(CompareOp op)
{
	switch (op)
	{
		case COMPARE_OP_NEVER:
			return D3D12_COMPARISON_FUNC_NEVER;
		case COMPARE_OP_LESS:
			return D3D12_COMPARISON_FUNC_LESS;
		case COMPARE_OP_EQUAL:
			return D3D12_COMPARISON_FUNC_EQUAL;
		case COMPARE_OP_LESS_OR_EQUAL:
			return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case COMPARE_OP_GREATER:
			return D3D12_COMPARISON_FUNC_GREATER;
		case COMPARE_OP_NOT_EQUAL:
			return D3D12_COMPARISON_FUNC_NOT_EQUAL;
		case COMPARE_OP_GREATER_OR_EQUAL:
			return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case COMPARE_OP_ALWAYS:
			return D3D12_COMPARISON_FUNC_ALWAYS;
		default:
			return D3D12_COMPARISON_FUNC_ALWAYS;
	}
}

inline D3D12_CULL_MODE cullModeFlagsToD3D12CullMode(PolygonCullMode cullMode)
{
	switch (cullMode)
	{
		case POLYGON_CULL_MODE_FRONT:
			return D3D12_CULL_MODE_FRONT;
		case POLYGON_CULL_MODE_BACK:
			return D3D12_CULL_MODE_BACK;
		case POLYGON_CULL_MODE_NONE:
		default:
			return D3D12_CULL_MODE_NONE;
	}
}

inline D3D12_FILL_MODE polygonModeToD3D12FillMode(PolygonMode mode)
{
	switch (mode)
	{
		case POLYGON_MODE_FILL:
			return D3D12_FILL_MODE_SOLID;
		case POLYGON_MODE_LINE:
			return D3D12_FILL_MODE_WIREFRAME;
		default:
			return D3D12_FILL_MODE_SOLID;
	}
}

inline D3D12_BLEND_OP blendOpToD3D12BlendOp(BlendOp op)
{
	switch (op)
	{
		case BLEND_OP_ADD:
			return D3D12_BLEND_OP_ADD;
		case BLEND_OP_SUBTRACT:
			return D3D12_BLEND_OP_SUBTRACT;
		case BLEND_OP_REVERSE_SUBTRACT:
			return D3D12_BLEND_OP_REV_SUBTRACT;
		case BLEND_OP_MIN:
			return D3D12_BLEND_OP_MIN;
		case BLEND_OP_MAX:
			return D3D12_BLEND_OP_MAX;
		default:
			return D3D12_BLEND_OP_ADD;
	}
}

inline D3D12_BLEND blendFactorToD3D12Blend(BlendFactor blend)
{
	switch (blend)
	{
		case BLEND_FACTOR_ZERO:
			return D3D12_BLEND_ZERO;
		case BLEND_FACTOR_ONE:
			return D3D12_BLEND_ONE;

		case BLEND_FACTOR_SRC_COLOR:
			return D3D12_BLEND_SRC_COLOR;
		case BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
			return D3D12_BLEND_INV_SRC_COLOR;
		case BLEND_FACTOR_DST_COLOR:
			return D3D12_BLEND_DEST_COLOR;
		case BLEND_FACTOR_ONE_MINUS_DST_COLOR:
			return D3D12_BLEND_INV_DEST_COLOR;

		case BLEND_FACTOR_SRC_ALPHA:
			return D3D12_BLEND_SRC_ALPHA;
		case BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
			return D3D12_BLEND_INV_SRC_ALPHA;
		case BLEND_FACTOR_DST_ALPHA:
			return D3D12_BLEND_DEST_ALPHA;
		case BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
			return D3D12_BLEND_INV_DEST_ALPHA;

		case BLEND_FACTOR_SRC_ALPHA_SATURATE:
			return D3D12_BLEND_SRC_ALPHA_SAT;
		case BLEND_FACTOR_SRC1_COLOR:
			return D3D12_BLEND_SRC1_COLOR;
		case BLEND_FACTOR_ONE_MINUS_SRC1_COLOR:
			return D3D12_BLEND_INV_SRC1_ALPHA;
		case BLEND_FACTOR_SRC1_ALPHA:
			return D3D12_BLEND_SRC1_ALPHA;
		case BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA:
			return D3D12_BLEND_INV_SRC1_ALPHA;
		default:
			return D3D12_BLEND_ZERO;
	}
}

inline std::string shaderStageFlagBitsToD3D12CompilerTargetStr(ShaderStageFlagBits stage)
{
	switch (stage)
	{
		case SHADER_STAGE_VERTEX_BIT:
			return "vs_5_1";
		case SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return "hs_5_1";
		case SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return "ds_5_1";
		case SHADER_STAGE_GEOMETRY_BIT:
			return "gs_5_1";
		case SHADER_STAGE_FRAGMENT_BIT:
			return "ps_5_1";
		case SHADER_STAGE_COMPUTE_BIT:
			return "cs_5_1";
		default:
			return "vs_5_1";
	}
}

inline D3D12_COMMAND_LIST_TYPE queueTypeToD3D12CommandListType(QueueType queue)
{
	switch (queue)
	{
		case QUEUE_TYPE_GRAPHICS:
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
		case QUEUE_TYPE_COMPUTE:
			return D3D12_COMMAND_LIST_TYPE_COMPUTE;
		case QUEUE_TYPE_TRANSFER:
			return D3D12_COMMAND_LIST_TYPE_COPY;
		default:
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
	}
}

inline D3D12_TEXTURE_ADDRESS_MODE samplerAddressModeToD3D12TextureAddressMode(SamplerAddressMode addressMode)
{
	switch (addressMode)
	{
		case SAMPLER_ADDRESS_MODE_REPEAT:
			return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		case SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
			return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		case SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
			return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		case SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
			return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		case SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
			return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
		default:
			return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	}
}

inline D3D12_FILTER samplerFilterToD3D12Filter(SamplerFilter minFilter, SamplerFilter magFilter, SamplerMipmapMode mipMode)
{
	switch (minFilter)
	{
		case SAMPLER_FILTER_NEAREST:
		{
			switch (magFilter)
			{
				case SAMPLER_FILTER_NEAREST:
				{
					switch (mipMode)
					{
						case SAMPLER_MIPMAP_MODE_NEAREST:
							return D3D12_FILTER_MIN_MAG_MIP_POINT;
						case SAMPLER_MIPMAP_MODE_LINEAR:
							return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
					}
				}
				case SAMPLER_FILTER_LINEAR:
				{
					switch (mipMode)
					{
						case SAMPLER_MIPMAP_MODE_NEAREST:
							return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
						case SAMPLER_MIPMAP_MODE_LINEAR:
							return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
					}
				}
			}
		}
		case SAMPLER_FILTER_LINEAR:
		{
			switch (magFilter)
			{
				case SAMPLER_FILTER_NEAREST:
				{
					switch (mipMode)
					{
						case SAMPLER_MIPMAP_MODE_NEAREST:
							return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
						case SAMPLER_MIPMAP_MODE_LINEAR:
							return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
					}
				}
				case SAMPLER_FILTER_LINEAR:
				{
					switch (mipMode)
					{
						case SAMPLER_MIPMAP_MODE_NEAREST:
							return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
						case SAMPLER_MIPMAP_MODE_LINEAR:
							return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
					}
				}
			}
		}
	}
	
	return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
}

inline DXGI_FORMAT ResourceFormatToDXGIFormat(ResourceFormat format)
{
	switch (format)
	{
		case RESOURCE_FORMAT_UNDEFINED:
			return DXGI_FORMAT_UNKNOWN;
		// Tiny packed formats
		case RESOURCE_FORMAT_B4G4R4A4_UNORM_PACK16:
			return DXGI_FORMAT_B4G4R4A4_UNORM;
		case RESOURCE_FORMAT_B5G6R5_UNORM_PACK16:
			return DXGI_FORMAT_B5G6R5_UNORM;
		case RESOURCE_FORMAT_B5G5R5A1_UNORM_PACK16:
			return DXGI_FORMAT_B5G5R5A1_UNORM;
		// r8 formats
		case RESOURCE_FORMAT_R8_UNORM:
			return DXGI_FORMAT_R8_UNORM;
		case RESOURCE_FORMAT_R8_SNORM:
			return DXGI_FORMAT_R8_SNORM;
		case RESOURCE_FORMAT_R8_UINT:
			return DXGI_FORMAT_R8_UINT;
		case RESOURCE_FORMAT_R8_SINT:
			return DXGI_FORMAT_R8_SINT;
		// r8g8 formats
		case RESOURCE_FORMAT_R8G8_UNORM:
			return DXGI_FORMAT_R8G8_UNORM;
		case RESOURCE_FORMAT_R8G8_SNORM:
			return DXGI_FORMAT_R8G8_SNORM;
		case RESOURCE_FORMAT_R8G8_UINT:
			return DXGI_FORMAT_R8G8_UINT;
		case RESOURCE_FORMAT_R8G8_SINT:
			return DXGI_FORMAT_R8G8_SINT;
		// There aren't any 24-bit rgb formats in DXGI
		// r8g8b8a8 formats
		case RESOURCE_FORMAT_R8G8B8A8_UNORM:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		case RESOURCE_FORMAT_R8G8B8A8_SNORM:
			return DXGI_FORMAT_R8G8B8A8_SNORM;
		case RESOURCE_FORMAT_R8G8B8A8_UINT:
			return DXGI_FORMAT_R8G8B8A8_UINT;
		case RESOURCE_FORMAT_R8G8B8A8_SINT:
			return DXGI_FORMAT_R8G8B8A8_SINT;
		case RESOURCE_FORMAT_R8G8B8A8_SRGB:
			return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		// b8g8r8a8 formats
		case RESOURCE_FORMAT_B8G8R8A8_UNORM:
			return DXGI_FORMAT_B8G8R8A8_UNORM;
		case RESOURCE_FORMAT_B8G8R8A8_SRGB:
			return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		// a2r10g10b10 formats
		case RESOURCE_FORMAT_A2R10G10B10_UNORM_PACK32:
			return DXGI_FORMAT_R10G10B10A2_UNORM;
		case RESOURCE_FORMAT_A2R10G10B10_UINT_PACK32:
			return DXGI_FORMAT_R10G10B10A2_UINT;
		// r16 formats
		case RESOURCE_FORMAT_R16_UNORM:
			return DXGI_FORMAT_R16_UNORM;
		case RESOURCE_FORMAT_R16_SNORM:
			return DXGI_FORMAT_R16_SNORM;
		case RESOURCE_FORMAT_R16_UINT:
			return DXGI_FORMAT_R16_UINT;
		case RESOURCE_FORMAT_R16_SINT:
			return DXGI_FORMAT_R16_SINT;
		case RESOURCE_FORMAT_R16_SFLOAT:
			return DXGI_FORMAT_R16_FLOAT;
		//r16g16 formats
		case RESOURCE_FORMAT_R16G16_UNORM:
			return DXGI_FORMAT_R16G16_UNORM;
		case RESOURCE_FORMAT_R16G16_SNORM:
			return DXGI_FORMAT_R16G16_SNORM;
		case RESOURCE_FORMAT_R16G16_UINT:
			return DXGI_FORMAT_R16G16_UINT;
		case RESOURCE_FORMAT_R16G16_SINT:
			return DXGI_FORMAT_R16G16_SINT;
		case RESOURCE_FORMAT_R16G16_SFLOAT:
			return DXGI_FORMAT_R16G16_FLOAT;
		// There aren't any 48-bit formats in DXGI (R16G16B16)
		// r16g16b16a16 formats
		case RESOURCE_FORMAT_R16G16B16A16_UNORM:
			return DXGI_FORMAT_R16G16B16A16_UNORM;
		case RESOURCE_FORMAT_R16G16B16A16_SNORM:
			return DXGI_FORMAT_R16G16B16A16_SNORM;
		case RESOURCE_FORMAT_R16G16B16A16_UINT:
			return DXGI_FORMAT_R16G16B16A16_UINT;
		case RESOURCE_FORMAT_R16G16B16A16_SINT:
			return DXGI_FORMAT_R16G16B16A16_SINT;
		case RESOURCE_FORMAT_R16G16B16A16_SFLOAT:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
		// r32 formats
		case RESOURCE_FORMAT_R32_UINT:
			return DXGI_FORMAT_R32_UINT;
		case RESOURCE_FORMAT_R32_SINT:
			return DXGI_FORMAT_R32_SINT;
		case RESOURCE_FORMAT_R32_SFLOAT:
			return DXGI_FORMAT_R32_FLOAT;
		// r32g32 formats
		case RESOURCE_FORMAT_R32G32_UINT:
			return DXGI_FORMAT_R32G32_UINT;
		case RESOURCE_FORMAT_R32G32_SINT:
			return DXGI_FORMAT_R32G32_SINT;
		case RESOURCE_FORMAT_R32G32_SFLOAT:
			return DXGI_FORMAT_R32G32_FLOAT;
		// r32g32b32 formats
		case RESOURCE_FORMAT_R32G32B32_UINT:
			return DXGI_FORMAT_R32G32B32_UINT;
		case RESOURCE_FORMAT_R32G32B32_SINT:
			return DXGI_FORMAT_R32G32B32_SINT;
		case RESOURCE_FORMAT_R32G32B32_SFLOAT:
			return DXGI_FORMAT_R32G32B32_FLOAT;
		// r32g32b32a32 formats
		case RESOURCE_FORMAT_R32G32B32A32_UINT:
			return DXGI_FORMAT_R32G32B32A32_UINT;
		case RESOURCE_FORMAT_R32G32B32A32_SINT:
			return DXGI_FORMAT_R32G32B32A32_SINT;
		case RESOURCE_FORMAT_R32G32B32A32_SFLOAT:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		// Some packed formats
		case RESOURCE_FORMAT_B10G11R11_UFLOAT_PACK32:
			return DXGI_FORMAT_R11G11B10_FLOAT;
		case RESOURCE_FORMAT_E5B9G9R9_UFLOAT_PACK32:
			return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
		// Depth formats
		case RESOURCE_FORMAT_D16_UNORM:
			return DXGI_FORMAT_D16_UNORM;
		case RESOURCE_FORMAT_D32_SFLOAT:
			return DXGI_FORMAT_D32_FLOAT;
		case RESOURCE_FORMAT_D24_UNORM_S8_UINT:
			return DXGI_FORMAT_D24_UNORM_S8_UINT;
		// Block compression textures
		case RESOURCE_FORMAT_BC1_RGBA_UNORM_BLOCK:
			return DXGI_FORMAT_BC1_UNORM;
		case RESOURCE_FORMAT_BC1_RGBA_SRGB_BLOCK:
			return DXGI_FORMAT_BC1_UNORM_SRGB;
		case RESOURCE_FORMAT_BC2_UNORM_BLOCK:
			return DXGI_FORMAT_BC2_UNORM;
		case RESOURCE_FORMAT_BC2_SRGB_BLOCK:
			return DXGI_FORMAT_BC2_UNORM_SRGB;
		case RESOURCE_FORMAT_BC3_UNORM_BLOCK:
			return DXGI_FORMAT_BC3_UNORM;
		case RESOURCE_FORMAT_BC3_SRGB_BLOCK:
			return DXGI_FORMAT_BC3_UNORM_SRGB;
		case RESOURCE_FORMAT_BC4_UNORM_BLOCK:
			return DXGI_FORMAT_BC4_SNORM;
		case RESOURCE_FORMAT_BC4_SNORM_BLOCK:
			return DXGI_FORMAT_BC4_SNORM;
		case RESOURCE_FORMAT_BC5_UNORM_BLOCK:
			return DXGI_FORMAT_BC5_UNORM;
		case RESOURCE_FORMAT_BC5_SNORM_BLOCK:
			return DXGI_FORMAT_BC5_SNORM;
		case RESOURCE_FORMAT_BC6H_UFLOAT_BLOCK:
			return DXGI_FORMAT_BC6H_UF16;
		case RESOURCE_FORMAT_BC6H_SFLOAT_BLOCK:
			return DXGI_FORMAT_BC6H_SF16;
		case RESOURCE_FORMAT_BC7_UNORM_BLOCK:
			return DXGI_FORMAT_BC7_UNORM;
		case RESOURCE_FORMAT_BC7_SRGB_BLOCK:
			return DXGI_FORMAT_BC7_UNORM_SRGB;
		default:
			return DXGI_FORMAT_UNKNOWN;
	}
}

#endif
#endif /* RENDERING_D3D12_D3D12ENUMS_H_ */