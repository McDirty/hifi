//
//  MetavoxelTests.cpp
//  tests/metavoxels/src
//
//  Created by Andrzej Kapolka on 2/7/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <stdlib.h>

#include <QScriptValueIterator>

#include <SharedUtil.h>

#include <MetavoxelMessages.h>

#include "MetavoxelTests.h"

REGISTER_META_OBJECT(TestSharedObjectA)
REGISTER_META_OBJECT(TestSharedObjectB)

IMPLEMENT_ENUM_METATYPE(TestSharedObjectA, TestEnum)

MetavoxelTests::MetavoxelTests(int& argc, char** argv) :
    QCoreApplication(argc, argv) {
}

static int datagramsSent = 0;
static int datagramsReceived = 0;
static int bytesSent = 0;
static int bytesReceived = 0;
static int highPriorityMessagesSent = 0;
static int highPriorityMessagesReceived = 0;
static int unreliableMessagesSent = 0;
static int unreliableMessagesReceived = 0;
static int reliableMessagesSent = 0;
static int reliableMessagesReceived = 0;
static int streamedBytesSent = 0;
static int streamedBytesReceived = 0;
static int sharedObjectsCreated = 0;
static int sharedObjectsDestroyed = 0;
static int objectMutationsPerformed = 0;
static int scriptObjectsCreated = 0;
static int scriptMutationsPerformed = 0;

static QByteArray createRandomBytes(int minimumSize, int maximumSize) {
    QByteArray bytes(randIntInRange(minimumSize, maximumSize), 0);
    for (int i = 0; i < bytes.size(); i++) {
        bytes[i] = rand();
    }
    return bytes;
}

static QByteArray createRandomBytes() {
    const int MIN_BYTES = 4;
    const int MAX_BYTES = 16;
    return createRandomBytes(MIN_BYTES, MAX_BYTES);
}

static TestSharedObjectA::TestEnum getRandomTestEnum() {
    switch (randIntInRange(0, 2)) {
        case 0: return TestSharedObjectA::FIRST_TEST_ENUM;
        case 1: return TestSharedObjectA::SECOND_TEST_ENUM;
        case 2:
        default: return TestSharedObjectA::THIRD_TEST_ENUM;
    }
}

static TestSharedObjectA::TestFlags getRandomTestFlags() {
    TestSharedObjectA::TestFlags flags = 0;
    if (randomBoolean()) {
        flags |= TestSharedObjectA::FIRST_TEST_FLAG;
    }
    if (randomBoolean()) {
        flags |= TestSharedObjectA::SECOND_TEST_FLAG;
    }
    if (randomBoolean()) {
        flags |= TestSharedObjectA::THIRD_TEST_FLAG;
    }
    return flags;
}

static QScriptValue createRandomScriptValue(bool complex = false, bool ensureHashOrder = false) {
    scriptObjectsCreated++;
    switch (randIntInRange(0, complex ? 5 : 3)) {
        case 0:
            return QScriptValue(QScriptValue::NullValue);
        
        case 1:
            return QScriptValue(randomBoolean());
        
        case 2:
            return QScriptValue(randFloat());
        
        case 3:
            return QScriptValue(QString(createRandomBytes()));
        
        case 4: {
            int length = randIntInRange(2, 6);
            QScriptValue value = ScriptCache::getInstance()->getEngine()->newArray(length);
            for (int i = 0; i < length; i++) {
                value.setProperty(i, createRandomScriptValue());
            }
            return value;
        }
        default: {
            QScriptValue value = ScriptCache::getInstance()->getEngine()->newObject();
            if (ensureHashOrder) {
                // we can't depend on the iteration order, so if we need it to be the same (as when comparing bytes), we
                // can only have one property
                value.setProperty("foo", createRandomScriptValue());
            } else {
                if (randomBoolean()) {
                    value.setProperty("foo", createRandomScriptValue());
                }
                if (randomBoolean()) {
                    value.setProperty("bar", createRandomScriptValue());
                }
                if (randomBoolean()) {
                    value.setProperty("baz", createRandomScriptValue());
                }
                if (randomBoolean()) {
                    value.setProperty("bong", createRandomScriptValue());
                }
            }
            return value;
        }
    }
}

