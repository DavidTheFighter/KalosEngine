#include "RendererCore/RenderGraphRenderPass.h"

inline bool attachmentContainsPassInDependencyChain(const std::string &name, size_t pass, const std::vector<std::unique_ptr<RenderGraphRenderPass>> &passes);
inline std::vector<size_t> getPassesThatWriteToResource(const std::string &resourceName, const std::vector<std::unique_ptr<RenderGraphRenderPass>> &passes);
inline bool passContainsPassInDependencyChain(size_t passIndex, size_t writePassIndex, const std::vector<std::unique_ptr<RenderGraphRenderPass>> &passes);
inline bool passAttachmentsHaveSameSize(const RenderPassAttachment& att0, const RenderPassAttachment &att1);

void RendererRenderGraph::build(bool doValidation)
{
	if (doValidation && !validate())
		return;

	if (outputAttachmentName.empty())
	{
		Log::get()->error("RendererRenderGraph: To build a render graph the output must be set using RendererRenderGraph::setFramegraphOutput(..)");
		return;
	}

	std::vector<size_t> passStack;

	recursiveFindWritePasses(outputAttachmentName, passStack);
	std::reverse(passStack.begin(), passStack.end());

	optimizePassOrder(passStack);

	findResourceAliasingCandidates(passStack);
	assignPhysicalResources(passStack);
	finishBuild(passStack);
}

void RendererRenderGraph::recursiveFindWritePasses(const std::string &resourceName, std::vector<size_t> &passStack)
{
	std::vector<size_t> writePasses = getPassesThatWriteToResource(resourceName, passes);

	for (size_t i = 0; i < writePasses.size(); i++)
	{
		auto it = std::find(passStack.begin(), passStack.end(), writePasses[i]);

		if (it != passStack.end())
		{
			if (passStack.size() > (size_t) (it - passStack.begin()))
				passStack.erase(it);
			else
				continue;
		}

		passStack.push_back(writePasses[i]);
		RenderGraphRenderPass &renderPass = *passes[writePasses[i]].get();

		for (size_t j = 0; j < renderPass.getSampledTextureInputs().size(); j++)
			recursiveFindWritePasses(renderPass.getSampledTextureInputs()[j], passStack);

		for (size_t j = 0; j < renderPass.getInputAttachmentInputs().size(); j++)
			recursiveFindWritePasses(renderPass.getInputAttachmentInputs()[j], passStack);

		for (size_t j = 0; j < renderPass.getStorageTextures().size(); j++)
			if (renderPass.getStorageTextures()[j].canReadAsInput)
				recursiveFindWritePasses(renderPass.getStorageTextures()[j].textureName, passStack);
	}
}

void RendererRenderGraph::optimizePassOrder(std::vector<size_t> &passStack)
{
	std::vector<size_t> unscheduledPasses(passStack);
	std::vector<size_t> scheduledPasses;

	scheduledPasses.push_back(passStack.back());
	unscheduledPasses.pop_back();

	while (unscheduledPasses.size() > 0)
	{
		size_t bestCandidate = unscheduledPasses.size() - 1;
		uint32_t bestOverlapScore = 0;

		//printf("Last: %s\n", passes[scheduledPasses.back()]->getNodeName().c_str());

		for (size_t i = 0; i < unscheduledPasses.size(); i++)
		{
			bool possibleCandidate = true;
			// Check to make sure we can even put this particular pass here and that it doesn't break a dependency
			for (size_t j = 0; j < unscheduledPasses.size(); j++)
			{
				if (i == j)
					continue;

				if (passContainsPassInDependencyChain(unscheduledPasses[j], unscheduledPasses[i], passes))
				{
					possibleCandidate = false;

					break;
				}
			}

			if (!possibleCandidate)
				continue;

			uint32_t overlapScore = 0;

			if (enableRenderPassMerging && checkIsMergeValid(scheduledPasses.back(), unscheduledPasses[i], unscheduledPasses))
			{
				overlapScore = ~0u;
			}
			else
			{
				for (int64_t j = int64_t(scheduledPasses.size()) - 1; j >= 0; j--)
				{
					if (passContainsPassInDependencyChain(scheduledPasses[j], unscheduledPasses[i], passes))
						break;

					overlapScore++;
				}
			}

			//printf("\tCandidate: %s, score is %u, best score is %u\n", passes[unscheduledPasses[i]]->getNodeName().c_str(), overlapScore, bestOverlapScore);

			if (overlapScore <= bestOverlapScore)
				continue;

			bestCandidate = i;
			bestOverlapScore = overlapScore;
		}

		scheduledPasses.push_back(unscheduledPasses[bestCandidate]);

		std::move(unscheduledPasses.begin() + bestCandidate + 1, unscheduledPasses.end(), unscheduledPasses.begin() + bestCandidate);
		unscheduledPasses.pop_back();
	}

	std::reverse(scheduledPasses.begin(), scheduledPasses.end());

	passStack = scheduledPasses;
}

