#include <iostream>
#include <unistd.h>
#include <vector>
#include <filesystem>
#include <string>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sched.h>
#include "CLI11.hpp"

#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];

int main_container(void* arg);

int main(int argc, char* argv[]) {
    //Possible commands: 
    // run, -d (detatched), -p (map to port), -it (interactive terminal)
    // ps
    // start, stop <id>
    // restart <id> 
    CLI::App app{"DockerClone"};

    auto run{app.add_subcommand("run", "Run a container")};
    bool detatched{false};
    // std::vector<std::string> port_mappings;
    std::string image;

    run->add_flag("-d,--detatch", detatched, "Run in background");
    run->add_option("image", image, "Image name")->required();

    // run->add_option("-p,--publish", port_mappings, "Connect to port");

    CLI11_PARSE(app, argc, argv);
    if (app.got_subcommand("run")) {
        std::cout << "Running " << image << (detatched ? " (detatched)" : "") << "\n";
    }



    int flags{CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD};

    //need to pass stack pointer of top of the stack (grows downwards)
    pid_t pid{clone(main_container, child_stack + STACK_SIZE, flags, (void*)image.data())};

    if (!detatched) {
        waitpid(pid, nullptr, 0);
    }
}

int main_container(void* arg) {
    std::string container_path{"container_" + std::to_string(getpid()) + "_playground"};

    //ensures what happens in host doesnt happen here too
    mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr);

    std::filesystem::create_directory(container_path);

    mount(container_path.data(), container_path.data(), nullptr, MS_BIND | MS_REC, nullptr);

    std::string binary_path{(char*)arg};
    std::string binary_name{std::filesystem::path(binary_path).filename().string()};
    std::string dest{container_path + "/" + binary_name};
    std::filesystem::copy_file(binary_path, dest,
    std::filesystem::copy_options::overwrite_existing);
    std::filesystem::permissions(dest, std::filesystem::perms::owner_exec,
    std::filesystem::perm_options::add);


    std::cout << "Container initialized. Inside as: " << getpid() << "\n";
    std::string hostname{"container" + std::to_string(getpid())};
    sethostname(hostname.data(), hostname.size());

    chdir(container_path.data());

    std::filesystem::create_directory(".pivot_old");
    if (syscall(SYS_pivot_root, ".", ".pivot_old") != 0) {
        perror("pivot_root failed");
        return 1;
    }

    chdir("/");
    if (umount2("/.pivot_old", MNT_DETACH) != 0) {
        perror("umount failed");
        return 1;
    }

    char* args[] = {(char*)arg, NULL};
    execvp(args[0], args);
    perror("execvp failed");
    return 1;
}