static TestMessageC createRandomMessageC(bool ensureHashOrder = false) {
    TestMessageC message;
    message.foo = randomBoolean();
    message.bar = rand();
    message.baz = randFloat();
    message.bong.foo = createRandomBytes();
    message.bong.baz = getRandomTestEnum();
    message.bizzle = createRandomScriptValue(true, ensureHashOrder);
    return message;
}

static bool testSerialization(Bitstream::MetadataType metadataType) {
    QByteArray array;
    QDataStream outStream(&array, QIODevice::WriteOnly);
    Bitstream out(outStream, metadataType);
    SharedObjectPointer testObjectWrittenA = new TestSharedObjectA(randFloat(), TestSharedObjectA::SECOND_TEST_ENUM,
        TestSharedObjectA::TestFlags(TestSharedObjectA::FIRST_TEST_FLAG | TestSharedObjectA::THIRD_TEST_FLAG));
    out << testObjectWrittenA;
    SharedObjectPointer testObjectWrittenB = new TestSharedObjectB(randFloat(), createRandomBytes(),
        TestSharedObjectB::THIRD_TEST_ENUM, TestSharedObjectB::SECOND_TEST_FLAG);
    out << testObjectWrittenB;
    TestMessageC messageWritten = createRandomMessageC(true);
    out << QVariant::fromValue(messageWritten);
    QByteArray endWritten = "end";
    out << endWritten;
    out.flush();
    
    QDataStream inStream(array);
    Bitstream in(inStream, metadataType);
    in.addMetaObjectSubstitution("TestSharedObjectA", &TestSharedObjectB::staticMetaObject);
    in.addMetaObjectSubstitution("TestSharedObjectB", &TestSharedObjectA::staticMetaObject);
    in.addTypeSubstitution("TestMessageC", TestMessageA::Type);
    in.addTypeSubstitution("TestSharedObjectA::TestEnum", "TestSharedObjectB::TestEnum");
    in.addTypeSubstitution("TestSharedObjectB::TestEnum", "TestSharedObjectA::TestEnum");
    in.addTypeSubstitution("TestSharedObjectA::TestFlags", "TestSharedObjectB::TestFlags");
    in.addTypeSubstitution("TestSharedObjectB::TestFlags", "TestSharedObjectA::TestFlags");
    SharedObjectPointer testObjectReadA;
    in >> testObjectReadA;
    
    if (!testObjectReadA || testObjectReadA->metaObject() != &TestSharedObjectB::staticMetaObject) {
        qDebug() << "Wrong class for A" << testObjectReadA << metadataType;
        return true;
    }
    if (metadataType == Bitstream::FULL_METADATA && (static_cast<TestSharedObjectA*>(testObjectWrittenA.data())->getFoo() !=
            static_cast<TestSharedObjectB*>(testObjectReadA.data())->getFoo() ||
            static_cast<TestSharedObjectB*>(testObjectReadA.data())->getBaz() != TestSharedObjectB::SECOND_TEST_ENUM ||
            static_cast<TestSharedObjectB*>(testObjectReadA.data())->getBong() !=
                TestSharedObjectB::TestFlags(TestSharedObjectB::FIRST_TEST_FLAG | TestSharedObjectB::THIRD_TEST_FLAG))) {
        QDebug debug = qDebug() << "Failed to transfer shared field from A to B";
        testObjectWrittenA->dump(debug);
        testObjectReadA->dump(debug); 
        return true;
    }
    
    SharedObjectPointer testObjectReadB;
    in >> testObjectReadB;
    if (!testObjectReadB || testObjectReadB->metaObject() != &TestSharedObjectA::staticMetaObject) {
        qDebug() << "Wrong class for B" << testObjectReadB << metadataType;
        return true;
    }
    if (metadataType == Bitstream::FULL_METADATA && (static_cast<TestSharedObjectB*>(testObjectWrittenB.data())->getFoo() !=
            static_cast<TestSharedObjectA*>(testObjectReadB.data())->getFoo() ||
            static_cast<TestSharedObjectA*>(testObjectReadB.data())->getBaz() != TestSharedObjectA::THIRD_TEST_ENUM ||
            static_cast<TestSharedObjectA*>(testObjectReadB.data())->getBong() != TestSharedObjectA::SECOND_TEST_FLAG)) {
        QDebug debug = qDebug() << "Failed to transfer shared field from B to A";
        testObjectWrittenB->dump(debug);
        testObjectReadB->dump(debug); 
        return true;
    }
    
    QVariant messageRead;
    in >> messageRead;
    if (!messageRead.isValid() || messageRead.userType() != TestMessageA::Type) {
        qDebug() << "Wrong type for message" << messageRead;
        return true;
    }
    if (metadataType == Bitstream::FULL_METADATA && messageWritten.foo != messageRead.value<TestMessageA>().foo) {
        QDebug debug = qDebug() << "Failed to transfer shared field between messages" <<
            messageWritten.foo << messageRead.value<TestMessageA>().foo;
        return true;
    }
    
    QByteArray endRead;
    in >> endRead;
    if (endWritten != endRead) {
        qDebug() << "End tag mismatch." << endRead;
        return true;
    }
    
    // go back to the beginning and read everything as generics
    inStream.device()->seek(0);
    Bitstream genericIn(inStream, metadataType, Bitstream::ALL_GENERICS);
    genericIn >> testObjectReadA;
    genericIn >> testObjectReadB;
    genericIn >> messageRead;
    genericIn >> endRead;
    
    // reassign the ids
    testObjectReadA->setID(testObjectWrittenA->getID());
    testObjectReadA->setOriginID(testObjectWrittenA->getOriginID());
    testObjectReadB->setID(testObjectWrittenB->getID());
    testObjectReadB->setOriginID(testObjectWrittenB->getOriginID());
    
    // write it back out and compare
    QByteArray compareArray;
    QDataStream compareOutStream(&compareArray, QIODevice::WriteOnly);
    Bitstream compareOut(compareOutStream, metadataType);
    compareOut << testObjectReadA;
    compareOut << testObjectReadB;
    compareOut << messageRead;
    compareOut << endRead;
    compareOut.flush();
    
    if (array != compareArray) {
        qDebug() << "Mismatch between written/generic written streams.";
        return true;
    }
    
    if (metadataType != Bitstream::FULL_METADATA) {
        return false;
    }
    
    // now write to JSON
    JSONWriter jsonWriter;
    jsonWriter << testObjectReadA;
    jsonWriter << testObjectReadB;
    jsonWriter << messageRead;
    jsonWriter << endRead;
    QByteArray encodedJson = jsonWriter.getDocument().toJson();
    
    // and read from JSON
    JSONReader jsonReader(QJsonDocument::fromJson(encodedJson), Bitstream::ALL_GENERICS);
    jsonReader >> testObjectReadA;
    jsonReader >> testObjectReadB;
    jsonReader >> messageRead;
    jsonReader >> endRead;
    
    // reassign the ids
    testObjectReadA->setID(testObjectWrittenA->getID());
    testObjectReadA->setOriginID(testObjectWrittenA->getOriginID());
    testObjectReadB->setID(testObjectWrittenB->getID());
    testObjectReadB->setOriginID(testObjectWrittenB->getOriginID());
    
    // and back to binary
    QByteArray secondCompareArray;
    QDataStream secondCompareOutStream(&secondCompareArray, QIODevice::WriteOnly);
    Bitstream secondCompareOut(secondCompareOutStream, Bitstream::FULL_METADATA);
    secondCompareOut << testObjectReadA;
    secondCompareOut << testObjectReadB;
    secondCompareOut << messageRead;
    secondCompareOut << endRead;
    secondCompareOut.flush();
    
    if (compareArray != secondCompareArray) {
        qDebug() << "Mismatch between written/JSON streams (generics).";
        return true;
    }
    
    // once more, with mapping!
    JSONReader secondJSONReader(QJsonDocument::fromJson(encodedJson));
    secondJSONReader >> testObjectReadA;
    secondJSONReader >> testObjectReadB;
    secondJSONReader >> messageRead;
    secondJSONReader >> endRead;
    
    // reassign the ids
    testObjectReadA->setID(testObjectWrittenA->getID());
    testObjectReadA->setOriginID(testObjectWrittenA->getOriginID());
    testObjectReadB->setID(testObjectWrittenB->getID());
    testObjectReadB->setOriginID(testObjectWrittenB->getOriginID());
    
    // and back to binary
    QByteArray thirdCompareArray;
    QDataStream thirdCompareOutStream(&thirdCompareArray, QIODevice::WriteOnly);
    Bitstream thirdCompareOut(thirdCompareOutStream, Bitstream::FULL_METADATA);
    thirdCompareOut << testObjectReadA;
    thirdCompareOut << testObjectReadB;
    thirdCompareOut << messageRead;
    thirdCompareOut << endRead;
    thirdCompareOut.flush();
    
    if (compareArray != thirdCompareArray) {
        qDebug() << "Mismatch between written/JSON streams (mapped).";
        return true;
    }
    
    return false;
}