bool RendererRenderGraph::checkIsMergeValid(size_t passIndex, size_t writePassIndex, const std::vector<size_t> &passStack)
{
	if (passIndex == writePassIndex)
		return false;

	if (!passContainsPassInDependencyChain(passIndex, writePassIndex, passes))
		return false;

	RenderGraphRenderPass &readPass = *passes[passIndex];
	RenderGraphRenderPass &writePass = *passes[writePassIndex];

	if (readPass.getPipelineType() != RENDER_GRAPH_PIPELINE_TYPE_GRAPHICS || writePass.getPipelineType() != RENDER_GRAPH_PIPELINE_TYPE_GRAPHICS)
		return false;

	std::vector<std::string> passInputs = readPass.getSampledTextureInputs();
	const std::vector<RenderPassStorageTexture> &readPassStorageTextures = readPass.getStorageTextures();

	for (size_t st = 0; st < readPassStorageTextures.size(); st++)
		if (readPassStorageTextures[st].canReadAsInput)
			passInputs.push_back(readPassStorageTextures[st].textureName);

	std::vector<std::string> passInputAtts = readPass.getInputAttachmentInputs();

	/* 
	Check to make sure the outputs of the two passes we're trying to merge have the same sizes
	*/
	std::vector<RenderPassOutputAttachment> passOutputs = readPass.getColorAttachmentOutputs();
	if (readPass.hasDepthStencilOutput())
		passOutputs.push_back(readPass.getDepthStencilAttachmentOutput());

	std::vector<RenderPassOutputAttachment> writePassOutputs = writePass.getColorAttachmentOutputs();
	if (writePass.hasDepthStencilOutput())
		writePassOutputs.push_back(writePass.getDepthStencilAttachmentOutput());

	for (size_t po = 0; po < passOutputs.size(); po++)
		for (size_t wpo = 0; wpo < writePassOutputs.size(); wpo++)
			if (!passAttachmentsHaveSameSize(passOutputs[po].attachment, writePassOutputs[wpo].attachment))
				return false;

	/*
	Check normal inputs first, because if 'writePass' writes to a resource that 'readPass' reads from, but is not an input attachment for 'readPass',
	then we can't merge them. This is because it's not possible for subpasses to write and read from each other when not using input attachments.
	*/
	for (size_t i = 0; i < passInputs.size(); i++)
	{
		for (size_t o = 0; o < writePass.getColorAttachmentOutputs().size(); o++)
			if (writePass.getColorAttachmentOutputs()[o].textureName == passInputs[i])
				return false;
		
		for (size_t o = 0; o < writePass.getStorageTextures().size(); o++)
			if (writePass.getStorageTextures()[o].canWriteAsOutput && writePass.getStorageTextures()[o].textureName == passInputs[i])
				return false;

		if (writePass.hasDepthStencilOutput())
			if (writePass.getDepthStencilAttachmentOutput().textureName == passInputs[i])
				return false;
	}

	// Checks to make sure that 'readPass' doesn't have an input attachment that 'writePass' writes to as a storage texture, which is illegal.
	for (size_t i = 0; i < passInputAtts.size(); i++)
	{
		for (size_t o = 0; o < writePass.getStorageTextures().size(); o++)
			if (writePass.getStorageTextures()[o].canWriteAsOutput && writePass.getStorageTextures()[o].textureName == passInputAtts[i])
				return false;
	}

	/*
	Check to make sure that any passes that 'writePass' could be merged with doesn't write to a resource that 'readPass' reads from, but is not an input
	attachment for 'readPass'. It essentially does the same thing as above but for all of the subpasses lower in the chain.
	*/
	std::vector<std::string> writePassInputs = writePass.getSampledTextureInputs();
	const std::vector<RenderPassStorageTexture> &writePassStorageTextures = writePass.getStorageTextures();

	for (size_t st = 0; st < writePassStorageTextures.size(); st++)
		if (writePassStorageTextures[st].canReadAsInput)
			writePassInputs.push_back(writePassStorageTextures[st].textureName);

	for (size_t p = 0; p < passStack.size(); p++)
	{
		if (checkIsMergeValid(writePassIndex, p, passStack))
		{
			RenderGraphRenderPass &writeWritePass = *passes[p];

			for (size_t i = 0; i < writePassInputs.size(); i++)
			{
				for (size_t o = 0; o < writeWritePass.getColorAttachmentOutputs().size(); o++)
					if (writeWritePass.getColorAttachmentOutputs()[o].textureName == writePassInputs[i])
						return false;

				for (size_t o = 0; o < writeWritePass.getStorageTextures().size(); o++)
					if (writeWritePass.getStorageTextures()[o].canWriteAsOutput && writeWritePass.getStorageTextures()[o].textureName == writePassInputs[i])
						return false;

				if (writeWritePass.hasDepthStencilOutput())
					if (writeWritePass.getDepthStencilAttachmentOutput().textureName == writePassInputs[i])
						return false;
			}
		}
	}

	return true;
}

