
#include "RendererCore/Vulkan/VulkanShaderLoader.h"

#include <Resources/FileLoader.h>

std::string getShaderStageMacroString (VkShaderStageFlagBits stage);

#ifdef __linux__
shaderc_shader_kind getShaderKindFromShaderStage (VkShaderStageFlagBits stage);
std::vector<uint32_t> VulkanShaderLoader::compileGLSL (shaderc::Compiler &compiler, const std::string &file, VkShaderStageFlagBits stages)
{
	std::vector<char> glslSource = readFile(file);

	return compileGLSLFromSource(compiler, glslSource, file, stages);
}

std::vector<uint32_t> VulkanShaderLoader::compileGLSLFromSource (shaderc::Compiler &compiler, const std::string &source, const std::string &sourceName, VkShaderStageFlagBits stages)
{
	return compileGLSLFromSource (compiler, std::vector<char> (source.data(), source.data() + source.length()), sourceName, stages);
}

std::vector<uint32_t> VulkanShaderLoader::compileGLSLFromSource (shaderc::Compiler &compiler, const std::vector<char> &glslSource, const std::string &sourceName, VkShaderStageFlagBits stages)
{
	shaderc::CompileOptions opts;
	opts.AddMacroDefinition(getShaderStageMacroString(stages));

	shaderc::SpvCompilationResult spvComp = compiler.CompileGlslToSpv(std::string(glslSource.data(), glslSource.data() + glslSource.size()), getShaderKindFromShaderStage(stages), sourceName.c_str(), opts);

	if (spvComp.GetCompilationStatus() != shaderc_compilation_status_success)
	{
		printf("%s Failed to compile GLSL shader: %s, shaderc returned: %u, gave message: %s\n", ERR_PREFIX, sourceName.c_str(), spvComp.GetCompilationStatus(), spvComp.GetErrorMessage().c_str());

		throw std::runtime_error("shaderc error - failed compilation");
	}

	return std::vector<uint32_t>(spvComp.cbegin(), spvComp.cend());
}
#elif defined(_WIN32)

std::vector<uint32_t> VulkanShaderLoader::compileGLSL (const std::string &file, VkShaderStageFlagBits stages, ShaderSourceLanguage lang, const std::string &entryPoint)
{
	std::vector<char> glslSource = FileLoader::instance()->readFileBuffer(file);

	return compileGLSLFromSource(glslSource, file, stages, lang, entryPoint);
}

std::vector<uint32_t> VulkanShaderLoader::compileGLSLFromSource (const std::string &source, const std::string &sourceName, VkShaderStageFlagBits stages, ShaderSourceLanguage lang, const std::string &entryPoint)
{
	return compileGLSLFromSource(std::vector<char>(source.data(), source.data() + source.length()), sourceName, stages, lang, entryPoint);
}

std::vector<uint32_t> VulkanShaderLoader::compileGLSLFromSource (const std::vector<char> &source, const std::string &sourceName, VkShaderStageFlagBits stages, ShaderSourceLanguage lang, const std::string &entryPoint)
{
	char tempDir[MAX_PATH];
	GetTempPath(MAX_PATH, tempDir);

	std::string stage = "vert";

	switch (stages)
	{
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			stage = "tesc";
			break;
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			stage = "tese";
			break;
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			stage = "geom";
			break;
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			stage = "frag";
			break;
		case VK_SHADER_STAGE_COMPUTE_BIT:
			stage = "comp";
			break;
		default:
			stage = "vert";
	}


	std::string tempShaderSourceFile = std::string(tempDir) + "starlightengine-shader-" + toString(stringHash(toString(&tempDir) + toString(std::this_thread::get_id()))) + ".glsl." + stage + ".tmp";
	std::string tempShaderOutputFile = std::string(tempDir) + "starlightengine-shader-" + toString(stringHash(toString(&tempDir) + toString(std::this_thread::get_id()))) + ".spv." + stage + ".tmp";

	writeFileCharData(tempShaderSourceFile, source);

	std::vector<std::string> macroDefines;
	macroDefines.push_back(getShaderStageMacroString(stages));
	macroDefines.push_back("PushConstantBuffer=\"[[vk::push_constant]] cbuffer __VK_PushConstantsUniformBuffer_42 : register(b0, space42)\"");

	std::string macroDefinesStr;

	for (size_t i = 0; i < macroDefines.size(); i++)
	{
		macroDefinesStr += " -D";
		macroDefinesStr += macroDefines[i];
	}

	char cmd[4096];
	sprintf(cmd, "glslangValidator -V %s -e %s %s -S %s -o %s %s", (lang == SHADER_LANGUAGE_HLSL ? "-D" : ""), entryPoint.c_str(), macroDefinesStr.c_str(), stage.c_str(), tempShaderOutputFile.c_str(), tempShaderSourceFile.c_str());

	system(cmd);

	std::vector<char> binary = FileLoader::instance()->readFileAbsoluteDirectoryBuffer(tempShaderOutputFile);
	std::vector<uint32_t> spvBinary = std::vector<uint32_t> (binary.size() / 4);

	memcpy(spvBinary.data(), binary.data(), binary.size());

	remove(tempShaderSourceFile.c_str());
	remove(tempShaderOutputFile.c_str());

	return spvBinary;
}

#endif

VkShaderModule VulkanShaderLoader::createVkShaderModule (const VkDevice &device, const std::vector<uint32_t> &spirv)
{
	VkShaderModuleCreateInfo moduleCreateInfo = {};
	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.codeSize = static_cast<uint32_t>(spirv.size() * 4);
	moduleCreateInfo.pCode = spirv.data();

	VkShaderModule module;
	VK_CHECK_RESULT(vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &module));

	return module;
}

#ifdef __linux__
shaderc_shader_kind getShaderKindFromShaderStage (VkShaderStageFlagBits stage)
{
	switch (stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
		return shaderc_vertex_shader;
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		return shaderc_tess_control_shader;
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		return shaderc_tess_evaluation_shader;
		case VK_SHADER_STAGE_GEOMETRY_BIT:
		return shaderc_geometry_shader;
		case VK_SHADER_STAGE_FRAGMENT_BIT:
		return shaderc_fragment_shader;
		case VK_SHADER_STAGE_COMPUTE_BIT:
		return shaderc_compute_shader;
		default:
		return shaderc_glsl_infer_from_source;
	}
}
#endif

std::string getShaderStageMacroString (VkShaderStageFlagBits stage)
{
	switch (stage)
	{
		case VK_SHADER_STAGE_VERTEX_BIT:
			return "SHADER_STAGE_VERTEX";
		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return "SHADER_STAGE_TESSELLATION_CONTROL";
		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return "SHADER_STAGE_TESSELLATION_EVALUATION";
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return "SHADER_STAGE_GEOMETRY";
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return "SHADER_STAGE_FRAGMENT";
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return "SHADER_STAGE_COMPUTE";
		case VK_SHADER_STAGE_ALL:
			return "SHADER_STAGE_ALL";
		default:
			return "SHADER_STAGE_UNDEFINED";
	}
}