bool MetavoxelTests::run() {
    // seed the random number generator so that our tests are reproducible
    srand(0xBAAAAABE);

    // check for an optional command line argument specifying a single test
    QStringList arguments = this->arguments();
    int test = (arguments.size() > 1) ? arguments.at(1).toInt() : 0;

    QByteArray datagramHeader("testheader");
    const int SIMULATION_ITERATIONS = 10000;
    if (test == 0 || test == 1) {
        qDebug() << "Running transmission tests...";
        qDebug();
    
        // create two endpoints with the same header
        Endpoint alice(datagramHeader), bob(datagramHeader);
        
        alice.setOther(&bob);
        bob.setOther(&alice);
        
        // perform a large number of simulation iterations
        for (int i = 0; i < SIMULATION_ITERATIONS; i++) {
            if (alice.simulate(i) || bob.simulate(i)) {
                return true;
            }
        }
        
        qDebug() << "Sent" << highPriorityMessagesSent << "high priority messages, received" << highPriorityMessagesReceived;
        qDebug() << "Sent" << unreliableMessagesSent << "unreliable messages, received" << unreliableMessagesReceived;
        qDebug() << "Sent" << reliableMessagesSent << "reliable messages, received" << reliableMessagesReceived;
        qDebug() << "Sent" << streamedBytesSent << "streamed bytes, received" << streamedBytesReceived;
        qDebug() << "Sent" << datagramsSent << "datagrams with" << bytesSent << "bytes, received" <<
            datagramsReceived << "with" << bytesReceived << "bytes";
        qDebug() << "Created" << sharedObjectsCreated << "shared objects, destroyed" << sharedObjectsDestroyed;
        qDebug() << "Performed" << objectMutationsPerformed << "object mutations";
        qDebug() << "Created" << scriptObjectsCreated << "script objects, mutated" << scriptMutationsPerformed;
        qDebug();
    }
    
    if (test == 0 || test == 2) {
        qDebug() << "Running serialization tests...";
        qDebug();
        
        if (testSerialization(Bitstream::HASH_METADATA) || testSerialization(Bitstream::FULL_METADATA)) {
            return true;
        }
    }
    
    if (test == 0 || test == 3) {
        qDebug() << "Running metavoxel data tests...";
        qDebug();
    
        // clear the stats
        datagramsSent = bytesSent = datagramsReceived = bytesReceived = 0;
    
        // create client and server endpoints
        Endpoint client(datagramHeader, Endpoint::METAVOXEL_CLIENT_MODE);
        Endpoint server(datagramHeader, Endpoint::METAVOXEL_SERVER_MODE);
        
        client.setOther(&server);
        server.setOther(&client);
        
        // simulate
        for (int i = 0; i < SIMULATION_ITERATIONS; i++) {
            if (client.simulate(i) || server.simulate(i)) {
                return true;
            }
        }
        
        qDebug() << "Sent" << datagramsSent << "datagrams with" << bytesSent << "bytes, received" <<
            datagramsReceived << "with" << bytesReceived << "bytes";
    }
    
    qDebug() << "All tests passed!";
    
    return false;
}

