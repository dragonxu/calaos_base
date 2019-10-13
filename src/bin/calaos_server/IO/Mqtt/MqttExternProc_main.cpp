/******************************************************************************
 **  Copyright (c) 2006-2018, Calaos. All Rights Reserved.
 **
 **  This file is part of Calaos.
 **
 **  Calaos is free software; you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation; either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Calaos is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Foobar; if not, write to the Free Software
 **  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 **
 ******************************************************************************/
#include "ExternProc.h"
#include <unordered_map>

#include <mosquittopp.h>
#include "Params.h"
#include "Utils.h"


class MqttClient : public mosqpp::mosquittopp
{
public:
    MqttClient(const string &id);

    void on_connect(int rc);
	void on_message(const struct mosquitto_message *message);
    void messageRcv(sigc::slot<void, const struct mosquitto_message *> callback);
	void on_subcribe(int mid, int qos_count, const int *granted_qos);
    void on_error();
    void on_log(int level, const char *str);
    void publishTopic(const string topic, const string payload);

private:
    std::vector<sigc::slot<void, const struct mosquitto_message *>> subscribeCb;
    bool connected = false;
};

MqttClient::MqttClient(const string &id) : mosquittopp(id.c_str())
{
}

void MqttClient::messageRcv(sigc::slot<void, const struct mosquitto_message *> callback)
{
    cDebugDom("mqtt") << "On message";
    // subscribeCb contains a map of list of callbacks, register this callback to the key  relative of this topic
    subscribeCb.push_back(callback);

    if (connected)
    {
        cDebugDom("mqtt") << "Subscribing to topic #";
        // mosquitto subscribe call
        subscribe(NULL, "#");
    }

}

void MqttClient::publishTopic(const string topic, const string payload)
{
    publish(NULL, topic.c_str(), payload.size(), payload.c_str());
}

void MqttClient::on_connect(int rc)
{
    cDebugDom("mqtt") << "Connected with code "  << rc << " : " << strerror(rc);

    if (!rc)
    {
        connected = true;
        cDebugDom("mqtt") << "Subscribing to topic #";
        subscribe(NULL, "#");
    }
}

void MqttClient::on_subcribe(int mid, int qos_count, const int *granted_qos)
{
    cDebugDom("mqtt") << "Subscription succeeded.";
}

void MqttClient::on_message(const struct mosquitto_message *message)
{
    cDebugDom("mqtt") << "New message received";
    ofstream myfile;
    myfile.open ("messages.txt", std::ios_base::app);
    myfile << message->topic << endl;
    myfile << (const char*)message->payload << endl;
    myfile.close();

    // Call all callback registered for this topic
    for(auto cb : subscribeCb)
    {
        cb(message);
    }
}

void MqttClient::on_log(int level, const char *str)
{
    cDebugDom("mqtt") << str;
    ofstream myfile;
    myfile.open ("logs.txt", std::ios_base::app);
    myfile << str << endl;
    myfile.close();

}

void MqttClient::on_error()
{
    cErrorDom("mqtt") << "Error";
    ofstream myfile;
    myfile.open ("errors.txt", std::ios_base::app);
    myfile << "error" << endl;
    myfile.close();

}

class MqttProcess: public ExternProcClient
{
public:

    //needs to be reimplemented
    virtual bool setup(int &argc, char **&argv);
    virtual int procMain();

    EXTERN_PROC_CLIENT_CTOR(MqttProcess)

protected:
    MqttClient *m_client;

    //needs to be reimplemented
    virtual void readTimeout();
    virtual void messageReceived(const string &msg);
};

void MqttProcess::readTimeout()
{
}

void MqttProcess::messageReceived(const string &msg)
{
    json_error_t jerr;
    json_t *jroot = json_loads(msg.c_str(), 0, &jerr);

    if (!jroot)
    {
        cWarningDom("mqtt") << "Error parsing json from sub process: " << jerr.text;
        if (jroot)
            json_decref(jroot);
        return;
    }

    Params p;
    jansson_decode_object(jroot, p);
    m_client->publishTopic(p["topic"].c_str(), p["payload"].c_str());
    cDebugDom("mqtt") << "Message recieved : " << msg;
    json_decref(jroot);
}

bool MqttProcess::setup(int &argc, char **&argv)
{
    string host = "127.0.0.1";
    int port = 1883;
    string username = "";
    string password = "";
    int keepalive = 120;

    cout << "test stdout" << endl;
    cDebugDom("mqtt") << "Mqtt external process";

    if (!connectSocket())
    {
        cError() << "process cannot connect to calaos_server";
        //return false;
    }

    mosqpp::lib_init();

    if (argc != 2)
    {
        cError() << "Unable to read configuration";
        ofstream myfile;
        myfile.open ("out.txt");
        myfile << argv[1] << endl;

        myfile.close();
        return false;
    }

    m_client = new MqttClient("calaos_" + Utils::createRandomUuid());

    json_error_t jerr;
    json_t *jroot = json_loads(argv[1], 0, &jerr);

    if (!jroot)
    {
        ofstream myfile;
        myfile.open ("out.txt");
        myfile << argv[1] << endl;
        myfile << jerr.text << endl;
        myfile.close();

        cError() << jerr.text;
        return false;
    }

    cDebugDom("mqtt") << "argc " << argc << " |  " <<
        argv[0] << " " <<
        argv[1] << " ";

    Params p;
    jansson_decode_object(jroot, p);

    if (p.Exists("host"))
        host = p["host"];
    if (p.Exists("port"))
        from_string(p["port"], port);

    if (p.Exists("keepalive"))
        from_string(p["keepalive"], keepalive);

    if (p.Exists("user") && p.Exists("password"))
    {
        m_client->username_pw_set(p["user"].c_str(), p["password"].c_str());
    }
    cDebugDom("mqtt") << "Connecting to broker " << host << ":" << port;

    int res = m_client->connect_async(host.c_str(), port, keepalive);

    switch (res)
    {
    case MOSQ_ERR_INVAL:
    {
        cErrorDom("mqtt") << "Error connecting to host : " << host;
        ofstream myfile;
        myfile.open ("out.txt");
        myfile << argv[1] << endl;
        myfile << host << endl;
        myfile.close();
        return false;
    }
    case MOSQ_ERR_SUCCESS:
        /* Connect ok ! */
        break;
    default:
    {
        ofstream myfile;
        myfile.open ("out.txt");
        myfile << argv[1] << endl;
        myfile << strerror(res) << endl;
        myfile.close();
        cErrorDom("mqtt") << "Error connecting : " << strerror(res);
        return false;
    }
    }

    m_client->messageRcv([=](const struct mosquitto_message *m)
    {

        json_t *root = json_object();
        json_object_set_new(root, "topic", json_string(m->topic));
        json_object_set_new(root, "payload", json_string((const char*)m->payload));
        cDebugDom("mqtt") << "Send : " << json_dumps(root, 0);
        sendMessage(json_dumps(root, 0));
    });

    return true;
}

int MqttProcess::procMain()
{
    m_client->loop_forever();

    //disconnect mqtt client
    m_client->loop_stop();
    delete m_client;
    return 0;
}

EXTERN_PROC_CLIENT_MAIN(MqttProcess)
