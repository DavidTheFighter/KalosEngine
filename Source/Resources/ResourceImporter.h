#ifndef RESOURCES_RESOURCEIMPORTER_H_
#define RESOURCES_RESOURCEIMPORTER_H_

#include <common.h>

#include <Resources/ResourceManager.h>

class ResourceImporter
{
public:
	ResourceImporter();
	virtual ~ResourceImporter();

	//bool importGLTFModel(const std::string &file, ModelResource *modelResource, tinygltf::TinyGLTF *gltfLoader);

	static ResourceImporter *instance();
	static void setInstance(ResourceImporter *inst);

private:

	static ResourceImporter *resourceImporterInstance;
};

#endif /* RESOURCES_RESOURCEIMPORTER_H_ */