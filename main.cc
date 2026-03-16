#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <sys/wait.h>
#include <sched.h>
#include "CLI11.hpp"

#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];

int main(int argc, char* argv[]) {
    //Possible commands: 
    // run, -d (detatched), -p (map to port), -it (interactive terminal)
    // ps
    // start, stop <id>
    // restart <id> 
    CLI::App app{"DockerClone"};

    auto run = app.add_subcommand("run", "Run a container");
    bool detatched = false;
    std::vector<std::string> port_mappings;
    std::string image;

    run->add_flag("-d,--detatch", detatched, "Run in background");
    run->add_option("image", image, "Image name")->required();

    run->add_flag("-p,--publish", port_mappings, "Connect to port");

    CLI11_PARSE(app, argc, argv);
    if (app.got_subcommand("run")) {
        std::cout << "Running " << image << (detatched ? " (detatched)" : "") << "\n";
    }



    int flags = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD;

    //need to pass stack pointer of top of the stack (grows downwards)
    pid_t pid = clone(main_container, child_stack + STACK_SIZE, flags, NULL);
}

int main_container(void* arg) {
    std::cout << "Container initialized. Inside as: " << getpid() << "\n";
    std::string hostname = "container" + getpid();
    sethostname(hostname.data(), hostname.size());
    std::string container_path = "container_playground";
    chdir(container_path.data());
    
    if(chroot(".") != 0) {
        perror("chroot failed");
        return 1;
    }

    char* args[] = {(char*)arg, NULL};
    execvp(args[0], args);
    return 0;
}
