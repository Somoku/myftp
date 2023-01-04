#include <gtest/gtest.h>
#include <string>
#include <filesystem>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <fstream>
#include <cstdlib>
#include <fcntl.h>

pid_t startSubProcess(int *writefd, std::string exe, std::vector<std::string> &&args, std::filesystem::path &working_directory) {
    std::filesystem::remove_all(working_directory);
    std::filesystem::create_directory(working_directory);
    int write_fds[2];
    if (writefd)
        if(pipe(write_fds) != 0)
            return -1;
    pid_t status = fork();
    if (status == -1)
        return -2;
    if (status == 0) {
        // child process
        chdir(working_directory.c_str());
        int empty_fd = open("/dev/null", O_RDWR);
        if (writefd) {
            dup2(write_fds[0], STDIN_FILENO);
            close(write_fds[0]);
            close(write_fds[1]);
        } else {
            dup2(empty_fd, STDIN_FILENO);
        }
        dup2(empty_fd, STDOUT_FILENO);
        dup2(empty_fd, STDERR_FILENO);
        close(empty_fd);
        const char** _args = new const char*[args.size() + 1];
        for (int i = 0 ; i < args.size(); i ++) {
            _args[i] = args[i].c_str();
        }
        _args[args.size()] = nullptr;
        system(("killall " + exe).c_str());
        if (execv(exe.c_str(), (char* const*)_args) == -1) {
            std::cerr << "GTest: Failed to start subprocess" << std::endl;
            exit(-1);
        }
    } else {
        if (writefd) {
            *writefd = write_fds[1];
            close(write_fds[0]);
        }
        usleep(10000); // Sleep 10 ms till STDIN/OUT_FILENO Correct
    }
    return status;
}

/**
 * @brief Wait process to exit and return the retval or -1
 * 
 * @param pid 
 * @return int -1 when process not exit
 */
int waitProcessExit(pid_t pid) {
    usleep(10000);
    int status;
    auto retval = waitpid(pid, &status, WNOHANG);
    if(retval != pid) {
        return -1;
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSTOPPED(status))
        std::cerr << "Signal\n";
    return -1;
}

void clearProcess(pid_t pid) {
    kill(pid, SIGKILL);
    usleep(10000);
}

static pthread_once_t once_init = PTHREAD_ONCE_INIT;
static int last_port;
void initRandom() {
    std::ifstream fin("last_port", std::ios::in);
    if (fin) {
        fin >> last_port;
        fin.close();
    } else {
        last_port = 30000;
    }
}

int randPort() {
    pthread_once(&once_init, initRandom);
    last_port = rand() % 100 + last_port + 1;

    std::ofstream fout("last_port", std::ios::out);
    fout << last_port;
    fout.close();

    return last_port;
}

static std::filesystem::path current_dir;
static std::filesystem::path tmp_dir_ser;
static std::filesystem::path tmp_dir_cli;

int prepare(int& client_fd, int& server_port, pid_t& server_pid, pid_t& client_pid, int test_id, int std_client) {
    current_dir = std::filesystem::current_path();
    tmp_dir_ser = current_dir / "tmp_dir_server";
    tmp_dir_cli = current_dir / "tmp_dir_client";

    client_fd = 0;
    server_port = randPort();
    
    if (!std_client)
        server_pid = startSubProcess(nullptr, current_dir / "ftp_server_std", {"", "127.0.0.1", std::to_string(server_port), std::to_string(test_id)}, tmp_dir_ser);
    else
        server_pid = startSubProcess(nullptr, current_dir / "ftp_server", {"", "127.0.0.1", std::to_string(server_port)}, tmp_dir_ser);
    EXPECT_GE(server_pid, 0);

    if (std_client)
        client_pid = startSubProcess(&client_fd, current_dir / "ftp_client_std", {"", std::to_string(test_id)}, tmp_dir_cli);
    else
        client_pid = startSubProcess(&client_fd, current_dir / "ftp_client", {""}, tmp_dir_cli);
    EXPECT_GE(client_pid, 0);

    if (server_pid <= 0 || client_pid <= 0) {
        clearProcess(server_pid);
        clearProcess(client_pid);
        return -1;
    }
    
    usleep(10000);

    return 0;
}

TEST(FTPServer, Open) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 1, 1) != 0)
        return ;
    
    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(10000);

    int code = waitProcessExit(client_pid);
    EXPECT_EQ(code, 1);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

TEST(FTPServer, Auth) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 2, 1) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    cmd_str = "auth user 123123\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    int code = waitProcessExit(client_pid);
    EXPECT_EQ(code, 2);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

