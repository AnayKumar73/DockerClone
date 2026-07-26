#include <iostream>
#include <unistd.h>
#include <vector>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/mount.h>
#include <sched.h>
#include "CLI11.hpp"
#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];



int main_container(void* arg);
void write_cgroup_file(const std::string& path, const std::string& value);
std::string setup_cgroup(pid_t pid);
void apply_cgroup_limits(const std::string& cgroup_path);
void add_pid_to_cgroup(const std::string& cgroup_path, pid_t pid);


int main(int argc, char* argv[]) {
    CLI::App app{"DockerClone"};
    auto run{app.add_subcommand("run", "Run a container")};
    bool detatched{false};
    std::string image;

    run->add_option("image", image, "Image name")->required();
    CLI11_PARSE(app, argc, argv);

    
    if (app.got_subcommand("run")) {
        std::cout << "Running " << image << "\n";
    }


    int flags{CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWIPC | CLONE_NEWUSER | SIGCHLD};
    //need to pass stack pointer of top of the stack (grows downwards)
    pid_t pid{clone(main_container, child_stack + STACK_SIZE, flags, (void*)image.data())};
    if(pid < 0) {
        perror("clone failed");
        return 1;
    }

    std::string cgroup_path{setup_cgroup(pid)};
    apply_cgroup_limits(cgroup_path);
    add_pid_to_cgroup(cgroup_path, pid);

    waitpid(pid, nullptr, 0);

    std::filesystem::remove(cgroup_path);
}


int main_container(void* arg) {
    std::string container_path{"container_" + std::to_string(getpid()) + "_playground"};
    //ensures what happens in host doesnt happen here too
    mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr);
    std::filesystem::create_directory(container_path);
    mount(container_path.data(), container_path.data(), nullptr, MS_BIND | MS_REC, nullptr);
    
    
    
    
    std::string binary_path{(char*)arg};
    std::string binary_name{std::filesystem::path(binary_path).filename().string()};
    std::string dest = container_path + "/" + binary_name;
    std::filesystem::copy_file(binary_path, dest, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::permissions(dest, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add);
    
    
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

    std::string exec_path{"/" + binary_name};
    char* args[] = {(char*)exec_path.data(), NULL};
    execvp(args[0], args);
    perror("execvp failed");
    return 1;
}


void write_cgroup_file(const std::string& path, const std::string& value) {
    std::ofstream f(path);
    if(!f) {
        perror("failed to open cgroup path");
        return;
    }
    f << value;
    if(!f) {
        perror("failed to write cgroup path");
    }
}


std::string setup_cgroup(pid_t pid) {
    //adjust the path
    //ensures the parent directory also has the cgroups, and then create for child
    write_cgroup_file("/sys/fs/cgroup/cgroup.subtree_control", "+cpu +memory +pids");

    std::string cgroup_path = "/sys/fs/cgroup/dockerclone_" + std::to_string(pid);
    std::filesystem::create_directory(cgroup_path);
    return cgroup_path;
}


void apply_cgroup_limits(const std::string& cgroup_path) {
    write_cgroup_file(cgroup_path + "/cpu.max", "50000 100000");
    write_cgroup_file(cgroup_path + "/memory.max", "500000000"); //in bytes
    write_cgroup_file(cgroup_path + "/pids.max", "64");
}


void add_pid_to_cgroup(const std::string& cgroup_path, pid_t pid) {
    write_cgroup_file(cgroup_path + "/cgroup.procs", std::to_string(pid));
}