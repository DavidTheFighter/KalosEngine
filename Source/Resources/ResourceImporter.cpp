#include "ResourceImporter.h"

ResourceImporter *ResourceImporter::resourceImporterInstance;

ResourceImporter::ResourceImporter()
{

}

ResourceImporter::~ResourceImporter()
{

}

ResourceImporter *ResourceImporter::instance()
{
	return resourceImporterInstance;
}

void ResourceImporter::setInstance(ResourceImporter *inst)
{
	resourceImporterInstance = inst;
}
