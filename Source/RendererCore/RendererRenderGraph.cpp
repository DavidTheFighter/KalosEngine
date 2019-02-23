#include "RendererCore/RendererRenderGraph.h"
#include <RendererCore/RenderGraphRenderPass.h>

RendererRenderGraph::~RendererRenderGraph()
{

}

RenderGraphRenderPass &RendererRenderGraph::addRenderPass(const std::string &name, RenderGraphPipelineType pipelineType)
{
	passes.push_back(std::unique_ptr<RenderGraphRenderPass>(new RenderGraphRenderPass(name, pipelineType)));

	return *passes.back().get();
}

bool RendererRenderGraph::validate()
{
	return true;
}

void RendererRenderGraph::findResourceAliasingCandidates(const std::vector<size_t> &passStack)
{
	// Determine usage lifetimes
	for (size_t i = 0; i < passStack.size(); i++)
	{
		RenderGraphRenderPass &pass = *passes[passStack[i]];

		std::vector<std::string> passResources = pass.getSampledTextureInputs();
		appendVector(passResources, pass.getInputAttachmentInputs());

		for (size_t st = 0; st < pass.getStorageTextures().size(); st++)
			passResources.push_back(pass.getStorageTextures()[st].textureName);

		for (size_t sb = 0; sb < pass.getStorageBuffers().size(); sb++)
			passResources.push_back(pass.getStorageBuffers()[sb].bufferName);

		for (size_t o = 0; o < pass.getColorAttachmentOutputs().size(); o++)
			passResources.push_back(pass.getColorAttachmentOutputs()[o].textureName);

		if (pass.hasDepthStencilOutput())
			passResources.push_back(pass.getDepthStencilAttachmentOutput().textureName);

		for (size_t s = 0; s < passResources.size(); s++)
		{
			if (attachmentUsageLifetimes.count(passResources[s]) == 0)
				attachmentUsageLifetimes[passResources[s]] = glm::uvec2(~0u, 0);

			glm::uvec2 &lifetime = attachmentUsageLifetimes[passResources[s]];
			lifetime = glm::uvec2(std::min<uint32_t>(lifetime.x, i), std::max<uint32_t>(lifetime.y, i));

			if (passResources[s] == outputAttachmentName)
				lifetime.y = ~0u;
		}
	}

	if (enableResourceAliasing)
	{
		// TODO Do resource aliasing
	}
}

void RendererRenderGraph::setRenderGraphOutput(const std::string &textureName)
{
	outputAttachmentName = textureName;
}

const std::string &RendererRenderGraph::getRenderGraphOutput()
{
	return outputAttachmentName;
}

void RendererRenderGraph::addNamedSize(const std::string &sizeName, glm::uvec2 initialSize)
{
	namedSizes[sizeName] = initialSize;
}

glm::uvec2 RendererRenderGraph::getNamedSize(const std::string &sizeName)
{
	return namedSizes[sizeName];
}