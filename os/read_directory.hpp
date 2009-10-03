#ifndef READ_DIRECTORY_H
#define READ_DIRECTORY_H

#include<sys/types.h>
#include<dirent.h>
#include<errno.h>

#include<vector>
#include<string>

class ReadDirectoryError {
private:
	ReadDirectoryError(void); // disallowed!
public:
	std::string error;
	ReadDirectoryError(std::string const& path, std::string const& ne)
		: error(std::string("reading directory ") + path + ": " + ne) { }
};

/*WARNING! not thread safe. protect with locks if necessary*/
std::vector<std::string> read_directory(std::string const& path) {
	std::vector<std::string> rv;
	errno = 0;
	DIR* d = opendir(path.c_str());
	if(!d) {
		std::string error;
		switch(errno) {
		case EACCES:
			error = "Permission denied while opening.";
			break;
		case EMFILE:
			error = "Too many files already opened for process.";
			break;
		case ENFILE:
			error = "Too many files already opened for system.";
			break;
		case ENOENT:
			error = "Directory not found.";
			break;
		case ENOMEM:
			error = "Not enough memory.";
			break;
		case ENOTDIR:
			error = "Not a directory.";
			break;
		default:
			error = "Unknown error while opening directory for reading.";
			break;
		}
		throw ReadDirectoryError(path, error);
	}
	struct dirent* dp;
	while(!(dp = readdir(d))) {
		rv.push_back(std::string(dp->d_name));
	}
	closedir(d);
	return rv;
}

#endif // READ_DIRECTORY_H