TEST(FTPServer, Get) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 3, 1) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    cmd_str = "auth user 123123\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    /** Generate Content **/
    uint32_t random_name = rand() % 1000;
    uint32_t random_content = rand() % 1000;
    std::ofstream fout((tmp_dir_ser / (std::to_string(random_name) + ".txt")).string(), std::ios::out);
    fout << random_content;
    fout.close();
    usleep(10000);
    /** Generate Content **/

    cmd_str = "get " + std::to_string(random_name) + ".txt\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(1000000);

    std::ifstream fin((tmp_dir_cli / (std::to_string(random_name) + ".txt")).string(), std::ios::in);
    uint32_t get_content;
    if (fin) {
        fin >> get_content;
        fin.close();
    } else get_content = ~0;
    EXPECT_EQ(get_content, random_content);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

TEST(FTPServer, Put) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 4, 1) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    cmd_str = "auth user 123123\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    /** Generate Content **/
    uint32_t random_name = rand() % 1000;
    uint32_t random_content = rand() % 1000;
    std::ofstream fout((tmp_dir_cli / (std::to_string(random_name) + ".txt")).string(), std::ios::out);
    fout << random_content;
    fout.close();
    usleep(10000);
    /** Generate Content **/

    cmd_str = "put " + std::to_string(random_name) + ".txt\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(1000000);

    std::ifstream fin((tmp_dir_ser / (std::to_string(random_name) + ".txt")).string(), std::ios::in);
    uint32_t get_content;
    if (fin) {
        fin >> get_content;
        fin.close();
    } else get_content = ~0;
    EXPECT_EQ(get_content, random_content);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

TEST(FTPServer, List) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 5, 1) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    cmd_str = "auth user 123123\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    /** Generate Content **/
    int num_files = rand() % 10;

    while (num_files --) {
        uint32_t random_name = rand() % 1000;
        uint32_t random_content = rand() % 1000;
        std::ofstream fout((tmp_dir_ser / (std::to_string(random_name) + ".txt")).string(), std::ios::out);
        fout << random_content;
        fout.close();
    }
    usleep(10000);
    /** Generate Content **/

    cmd_str = "ls\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(1000000);

    int exitCode = waitProcessExit(client_pid);
    EXPECT_EQ(exitCode, 5);

    FILE* fin = fopen((tmp_dir_cli / "tmp.out").string().c_str(), "r");
    EXPECT_NE(fin, nullptr);
    char buff_list[2048];
    int n = ::fread(buff_list, 1, 2048, fin);
    fclose(fin);

    chdir(tmp_dir_ser.string().c_str());
    FILE* fin_std = popen("ls", "r");
    char buff_list_std[2048];
    int m = ::fread(buff_list_std, 1, 2048, fin_std);
    chdir(current_dir.string().c_str());

    EXPECT_EQ(n, m);
    if (n == m) EXPECT_EQ(memcmp(buff_list_std, buff_list, n), 0);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

TEST(FTPServer, GetBig) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 6, 1) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    cmd_str = "auth user 123123\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    /** Generate Content **/
    uint32_t random_name = rand() % 1000;
    uint8_t random_content = rand() % 10;
    std::ofstream fout((tmp_dir_ser / (std::to_string(random_name) + "qwert" + ".txt")).string(), std::ios::out);
    for(int i=0;i<1048576;i++)
        fout << random_content;
    fout.close();
    usleep(10000);
    /** Generate Content **/

    cmd_str = "get " + std::to_string(random_name) + "qwert" + ".txt\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(1000000);

    std::ifstream fin((tmp_dir_cli / (std::to_string(random_name) + "qwert" + ".txt")).string(), std::ios::in);
    uint8_t get_content;
    /*
    if (fin) {
        fin >> get_content;
        fin.close();
    } else get_content = ~0;
    EXPECT_EQ(get_content, random_content);
    */
   if(fin){
        for(int i=0;i<1048576;i++){
            fin >> get_content;
            EXPECT_EQ(get_content, random_content);
        }
        fin.close();
    }
    else get_content = ~0;
    EXPECT_NE(get_content, ~0);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

TEST(FTPServer, PutBig) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 7, 1) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    cmd_str = "auth user 123123\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    /** Generate Content **/
    uint32_t random_name = rand() % 1000;
    uint8_t random_content = rand() % 10;
    std::ofstream fout((tmp_dir_cli / (std::to_string(random_name) + "zxcvb" + ".txt")).string(), std::ios::out);
    for(int i=0;i<1048576;i++)
        fout << random_content;
    fout.close();
    usleep(10000);
    /** Generate Content **/

    cmd_str = "put " + std::to_string(random_name) + "zxcvb" + ".txt\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(1000000);

    std::ifstream fin((tmp_dir_ser / (std::to_string(random_name) + "zxcvb" + ".txt")).string(), std::ios::in);
    uint8_t get_content;
    /*
    if (fin) {
        fin >> get_content;
        fin.close();
    } else get_content = ~0;
    EXPECT_EQ(get_content, random_content);
    */

    if(fin){
        for(int i=0;i<1048576;i++){
            fin >> get_content;
            EXPECT_EQ(get_content, random_content);
        }
        fin.close();
    }
    else get_content = ~0;
    EXPECT_NE(get_content, ~0);

    clearProcess(client_pid);
    clearProcess(server_pid);
}
/*********** FTP_SERVER ***********/
/*********** FTP_SERVER ***********/