inline bool attachmentContainsPassInDependencyChain(const std::string &name, size_t pass, const std::vector<std::unique_ptr<RenderGraphRenderPass>> &passes)
{
	std::vector<size_t> writePasses = getPassesThatWriteToResource(name, passes);

	for (size_t i = 0; i < writePasses.size(); i++)
	{
		if (writePasses[i] == pass)
			continue;

		RenderGraphRenderPass &writePass = *passes[writePasses[i]];

		std::vector<std::string> passInputs = writePass.getSampledTextureInputs();
		appendVector(passInputs, writePass.getInputAttachmentInputs());

		const std::vector<RenderPassStorageTexture> &writePassStorageTextures = writePass.getStorageTextures();

		for (size_t st = 0; st < writePassStorageTextures.size(); st++)
			if (writePassStorageTextures[st].canReadAsInput)
				passInputs.push_back(writePassStorageTextures[st].textureName);

		for (size_t wi = 0; wi < passInputs.size(); wi++)
			if (attachmentContainsPassInDependencyChain(passInputs[wi], pass, passes))
				return true;
	}

	return false;
}

inline std::vector<size_t> getPassesThatWriteToResource(const std::string &resourceName, const std::vector<std::unique_ptr<RenderGraphRenderPass>> &passes)
{
	std::vector<size_t> writePasses;

	for (size_t p = 0; p < passes.size(); p++)
	{
		const std::vector<RenderPassOutputAttachment> &colorOutputs = passes[p]->getColorAttachmentOutputs();
		const std::vector<RenderPassStorageTexture> &storageTextures = passes[p]->getStorageTextures();

		for (size_t o = 0; o < colorOutputs.size(); o++)
		{
			if (colorOutputs[o].textureName == resourceName)
			{
				writePasses.push_back(p);
				break;
			}
		}

		for (size_t o = 0; o < storageTextures.size(); o++)
		{
			if (storageTextures[o].canWriteAsOutput && storageTextures[o].textureName == resourceName)
			{
				writePasses.push_back(p);
				break;
			}
		}

		if (passes[p]->hasDepthStencilOutput())
			if (passes[p]->getDepthStencilAttachmentOutput().textureName == resourceName)
				writePasses.push_back(p);
	}

	return writePasses;
}

inline bool passContainsPassInDependencyChain(size_t passIndex, size_t writePassIndex, const std::vector<std::unique_ptr<RenderGraphRenderPass>> &passes)
{
	if (passIndex == writePassIndex)
		return true;

	RenderGraphRenderPass &pass = *passes[passIndex];
	std::vector<std::string> passInputs = pass.getSampledTextureInputs();
	appendVector(passInputs, pass.getInputAttachmentInputs());

	const std::vector<RenderPassStorageTexture> &writePassStorageTextures = pass.getStorageTextures();

	for (size_t st = 0; st < writePassStorageTextures.size(); st++)
		if (writePassStorageTextures[st].canReadAsInput)
			passInputs.push_back(writePassStorageTextures[st].textureName);

	for (size_t i = 0; i < passInputs.size(); i++)
	{
		std::vector<size_t> writePasses = getPassesThatWriteToResource(passInputs[i], passes);

		for (size_t w = 0; w < writePasses.size(); w++)
		{
			if (writePasses[w] == writePassIndex)
				return true;

			if (passContainsPassInDependencyChain(writePasses[w], writePassIndex, passes))
				return true;
		}
	}

	return false;
}

inline bool passAttachmentsHaveSameSize(const RenderPassAttachment& att0, const RenderPassAttachment &att1)
{
	return att0.arrayLayers == att1.arrayLayers &&
		att0.namedRelativeSize == att1.namedRelativeSize &&
		att0.sizeX == att1.sizeX && att0.sizeY == att1.sizeY && att0.sizeZ == att1.sizeZ;
}