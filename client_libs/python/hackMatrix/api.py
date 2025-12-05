import os
import zmq
import hackMatrix.protos.api_pb2 as api_pb2


class Client:
    def __init__(self, address: str = None) -> None:
        if address is None:
            address = os.getenv("VOXEL_API_ADDRESS", "tcp://127.0.0.1:4455")
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        self.socket.connect(address)
        self.noPayload = api_pb2.NoPayload()

    def _send(self, request: api_pb2.ApiRequest) -> api_pb2.ApiRequestResponse:
        serialized = request.SerializeToString()
        self.socket.send(serialized)
        data = self.socket.recv()
        response = api_pb2.ApiRequestResponse()
        response.ParseFromString(data)
        return response

    def turnKey(self, entityId, onOrOff):
        commandMessage = api_pb2.TurnKey(on=onOrOff)
        apiRequest = api_pb2.ApiRequest(
            entityId=entityId, type="TURN_KEY", turnKey=commandMessage
        )
        return self._send(apiRequest)

    def move(self, entityId, xDelta, yDelta, zDelta, unitsPerSecond):
        commandMessage = api_pb2.Move(
            xDelta=xDelta,
            yDelta=yDelta,
            zDelta=zDelta,
            unitsPerSecond=unitsPerSecond,
        )
        apiRequest = api_pb2.ApiRequest(
            entityId=entityId, type="MOVE", move=commandMessage
        )
        return self._send(apiRequest)

    def player_move(self, position, rotation, unitsPerSecond):
        positionMessage = api_pb2.Vector(
            x=position[0], y=position[1], z=position[2]
        )
        frontMessage = api_pb2.Vector(
            x=rotation[0], y=rotation[1], z=rotation[2]
        )
        commandMessage = api_pb2.PlayerMove(
            position=positionMessage, rotation=frontMessage, unitsPerSecond=unitsPerSecond
        )
        apiRequest = api_pb2.ApiRequest(
            entityId=0, type="PLAYER_MOVE", playerMove=commandMessage
        )
        return self._send(apiRequest)

    def unfocus_app(self):
        apiRequest = api_pb2.ApiRequest(
            entityId=0, type="UNFOCUS_WINDOW", noPayload=self.noPayload
        )
        return self._send(apiRequest)

    def add_voxels(self, positions, replace=False, size=1.0):
        voxels_msg = api_pb2.AddVoxels(
            replace=replace,
            size=size,
            voxels=[api_pb2.VoxelCoord(x=p[0], y=p[1], z=p[2]) for p in positions],
        )
        apiRequest = api_pb2.ApiRequest(
            entityId=0, type="ADD_VOXELS", addVoxels=voxels_msg
        )
        return self._send(apiRequest)

    def clear_voxels(self, x_range, y_range, z_range):
        clear_msg = api_pb2.ClearVoxels(
            x=api_pb2.Range(min=min(x_range), max=max(x_range)),
            y=api_pb2.Range(min=min(y_range), max=max(y_range)),
            z=api_pb2.Range(min=min(z_range), max=max(z_range)),
        )
        apiRequest = api_pb2.ApiRequest(
            entityId=0, type="CLEAR_VOXELS", clearVoxels=clear_msg
        )
        response = self._send(apiRequest)
        return response.actionId

    def confirm_action(self, action_id: int):
        confirm_msg = api_pb2.ConfirmAction(actionId=action_id)
        apiRequest = api_pb2.ApiRequest(
            entityId=0, type="CONFIRM_ACTION", confirmAction=confirm_msg
        )
        return self._send(apiRequest)


if __name__ == "__main__":
    client = Client()
    clear_id = client.clear_voxels((-1, 1), (0, 2), (-1, 1))
    client.confirm_action(clear_id)
