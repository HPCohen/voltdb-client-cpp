/* This file is part of VoltDB.
 * Copyright (C) 2008-2016 VoltDB Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#define __STDC_CONSTANT_MACROS
#define __STDC_LIMIT_MACROS

#include <vector>
#include <boost/shared_ptr.hpp>
#include "Client.h"
#include "Table.h"
#include "TableIterator.h"
#include "Row.hpp"
#include "WireType.h"
#include "Parameter.hpp"
#include "ParameterSet.hpp"
#include "ProcedureCallback.hpp"
#include "ClientConfig.h"
#include "ClientLogger.h"

bool debugEnabled = false;

/*
 * A callback that counts the number of times that it is invoked and returns true
 * when the counter reaches zero to instruct the client library to break out of the event loop.
 */
class CountingCallback : public voltdb::ProcedureCallback {
public:
    CountingCallback(int64_t count) : m_count(count), m_success(0), m_failure(0) {}

    bool callback(voltdb::InvocationResponse response) throw (voltdb::Exception) {
        //m_count--;

        //Print the error response if there was a problem
        if (response.failure()) {
            ++m_failure;
        }
        else {
            ++m_success;
        }
/*
        //If the callback has been invoked count times, return true to break event loop
        if (m_count == 0) {
            std::cout<< "Invocations "<< m_count << ", success " << m_success << "; failure " << m_failure << std::endl;
            //return true;
        } else {
            //return false;
        }
        bool status = false;
        processed++;
        if (numSPCalls - processed < 1000) {
            status = true;
        }
        return status;
        */
        return true;
    }
private:
    int64_t m_count;
    int64_t m_success;
    int64_t m_failure;
};

/*
 * A callback that prints the response it receives and then requests the event loop
 * break
 */
class PrintingCallback : public voltdb::ProcedureCallback {
public:
    bool callback(voltdb::InvocationResponse response) throw (voltdb::Exception) {
        std::cout << response.toString() << std::endl;

        return true;
    }
};

class ConnectionListener : public voltdb::StatusListener {
    public:

    ConnectionListener() : m_connectionActive(false),
                           m_printRateLimited(1000),
                           m_connectionLst(0),
                           m_hasBackpressure(false),
                           m_bpCount(0) {}

    virtual bool uncaughtException(std::exception exception,
                                   boost::shared_ptr<voltdb::ProcedureCallback> callback,
                                   voltdb::InvocationResponse response) {
        std::cout << "uncaught exception: " << exception.what() << std::endl;
        return true;
    }

    virtual bool connectionLost(std::string hostname, unsigned short port, int32_t connectionsLeft) {
        m_connectionActive = false;
        //if ((m_connectionLst % m_printRateLimited == 0) && debugEnabled)
        {
            std::cout << "Connection Lost - hostname: " << hostname << ", port: " << static_cast <unsigned int>(port)
                    << ", # left: " << connectionsLeft <<", # lost: " << m_connectionLst << std::endl;
        }
        ++m_connectionLst;
        return false;
    }

    virtual bool connectionActive(std::string hostname, unsigned short port, int32_t connectionsLeft) {
        m_connectionActive = true;
        //if (debugEnabled)
            std::cout << "Connection Active host: " << hostname << ", port: " << static_cast <int>(port)
                    << ", # left: " << connectionsLeft <<", # active: " << m_connectionLst << std::endl;
        return true;
    }

    virtual bool backpressure(bool hasBackpressure, int32_t outstandingRequests, int32_t maxOutstandingRequests) {
        //if (hasBackpressure != m_hasBackpressure)
        {
            std::ostringstream os;
            ++m_bpCount;
            if (hasBackpressure) {
                if ( (m_bpCount % (m_printRateLimited*1000)) == 0) {
                    os << "BP on: " << hasBackpressure << ", Pend Reqs: " << outstandingRequests <<
                            ", MaxPendReqs: " << maxOutstandingRequests;
                    std::cout << os.str() << std::endl;
                }
            }
            else {
                os << "BP off: " << hasBackpressure << ", Pend Reqs: " << outstandingRequests <<
                        ", MaxPendReqs: " << maxOutstandingRequests;
                std::cout << os.str() << std::endl;
            }

            m_hasBackpressure = hasBackpressure;
        }
        return true;
    }

    bool isConnectionActive() const { return m_connectionActive; }

private:
    bool m_connectionActive;
    const int64_t m_printRateLimited;
    int64_t m_connectionLst;
    bool m_hasBackpressure;
    int32_t m_bpCount;
};

void waitForClusterTobeActive(voltdb::Client &client, ConnectionListener &listner) {
    while (!listner.isConnectionActive()) {
        try {
            client.runOnce();
        }
        catch (const voltdb::NoConnectionsException &excp) {
            std::cout << excp.what() << std::endl;
            assert(false);
            exit(-1);
        }
        catch (const voltdb::Exception &excp) {
            std::cout << "!!!" << excp.what() << std::endl;
            exit(-1);
        }
    }
}


