import numpy as np
from tinygrad.nn.state import get_parameters
from tinygrad.nn import Linear
from tinygrad.helpers import getenv

GPU = getenv("GPU")


class Net():
    def __init__(self):
        self.fc1 = Linear(28 * 28, 32 * 32)
        self.fc2 = Linear(32 * 32, 32 * 32)
        self.fc3 = Linear(32 * 32, 32 * 32)
        self.fc4 = Linear(32 * 32, 10)

    def forward(self, x):
        x = x.reshape([-1, 28*28])
        x = self.fc1(x)
        x = x.leakyrelu()
        x = self.fc2(x)
        x = x.leakyrelu()
        x = self.fc3(x)
        x = x.leakyrelu()
        x = self.fc4(x)
        x = x.leakyrelu()
        return x.log_softmax()

    def save(self, filename):
        with open(filename+'.npy', 'wb') as f:
            for par in get_parameters(self):
                np.save(f, par.numpy())

    def load(self, filename):
        with open(filename+'.npy', 'rb') as f:
            for par in get_parameters(self):
                #if par.requires_grad:
                try:
                    par.numpy()[:] = np.load(f)
                except:
                    print('Could not load parameter')
