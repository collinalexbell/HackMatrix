import zmq
import hackMatrix.protos.api_pb2  as api_pb2

context = zmq.Context()

socket = context.socket(zmq.REQ)
socket.connect("tcp://127.0.0.1:3333")

def add_cube(x, y, z, block_type):
    # Create an AddCube message
    add_cube_msg = api_pb2.AddCube(x=x, y=y, z=z, blockType=block_type)

    # Create an API request
    api_request = api_pb2.ApiRequest(type="ADD_CUBE", addCube=add_cube_msg)

    serialized_request = api_request.SerializeToString()

    # Send the serialized request to the server
    socket.send(serialized_request)

    # Receive the response
    socket.recv()

def clear_box(x1, y1, z1, x2, y2, z2):
    # Create a ClearBox message
    clear_box_msg = api_pb2.ClearBox(x1=x1, y1=y1, z1=z1, x2=x2, y2=y2, z2=z2)

    # Create an API request
    api_request = api_pb2.ApiRequest(type="CLEAR_BOX", clearBox=clear_box_msg)

    serialized_request = api_request.SerializeToString()

    # Send the serialized request to the server
    socket.send(serialized_request)

    # Receive the response
    socket.recv()

def add_line(x1, y1, z1, x2, y2, z2):
    add_line_msg = api_pb2.AddLine(x1=x1, y1=y1, z1=z1, x2=x2, y2=y2, z2=z2)

    # Create an API request
    api_request = api_pb2.ApiRequest(type="ADD_LINE", addLine=add_line_msg)

    serialized_request = api_request.SerializeToString()

    # Send the serialized request to the server
    socket.send(serialized_request)

    # Receive the response
    socket.recv()



if __name__ == "__main__":
    add_cube(2, 2, 2, 0)
