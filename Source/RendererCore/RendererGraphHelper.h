#ifndef RENDERERCORE_RENDERERGRAPHHELPER_H_
#define RENDERERCORE_RENDERERGRAPHHELPER_H_

// Gets a list of all passes that contain a resource as one of their outputs
inline std::vector<size_t> getPassesThatWriteToResource(const std::string &name, const std::vector<std::unique_ptr<RendererGraphRenderPass>> &passes)
{
	std::vector<size_t> writePasses;

	for (size_t p = 0; p < passes.size(); p++)
	{
		std::vector<std::pair<std::string, RenderPassOutputAttachment>> colorOutputs = passes[p]->getColorOutputs();

		for (size_t o = 0; o < colorOutputs.size(); o++)
		{
			if (colorOutputs[o].first == name)
			{
				writePasses.push_back(p);
				break;
			}
		}

		if (passes[p]->hasDepthStencilOutput())
		{
			if (passes[p]->getDepthStencilOutput().first == name)
			{
				writePasses.push_back(p);
				continue;
			}
		}
	}

	return writePasses;
}

/*
Adds all passes that writes to a resource, then recursively adds the passes that write to those passes inputs, etc, etc,
basically traveres the dependency graph. Note it does not check for cyclic dependencies, so check beforehand for those.
*/
inline void recursiveFindWritePasses(const std::string &name, std::vector<size_t> &passStack, const std::vector<std::unique_ptr<RendererGraphRenderPass>> &passes)
{
	std::vector<size_t> writePasses = getPassesThatWriteToResource(name, passes);

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

		//printf("%s\n", passes[writePasses[i]]->getNodeName().c_str());

		passStack.push_back(writePasses[i]);
		RendererGraphRenderPass &renderPass = *passes[writePasses[i]].get();

		for (size_t j = 0; j < renderPass.getColorInputs().size(); j++)
			recursiveFindWritePasses(renderPass.getColorInputs()[j], passStack, passes);

		for (size_t j = 0; j < renderPass.getColorFragmentInputAttachments().size(); j++)
			recursiveFindWritePasses(renderPass.getColorFragmentInputAttachments()[j], passStack, passes);

		for (size_t j = 0; j < renderPass.getDepthStencilInputs().size(); j++)
			recursiveFindWritePasses(renderPass.getDepthStencilInputs()[j], passStack, passes);

		for (size_t j = 0; j < renderPass.getDepthStencilFragmentInputAttachments().size(); j++)
			recursiveFindWritePasses(renderPass.getDepthStencilFragmentInputAttachments()[j], passStack, passes);
	}
}