static SharedObjectPointer createRandomSharedObject() {
    switch (randIntInRange(0, 2)) {
        case 0: return new TestSharedObjectA(randFloat(), getRandomTestEnum(), getRandomTestFlags());
        case 1: return new TestSharedObjectB();
        case 2:
        default: return SharedObjectPointer();
    }
}

Endpoint::Endpoint(const QByteArray& datagramHeader, Mode mode) :
    _mode(mode),
    _sequencer(new DatagramSequencer(datagramHeader, this)),
    _highPriorityMessagesToSend(0.0f),
    _reliableMessagesToSend(0.0f) {
    
    connect(_sequencer, SIGNAL(readyToWrite(const QByteArray&)), SLOT(sendDatagram(const QByteArray&)));
    connect(_sequencer, SIGNAL(readyToRead(Bitstream&)), SLOT(readMessage(Bitstream&)));
    connect(_sequencer, SIGNAL(receivedHighPriorityMessage(const QVariant&)),
        SLOT(handleHighPriorityMessage(const QVariant&)));
    
    connect(_sequencer, SIGNAL(sendAcknowledged(int)), SLOT(clearSendRecordsBefore(int)));
    connect(_sequencer, SIGNAL(receiveAcknowledged(int)), SLOT(clearReceiveRecordsBefore(int)));
    
    // insert the baseline send record
    SendRecord sendRecord = { 0 };
    _sendRecords.append(sendRecord);
    
    // insert the baseline receive record
    ReceiveRecord receiveRecord = { 0 };
    _receiveRecords.append(receiveRecord);
    
    if (mode == METAVOXEL_CLIENT_MODE) {
        _lod = MetavoxelLOD(glm::vec3(), 0.01f);
        return;
    }
    if (mode == METAVOXEL_SERVER_MODE) {
        return;
    }
    // create the object that represents out delta-encoded state
    _localState = new TestSharedObjectA();
    
    connect(_sequencer->getReliableInputChannel(), SIGNAL(receivedMessage(const QVariant&)),
        SLOT(handleReliableMessage(const QVariant&)));
    
    ReliableChannel* secondInput = _sequencer->getReliableInputChannel(1);
    secondInput->setMessagesEnabled(false);
    connect(&secondInput->getBuffer(), SIGNAL(readyRead()), SLOT(readReliableChannel()));
    
    // enqueue a large amount of data in a low-priority channel
    ReliableChannel* output = _sequencer->getReliableOutputChannel(1);
    output->setPriority(0.25f);
    output->setMessagesEnabled(false);
    const int MIN_STREAM_BYTES = 100000;
    const int MAX_STREAM_BYTES = 200000;
    QByteArray bytes = createRandomBytes(MIN_STREAM_BYTES, MAX_STREAM_BYTES);
    _dataStreamed.append(bytes);
    output->getBuffer().write(bytes);
    streamedBytesSent += bytes.size();    
}

