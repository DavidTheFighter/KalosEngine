#include "RendererCore/RendererRenderGraph.h"

#include <algorithm>

#include <RendererCore/Renderer.h>
#include <RendererCore/RendererGraphHelper.h>

RendererRenderGraph::~RendererRenderGraph()
{

}

bool RendererRenderGraph::validate()
{
	return true;
}

void RendererRenderGraph::build(bool doValidation)
{
	if (doValidation && !validate())
	{
		Log::get()->error("RendererRenderGraph: Failed to validate framegraph with output \"{}\", not building framegraph", renderGraphOutputAttachment);
		return;
	}

	// A list of indices referring to the index of each RendererGraphRenderPass
	std::vector<size_t> passStack;

	auto sT = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());

	recursiveFindWritePasses(renderGraphOutputAttachment, passStack, passes);
	std::reverse(passStack.begin(), passStack.end());

	Log::get()->info("Recursive took: {}ms", (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()) - sT).count() / 1000000.0f);
	
	/*
	printf("\n");
	for (size_t p : passStack)
	{
		printf("%s\n", passes[p]->getNodeName().c_str());
	}
	printf("\n");
	*/
	sT = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());

	optimizePassOrder(passStack, enableSubpassMerging, passes);

	Log::get()->info("Optimize took: {}ms", (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()) - sT).count() / 1000000.0f);

	/*
	printf("\n");
	for (size_t p : passStack)
	{
		printf("%s\n", passes[p]->getNodeName().c_str());
	}
	printf("\n");
	*/
	sT = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());

	findPotentialResourceAliases(passStack);

	Log::get()->info("Resource aliasing took: {}ms", (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()) - sT).count() / 1000000.0f);
	
	sT = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());
	assignPhysicalResources(passStack);
	Log::get()->info("Resource assignment took: {}ms", (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()) - sT).count() / 1000000.0f);
	
	sT = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());
	finishBuild(passStack);
	Log::get()->info("finishBuild() took: {}ms", (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()) - sT).count() / 1000000.0f);

}

void RendererRenderGraph::findPotentialResourceAliases(const std::vector<size_t> &passStack)
{
	std::map<std::string, RenderPassAttachment> allAttachments;

	// Determine usage lifetimes
	for (size_t i = 0; i < passStack.size(); i++)
	{
		RendererGraphRenderPass &pass = *passes[passStack[i]];

		std::vector<std::string> passInputs = pass.getColorInputs();
		appendVector(passInputs, pass.getDepthStencilInputs());
		appendVector(passInputs, pass.getColorFragmentInputAttachments());
		appendVector(passInputs, pass.getDepthStencilFragmentInputAttachments());

		for (size_t s = 0; s < passInputs.size(); s++)
		{
			if (attachmentUsageLifetimes.count(passInputs[s]) == 0)
				attachmentUsageLifetimes[passInputs[s]] = glm::uvec2(~0u, 0);

			glm::uvec2 &lifetime = attachmentUsageLifetimes[passInputs[s]];
			lifetime = glm::uvec2(std::min<uint32_t>(lifetime.x, i), std::max<uint32_t>(lifetime.y, i));
		}
		
		for (size_t s = 0; s < pass.getColorOutputs().size(); s++)
		{
			if (attachmentUsageLifetimes.count(pass.getColorOutputs()[s].first) == 0)
				attachmentUsageLifetimes[pass.getColorOutputs()[s].first] = glm::uvec2(~0u, 0);

			allAttachments[pass.getColorOutputs()[s].first] = pass.getColorOutputs()[s].second.attachment;

			glm::uvec2 &lifetime = attachmentUsageLifetimes[pass.getColorOutputs()[s].first];
			lifetime = glm::uvec2(std::min<uint32_t>(lifetime.x, i), std::max<uint32_t>(lifetime.y, i));
		}

		if (pass.hasDepthStencilOutput())
		{
			if (attachmentUsageLifetimes.count(pass.getDepthStencilOutput().first) == 0)
				attachmentUsageLifetimes[pass.getDepthStencilOutput().first] = glm::uvec2(~0u, 0);

			allAttachments[pass.getDepthStencilOutput().first] = pass.getDepthStencilOutput().second.attachment;

			glm::uvec2 &lifetime = attachmentUsageLifetimes[pass.getDepthStencilOutput().first];
			lifetime = glm::uvec2(std::min<uint32_t>(lifetime.x, i), std::max<uint32_t>(lifetime.y, i));
		}
	}

	// Find the aliasing candidates
	for (auto it0 = allAttachments.begin(); it0 != allAttachments.end(); it0++)
	{
		for (auto it1 = it0; it1 != allAttachments.end(); it1++)
		{
			if (it0->second == it1->second && (attachmentUsageLifetimes[it0->first].x > attachmentUsageLifetimes[it1->first].y || attachmentUsageLifetimes[it0->first].y < attachmentUsageLifetimes[it1->first].x))
			{			
				attachmentAliasingCandidates.push_back(std::make_pair(it0->first, it1->first));

				break;
			}
		}
	}
}

void RendererRenderGraph::addNamedSize(const std::string &sizeName, glm::uvec3 initialSize)
{
	namedSizes[sizeName] = initialSize;
}

glm::uvec3 RendererRenderGraph::getNamedSize(const std::string &name)
{
	return namedSizes[name];
}

void RendererRenderGraph::setFrameGraphOutput(const std::string &textureName)
{
	renderGraphOutputAttachment = textureName;
}

RendererGraphRenderPass &RendererRenderGraph::addRenderPass(const std::string &name, FrameGraphPipelineType pipelineType)
{
	passes.push_back(std::unique_ptr<RendererGraphRenderPass>(new RendererGraphRenderPass(name, pipelineType)));

	return *passes.back().get();
}