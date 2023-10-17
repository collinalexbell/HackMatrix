// Write a path finder
const protobuf = require("protobufjs");
const zmq = require("zeromq");
const root = protobuf.loadSync("../../include/protos/api.proto");
const AddCube = root.lookupType("AddCube");

async function addCube(x, y, z, blockType) {
    const socket = new zmq.Request();

    await socket.connect("tcp://127.0.0.1:3333");

    const request = AddCube.encode({ x, y, z, blockType }).finish();

    // Send the request to the server
    socket.send(request);
    const [result] = await socket.receive();
}


module.exports = {
    addCube
};
