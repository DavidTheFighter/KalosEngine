#ifndef RENDERERCORE_RENDERERRENDERGRAPH_H_
#define RENDERERCORE_RENDERERRENDERGRAPH_H_

#include <RendererCore/renderer_common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class RenderGraphRenderPass;

class RendererRenderGraph
{
	public:

	virtual ~RendererRenderGraph();

	RenderGraphRenderPass &addRenderPass(const std::string &name, RenderGraphPipelineType pipelineType);

	virtual bool validate();

	// Implemented in RendererRenderGraphBuilder.cpp
	virtual void build(bool doValidation = true);

	virtual Semaphore execute(bool returnWaitableSemaphore) = 0;

	void setRenderGraphOutput(const std::string &textureName);
	void addNamedSize(const std::string &sizeName, glm::uvec2 initialSize);

	glm::uvec2 getNamedSize(const std::string &sizeName);
	const std::string &getRenderGraphOutput();

	virtual TextureView getRenderGraphOutputTextureView() = 0;
	
	protected:

	std::vector<std::unique_ptr<RenderGraphRenderPass>> passes;
	std::map<std::string, glm::uvec2> namedSizes;

	std::map<std::string, glm::uvec2> attachmentUsageLifetimes; // A list of usage lifetimes for attachments, in format [firstUsePassIndex, lastUsePassIndex]. Used to determine resource aliasing

	std::string outputAttachmentName;

	bool enableRenderPassMerging; // Whether the merging of passes into subpasses should be allowed
	bool enableResourceAliasing;

	virtual void findResourceAliasingCandidates(const std::vector<size_t> &passStack);
	virtual void assignPhysicalResources(const std::vector<size_t> &passStack) = 0;
	virtual void finishBuild(const std::vector<size_t> &passStack) = 0;
};

typedef RendererRenderGraph *RenderGraph;

#endif /* RENDERERCORE_FRAMEGRAPH_H_ */