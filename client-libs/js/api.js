// Write a path finder
const protobuf = require("protobufjs");
const zmq = require("zeromq");
const root = protobuf.loadSync("../../include/protos/api.proto");
const AddCube = root.lookupType("AddCube");

const socket = new zmq.Request();
async function init() {
    await socket.connect("tcp://127.0.0.1:3333");
}

async function addCube(x, y, z, blockType) {


    const request = AddCube.encode({ x, y, z, blockType }).finish();

    // Send the request to the server
    socket.send(request);
    const [result] = await socket.receive();
}


module.exports = {
    init,
    addCube
};