void processReq(voltdb::Client& client, voltdb::Procedure &proc, boost::shared_ptr<CountingCallback>& callback, const std::string &seed) {
    std::ostringstream os;
    int invokeCnt = 0;
    while (++invokeCnt < 10) {
        os.str(""); os << seed << "_" << invokeCnt;
        voltdb::ParameterSet* params = proc.params();
        params->addString(os.str()).addString("Hello").addString("World");
        try {
            client.invoke(proc, callback);
        }
        catch (const voltdb::NoConnectionsException &excp) {
            //waitForClusterTobeActive(client, listner);
        }
    }
    //std::cout << "invoke count " << invokeCnt << " " << static_cast<unsigned long> (pthread_self()) << std::endl;
}

static void *clientThread(void *seedData) {
    /*
     * Instantiate a client and connect to the database.
     * SHA-256 can be used as of VoltDB5.2 by specifying voltdb::HASH_SHA256
     */
    ConnectionListener listner;
    voltdb::ClientConfig config("myusername", "mypassword", &listner, voltdb::HASH_SHA1);
    config.m_enableAbandon = false;
    config.m_maxOutstandingRequests = 1000000;
    voltdb::Client client = voltdb::Client::create(config);
    struct timespec tv, rem;
    int64_t numSPCalls = 0;
    int64_t requests = config.m_maxOutstandingRequests * 10;
    tv.tv_nsec = 10;
    tv.tv_sec = 0;
    memset(&rem, 0, sizeof(rem));

    const std::string lang = std::string((char *)seedData);
    std::ostringstream os;
    os << "THREAD: " << static_cast<unsigned long> (pthread_self()) << ", seed: " << lang << " " << lang.length() <<"\n";
    std::cout << os.str() <<std::endl;

    //for (int i = 0; i < 1; i++)
    {
        //client.createConnection("10.10.183.169", 10002 + (i*1000), true);
        //client.createConnection("localhost", 10002 + (i*1000), true);
        client.createConnection("10.10.183.237", 21212, true);
        client.createConnection("10.10.183.242", 21212, true);
    }
    client.setClientAffinity(true);

    //if (listner.isConnectionActive())
    {
    std::vector<voltdb::Parameter> parameterTypes(3);
    parameterTypes[0] = voltdb::Parameter(voltdb::WIRE_TYPE_STRING);
    parameterTypes[1] = voltdb::Parameter(voltdb::WIRE_TYPE_STRING);
    parameterTypes[2] = voltdb::Parameter(voltdb::WIRE_TYPE_STRING);
    voltdb::Procedure procedure("Insert", parameterTypes);
    boost::shared_ptr<CountingCallback> callback(new CountingCallback(requests));


    /*
     * Load the database.
     */
    int64_t i = 0;
    //while ( numSPCalls < requests ) {
    while ( true ) {
        nanosleep(&tv, &rem);
        voltdb::ParameterSet* params = procedure.params();
        os.str(""); os << lang << ++numSPCalls;
        params->addString(os.str()).addString("Hello").addString("World");

        try {
            client.runOnce();
            //client.invoke(procedure, callback);
            processReq(client, procedure, callback, os.str());
        }
        catch (const voltdb::NoConnectionsException &excp) {
            //waitForClusterTobeActive(client, listner);
        }
        if (numSPCalls * 10 == requests) {
            std::cout <<"\n\n\nquarter way\n\n" << std::endl;
            //sleep(2);
        }
    }

    /*
     * Run the client event loop to poll the network and invoke callbacks.
     * The event loop will break on an error or when a callback returns true
     */
    client.run();
    std::cout << "DONE"<<std::endl;
#if 1
    /*
     * Describe procedure to retrieve message
     */
    parameterTypes.resize( 1, voltdb::Parameter(voltdb::WIRE_TYPE_STRING));
    voltdb::Procedure selectProc("Select", parameterTypes);

    /*
     * Retrieve the message
     */
    selectProc.params()->addString("Spanish");
    try {
        client.invoke(selectProc, boost::shared_ptr<PrintingCallback>(new PrintingCallback()));
    }
    catch (const voltdb::NoConnectionsException &excp) {
        waitForClusterTobeActive(client, listner);
    }

    /*
     * Invoke event loop
     */
    client.run();
#endif
    }
    return 0;
}


#define CLIENT_THREADS 4
int main(int argc, char **argv) {
    std::string dialect[CLIENT_THREADS];
    std::ostringstream os;
    pthread_t thread[CLIENT_THREADS];
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    for (int i = 0; i < CLIENT_THREADS; i++) {
        os.str("");
        os << "eupt" << i;
        dialect[i] = os.str();
        std::cout << dialect[i] <<" ";
    }
    std::cout << std::endl;

    for (int i = 0; i < CLIENT_THREADS; i++) {
        pthread_create(&thread[i], &attr, clientThread, (void *) dialect[i].c_str());
    }

    pthread_attr_destroy(&attr);
    for (int i = 0; i < CLIENT_THREADS; i++) {
        pthread_join(thread[i], NULL);
    }
    pthread_exit(NULL);

    return 0;
}