static QVariant createRandomMessage() {
    switch (randIntInRange(0, 2)) {
        case 0: {
            TestMessageA message = { randomBoolean(), rand(), randFloat() };
            return QVariant::fromValue(message);
        }
        case 1: {
            TestMessageB message = { createRandomBytes(), createRandomSharedObject(), getRandomTestEnum() };
            return QVariant::fromValue(message); 
        }
        default: {
            return QVariant::fromValue(createRandomMessageC());
        }
    }
}

static SharedObjectPointer mutate(const SharedObjectPointer& state) {
    switch (randIntInRange(0, 4)) {
        case 0: {
            SharedObjectPointer newState = state->clone(true);
            static_cast<TestSharedObjectA*>(newState.data())->setFoo(randFloat());
            objectMutationsPerformed++;
            return newState;
        }
        case 1: {
            SharedObjectPointer newState = state->clone(true);
            static_cast<TestSharedObjectA*>(newState.data())->setBaz(getRandomTestEnum());
            objectMutationsPerformed++;
            return newState;
        }   
        case 2: {
            SharedObjectPointer newState = state->clone(true);
            static_cast<TestSharedObjectA*>(newState.data())->setBong(getRandomTestFlags());
            objectMutationsPerformed++;
            return newState;
        }
        case 3: {
            SharedObjectPointer newState = state->clone(true);
            QScriptValue oldValue = static_cast<TestSharedObjectA*>(newState.data())->getBizzle();
            QScriptValue newValue = ScriptCache::getInstance()->getEngine()->newObject();
            for (QScriptValueIterator it(oldValue); it.hasNext(); ) {
                it.next();
                newValue.setProperty(it.scriptName(), it.value());
            }
            switch (randIntInRange(0, 2)) {
                case 0: {
                    QScriptValue oldArray = oldValue.property("foo");
                    int oldLength = oldArray.property(ScriptCache::getInstance()->getLengthString()).toInt32();
                    QScriptValue newArray = ScriptCache::getInstance()->getEngine()->newArray(oldLength);
                    for (int i = 0; i < oldLength; i++) {
                        newArray.setProperty(i, oldArray.property(i));
                    }
                    newArray.setProperty(randIntInRange(0, oldLength - 1), createRandomScriptValue(true));
                    break;
                }
                case 1:
                    newValue.setProperty("bar", QScriptValue(randFloat()));
                    break;
                    
                default:
                    newValue.setProperty("baz", createRandomScriptValue(true));
                    break;
            }
            static_cast<TestSharedObjectA*>(newState.data())->setBizzle(newValue);
            scriptMutationsPerformed++;
            objectMutationsPerformed++;
            return newState;
        }
        default:
            return state;
    }
}

