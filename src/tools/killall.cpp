#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

static std::vector<int> findPids(const std::string &comm)
{
    std::vector<int> out;
    DIR *d = opendir("/proc");
    if (!d)
        return out;
    dirent *e;
    while ((e = readdir(d)) != nullptr) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9')
            continue;
        std::ifstream f(std::string("/proc/") + e->d_name + "/comm");
        std::string name;
        std::getline(f, name);
        if (name == comm)
            out.push_back(std::atoi(e->d_name));
    }
    closedir(d);
    return out;
}

static void killList(const std::vector<int> &pids, int sig)
{
    for (int pid : pids)
        kill(pid, sig);
}

int main(int argc, char **argv)
{
    bool withSession = argc > 1 && std::string(argv[1]) == "--session";

    killList(findPids("winlogin.exe"), SIGUSR1);
    usleep(150000);

    static const char *targets[] = {"winlogin.exe", "explorer.exe", "crashguard"};
    for (const char *t : targets)
        killList(findPids(t), SIGKILL);

    std::printf("explorer-killall: 已在 TTY 上释放所有被囚禁的进程\n");

    if (withSession) {
        const char *bind = std::getenv("EXPLORER_SESSION_KILL");
        if (bind && std::strcmp(bind, "0") != 0) {
            const char *sid = std::getenv("XDG_SESSION_ID");
            if (sid && *sid) {
                std::string cmd = "loginctl terminate-session '" + std::string(sid) + "'";
                std::printf("返回 TTY: %s\n", cmd.c_str());
                std::system(cmd.c_str());
            }
        } else {
            std::printf("跳过会话终止（未设置 EXPLORER_SESSION_KILL=1）\n");
        }
    }
    return 0;
}