inline bool passContainsPassInDependencyChain(size_t passIndex, size_t writePassIndex, const std::vector<std::unique_ptr<RendererGraphRenderPass>> &passes)
{
	if (passIndex == writePassIndex)
		return true;

	RendererGraphRenderPass &pass = *passes[passIndex];
	std::vector<std::string> passInputs = pass.getColorInputs();
	appendVector(passInputs, pass.getDepthStencilInputs());
	appendVector(passInputs, pass.getColorFragmentInputAttachments());
	appendVector(passInputs, pass.getDepthStencilFragmentInputAttachments());

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

inline bool attachmentContainsPassInDependencyChain(const std::string &name, size_t pass, const std::vector<std::unique_ptr<RendererGraphRenderPass>> &passes)
{
	std::vector<size_t> writePasses = getPassesThatWriteToResource(name, passes);

	for (size_t i = 0; i < writePasses.size(); i++)
	{
		if (writePasses[i] == pass)
			return true;

		RendererGraphRenderPass &writePass = *passes[writePasses[i]];

		std::vector<std::string> passInputs = writePass.getColorInputs();
		appendVector(passInputs, writePass.getDepthStencilInputs());
		appendVector(passInputs, writePass.getColorFragmentInputAttachments());
		appendVector(passInputs, writePass.getDepthStencilFragmentInputAttachments());

		for (size_t wi = 0; wi < passInputs.size(); wi++)
			if (attachmentContainsPassInDependencyChain(passInputs[wi], pass, passes))
				return true;
	}

	return false;
}

inline bool inputAttachmentHasInvalidSubpassChainToPass(const std::string &name, size_t passIndex, const std::vector<std::unique_ptr<RendererGraphRenderPass>> &passes)
{
	std::vector<size_t> writePasses = getPassesThatWriteToResource(name, passes);

	for (size_t w = 0; w < writePasses.size(); w++)
	{
		RendererGraphRenderPass &pass = *passes[writePasses[w]];

		std::vector<std::string> passInputs = pass.getColorInputs();
		appendVector(passInputs, pass.getDepthStencilInputs());
		appendVector(passInputs, pass.getColorFragmentInputAttachments());
		appendVector(passInputs, pass.getDepthStencilFragmentInputAttachments());

		for (size_t i = 0; i < passInputs.size(); i++)
			if (attachmentContainsPassInDependencyChain(passInputs[i], passIndex, passes))
				return true;
	}

	/*
	If we get here, we either have no chain, or the chain is all input attachments.
	Now we check, if it's an input attachment chain, if they all have the same size
	*/



	return false;
}

inline bool passAttachmentsHaveSameSize(const RenderPassAttachment& att0, const RenderPassAttachment &att1)
{
	return att0.arrayLayers == att1.arrayLayers && 
		att0.namedRelativeSize == att1.namedRelativeSize &&
		att0.sizeX == att1.sizeX && att0.sizeY == att1.sizeY && att0.sizeZ == att1.sizeZ;
}

inline bool checkIsMergeValid(size_t passIndex, size_t writePassIndex, const std::vector<std::unique_ptr<RendererGraphRenderPass>> &passes)
{
	if (passIndex == writePassIndex)
		return false;

	if (!passContainsPassInDependencyChain(passIndex, writePassIndex, passes))
		return false;

	RendererGraphRenderPass &pass = *passes[passIndex];
	RendererGraphRenderPass &writePass = *passes[writePassIndex];

	if (pass.getPipelineType() != writePass.getPipelineType())
		return false;

	for (size_t i = 0; i < writePass.getColorOutputs().size(); i++)
		if (writePass.getColorOutputs()[i].second.genMipsWithPostBlit)
			return false;

	for (size_t i = 0; i < pass.getColorOutputs().size(); i++)
		if (pass.getColorOutputs()[i].second.genMipsWithPostBlit)
			return false;

	std::vector<std::string> depthStencilInputs = pass.getDepthStencilInputs();
	std::vector<std::string> passInputs = pass.getColorInputs();
	passInputs.insert(passInputs.end(), depthStencilInputs.begin(), depthStencilInputs.end());

	std::vector<std::string> depthStencilInputAttachments = pass.getDepthStencilFragmentInputAttachments();
	std::vector<std::string> passInputAtts = pass.getColorFragmentInputAttachments();
	passInputAtts.insert(passInputAtts.end(), depthStencilInputAttachments.begin(), depthStencilInputAttachments.end());

	/*
	Check normal inputs first, because if the writePass that is going to be merged is in this input's depedency chain, then it's impossible
	for them to be merged.
	*/
	for (size_t i = 0; i < passInputs.size(); i++)
		if (attachmentContainsPassInDependencyChain(passInputs[i], writePassIndex, passes))
			return false;

	for (size_t i = 0; i < passInputAtts.size(); i++)
		if (inputAttachmentHasInvalidSubpassChainToPass(passInputAtts[i], writePassIndex, passes))
			return false;

	// Check to make sure the outputs of the two passes we're trying to merge have the same sizes
	auto passOutputs = pass.getColorOutputs();
	if (pass.hasDepthStencilOutput())
		appendVector(passOutputs, { pass.getDepthStencilOutput() });

	auto writePassOutputs = writePass.getColorOutputs();
	if (writePass.hasDepthStencilOutput())
		appendVector(writePassOutputs, { writePass.getDepthStencilOutput() });

	for (size_t po = 0; po < passOutputs.size(); po++)
	{
		for (size_t wpo = 0; wpo < writePassOutputs.size(); wpo++)
		{
			if (!passAttachmentsHaveSameSize(passOutputs[po].second.attachment, writePassOutputs[wpo].second.attachment))
				return false;
		}
	}

	return true;
}

inline void optimizePassOrder(std::vector<size_t> &passStack, bool enablePassMerging, const std::vector<std::unique_ptr<RendererGraphRenderPass>> &passes)
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

			if (enablePassMerging && checkIsMergeValid(scheduledPasses.back(), unscheduledPasses[i], passes))
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

#endif