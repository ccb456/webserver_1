#include "pathInfo.h"

using std::string;

std::string getSrcPath()
{
    // 1. 将程序路径转换为绝对路径（如： /home/ccb/Code/project/webserver_1/build/webserver）
    char exePath[1024];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len == -1) 
    {
#ifdef DEBUG
    std::cout << "getConfigPath readlink failed..." << std::endl;
#endif
        perror("readlink failed");
        exit(EXIT_FAILURE);
    }
    exePath[len] = '\0';

    string root(exePath);

    // 2. 剔除路径中的"build"目录（关键步骤）
    size_t buildPos = root.find("/build");
    if (buildPos != std::string::npos) 
    {
        // 截取到"build"的前一级目录（如：/home/ccb/Code/project/webserver）
        root = root.substr(0, buildPos);
    }
    return root;
}

std::string getConfigPath()
{
    return getSrcPath() + "/config/";
}
