// Write a path finder
const protobuf = require("protobufjs");
const zmq = require("zeromq");
const root = protobuf.loadSync("../../include/protos/api.proto");
const AddCube = root.lookupType("AddCube");

async function runRequester() {
    const socket = new zmq.Request();

    await socket.connect("tcp://127.0.0.1:3333");

    const request = AddCube.encode({ x:4, y:4, z:4, blockType:0 }).finish();

    // Send the request to the server
    socket.send(request);
    const [result] = await socket.receive();
}

runRequester();