static bool messagesEqual(const QVariant& firstMessage, const QVariant& secondMessage) {
    int type = firstMessage.userType();
    if (secondMessage.userType() != type) {
        return false;
    }
    if (type == TestMessageA::Type) {
        return firstMessage.value<TestMessageA>() == secondMessage.value<TestMessageA>();
    
    } else if (type == TestMessageB::Type) {
        TestMessageB first = firstMessage.value<TestMessageB>();
        TestMessageB second = secondMessage.value<TestMessageB>();
        return first.foo == second.foo && equals(first.bar, second.bar) && first.baz == second.baz;
        
    } else if (type == TestMessageC::Type) {
        return firstMessage.value<TestMessageC>() == secondMessage.value<TestMessageC>();
    
    } else {
        return firstMessage == secondMessage;
    }
}

bool Endpoint::simulate(int iterationNumber) {
    // update/send our delayed datagrams
    for (QList<QPair<QByteArray, int> >::iterator it = _delayedDatagrams.begin(); it != _delayedDatagrams.end(); ) {
        if (it->second-- == 1) {
            _other->_sequencer->receivedDatagram(it->first);
            datagramsReceived++;    
            it = _delayedDatagrams.erase(it);
        
        } else {
            it++;
        }
    }

    if (_mode == METAVOXEL_CLIENT_MODE) {
        Bitstream& out = _sequencer->startPacket();
    
        ClientStateMessage state = { _lod };
        out << QVariant::fromValue(state);
        _sequencer->endPacket();
        
        // record the send
        SendRecord record = { _sequencer->getOutgoingPacketNumber(), SharedObjectPointer(), MetavoxelData(), _lod };
        _sendRecords.append(record);
        return false;
    }
    if (_mode == METAVOXEL_SERVER_MODE) {
        // wait until we have a valid lod
        if (!_lod.isValid()) {
            return false;
        }
        Bitstream& out = _sequencer->startPacket();
        out << QVariant::fromValue(MetavoxelDeltaMessage());
        _data.writeDelta(_sendRecords.first().data, _sendRecords.first().lod, out, _lod);
        _sequencer->endPacket();
        
        // record the send
        SendRecord record = { _sequencer->getOutgoingPacketNumber(), SharedObjectPointer(), _data, _lod };
        _sendRecords.append(record);
        return false;
    }

    // enqueue some number of high priority messages
    const float MIN_HIGH_PRIORITY_MESSAGES = 0.0f;
    const float MAX_HIGH_PRIORITY_MESSAGES = 2.0f;
    _highPriorityMessagesToSend += randFloatInRange(MIN_HIGH_PRIORITY_MESSAGES, MAX_HIGH_PRIORITY_MESSAGES);   
    while (_highPriorityMessagesToSend >= 1.0f) {
        QVariant message = createRandomMessage();
        _highPriorityMessagesSent.append(message);
        _sequencer->sendHighPriorityMessage(message);
        highPriorityMessagesSent++;
        _highPriorityMessagesToSend -= 1.0f;
    }
    
    // and some number of reliable messages
    const float MIN_RELIABLE_MESSAGES = 0.0f;
    const float MAX_RELIABLE_MESSAGES = 4.0f;
    _reliableMessagesToSend += randFloatInRange(MIN_RELIABLE_MESSAGES, MAX_RELIABLE_MESSAGES);   
    while (_reliableMessagesToSend >= 1.0f) {
        QVariant message = createRandomMessage();
        _reliableMessagesSent.append(message);
        _sequencer->getReliableOutputChannel()->sendMessage(message);
        reliableMessagesSent++;
        _reliableMessagesToSend -= 1.0f;
    }
    
    // tweak the local state
    _localState = mutate(_localState);
    
    // send a packet
    try {
        Bitstream& out = _sequencer->startPacket();
        SequencedTestMessage message = { iterationNumber, createRandomMessage(), _localState };
        _unreliableMessagesSent.append(message);
        unreliableMessagesSent++;
        out << message;
        _sequencer->endPacket();
    
    } catch (const QString& message) {
        qDebug() << message;
        return true;
    }
    
    // record the send
    SendRecord record = { _sequencer->getOutgoingPacketNumber(), _localState };
    _sendRecords.append(record);
    
    return false;
}

