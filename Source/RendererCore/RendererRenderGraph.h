#ifndef RENDERERCORE_RENDERERRENDERGRAPH_H_
#define RENDERERCORE_RENDERERRENDERGRAPH_H_

#include <common.h>
#include <RendererCore/RendererGraphRenderPass.h>

class Renderer;
class RendererGraphRenderPass;

typedef enum
{
	RG_PIPELINE_GRAPHICS = 0,
	RG_PIPELINE_COMPUTE = 1,
	//FG_PIPELINE_TRANSFER = 2,
	RG_PIPELINE_MAX_ENUM
} FrameGraphPipelineType;

class RendererRenderGraph
{
	public:

	virtual ~RendererRenderGraph();

	void setFrameGraphOutput(const std::string &textureName);
	void addNamedSize(const std::string &sizeName, glm::uvec3 initialSize);

	virtual void resizeNamedSize(const std::string &sizeName, glm::uvec2 newSize) = 0;

	RendererGraphRenderPass &addRenderPass(const std::string &name, FrameGraphPipelineType pipelineType);

	bool validate();
	void build(bool doValidation = true);
	virtual Semaphore execute() = 0;

	virtual TextureView getRenderGraphOutputTextureView() = 0;

	protected:

	Renderer *renderer;

	std::string renderGraphOutputAttachment;
	std::vector<std::unique_ptr<RendererGraphRenderPass>> passes;
	std::map<std::string, glm::uvec3> namedSizes;

	std::map<std::string, glm::uvec2> attachmentUsageLifetimes; // A list of usage lifetimes for attachments, in format [firstUsePassIndex, lastUsePassIndex]. Used to determine resource aliasing
	std::vector<std::pair<std::string, std::string>> attachmentAliasingCandidates;

	bool enableSubpassMerging;
	bool enableResourceAliasing;

	void findPotentialResourceAliases(const std::vector<size_t> &passStack);
	virtual void assignPhysicalResources(const std::vector<size_t> &passStack) = 0;
	virtual void finishBuild(const std::vector<size_t> &passStack) = 0;
};

typedef RendererRenderGraph *RenderGraph;

#endif /* RENDERERCORE_FRAMEGRAPH_H_ */