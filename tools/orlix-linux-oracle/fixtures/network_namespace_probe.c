// SPDX-License-Identifier: MIT

#define _GNU_SOURCE

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

struct child_result {
    char inode_changed;
    char proc_net_readable;
    char rtnetlink_local;
    char route_error_linux_shaped;
};

static void print_observation(const char *name, const char *value)
{
    printf("{\"observation\":\"%s\",\"value\":\"%s\"}\n", name, value);
    fflush(stdout);
}

static int read_file(const char *path, char *buffer, size_t capacity)
{
    int fd;
    ssize_t nread;
    size_t total = 0;

    if (capacity == 0)
        return 0;

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return 0;

    while (total + 1 < capacity) {
        nread = read(fd, buffer + total, capacity - total - 1);
        if (nread < 0) {
            close(fd);
            return 0;
        }
        if (nread == 0)
            break;
        total += (size_t)nread;
    }

    close(fd);
    buffer[total] = '\0';
    return total > 0;
}

static int proc_net_files_are_readable(void)
{
    char buffer[512];

    return read_file("/proc/net/dev", buffer, sizeof(buffer)) &&
        read_file("/proc/net/tcp", buffer, sizeof(buffer)) &&
        read_file("/proc/net/udp", buffer, sizeof(buffer));
}

static int rtnetlink_socket_opens(void)
{
    int fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);

    if (fd < 0)
        return 0;
    close(fd);
    return 1;
}

static int rtnetlink_reports_loopback_link(void)
{
    struct {
        struct nlmsghdr header;
        struct ifinfomsg interface;
    } request = {
        .header = {
            .nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
            .nlmsg_type = RTM_GETLINK,
            .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
            .nlmsg_seq = 1,
        },
        .interface = {
            .ifi_family = AF_UNSPEC,
        },
    };
    char buffer[8192];
    int fd;
    int saw_loopback = 0;

    fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (fd < 0)
        return 0;

    if (send(fd, &request, request.header.nlmsg_len, 0) < 0) {
        close(fd);
        return 0;
    }

    for (;;) {
        ssize_t nread = recv(fd, buffer, sizeof(buffer), 0);
        struct nlmsghdr *header;

        if (nread <= 0)
            break;

        for (header = (struct nlmsghdr *)buffer;
             NLMSG_OK(header, (unsigned int)nread);
             header = NLMSG_NEXT(header, nread)) {
            if (header->nlmsg_type == NLMSG_DONE) {
                close(fd);
                return saw_loopback;
            }
            if (header->nlmsg_type == RTM_NEWLINK) {
                struct ifinfomsg *info = NLMSG_DATA(header);

                if (info->ifi_index == 1)
                    saw_loopback = 1;
            }
        }
    }

    close(fd);
    return saw_loopback;
}

static int rtnetlink_rejects_incomplete_route_request(void)
{
    struct {
        struct nlmsghdr header;
        struct rtmsg route;
    } request = {
        .header = {
            .nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg)),
            .nlmsg_type = RTM_NEWROUTE,
            .nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
            .nlmsg_seq = 7,
        },
        .route = {
            .rtm_family = AF_INET,
            .rtm_table = RT_TABLE_MAIN,
            .rtm_protocol = RTPROT_STATIC,
            .rtm_scope = RT_SCOPE_UNIVERSE,
            .rtm_type = RTN_UNICAST,
        },
    };
    char buffer[512];
    int fd;
    ssize_t nread;
    struct nlmsghdr *header;
    struct nlmsgerr *error;

    fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (fd < 0)
        return 0;

    if (send(fd, &request, request.header.nlmsg_len, 0) < 0) {
        close(fd);
        return 0;
    }

    nread = recv(fd, buffer, sizeof(buffer), 0);
    close(fd);
    if (nread <= 0)
        return 0;

    header = (struct nlmsghdr *)buffer;
    if (!NLMSG_OK(header, (unsigned int)nread) ||
        header->nlmsg_type != NLMSG_ERROR)
        return 0;

    error = NLMSG_DATA(header);
    return error->error < 0;
}

static int loopback_udp_exchanges_datagram(void)
{
    struct sockaddr_in address;
    socklen_t address_len = sizeof(address);
    char byte = 'u';
    char received = '\0';
    int server = -1;
    int client = -1;
    int ok = 0;

    server = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (server < 0)
        goto out;

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = 0;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server, (struct sockaddr *)&address, sizeof(address)) < 0)
        goto out;
    if (getsockname(server, (struct sockaddr *)&address, &address_len) != 0)
        goto out;

    client = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (client < 0)
        goto out;

    if (sendto(client, &byte, 1, 0, (struct sockaddr *)&address,
               sizeof(address)) != 1)
        goto out;
    if (recvfrom(server, &received, 1, 0, NULL, NULL) != 1)
        goto out;

    ok = received == byte;

out:
    if (client >= 0)
        close(client);
    if (server >= 0)
        close(server);
    return ok;
}

static int namespace_inode_changed(const struct stat *before,
                                   const struct stat *after)
{
    return before->st_dev != after->st_dev || before->st_ino != after->st_ino;
}

static struct child_result check_child_network_namespace(void)
{
    struct child_result result = { 0 };
    struct stat before;
    struct stat after;

    if (stat("/proc/self/ns/net", &before) != 0)
        return result;
    if (unshare(CLONE_NEWNET) != 0)
        return result;
    if (stat("/proc/self/ns/net", &after) == 0 &&
        namespace_inode_changed(&before, &after))
        result.inode_changed = 1;

    result.proc_net_readable = proc_net_files_are_readable();
    result.rtnetlink_local = rtnetlink_socket_opens();
    result.route_error_linux_shaped = rtnetlink_rejects_incomplete_route_request();
    return result;
}

static struct child_result fork_and_check_network_namespace(void)
{
    struct child_result result = { 0 };
    int pipefd[2];
    pid_t child;
    int status;

    if (pipe(pipefd) != 0)
        return result;

    child = fork();
    if (child < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return result;
    }

    if (child == 0) {
        struct child_result child_result;

        close(pipefd[0]);
        child_result = check_child_network_namespace();
        (void)write(pipefd[1], &child_result, sizeof(child_result));
        close(pipefd[1]);
        _exit(child_result.inode_changed &&
            child_result.proc_net_readable &&
            child_result.rtnetlink_local &&
            child_result.route_error_linux_shaped ? 0 : 1);
    }

    close(pipefd[1]);
    (void)read(pipefd[0], &result, sizeof(result));
    close(pipefd[0]);

    if (waitpid(child, &status, 0) != child)
        return (struct child_result){ 0 };
    if (!WIFEXITED(status))
        return (struct child_result){ 0 };

    return result;
}

static int observe(const char *name, int ok)
{
    print_observation(name, ok ? "ok" : "fail");
    return ok;
}

int main(void)
{
    int ok = 1;
    struct child_result child;

    ok &= observe("proc-net-readable", proc_net_files_are_readable());
    ok &= observe("rtnetlink-opens", rtnetlink_socket_opens());
    ok &= observe("rtnetlink-loopback-link", rtnetlink_reports_loopback_link());
    ok &= observe("loopback-udp-datagram", loopback_udp_exchanges_datagram());

    child = fork_and_check_network_namespace();
    ok &= observe("child-newnet-inode-changed", child.inode_changed);
    ok &= observe("child-proc-net-readable", child.proc_net_readable);
    ok &= observe("child-rtnetlink-local", child.rtnetlink_local);
    ok &= observe("child-route-error-linux-shaped",
        child.route_error_linux_shaped);

    return ok ? 0 : 1;
}
