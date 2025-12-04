import zmq
import hackMatrix.protos.api_pb2 as api_pb2


class Client:
    def __init__(self, address: str = "tcp://127.0.0.1:4455") -> None:
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        self.socket.connect(address)
        self.noPayload = api_pb2.NoPayload()

    def _send(self, request: api_pb2.ApiRequest) -> None:
        serialized = request.SerializeToString()
        self.socket.send(serialized)
        self.socket.recv()

    def turnKey(self, entityId, onOrOff):
        commandMessage = api_pb2.TurnKey(on=onOrOff)
        apiRequest = api_pb2.ApiRequest(
            entityId=entityId, type="TURN_KEY", turnKey=commandMessage
        )
        self._send(apiRequest)

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
        self._send(apiRequest)

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
        self._send(apiRequest)

    def unfocus_app(self):
        apiRequest = api_pb2.ApiRequest(
            entityId=0, type="UNFOCUS_WINDOW", noPayload=self.noPayload
        )
        self._send(apiRequest)

    def add_voxels(self, positions, replace=False, size=1.0):
        voxels_msg = api_pb2.AddVoxels(
            replace=replace,
            size=size,
            voxels=[api_pb2.VoxelCoord(x=p[0], y=p[1], z=p[2]) for p in positions],
        )
        apiRequest = api_pb2.ApiRequest(
            entityId=0, type="ADD_VOXELS", addVoxels=voxels_msg
        )
        self._send(apiRequest)


if __name__ == "__main__":
    client = Client()
    client.add_voxels([(0, 4, 4)], replace=True, size=2.0)
