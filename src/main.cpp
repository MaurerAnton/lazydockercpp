// lazydockercpp - C++ Docker TUI (port of lazydocker)
// Uses system docker CLI, renders with ANSI escape codes

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>

static const char* VERSION = "0.1.0";
static std::atomic<bool> running(true);
static struct termios origTerm;

static const char *R="\033[0m",*B="\033[1m",*D="\033[2m",*C="\033[36m",*M="\033[35m",*Y="\033[33m",*G="\033[32m",*RD="\033[31m",*W="\033[37m",*BL="\033[34m";

static void enableRaw() { struct termios t; tcgetattr(STDIN_FILENO, &t); origTerm = t; t.c_lflag &= ~(ECHO | ICANON); t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 1; tcsetattr(STDIN_FILENO, TCSANOW, &t); }
static void disableRaw() { tcsetattr(STDIN_FILENO, TCSANOW, &origTerm); }
static int termW() { struct winsize ws; ioctl(0, TIOCGWINSZ, &ws); return ws.ws_col > 0 ? ws.ws_col : 80; }
static int termH() { struct winsize ws; ioctl(0, TIOCGWINSZ, &ws); return ws.ws_row > 0 ? ws.ws_row : 24; }

/* Run docker command, return output */
static std::string docker(const std::string& args) {
    std::string cmd = "docker " + args + " 2>&1";
    FILE* f = popen(cmd.c_str(), "r");
    if (!f) return "";
    char buf[4096]; std::string out;
    while (fgets(buf, sizeof(buf), f)) out += buf;
    pclose(f);
    return out;
}

/* Container info */
struct Container {
    std::string id, name, image, status, ports, cpu, mem;
    std::string created, size;
};

/* Parse docker ps output */
static std::vector<Container> getContainers(bool all) {
    std::vector<Container> list;
    std::string fmt = "{{.ID}}|{{.Names}}|{{.Image}}|{{.Status}}|{{.Ports}}|{{.CreatedAt}}|{{.Size}}";
    if (all) fmt = "{{.ID}}|{{.Names}}|{{.Image}}|{{.Status}}|{{.Ports}}|{{.CreatedAt}}|{{.Size}}";
    std::string args = "ps --no-trunc --format '" + fmt + "'";
    if (all) args = "ps -a --no-trunc --format '" + fmt + "'";
    std::string out = docker(args);
    std::istringstream ss(out); std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        Container c;
        size_t pos = 0; int field = 0;
        while (pos <= line.size()) {
            size_t pipe = line.find('|', pos);
            std::string val = (pipe == std::string::npos) ? line.substr(pos) : line.substr(pos, pipe - pos);
            switch (field++) {
                case 0: c.id = val.substr(0, 12); break;
                case 1: c.name = val; break;
                case 2: c.image = val; break;
                case 3: c.status = val; break;
                case 4: c.ports = val; break;
                case 5: c.created = val; break;
                case 6: c.size = val; break;
            }
            if (pipe == std::string::npos) break;
            pos = pipe + 1;
        }
        if (field >= 3) list.push_back(c);
    }
    return list;
}

/* Get container stats (CPU/mem via docker stats) */
static void getStats(std::map<std::string, std::pair<std::string, std::string>>& stats) {
    std::string out = docker("stats --no-stream --format '{{.Name}}|{{.CPUPerc}}|{{.MemPerc}}'");
    std::istringstream ss(out); std::string line;
    while (std::getline(ss, line)) {
        size_t p1 = line.find('|'); if (p1 == std::string::npos) continue;
        size_t p2 = line.find('|', p1 + 1); if (p2 == std::string::npos) continue;
        std::string name = line.substr(0, p1);
        std::string cpu = line.substr(p1 + 1, p2 - p1 - 1);
        std::string mem = line.substr(p2 + 1);
        stats[name] = {cpu, mem};
    }
}

