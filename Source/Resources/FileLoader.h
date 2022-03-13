#ifndef RESOURCE_FILELOADER_H_
#define RESOURCE_FILELOADER_H_

#include <string>
#include <vector>
#include <fstream>

/*
A unified class to load files, mainly helps with choosing the right directory to read the file from. It allows for multiple instances
of the same file, such as mod overwriting or patches, and choosing between them.
Directories are searched in this order:
 - For all mods: in <exec_dir>/Mods/<mod_name>/.., and then in that mod's archive files
 - For all patches: in <exec_dir>/GameData/Patches/<patch_name>/.., and then in the patch's archive files
 - In <exec_dir>/..
 - In the main game's loaded archive files
*/
class FileLoader
{
	public:

	FileLoader();
	virtual ~FileLoader();

	/*
	Reads a file and returns its contents. Searches directories as described in the class description.
	*/
	std::string readFile(const std::string &filename);

	/*
	Reads a file and returns its contents as a buffer. Searches directories as described in the class description.
	*/
	std::vector<char> readFileBuffer(const std::string &filename);

	/*
	Reads a file and returns its contents. This doesn't search any local directories and treats <filename> as having a full directory attached to it.
	*/
	std::string readFileAbsoluteDirectory(const std::string &filename);

	/*
	Reads a file and returns its contents as a buffer. This doesn't search any local directories and treats <filename> as having a full directory attached to it.
	*/
	std::vector<char> readFileAbsoluteDirectoryBuffer(const std::string &filename);

	std::ifstream openFileStream(const std::string &filename);

	void setWorkingDir(const std::string &dir);
	std::string getWorkingDir();

	static FileLoader *instance();
	static void setInstance(FileLoader *instance);

	private:

	static FileLoader *fileLoaderInstance;

	// The working/local directory, DOES have a separator on the end of it, e.g "C:\Users\Someone\Documents\"
	std::string workingDir;
};

#endif /* RESOURCE_FILELOADER_H_ */