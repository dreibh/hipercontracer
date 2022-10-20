// From: https://stackoverflow.com/questions/16396387/asynchronous-reading-on-the-inotify-descriptor-failed

#include <string>
#include <algorithm>
#include <cstring>
#include <assert.h>
#include <sys/signalfd.h>
#include <sys/inotify.h>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <iostream>



//compile with
//-------------
// g++ t4.cc -o t4 -lboost_thread -lboost_exception -lboost_system -lpthread

static int inotifyFd = -1;
static int signalFd = -1;
static boost::asio::io_service gIoSvc;
//A simple test program to test whether signalfd works with boost::asio or not.
static boost::asio::posix::stream_descriptor *gwMqFd = nullptr; //message queue on which the gateway listens for control requests.
static boost::asio::posix::stream_descriptor *inFd = nullptr; //message queue on which the gateway listens for control requests.
static void
handleMqRead(boost::system::error_code ec)
{
    std::cerr<<"\nRecvd signal";
    struct signalfd_siginfo fdsi;
    memset(&fdsi, 0, sizeof(fdsi));
    ssize_t s = ::read(signalFd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)){
        std::cerr<<"read() on signalfd returns inconsistent size.";
        return;
    }
    gwMqFd->async_read_some(boost::asio::null_buffers(),
            boost::bind(&handleMqRead,
                boost::asio::placeholders::error));
    return;
}

#define EVENT_SIZE  (sizeof (struct inotify_event))
#define EVENT_BUF_LEN  (1024*(EVENT_SIZE + 16))
static void
observeFilesystem(boost::system::error_code ec)
{
    std::cerr<<"\nDirectory modified ...";
    char buf[EVENT_BUF_LEN];
    int length = ::read(inotifyFd, buf, sizeof(buf));
    inFd->async_read_some(boost::asio::null_buffers(),
            boost::bind(&observeFilesystem,
                boost::asio::placeholders::error));
    return;
}

int
main(int argc, char* argv[])
{
    try{
        inFd = new boost::asio::posix::stream_descriptor(gIoSvc);
        gwMqFd = new boost::asio::posix::stream_descriptor(gIoSvc);
        sigset_t signalMask;
        sigemptyset(&signalMask);
        sigaddset(&signalMask, SIGCHLD);
        sigaddset(&signalMask, SIGTERM);
        sigprocmask(SIG_BLOCK, &signalMask, nullptr);
        signalFd = signalfd(-1, &signalMask, SFD_NONBLOCK | SFD_CLOEXEC);
        assert(signalFd > 0);
        gwMqFd->assign(signalFd);
        gwMqFd->async_read_some(boost::asio::null_buffers(),
             boost::bind(&handleMqRead,
                boost::asio::placeholders::error));

        inotifyFd = inotify_init();
        assert(inotifyFd > 0);
        inFd->assign(inotifyFd);
        std::string fqpn = ".";
        inotify_add_watch(inotifyFd, fqpn.c_str(), IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_MODIFY);
        inFd->async_read_some(boost::asio::null_buffers(),
             boost::bind(&observeFilesystem,
                boost::asio::placeholders::error));
        gIoSvc.run();
    }catch (std::exception& e){
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
