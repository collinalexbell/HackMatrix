// Write a path finder
const protobuf = require("protobufjs");
const zmq = require("zeromq");
const proto = protobuf.loadSync("../../include/protos/api.proto");
const AddCube = proto.lookupType("AddCube");
const ApiRequest = proto.lookupType("ApiRequest");

const socket = new zmq.Request();
async function init() {
    await socket.connect("tcp://127.0.0.1:3333");
}

async function addCube(x, y, z, blockType) {
    // Create an AddCube message
    const addCube = AddCube.create({x,y,z,blockType});

    // Create an API request
    const request = ApiRequest.create({type:proto.MessageType.ADD_CUBE,
                                       addCube: addCube});

    const serializedRequest = ApiRequest.encode(request).finish();
    // Send the serialized request to the server
    socket.send(serializedRequest);

    // Receive the response
    const [result] = await socket.receive();
}



module.exports = {
    init,
    addCube
};
