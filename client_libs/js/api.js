const protobuf = require("protobufjs");
const zmq = require("zeromq");
const proto = protobuf.loadSync("../../protos/api.proto");
const ApiRequestResponse = proto.lookupType("ApiRequestResponse");
const ApiRequest = proto.lookupType("ApiRequest");
const AddVoxels = proto.lookupType("AddVoxels");
const ClearVoxels = proto.lookupType("ClearVoxels");
const ConfirmAction = proto.lookupType("ConfirmAction");
const Range = proto.lookupType("Range");
const VoxelCoord = proto.lookupType("VoxelCoord");

const socket = new zmq.Request();
async function init(address) {
    const target = address || process.env.VOXEL_API_ADDRESS || "tcp://127.0.0.1:4455";
    await socket.connect(target);
}

async function addVoxels(voxels, replace = false, size = 1.0) {
    const addVoxels = AddVoxels.create({
        replace,
        size,
        voxels: voxels.map(v => VoxelCoord.create({ x: v[0], y: v[1], z: v[2] })),
    });
    const request = ApiRequest.create({
        type: proto.MessageType.ADD_VOXELS,
        addVoxels,
    });
    const serializedRequest = ApiRequest.encode(request).finish();
    await socket.send(serializedRequest);
    const [result] = await socket.receive();
    return ApiRequestResponse.decode(result);
}

async function clearVoxels(xRange, yRange, zRange) {
    const clearVoxels = ClearVoxels.create({
        x: Range.create({ min: Math.min(...xRange), max: Math.max(...xRange) }),
        y: Range.create({ min: Math.min(...yRange), max: Math.max(...yRange) }),
        z: Range.create({ min: Math.min(...zRange), max: Math.max(...zRange) }),
    });
    const request = ApiRequest.create({
        type: proto.MessageType.CLEAR_VOXELS,
        clearVoxels,
    });
    const serializedRequest = ApiRequest.encode(request).finish();
    await socket.send(serializedRequest);
    const [result] = await socket.receive();
    return ApiRequestResponse.decode(result).actionId;
}

async function confirmAction(actionId) {
    const confirm = ConfirmAction.create({ actionId });
    const request = ApiRequest.create({
        type: proto.MessageType.CONFIRM_ACTION,
        confirmAction: confirm,
    });
    const serializedRequest = ApiRequest.encode(request).finish();
    await socket.send(serializedRequest);
    const [result] = await socket.receive();
    return ApiRequestResponse.decode(result);
}


module.exports = {
    init,
    addVoxels,
    clearVoxels,
    confirmAction,
};