/*********** FTP_CLIENT ***********/
/*********** FTP_CLIENT ***********/

TEST(FTPClient, Open) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 1, 0) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(10000);

    int code = waitProcessExit(server_pid);
    EXPECT_EQ(code, 1);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

TEST(FTPClient, Auth) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 2, 0) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    cmd_str = "auth user 123123\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    int code = waitProcessExit(server_pid);
    EXPECT_EQ(code, 2);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

TEST(FTPClient, Get) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 3, 0) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    cmd_str = "auth user 123123\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    /** Generate Content **/
    uint32_t random_name = rand() % 1000;
    uint32_t random_content = rand() % 1000;
    std::ofstream fout((tmp_dir_ser / (std::to_string(random_name) + ".txt")).string(), std::ios::out);
    fout << random_content;
    fout.close();
    usleep(10000);
    /** Generate Content **/

    cmd_str = "get " + std::to_string(random_name) + ".txt\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(1000000);

    std::ifstream fin((tmp_dir_cli / (std::to_string(random_name) + ".txt")).string(), std::ios::in);
    uint32_t get_content;
    if (fin) {
        fin >> get_content;
        fin.close();
    } else get_content = ~0;
    EXPECT_EQ(get_content, random_content);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

TEST(FTPClient, Put) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 4, 0) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    cmd_str = "auth user 123123\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    /** Generate Content **/
    uint32_t random_name = rand() % 1000;
    uint32_t random_content = rand() % 1000;
    std::ofstream fout((tmp_dir_cli / (std::to_string(random_name) + ".txt")).string(), std::ios::out);
    fout << random_content;
    fout.close();
    usleep(10000);
    /** Generate Content **/

    cmd_str = "put " + std::to_string(random_name) + ".txt\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(1000000);

    std::ifstream fin((tmp_dir_ser / (std::to_string(random_name) + ".txt")).string(), std::ios::in);
    uint32_t get_content;
    if (fin) {
        fin >> get_content;
        fin.close();
    } else get_content = ~0;
    EXPECT_EQ(get_content, random_content);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

TEST(FTPClient, GetBig) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 5, 0) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    cmd_str = "auth user 123123\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    /** Generate Content **/
    uint32_t random_name = rand() % 1000;
    uint8_t random_content = rand() % 10;
    std::ofstream fout((tmp_dir_ser / (std::to_string(random_name) + "abcde" + ".txt")).string(), std::ios::out);
    for(int i=0;i<1048576;i++)
        fout << random_content;
    fout.close();
    usleep(10000);
    /** Generate Content **/

    cmd_str = "get " + std::to_string(random_name) + "abcde" + ".txt\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(1000000);

    std::ifstream fin((tmp_dir_cli / (std::to_string(random_name) + "abcde" + ".txt")).string(), std::ios::in);
    uint8_t get_content;

    if(fin){
        for(int i=0;i<1048576;i++){
            fin >> get_content;
            EXPECT_EQ(get_content, random_content);
        }
        fin.close();
    }
    else get_content = ~0;
    EXPECT_NE(get_content, ~0);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

TEST(FTPClient, PutBig) {
    pid_t server_pid, client_pid;
    int server_port, client_fd;
    std::string cmd_str;

    if (prepare(client_fd, server_port, server_pid, client_pid, 6, 0) != 0)
        return ;

    cmd_str = "open 127.0.0.1 " + std::to_string(server_port) + "\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    cmd_str = "auth user 123123\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(100000);

    /** Generate Content **/
    uint32_t random_name = rand() % 1000;
    uint8_t random_content = rand() % 10;
    std::ofstream fout((tmp_dir_cli / (std::to_string(random_name) + "asdfg" + ".txt")).string(), std::ios::out);
    for(int i=0;i<1048576;i++)
        fout << random_content;
    fout.close();
    usleep(10000);
    /** Generate Content **/

    cmd_str = "put " + std::to_string(random_name) + "asdfg" + ".txt\n";
    write(client_fd, cmd_str.c_str(), cmd_str.length());
    usleep(1000000);

    std::ifstream fin((tmp_dir_ser / (std::to_string(random_name) + "asdfg" + ".txt")).string(), std::ios::in);
    uint8_t get_content;

    if(fin){
        for(int i=0;i<1048576;i++){
            fin >> get_content;
            EXPECT_EQ(get_content, random_content);
        }
        fin.close();
    }
    else get_content = ~0;
    EXPECT_NE(get_content, ~0);

    clearProcess(client_pid);
    clearProcess(server_pid);
}

int _tmain(int argc, wchar_t* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
