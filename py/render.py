import numpy as np
from net import Net
from hackMatrix.api import add_cube,clear_box,add_line
import time

model = Net()
model.load('mnist')
activity = np.load('activity.npz')

# Layer units
in_z, in_x = np.indices((28, 28))
fc1_z, fc1_x = np.indices((32, 32))
fc2_z, fc2_x = np.indices((32, 32))
fc3_z, fc3_x = np.indices((32, 32))
in_x = in_x.ravel() - 12
in_z = in_z.ravel() - 12
in_y = np.zeros_like(in_x)
fc1_x = fc1_x.ravel() - 16
fc1_z = fc1_z.ravel() - 16
fc1_y = np.ones_like(fc1_x) + 10
fc2_x = fc2_x.ravel() - 16
fc2_z = fc2_z.ravel() - 16
fc2_y = np.ones_like(fc2_x) + 30
fc3_x = fc3_x.ravel() - 16
fc3_z = fc3_z.ravel() - 16
fc3_y = np.ones_like(fc3_x) + 50
out_x = np.arange(10)
out_x = out_x.ravel() - 5
out_y = np.ones_like(out_x) + 80
out_z = np.zeros_like(out_x)

fc1_x = fc1_x + np.random.rand(len(fc1_x)) * 1
fc1_z = fc1_z + np.random.rand(len(fc1_z)) * 1
fc1_y = fc1_y + np.random.rand(len(fc1_y)) * 10
fc2_x = fc2_x + np.random.rand(len(fc2_x)) * 1
fc2_z = fc2_z + np.random.rand(len(fc2_z)) * 1
fc2_y = fc2_y + np.random.rand(len(fc2_y)) * 10
fc3_x = fc3_x + np.random.rand(len(fc3_x)) * 1
fc3_z = fc3_z + np.random.rand(len(fc3_z)) * 1
fc3_y = fc3_y + np.random.rand(len(fc3_y)) * 10
out_x = out_x * 3

# Connections between layers
fc1 = model.fc1.weight.detach().numpy().T
print(fc1.shape)
fc2 = model.fc2.weight.detach().numpy().T
print(fc2.shape)
fc3 = model.fc3.weight.detach().numpy().T
print(fc3.shape)
out = model.fc4.weight.detach().numpy().T
print(out.shape)
fr_in, to_fc1 = (np.abs(fc1) > 0.1).nonzero()
fr_fc1, to_fc2 = (np.abs(fc2) > 0.05).nonzero()
fr_fc2, to_fc3 = (np.abs(fc3) > 0.05).nonzero()
fr_fc3, to_out = (np.abs(out) > 0.1).nonzero()
fr_fc1 += len(in_x)
to_fc1 += len(in_x)
fr_fc2 += len(in_x) + len(fc1_x)
to_fc2 += len(in_x) + len(fc1_x)
fr_fc3 += len(in_x) + len(fc1_x) + len(fc2_x)
to_fc3 += len(in_x) + len(fc1_x) + len(fc2_x)
to_out += len(in_x) + len(fc1_x) + len(fc2_x) + len(fc3_x)

# Create the points
x = np.hstack((in_x, fc1_x, fc2_x, fc3_x, out_x))
y = np.hstack((in_y, fc1_y, fc2_y, fc3_y, out_y))
z = np.hstack((in_z, fc1_z, fc2_z, fc3_z, out_z))

xOffset = 40
yOffset = 5
zOffset = 110

x = np.round(x).astype(int) + xOffset
y = np.round(y).astype(int) + yOffset
z = np.round(z).astype(int) + zOffset

while(True):
    for i in range(100):
        clear_box(min(x), min(y), min(z), max(x), max(y)+6, max(z))
        act_input = activity['input'][i]
        act_fc1 = activity['fc1'][i]
        act_fc2 = activity['fc2'][i]
        act_fc3 = activity['fc3'][i]
        act_out = activity['fc4'][i]
        out = activity['output'][i]
        s = np.hstack((
            act_input.ravel() / act_input.max(),
            act_fc1 / act_fc1.max(),
            act_fc2 / act_fc2.max(),
            act_fc3 / act_fc3.max(),
            act_out / act_out.max(),
        ))


        for xi, yi, zi,si in zip(x,y,z,s):
            if(si > 0.85):
                add_cube(xi, yi, zi, 4)
            else:
                add_cube(xi, yi, zi, 0)

        connections = np.vstack((
            np.hstack((fr_in, fr_fc1, fr_fc2, fr_fc3)),
            np.hstack((to_fc1, to_fc2, to_fc3, to_out)),
        )).T

        print(x.shape)
        for _,connection in enumerate(connections):
            a = connection[0]
            b = connection[1]
            add_line(x[a]/10.0, y[a]/10.0, z[a]/10.0, x[b]/10.0, y[b]/10.0, z[b]/10.0)
            time.sleep(0.1)

        time.sleep(5)

