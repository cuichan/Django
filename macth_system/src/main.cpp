// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include "match_server/Match.h"
#include "save_client/Save.h"//自己的头文件用双引号来写
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/ThreadFactory.h>
#include <thrift/TToString.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/transport/TSocket.h>
#include <iostream>
#include <thread>//多线程
#include <mutex>//锁
#include <condition_variable>//条件变量
#include <queue>//消息队列
#include <vector>
#include <unistd.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::save_service;//引入存储服务
using namespace  ::match_service;//引入匹配服务
using namespace std;
struct Task{//定义一个任务，里面有用户和操作种类
    User user;
    string type;
};
struct MesssageQueue{//消息队列
    queue<Task>q;
    mutex m;
    condition_variable cv;
}message_queue;
class Pool{
    public:
        void save_result(int a,int b){
            printf("Match Result : %d %d\n",a,b);
            std::shared_ptr<TTransport> socket(new TSocket("123.57.47.211", 9090));//一直到55行都是官网上引入的接口代码；
            std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));//改成数据段的ip地址
            std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
            SaveClient client(protocol);

            try {
                transport->open();
                client.save_data("acs_3259","e16ca6fb",a,b);//将匹配结果存到myserver上，令牌是mdsum5值
                transport->close();
            } catch (TException& tx) {
                cout << "ERROR: " << tx.what() << endl;
            }
        }
        bool check_match(uint32_t i,uint32_t j)
        {
            auto a=users[i],b=users[j];
            int dt=abs(a.score-b.score);
            int a_max_dif=wt[i]*50;
            int b_max_dif=wt[j]*50;
            return dt<=a_max_dif && dt<=b_max_dif;
        }
        void match()
        {
            for(uint32_t i=0;i<wt.size();i++)
                wt[i]++;//等待秒数 +1;
            while(users.size()>1)
            {
                bool flag=true;
                for(uint32_t i=0;i<users.size();i++)
                {
                for(uint32_t j=i+1;j<users.size();j++)
                 {
                   if(check_match(i,j))
                   {
                       auto a=users[i],b=users[j];
                       users.erase(users.begin()+j);
                       users.erase(users.begin()+i);
                       wt.erase(wt.begin()+j);
                       wt.erase(wt.begin()+i);
                       save_result(a.id,b.id);
                       flag=false;
                       break;
                   }
                 }
                if(!flag)break;
                }
                if(flag)break;
            }
        }
        void add (User user){

            users.push_back(user);
            wt.push_back(0);
        }
        void remove(User user){
            for(uint32_t i=0;i<users.size();i++)
                if(users[i].id==user.id){
                    users.erase(users.begin()+i);
                    wt.erase(wt.begin()+i);
                    break;
                }
        }
    private:
        vector<User>users;
        vector<int>wt;//等待的秒数，单位 s;
}pool;
class MatchHandler : virtual public MatchIf {
    public:
        MatchHandler() {
            // Your initialization goes here
        }

        int32_t add_user(const User& user, const std::string& info) {
            // Your implementation goes here
            printf("add_user\n");
            unique_lock<mutex>lck(message_queue.m);
            message_queue.q.push({user,"add"});
            message_queue.cv.notify_all();
            return 0;
        }

        int32_t remove_user(const User& user, const std::string& info) {
            // Your implementation goes here
            printf("remove_user\n");
            unique_lock<mutex> lck(message_queue.m);//获得锁，直到该任务完成
            message_queue.q.push({user,"remove"});
            message_queue.cv.notify_all();//唤醒操作
            return 0;
        }

};
class MatchCloneFactory : virtual public MatchIfFactory {//加个工厂
    public:
        ~MatchCloneFactory() override = default;
        MatchIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) override
        {
            std::shared_ptr<TSocket> sock = std::dynamic_pointer_cast<TSocket>(connInfo.transport);
          /*  cout << "Incoming connection\n";
            cout << "\tSocketInfo: "  << sock->getSocketInfo() << "\n";
            cout << "\tPeerHost: "    << sock->getPeerHost() << "\n";
            cout << "\tPeerAddress: " << sock->getPeerAddress() << "\n";
            cout << "\tPeerPort: "    << sock->getPeerPort() << "\n";*/
            return new MatchHandler;
        }
        void releaseHandler( ::MatchIf* handler) override {//改成自己的Match
            delete handler;
        }
};

void consume_task(){
    while(true){
        unique_lock<mutex> lck(message_queue.m);
        if(message_queue.q.empty()){
            // message_queue.cv.wait(lck);
            lck.unlock();
            pool.match();
            sleep(1);
        }
        else {
            auto task=message_queue.q.front();
            message_queue.q.pop();
            lck.unlock();//先唤醒它，避免cpu占用率过高
            //do task;
            if(task.type=="add")pool.add(task.user);
            else if (task.type=="remove")pool.remove(task.user);
        }
    }

}


int main(int argc, char **argv) {
    TThreadedServer server(//提高并发量，Simple换成这个
            std::make_shared<MatchProcessorFactory>(std::make_shared<MatchCloneFactory>()),
            std::make_shared<TServerSocket>(9090), //port
            std::make_shared<TBufferedTransportFactory>(),
            std::make_shared<TBinaryProtocolFactory>());


    cout<< "Start Match Server" <<endl;
    thread matching_thread(consume_task);//让这个成为多线程
    server.serve();
    return 0;
}

