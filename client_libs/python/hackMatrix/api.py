import zmq
import hackMatrix.protos.api_pb2  as api_pb2

context = zmq.Context()

socket = context.socket(zmq.REQ)
socket.connect("tcp://127.0.0.1:3333")

def turnKey(entityId, onOrOff):
    commandMessage = api_pb2.TurnKey(on=onOrOff)
    apiRequest = api_pb2.ApiRequest(entityId=entityId,
                                    type="TURN_KEY",
                                    turnKey=commandMessage)
    serializedRequest = apiRequest.SerializeToString()
    socket.send(serializedRequest)
    socket.recv()

def move(entityId, xDelta, yDelta, zDelta, unitsPerSecond):
    commandMessage = api_pb2.Move(xDelta=xDelta, yDelta=yDelta, zDelta=zDelta,
                                  unitsPerSecond=unitsPerSecond )
    apiRequest = api_pb2.ApiRequest(entityId=entityId,
                                    type="MOVE",
                                    move=commandMessage)
    serializedRequest = apiRequest.SerializeToString()
    socket.send(serializedRequest)
    socket.recv()

def player_move(entityId, position, rotation, unitsPerSecond):
    positionMessage = api_pb2.Vector(x=position[0], y=position[1], z=position[2])
    frontMessage = api_pb2.Vector(x=rotation[0], y=rotation[1], z=rotation[2])
    commandMessage = api_pb2.PlayerMove(position=positionMessage,
                                        rotation=frontMessage,
                                  unitsPerSecond=unitsPerSecond )
    apiRequest = api_pb2.ApiRequest(entityId=0,
                                    type="PLAYER_MOVE",
                                    playerMove=commandMessage)
    serializedRequest = apiRequest.SerializeToString()
    socket.send(serializedRequest)
    socket.recv()






if __name__ == "__main__":
    pass
