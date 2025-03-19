#include <csignal>
#include <sys/inotify.h>
#include <sys/prctl.h>
#include "felix86/common/log.hpp"

std::string log_path;

const char* get_log_path() {
    return log_path.c_str();
}

void start_log_server() {
    log_path = "/tmp/felix86-XXXXXX.log";
    int fd = mkstemps(log_path.data(), 4);
    g_output_fd = fd;
    ASSERT(fd != -1);
    int pid = fork();
    if (pid == 0) {
        // This is going to be the logging "server". Basically we don't want to print anything to stdout
        // as applications may read it. So we start a separate process with its own stdout to handle
        // the displaying of messages.
        // When the parent dies (main emulator thread), make sure the logging "server" also dies
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        FILE* f = fdopen(fd, "r");
        int notify_fd = inotify_init();
        int watch = inotify_add_watch(notify_fd, log_path.c_str(), IN_MODIFY);
        size_t index = 0;
        while (true) {
            inotify_event event;
            int result = read(notify_fd, &event, sizeof(inotify_event));
            if (result == -1) {
                if (errno == EAGAIN) {
                    continue;
                } else {
                    ERROR("Logging server got error %d during read?", errno);
                }
            } else if (result != sizeof(inotify_event)) {
                ERROR("Bad size during read?");
            }

            ASSERT(event.wd == watch);
            ASSERT(event.mask == IN_MODIFY);

            // File has been modified with new logs!
            fseek(f, 0, SEEK_END);
            u64 size = ftell(f) - index;
            fseek(f, index, SEEK_SET);

            std::string message(size, 0);
            fread(message.data(), size, 1, f);

            // Print the message to our stdout
            printf("%s", message.c_str());

            index += size;
        }
    }
}