void Endpoint::sendDatagram(const QByteArray& datagram) {
    datagramsSent++;
    bytesSent += datagram.size();
    
    // some datagrams are dropped
    const float DROP_PROBABILITY = 0.1f;
    if (randFloat() < DROP_PROBABILITY) {
        return;
    }
    
    // some are received out of order
    const float REORDER_PROBABILITY = 0.1f;
    if (randFloat() < REORDER_PROBABILITY) {
        const int MIN_DELAY = 1;
        const int MAX_DELAY = 5;
        // have to copy the datagram; the one we're passed is a reference to a shared buffer
        _delayedDatagrams.append(QPair<QByteArray, int>(QByteArray(datagram.constData(), datagram.size()),
            randIntInRange(MIN_DELAY, MAX_DELAY)));
        
        // and some are duplicated
        const float DUPLICATE_PROBABILITY = 0.01f;
        if (randFloat() > DUPLICATE_PROBABILITY) {
            return;
        }
    }
    
    _other->_sequencer->receivedDatagram(datagram);
    datagramsReceived++;
    bytesReceived += datagram.size();
}

void Endpoint::handleHighPriorityMessage(const QVariant& message) {
    if (message.userType() == ClearSharedObjectMessage::Type) {
        return;
    }
    if (_other->_highPriorityMessagesSent.isEmpty()) {
        throw QString("Received unsent/already sent high priority message.");
    }
    QVariant sentMessage = _other->_highPriorityMessagesSent.takeFirst();
    if (!messagesEqual(message, sentMessage)) {
        throw QString("Sent/received high priority message mismatch.");
    }
    highPriorityMessagesReceived++;
}

