#include <fstream>
#include <string>

#define IN_DIR 		"input"
#define OUT_DIR 	"output"
#define IN_EXT		".input"
#define OUT_EXT		".output"
#define PATH_DELIM 	"/"
#define MAX_STR_LEN 100

class ServerInputFile {
 public:
	ServerInputFile(const char *hname, int c);
	void gen_updates();
	~ServerInputFile();
	
 private:
	std::fstream ipfile;
	std::string host_name;
	int updates_cnt = 0;

	void _gen_rand_str(std::string& str, size_t len);
};
