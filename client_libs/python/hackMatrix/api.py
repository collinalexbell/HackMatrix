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

class Cube:
    def __init__(self, x,y,z,block_type):
        self.x = x
        self.y = y
        self.z = z
        self.block_type = block_type

def add_cubes(cubes):
    cube_msgs = []
    for _, cube in enumerate(cubes):
        add_cube_msg = api_pb2.AddCube(
            x=cube.x, y=cube.y, z=cube.z, blockType=cube.block_type)
        cube_msgs.append(add_cube_msg)

    add_cubes_msg = api_pb2.AddCubes(cubes=cube_msgs)
    # Create an API request
    api_request = api_pb2.ApiRequest(type="ADD_CUBES", addCubes=add_cubes_msg)

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

def add_line(x1, y1, z1, x2, y2, z2, intensity):
    add_line_msg = api_pb2.AddLine(x1=x1, y1=y1, z1=z1, x2=x2, y2=y2, z2=z2, intensity=intensity)

    # Create an API request
    api_request = api_pb2.ApiRequest(type="ADD_LINE", addLine=add_line_msg)

    serialized_request = api_request.SerializeToString()

    # Send the serialized request to the server
    socket.send(serialized_request)

    # Receive the response
    socket.recv()



if __name__ == "__main__":
    add_cube(2, 2, 2, 0)