void Endpoint::readMessage(Bitstream& in) {
    if (_mode == METAVOXEL_CLIENT_MODE) {
        QVariant message;
        in >> message;
        handleMessage(message, in);
    
        // record the receipt
        ReceiveRecord record = { _sequencer->getIncomingPacketNumber(), SharedObjectPointer(),
            _data, _sendRecords.first().lod };
        _receiveRecords.append(record);
        return;
    }
    if (_mode == METAVOXEL_SERVER_MODE) {
        QVariant message;
        in >> message;
        handleMessage(message, in);
        
        // record the receipt
        ReceiveRecord record = { _sequencer->getIncomingPacketNumber() };
        _receiveRecords.append(record);
        return;
    }

    SequencedTestMessage message;
    in >> message;
    
    _remoteState = message.state;
    
    // record the receipt
    ReceiveRecord record = { _sequencer->getIncomingPacketNumber(), message.state };
    _receiveRecords.append(record);
    
    for (QList<SequencedTestMessage>::iterator it = _other->_unreliableMessagesSent.begin();
            it != _other->_unreliableMessagesSent.end(); it++) {
        if (it->sequenceNumber == message.sequenceNumber) {
            if (!messagesEqual(it->submessage, message.submessage)) {
                throw QString("Sent/received unreliable message mismatch.");
            }
            if (!it->state->equals(message.state)) {
                throw QString("Delta-encoded object mismatch.");
            }
            _other->_unreliableMessagesSent.erase(_other->_unreliableMessagesSent.begin(), it + 1);
            unreliableMessagesReceived++;
            return;
        }
    }
    throw QString("Received unsent/already sent unreliable message.");
}

void Endpoint::handleReliableMessage(const QVariant& message) {
    if (message.userType() == ClearSharedObjectMessage::Type ||
            message.userType() == ClearMainChannelSharedObjectMessage::Type) {
        return;
    }
    if (_other->_reliableMessagesSent.isEmpty()) {
        throw QString("Received unsent/already sent reliable message.");
    }
    QVariant sentMessage = _other->_reliableMessagesSent.takeFirst();
    if (!messagesEqual(message, sentMessage)) {
        throw QString("Sent/received reliable message mismatch.");
    }
    reliableMessagesReceived++;
}

void Endpoint::readReliableChannel() {
    CircularBuffer& buffer = _sequencer->getReliableInputChannel(1)->getBuffer();
    QByteArray bytes = buffer.read(buffer.bytesAvailable());
    if (_other->_dataStreamed.size() < bytes.size()) {
        throw QString("Received unsent/already sent streamed data.");
    }
    QByteArray compare = _other->_dataStreamed.readBytes(0, bytes.size());
    _other->_dataStreamed.remove(bytes.size());
    if (compare != bytes) {
        throw QString("Sent/received streamed data mismatch.");
    }
    streamedBytesReceived += bytes.size();
}

void Endpoint::clearSendRecordsBefore(int index) {
    _sendRecords.erase(_sendRecords.begin(), _sendRecords.begin() + index + 1);
}

void Endpoint::clearReceiveRecordsBefore(int index) {
    _receiveRecords.erase(_receiveRecords.begin(), _receiveRecords.begin() + index + 1);
}

void Endpoint::handleMessage(const QVariant& message, Bitstream& in) {
    int userType = message.userType();
    if (userType == ClientStateMessage::Type) {
        ClientStateMessage state = message.value<ClientStateMessage>();
        _lod = state.lod;
    
    } else if (userType == MetavoxelDeltaMessage::Type) {
        _data.readDelta(_receiveRecords.first().data, _receiveRecords.first().lod, in, _sendRecords.first().lod);
    
    } else if (userType == QMetaType::QVariantList) {
        foreach (const QVariant& element, message.toList()) {
            handleMessage(element, in);
        }
    }
}

TestSharedObjectA::TestSharedObjectA(float foo, TestEnum baz, TestFlags bong) :
        _foo(foo),
        _baz(baz),
        _bong(bong) {
    sharedObjectsCreated++; 
    
    _bizzle = ScriptCache::getInstance()->getEngine()->newObject();
    _bizzle.setProperty("foo", ScriptCache::getInstance()->getEngine()->newArray(4));
}

TestSharedObjectA::~TestSharedObjectA() {
    sharedObjectsDestroyed++;
}

void TestSharedObjectA::setFoo(float foo) {
    if (_foo != foo) {
        emit fooChanged(_foo = foo);
    }
}

TestSharedObjectB::TestSharedObjectB(float foo, const QByteArray& bar, TestEnum baz, TestFlags bong) :
        _foo(foo),
        _bar(bar),
        _baz(baz),
        _bong(bong) {
    sharedObjectsCreated++;
}

TestSharedObjectB::~TestSharedObjectB() {
    sharedObjectsDestroyed++;
}