/* Render containers table */
static void render(const std::vector<Container>& containers, int selected, bool showAll) {
    auto stats = std::map<std::string, std::pair<std::string, std::string>>();
    getStats(stats);

    int w = termW(), h = termH();
    printf("\033[2J\033[H"); /* clear screen */

    /* Header */
    printf("%s%s lazydockercpp %s%s  %s[1-3:view|s:start|x:stop|r:restart|l:logs|a:all|q:quit]%s\n",
           B, C, VERSION, R, D, R);
    printf("%s──%*s%s\n", D, w - 2, "", R);

    /* Column widths */
    int idW = 12, nameW = 20, imgW = 20, statusW = 15, cpuW = 8, memW = 8;
    /* Dynamic adjustment */
    nameW = std::min(nameW, (w - idW - imgW - statusW - cpuW - memW - 10) / 2);
    imgW = w - idW - nameW - statusW - cpuW - memW - 12;
    if (imgW < 5) imgW = 5;

    /* Header row */
    printf("%s%-*s %-*s %-*s %-*s %*s %*s%s\n",
           B, idW, "CONTAINER ID", nameW, "NAME", imgW, "IMAGE",
           statusW, "STATUS", cpuW, "CPU", memW, "MEM", R);

    for (size_t i = 0; i < containers.size(); i++) {
        auto& c = containers[i];
        const char* color = (c.status.find("Up") != std::string::npos) ? G : RD;
        std::string id = c.id; if (id.size() > (size_t)idW) id = id.substr(0, idW);
        std::string name = c.name; if (name.size() > (size_t)nameW) name = name.substr(0, nameW);
        std::string img = c.image; if (img.size() > (size_t)imgW) img = img.substr(0, imgW);
        std::string status = c.status; if (status.size() > (size_t)statusW) status = status.substr(0, statusW);
        auto it = stats.find(c.name);
        std::string cpu = it != stats.end() ? it->second.first : "N/A";
        std::string mem = it != stats.end() ? it->second.second : "N/A";

        if ((int)i == selected) printf("\033[7m"); /* reverse video for selected */
        printf("%s%-*s %-*s %-*s %-*s %*s %*s%s\n",
               color, idW, id.c_str(), nameW, name.c_str(), imgW, img.c_str(),
               statusW, status.c_str(), cpuW, cpu.c_str(), memW, mem.c_str(), R);
        if ((int)i == selected) printf("\033[0m");
    }

    /* Footer */
    printf("\n%s[%s%d/%zu%s containers, selected: %s%d%s]%s\n",
           D, G, (int)containers.size(), containers.size(), D, Y, selected, D, R);
    fflush(stdout);
}

/* Run a docker action and show output */
static void doAction(const std::string& cmd) {
    printf("\033[2J\033[H");
    printf("%s%s> docker %s%s\n\n", B, Y, cmd.c_str(), R);
    std::string out = docker(cmd);
    printf("%s", out.c_str());
    printf("\n%sPress any key to return...%s", D, R);
    fflush(stdout);
    getchar(); /* wait for key */
    getchar(); /* consume escape chars if any */
}

/* Show container logs */
static void showLogs(const std::string& name) {
    printf("\033[2J\033[H%s%s=== Logs: %s ===%s\n\n", B, C, name.c_str(), R);
    std::string out = docker("logs --tail 50 " + name);
    printf("%s", out.c_str());
    printf("\n%sPress any key to return...%s", D, R);
    fflush(stdout);
    getchar(); getchar();
}

/* Inspect container */
static void showInspect(const std::string& name) {
    printf("\033[2J\033[H%s%s=== Inspect: %s ===%s\n\n", B, C, name.c_str(), R);
    std::string out = docker("inspect " + name);
    printf("%s", out.c_str());
    printf("\n%sPress any key to return...%s", D, R);
    fflush(stdout);
    getchar(); getchar();
}

int main() {
    signal(SIGINT, [](int){ running = false; });
    enableRaw();
    atexit(disableRaw);

    bool showAll = false;
    int selected = 0;

    /* Auto-refresh thread */
    std::thread refresh([&]() {
        while (running) {
            auto containers = getContainers(showAll);
            if (selected >= (int)containers.size()) selected = containers.size() - 1;
            if (selected < 0) selected = 0;
            render(containers, selected, showAll);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    });

    /* Main loop — keyboard handling */
    while (running) {
        char c = getchar();
        if (c == 'q' || c == 27) running = false; /* q or ESC */
        else if (c == 'j' || c == 'J') selected++;
        else if (c == 'k' || c == 'K') selected--;
        else if (c == 'a') showAll = !showAll;
        else if (c == '1') { /* inspect */
            auto containers = getContainers(showAll);
            if (selected >= 0 && selected < (int)containers.size())
                showInspect(containers[selected].name);
        }
        else if (c == '2') { /* stats */
            auto containers = getContainers(showAll);
            if (selected >= 0 && selected < (int)containers.size())
                doAction("stats --no-stream " + containers[selected].name);
        }
        else if (c == '3' || c == 'l') { /* logs */
            auto containers = getContainers(showAll);
            if (selected >= 0 && selected < (int)containers.size())
                showLogs(containers[selected].name);
        }
        else if (c == 's') { /* start */
            auto containers = getContainers(showAll);
            if (selected >= 0 && selected < (int)containers.size())
                doAction("start " + containers[selected].name);
        }
        else if (c == 'x') { /* stop */
            auto containers = getContainers(showAll);
            if (selected >= 0 && selected < (int)containers.size())
                doAction("stop " + containers[selected].name);
        }
        else if (c == 'r') { /* restart */
            auto containers = getContainers(showAll);
            if (selected >= 0 && selected < (int)containers.size())
                doAction("restart " + containers[selected].name);
        }
        else if (c == 'd') { /* remove */
            auto containers = getContainers(showAll);
            if (selected >= 0 && selected < (int)containers.size())
                doAction("rm -f " + containers[selected].name);
        }

        /* Clamp selected */
        auto containers = getContainers(showAll);
        if (selected < 0) selected = 0;
        if (selected >= (int)containers.size() && !containers.empty()) selected = containers.size() - 1;
        if (containers.empty()) selected = 0;
    }

    refresh.join();
    printf("\033[2J\033[H"); /* clear on exit */
    return 0;
}
