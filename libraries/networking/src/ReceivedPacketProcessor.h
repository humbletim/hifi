//
//  ReceivedPacketProcessor.h
//  libraries/networking/src
//
//  Created by Brad Hefta-Gaub on 8/12/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_ReceivedPacketProcessor_h
#define hifi_ReceivedPacketProcessor_h

#include <QWaitCondition>

#include "GenericThread.h"

/// Generalized threaded processor for handling received inbound packets.
class ReceivedPacketProcessor : public GenericThread {
    Q_OBJECT
public:
    ReceivedPacketProcessor() { }

    /// Add packet from network receive thread to the processing queue.
    void queueReceivedPacket(const SharedNodePointer& sendingNode, const QByteArray& packet);

    /// Are there received packets waiting to be processed
    bool hasPacketsToProcess() const { return _packets.size() > 0; }

    /// Is a specified node still alive?
    bool isAlive(const QUuid& nodeUUID) const {
        return _nodePacketCounts.contains(nodeUUID);
    }

    /// Are there received packets waiting to be processed from a specified node
    bool hasPacketsToProcessFrom(const SharedNodePointer& sendingNode) const {
        return hasPacketsToProcessFrom(sendingNode->getUUID());
    }

    /// Are there received packets waiting to be processed from a specified node
    bool hasPacketsToProcessFrom(const QUuid& nodeUUID) const {
        return _nodePacketCounts[nodeUUID] > 0;
    }

    /// How many received packets waiting are to be processed
    int packetsToProcessCount() const { return _packets.size(); }

    virtual void terminating();

public slots:
    void nodeKilled(SharedNodePointer node);

protected:
    /// Callback for processing of recieved packets. Implement this to process the incoming packets.
    /// \param SharedNodePointer& sendingNode the node that sent this packet
    /// \param QByteArray& the packet to be processed
    virtual void processPacket(const SharedNodePointer& sendingNode, const QByteArray& packet) = 0;

    /// Implements generic processing behavior for this thread.
    virtual bool process();

    /// Determines the timeout of the wait when there are no packets to process. Default value means no timeout
    virtual unsigned long getMaxWait() const { return ULONG_MAX; }

    /// Override to do work before the packets processing loop. Default does nothing.
    virtual void preProcess() { }

    /// Override to do work inside the packet processing loop after a packet is processed. Default does nothing.
    virtual void midProcess() { }

    /// Override to do work after the packets processing loop.  Default does nothing.
    virtual void postProcess() { }

protected:
    std::list<NodePacketPair> _packets;
    QHash<QUuid, int> _nodePacketCounts;

    QWaitCondition _hasPackets;
    QMutex _waitingOnPacketsMutex;
};

#endif // hifi_ReceivedPacketProcessor_h
