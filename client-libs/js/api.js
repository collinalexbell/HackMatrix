// Write a path finder
const protobuf = require("protobufjs");
const zmq = require("zeromq");
const root = protobuf.loadSync("include/protos/api.proto");
const AddCube = root.lookupType("AddCube");

async function runRequester() {
    const requester = new zmq.Request();

    await requester.connect("tcp://127.0.0.1:5555");

    const request = AddCube.encode({ x:4, y:4, z:4, blockType:0 }).finish();

    requester.on("message", (response) => {});

    // Send the request to the server
    requester.send(request);
}

runRequester();
