// Write a path finder
const protobuf = require("protobufjs");
const zmq = require("zeromq");
const root = protobuf.loadSync("include/protos/api.proto");
const AddCube = root.lookupType("AddCube");

async function runRequester() {
    const requester = new zmq.Request();

    await requester.connect("tcp://127.0.0.1:3333");

    const request = AddCube.encode({ x:2, y:0, z:0, blockType:1 }).finish();

    requester.on("message", (response) => {});

    // Send the request to the server
    requester.send(request);
    const [result] = await requester.receive();
}

runRequester();
