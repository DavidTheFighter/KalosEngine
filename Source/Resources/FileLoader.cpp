#include "FileLoader.h"

#include <common.h>

FileLoader *FileLoader::fileLoaderInstance;

FileLoader::FileLoader()
{
	workingDir = "";
}

FileLoader::~FileLoader()
{

}

std::string FileLoader::readFile(const std::string &filename)
{
	std::ifstream file;

	// Search mod directories

	// Search mod archives

	// Search patch directories

	// Search patch archives

	// Search working directory
	if ((file = openFileStream(workingDir + filename)).is_open())
	{
		size_t fileSize = (size_t) file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return std::string(buffer.data(), buffer.data() + buffer.size());
	}
	
	// Search main game archives

	Log::get()->error("Failed to open file: {}", filename);

	return "";
}

std::vector<char> FileLoader::readFileBuffer(const std::string &filename)
{
	std::ifstream file;

	// Search mod directories

	// Search mod archives

	// Search patch directories

	// Search patch archives

	// Search working directory
	if ((file = openFileStream(workingDir + filename)).is_open())
	{
		size_t fileSize = (size_t) file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}

	// Search main game archives

	Log::get()->error("Failed to open file: {}", filename);

	return {};
}

std::ifstream FileLoader::openFileStream(const std::string &filename)
{
	// Search mod directories

	// Search mod archives

	// Search patch directories

	// Search patch archives

	// Search working directory
	{
#ifdef _WIN32
		std::ifstream file(utf8_to_utf16(filename).c_str(), std::ios::ate | std::ios::binary);
#else
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
#endif

		if (file.is_open())
			return file;
	}

	// Search main game archives

	return std::ifstream();
}

std::string FileLoader::readFileAbsoluteDirectory(const std::string &filename)
{
#ifdef _WIN32
	std::ifstream file(utf8_to_utf16(filename).c_str(), std::ios::ate | std::ios::binary);
#else
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
#endif

	if (!file.is_open())
	{
		Log::get()->error("Failed to open file: {}", filename);

		return "";
	}

	size_t fileSize = (size_t) file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return std::string(buffer.begin(), buffer.end());
}

std::vector<char> FileLoader::readFileAbsoluteDirectoryBuffer(const std::string &filename)
{
#ifdef _WIN32
	std::ifstream file(utf8_to_utf16(filename).c_str(), std::ios::ate | std::ios::binary);
#else
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
#endif

	if (!file.is_open())
	{
		Log::get()->error("Failed to open file: {}", filename);

		return {};
	}

	size_t fileSize = (size_t) file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

void FileLoader::setWorkingDir(const std::string &dir)
{
	workingDir = dir;
}

std::string FileLoader::getWorkingDir()
{
	return workingDir;
}

FileLoader *FileLoader::instance()
{
	return fileLoaderInstance;
}

void FileLoader::setInstance(FileLoader *instance)
{
	fileLoaderInstance = instance;
}
