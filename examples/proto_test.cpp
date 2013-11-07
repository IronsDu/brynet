#include <string>
#include <iostream>
#include <assert.h>
#include <stdint.h>

#include "person.pb.h"
#include "test.pb.h"

using namespace std;

class ProtoMsgHandle
{
public:

    /*  注册消息处理函数    */
    void    initHandles()
    {
        registerHandle(&ProtoMsgHandle::handleProtoPerson);
        registerHandle(&ProtoMsgHandle::handleProtoTest);
    }

    /*  处理网络消息  */
    void    handle(const char* data)
    {
        bool ret = false;
        const char* current = data;
        int32_t* p_packetlen = (int32_t*)current;   current += sizeof(*p_packetlen);
        int32_t* p_namelen = (int32_t*)current;     current += sizeof(*p_namelen);

        const char* p_name = current;
        string name(p_name, *p_namelen);            current += *p_namelen;

        do
        {
            msg_handle callback = m_callbacks[name];
            assert(callback != NULL);
            if(callback == NULL)
            {
                break;
            }

            const ::google::protobuf::Descriptor* descriptor = m_descriptors[name];
            assert(descriptor != NULL);
            if(descriptor == NULL)
            {
                break;
            }

            const google::protobuf::Message* prototype = google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
            assert(prototype != NULL);
            if(prototype == NULL)
            {
                break;
            }

            google::protobuf::Message* msg = prototype->New();
            ret = msg->ParseFromArray(current, (*p_packetlen)-sizeof(*p_packetlen)-sizeof(*p_namelen)-*p_namelen);

            if(ret)
            {
                (this->*callback)(msg);
            }

        }while(0);
    }

private:
    void        handleProtoTest(test* test)
    {
        cout << test->price() << endl;
        cout << test->userid() << endl;
        cout << test->time() << endl;
    }

    void        handleProtoPerson(person* person)
    {
        cout << person->age() << endl;
        cout << person->userid() << endl;
        cout << person->name() << endl;
    }

private:
    typedef void (ProtoMsgHandle::*msg_handle)(::google::protobuf::Message*);

private:

    template<typename MSGTYPE>
    void        registerHandle(void (ProtoMsgHandle::*callback)(MSGTYPE*))
    {
        const ::google::protobuf::Descriptor* des = MSGTYPE::descriptor();
        assert(des != NULL);

        if(des != NULL)
        {
            m_callbacks[des->full_name()] = (msg_handle)callback;
            m_descriptors[des->full_name()] = des;
        }
    }

private:
    map<string, msg_handle>                                 m_callbacks;
    map<string, const ::google::protobuf::Descriptor*>      m_descriptors;
};

class ProtoMsgSender
{
public:
    /*  发送proto msg到指定缓冲区   */
    /*  int32_t -   packet_len
        int32_t     name_len
        char[]      name
        char[]      proto_data
    */
    template<typename MSGTYPE>
    void    sentProtoMsg(MSGTYPE& msg, char* buffer, int max_len)
    {
        char* current = buffer;
        int32_t* p_packetlen = (int32_t*)current;                       current += sizeof(*p_packetlen);
        int32_t* p_namelen = (int32_t*)current;                         current += sizeof(*p_namelen);

        string msg_name = MSGTYPE::descriptor()->full_name();
        *p_namelen = msg_name.size();
        strcpy(current, msg_name.c_str());                              current += msg_name.size();

        /*  判断是否成功  */
        msg.SerializeToArray(current, max_len - (current-buffer));      current += msg.GetCachedSize();

        *p_packetlen = (current-buffer);
    }
};

int main()
{
    ProtoMsgSender msgsender;
    ProtoMsgHandle msghandle;
    msghandle.initHandles();

    test t;
    t.set_price(100.0);
    t.set_userid(110);
    t.set_time(123);

    person person;
    person.set_age(18);
    person.set_userid(200508);
    person.set_name("irons");

    char tmp[10*1024];
    msgsender.sentProtoMsg(t, tmp, sizeof(tmp));
    msghandle.handle(tmp);

    msgsender.sentProtoMsg(person, tmp, sizeof(tmp));
    msghandle.handle(tmp);

    cin.get();

    return 0;
}

/*
message test
{
required int32 time = 1;
required int32 userid = 2;
required float price = 3;
optional string desc = 4;
}

message person
{
required int32 age = 1;
required int32 userid = 2;
optional string name = 3;
}
*/
