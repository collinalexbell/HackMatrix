import zmq
import hackMatrix.protos.api_pb2  as api_pb2

context = zmq.Context()

socket = context.socket(zmq.REQ)
socket.connect("tcp://127.0.0.1:3333")

def turnKey(entityId, onOrOff):
    turnKeyMessage = api_pb2.TurnKey(on=onOrOff)
    apiRequest = api_pb2.ApiRequest(entityId=entityId,
                                    type="TURN_KEY",
                                    turnKey=turnKeyMessage)
    serializedRequest = apiRequest.SerializeToString()
    socket.send(serializedRequest)
    socket.recv()


if __name__ == "__main__":
    pass
